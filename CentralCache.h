#pragma once

#include "Common.h"

// 单例模式 懒汉
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	Span* GetOneSpan(SpanList& list, size_t byte_size);

	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	void ReleaseListToSpans(void* start, size_t size);

	
private:
	SpanList _spanLists[NFREELIST];

private:
	static CentralCache _sInst; // 静态实例，确保只有一个 CentralCache 实例存在

	CentralCache() = default;
	CentralCache(const CentralCache&) = delete;
};