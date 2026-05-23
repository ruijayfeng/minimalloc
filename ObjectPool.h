#pragma once
#include "Common.h"

// 定长内存池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;

		// 优先把回收的内存再次复用
		if (_freeList)
		{
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			// 剩余内存不够一个对象大小时，则重新开辟空间
			if (_remainBytes < sizeof(T))
			{
				// 确保分配的内存足够容纳对象，至少128KB
				size_t needBytes = sizeof(T) > 128 * 1024 ? sizeof(T) : 128 * 1024;
				_remainBytes = needBytes;
				
				// 计算需要多少页（向上取整）
				size_t needPages = (needBytes + (1 << PAGE_SHIFT) - 1) >> PAGE_SHIFT;
				_memory = (char*)SystemAlloc(needPages);
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory;
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		// 定位new，显示调用T的构造函数初始化
		new(obj)T;

		return obj;
	}

	void Delete(T* obj)
	{
		// 显示调用析构函数清理对象
		obj->~T();

		// 头插
		*(void**)obj = _freeList;
		_freeList = obj;
	}

private:
	char* _memory = nullptr; // 指向大块内存的指针
	size_t _remainBytes = 0; // 大块内存在切分给用户后的剩余字节数

	void* _freeList = nullptr; // 还回来过程中链接的自由链表的头指针
};