// ConcurrentAlloc.h
// This file is part of a C++ project that implements a thread-local storage (TLS)
// memory allocation system using a thread cache. It provides functions for allocating
// and deallocating memory in a thread-safe manner, leveraging the ThreadCache class.

#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	// 如果超过大内存设定，大内存都会执行该逻辑
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;

		// 通过PageCache的NewSpan接口，直接向系统申请这样的内存
		// 因为PageCache的spanLists最大页数为128，此时已经超过128页
		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else // 小内存执行 else
	{
		if (pTLSThreadCache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;
			pTLSThreadCache = tcPool.New(); // 使用对象池
		}

		//cout << "Thread ID: " << std::this_thread::get_id() << ":" << pTLSThreadCache << endl; // 这行用于调试，查看每个线程的 ThreadCache 实例地址

		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;

	// 大于线程要管理的ThreadCache的内存大小MAX_BYTES
	if(size > MAX_BYTES)
	{
		// 因为之前超过MAX_BYTES的内存都是直接向系统申请
		// 所以直接通过PageCache的ReleaseSpanToPageCache接口直接通过系统调用释放
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else // 小内存
	{
		assert(ptr && pTLSThreadCache);

		pTLSThreadCache->Deallocate(ptr, size);
	}
}