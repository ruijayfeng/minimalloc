#pragma once

#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// �� PageCache �л�ȡһ�� Span
	// ���� k ��ʾ�����ҳ��
	Span* NewSpan(size_t k);

	Span* MapObjectToSpan(void* obj);

	void ReleaseSpanToPageCache(Span* span);

	std::mutex _pageMtx; // ���ڱ��� _spanLists �ķ���,PageCache ����������һ������������Ĵ����������Ǻ�Centralһ��ֻ��Ͱ����

private:
	SpanList _spanLists[NPAGES];

	ObjectPool<Span> _spanPool; // ����Span�ڴ��

	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//std::map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap2<28> _idSpanMap;  // 使用28位，支持256M页面(2TB地址空间)

private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;

private:
	static PageCache _sInst; // ��̬ʵ����ȷ��ֻ��һ�� PageCache ʵ������
};