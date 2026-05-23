#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include <time.h>
#include <new>
#include <assert.h>


#include <mutex>
#include <thread>
#include <atomic>

using std::cout;
using std::endl;

/*注意 _WIN64只代表64位系统，而_WIN32代表64位和32位*/
#ifdef _WIN32
	#include <windows.h> // 表示Windows平台（包括32位和64位）   
#else
	// ... 
#endif

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELIST = 208;
static const size_t NPAGES = 129; // 129 个页号，0-128 共 129 个页号
static const size_t PAGE_SHIFT = 13;  // 2^13 = 8192, 8KB per page 设置一页为8KB，则2的13次方为8KB

// 因为 PAGE_ID 的宽度 ≥ 当前平台寻址位数对应的页号最大值
// 将不同平台下的 PAGE_ID 定义为不同的类型，以适应不同的寻址方式
#ifdef _WIN64
	typedef unsigned long long PAGE_ID; 
#elif _WIN32
	typedef size_t PAGE_ID;
#else
	// 其他平台
#endif 


inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// Linux下使用brk或mmap等系统调用
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

//inline static void SystemFree(void* ptr)
//{
//#ifdef _WIN32
//	VirtualFree(ptr, 0, MEM_RELEASE);
//#else
//	// Linux下使用sbrk或unmmap等系统调用
//#endif
//}


inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// Linux下使用sbrk或unmmap等系统调用
#endif
}


// 既可以做左值也可以做右值
// 左值时做写入，将右值某地址写进 next 指针
static void*& NextObj(void* obj)
{
	return *(void**)obj;
}

// 管理切分好的小对象的自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);

		// 头插
		*(void**)obj = _freeList;
		_freeList = obj;

		++_size;
	}

	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;

		_size += n;
	}

	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size); // 确保有足够的对象可以取出

		start = _freeList;
		end = _freeList;

		for (size_t i = 0; i < n - 1; ++ i) // n - 1 因为是从 0 索引向后走的，已经占了 0 
		{
			end = NextObj(end);
		}

		_freeList = NextObj(end); // 将list后续的对象接上
		NextObj(end) = nullptr; // 将拿出的一段范围 end 断开连接，用nullptr 接上
		_size -= n;
	}

	void* Pop()
	{
		assert(_freeList);

		void* obj = _freeList;
		_freeList = NextObj(obj);

		--_size;

		return obj;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0; 
};

// 辅助类：计算内存对齐和索引
class SizeClass
{
public:
	// [1,128]					8byte	    freelist[0,16)
	// [128+1,1024]				16byte	    freelist[16,72)
	// [1024+1,8*1024]			128byte     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte    freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byt   freelist[184,208)
	/*如果全局对齐数都是 8 那将仍然是很大的数量，所以在不同范围内使用不同的对齐数*/

	// 通俗版本，但是位运算对于计算机来说性能更优
	//size_t _RoundUp(size_t size, size_t alignNum)
	//{
	//	size_t alignSize;
	//	if (size % alignNum != 0)           // 1 % 8 = 1 ≠ 0 → 进入 if
	//	{
	//		alignSize = (size / alignNum + 1) * alignNum;
	//		// (1 / 8 + 1) * 8 = (0 + 1) * 8 = 8
	//	}
	//	else
	//	{
	//		alignSize = size;               // 如果刚好整除，直接返回原值
	//	}

	//	return alignSize;                   // 返回 8
	//}


	// static 用来让包含头文件的cc文件都有一份， inline 让编译器在每个使用的地方都生成代码，避免函数调用开销
	
