#include "CentralCache.h"
#include "PageCache.h"
#include "Common.h"

CentralCache CentralCache::_sInst; // 静态实例，确保只有一个 CentralCache 实例存在

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	// 两种情况：1. span list中有span；2. span list中没有span，需要从page cache中获取新的span

	// 这里需要实现获取一个 Span 的逻辑
	// 可能需要从 _spanLists 中选择一个合适的 Span
	// 并且需要考虑 Span 的大小是否满足请求的 size
	// 如果没有合适的 Span，则可能需要创建一个新的 Span

	// 1. 先查找当前桶中是否有可用的Span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next; // 如果当前 Span 的自由链表为空，则继续查找下一个 Span
		}

	}

	//  2. 没找到可用Span，需要从PageCache获取新的
	list._mtx.unlock(); // 🔓 ：先解锁CentralCache的桶，防止有其他线程释放内存对象回来的时候阻塞

	// 3. 向PageCache申请新Span
	PageCache::GetInstance()->_pageMtx.lock(); // 🔒 ：加锁PageCache的全局锁，防止其他线程同时修改
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock(); // 🔓 ：解锁PageCache的全局锁

	// 4. 初始化新Span
	// 切分 span 的过程不需要加锁，因为这个时候其他线程不会访问到这个 span
	char* start = (char*)(span->_pageId << PAGE_SHIFT); // 计算 Span 的起始地址   char* 是为了方便地址计算，_pageId 是 Span 在 PageCache 中的页 ID，PAGE_SHIFT 是每页的大小（通常是 8 KB）
	size_t bytes = span->_n << PAGE_SHIFT; // 计算拿到的内存块的总大小  _n (span内页数) * PAGE_SHIFT (每页大小 = 8 KB)  
	char* end = start + bytes; // 计算 Span 的结束地址
	 
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList; // 初始化尾指针为自由链表的起始地址 方便之后链接
	while (start < end)
	{
		NextObj(tail) = start; // 将当前对象的下一个指针指向下一个对象
		tail = NextObj(tail); // 更新尾指针到下一个对象
		start += size; // 移动到下一个对象
	}
	NextObj(tail) = nullptr; // 最后一个对象的下一个指针设置为 nullptr，表示链表结束


	list._mtx.lock(); // 🔒
	// 并且在之在当前函数内不用解锁，因为当走到这个去 PageCache 内拿内存的逻辑时
	// ，前面已经把 FetchRangeObj加锁的 list解锁了，之后直接回调到 FetchRangeObj 的后面会解锁这个，相当于把之前解的锁还回去了
	list.PushFront(span); // 将新创建的 Span 插入到 SpanList 的前面
	// 为什么上面要返回 span 到原 SpanList 中？
	// Span 可能还有剩余对象供后续分配，所以需要将其重新加入管理列表，并且该Span中所有分出去的内存块回收后还要将整体Span回收给 PageCache
	/*
	1. Span的双重身份

		- 作为容器：span包含一个切分好的对象链表(_freeList)
		- 作为管理单元：span需要被CentralCache管理和跟踪

	2. 为什么要插入回list

		list.PushFront(span);  // 将span重新加入管理列表

	   原因：
		- Span可能还有剩余对象供后续分配
		- CentralCache需要跟踪所有活跃的span
		- 后续其他ThreadCache请求时可能还会用到这个span
	*/

	return span; // 返回新创建的 Span
}

// 从span里面获取一段内存块，返回实际获取的数量
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	
	// 桶锁
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1; // 实际获取了多少个 指向start的时候实际已经拿了一个了，所以初始为 1    因为可能找到的span里面不一定有batchNum个对象
	while (i < batchNum && ( NextObj(end) != nullptr ))
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}

	// 将获取的范围结尾 end 后面的其他对象接着链接回 span 的自由链表中，然后把begin和end之间的对象都从 span 的自由链表中取出
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;

	span->_useCount += actualNum; // 更新 span 的使用计数

	_spanLists[index]._mtx.unlock();

	return actualNum;
}

// 将准备好的对象链表回收到中央缓存
// 传入 start ，到 nullptr 为止
// size 用来找到放在哪个 spanlist
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size); // 放进 index 索引下的桶
	_spanLists[index]._mtx.lock(); // 对要放入的桶进行加锁

	while (start)
	{
		void* next = NextObj(start); // 把要回收的这个对象链一个一个遍历回收

		// 找到当前对象的 span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// 将对象插入到Span的自由链表头部
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--; // 收回来了一个，-1

		// 如果这个span的_useCount 已经全部收回来了
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);

			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			
			// 释放CentralCache的锁，防止死锁
			// 开始归还span给pageCache
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock(); // 重新加锁，继续处理后续的对象
			// 就加这一个桶，因为从同一个freelist中取出的范围对象肯定是同一个span的
		}

		start = next; // go on next obj.
	}

	_spanLists[index]._mtx.unlock();
}

