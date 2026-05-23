#include "PageCache.h"

PageCache PageCache::_sInst;

// 获取一个K页的span
Span* PageCache::NewSpan(size_t k)
{
	assert(k > 0);

	// 大于128 page，直接向系统申请
	if (k > NPAGES-1)
	{
		void* ptr = SystemAlloc(k);
		//Span* span = new Span;
		Span* span = _spanPool.New();

		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;

		// 建立id和span的映射，大内存也要映射所有页面
		for (PAGE_ID i = 0; i < span->_n; ++i)
		{
			_idSpanMap.set(span->_pageId + i, span);
		}

		return span;
	}

	// 先检查第k个桶里面有没有span
	if (!_spanLists[k].Empty())
	{
		Span* kSpan = _spanLists[k].PopFront();

		// 建立id和span的映射，方便central cache回收小块内存时查找对应的span
		for (PAGE_ID i = 0; i < kSpan->_n; ++i)
		{
			// 建立id和span的映射，方便central cache回收小块内存时查找对应的span
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}

		return kSpan;
	}

	// 检查一下后面的桶里面有没有span，如果有可以把它进行切分
	for (size_t i = k+1; i < NPAGES; ++i)
	{
		if (!_spanLists[i].Empty())
		{
			Span* nSpan = _spanLists[i].PopFront();
			//Span* kSpan = new Span;
			Span* kSpan = _spanPool.New();

			// 在nSpan的头上切一个k页下来
			// k页span返回
			// nSpan再挂到对应映射位置
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;

			_spanLists[nSpan->_n].PushFront(nSpan);
			// 存储nSpan的首尾页号给nSpan映射，方便page cache释放内存时
			// 进行的合并查找
			// 存储nSpan的首尾页号给nSpan映射，方便page cache释放内存时进行的合并查找
			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

			// 建立id和span的映射，方便central cache回收小块内存时查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i)
			{
				// 映射页面ID到span
				_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}

			return kSpan;
		}
	}

	// 走到这个位置就说明后面没有大页的span了
	// 这时就去找堆要一个128页的span
	//Span* bigSpan = new Span;
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;

	_spanLists[bigSpan->_n].PushFront(bigSpan);

	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);

	// 使用基数树快速查找页面ID对应的span
	auto ret = (Span*)_idSpanMap.get(id);
	
	// 基数树查找失败，这不应该发生
	assert(ret != nullptr);
	
	return ret;
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	// 大于128 page，直接还给堆
	if (span->_n > NPAGES-1)
	{
		// 清理所有页面的映射
		for (PAGE_ID i = 0; i < span->_n; ++i)
		{
			_idSpanMap.set(span->_pageId + i, nullptr);
		}
		
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}

	// 向span前面的页，尝试进行合并，缓解内存碎片问题
	while (1)
	{
		PAGE_ID prevId = span->_pageId - 1;
		// 查找前一页是否存在对应的span

		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr)
		{
			break;
		}

		// 前面相邻页的span在使用，不合并了
		Span* prevSpan = ret;
		if (prevSpan->_isUse == true)
		{
			break;
		}

		// 合并出超过128页的span没办法管理，不合并了
		if (prevSpan->_n + span->_n > NPAGES-1)
		{
			break;
		}

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_spanLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	// 向后合并
	while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;
		// 查找后一页是否存在对应的span

		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}

		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > NPAGES-1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}

	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;
	// 重新建立首尾页面映射

	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}