	// 计算size大小应该给分配的合适的实际内存大小 RoudUp的子方法
	static inline size_t _RoundUp(size_t size, size_t alignNum)
	{
		return (size + alignNum - 1) & ~(alignNum - 1);
	}

	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= MAX_BYTES)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			//assert(false && "Size too large for object pool");

			//return -1; // 如果超过最大限制，返回0

			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	// Index 的工具子函数 用来计算不同对齐数下的 bytes 大小对应的桶的编号（自由链表下标）
	// bytes: 待申请的内存大小
	// align_shift: 对齐方式，用2的幂次方表示（如3表示8字节对齐，4表示16字节对齐）
	// 返回值: 计算得到的自由链表下标
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	// 用来计算 RoundUp 后得到的实际内存大小应该对应的哪个编号的桶的自由链表
	// 根据申请的内存大小计算其在自由链表数组中的下标位置
	// bytes: 待申请的内存大小，必须小于等于MAX_BYTES
	// 返回值: 对应的自由链表下标
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// 定义每个内存大小区间的链表个数
		// [1,128]            占用16个链表
		// (128,1024]         占用56个链表
		// (1024,8*1024]      占用56个链表
		// (8*1024,64*1024]   占用56个链表

		// “减去区间起点”是为了让 _Index 计算“段内第几格”，
		// “加上前面所有段的总桶数”是为了把段内下标变成 全局下标。
		static int group_array[4] = { 16, 56, 56, 56 };

		if (bytes <= 128)
		{
			// 对于小于等于128字节的内存，8字节对齐(2^3=8)
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) // 8 * 128
		{
			// 对于(128,1024]字节的内存，16字节对齐(2^4=16)
			// 需要减去128是为了将范围归一化到[0,896]
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if(bytes <= 8 * 1024)
		{
			// 对于(1024,8*1024]字节的内存，128字节对齐(2^7=128)
			// 减去1024将范围归一化，加上前面的偏移量
			return _Index(bytes - 1024, 7) + group_array[0] + group_array[1];
		}
		else if (bytes <= 64 * 1024)
		{
			// 对于(8*1024,64*1024]字节的内存，1024字节对齐(2^10=1024)
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024)
		{
			// 对于(64*1024,256*1024]字节的内存，8*1024字节对齐(2^13=8192)
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}

		return -1;
	}

	// 用来计算对应申请size大小的内存块时候应该一次性让最多拿多少个对象
	// ThreadCache一次性能从central cache拿的最多量
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		// MAX_BYTES = 256 * 1024 
		size_t num = MAX_BYTES / size;

		// 如果 size 过大，可能会导致 num 过小，防止失去批量的效率限制最少为2
		if (num < 2)
		{
			num = 2;
		}
		
		// 如果size过小，可能会导致 num 过大，限制最大为512
		if (num > 512)
		{
			num = 512;
		}

		return num;
	}

	//
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size; // 每次从中央缓存获取的内存块的总大小

		npage >>= PAGE_SHIFT; // 将总大小转换为页数，右移 PAGE_SHIFT 位相当于除以 2^13(一页的大小：8KB)，即除以 8192
		if (npage == 0)
		{
			npage = 1;
		}

		return npage;
	}
};

struct Span
{
	PAGE_ID _pageId = 0;
	size_t _n = 0; // 该 Span 中的页的数量

	Span* _prev = nullptr;
	Span* _next = nullptr;

	size_t _objSize = 0;

	size_t _useCount = 0; // 切好的小块，被分配给 ThreadCache 的计数，为 0 则表示未分配
	void* _freeList = nullptr; // 指向该 Span 中的存放切好小块内存的自由链表的头指针

	bool _isUse = false;
};

class SpanList
{
public:
	SpanList()
	{
		// 形成双向环链
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos && newSpan);

		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_next = pos;
		newSpan->_prev = prev;
		pos->_prev = newSpan;
	}

	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	Span* PopFront()
	{
		assert(!Empty()); // 确保链表不为空
		Span* front = _head->_next; // 获取头结点的下一个节点，即第一个元素
		Erase(front); // 删除第一个元素
		return front; // 返回被删除的元素
	}


	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head); // 因为双向带头链表

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	Span* Begin()
	{
		return _head->_next; // 返回第一个元素的地址
	}

	Span* End()
	{
		return _head; // 返回头结点的地址，表示链表的结束
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

private:
	Span* _head;

public:
	std::mutex _mtx;
};