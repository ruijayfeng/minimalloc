#pragma once

#include "ThreadCache.h"
#include "Common.h"
#include "CentralCache.h"

// 从中央缓存获取内存块 并将获取到的内存块拿出一块供当前调用的线程先使用 其余的插入对应的自由链表中
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 慢开始反馈调节算法
	// 期望小对象给多一点，大对象给少一点
	// 最开始不要向central cache请求太多对象，因为太多了也用不完
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchNum)
	{
		_freeLists[index].MaxSize()++;
	}

	// start 和 end 用于记录从中央缓存获取的内存块的起始和结束地址
	void* start = nullptr;
	void* end = nullptr;
	
	// 因为中央缓存的 SpanList 上对应的 Span 上面可能不够一次性获取 batchNum 个对象，所以返回实际上获取的数量 actualNum
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0); // 至少也得大于一个

	if (actualNum == 1)
	{
		assert(start == end); // 如果实际获取的数量为1，则 start 和 end 应该是同一个地址
		return start;
	}
	else
	{
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1);

		return start; // 返回起始地址，后续的内存块已经被放入自由链表中
	}

	
	return nullptr;
}

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);

	size_t index = SizeClass::Index(size); // 计算自由链表下标

	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();
	}
	else
	{
		// 如果自由链表为空，则从中央缓存获取  获取 alignSize 大小的内存
		size_t alignSize = SizeClass::RoundUp(size); // 对齐大小

		// 暂时注释，可能有问题，看后续设计
		//1. index:
		//告诉中心缓存应该从哪自由链表中获取内存块。中心缓存也维护着多个不同大小的自由链表，需要知道具体的索引位置。
		//2. alignSize : 
		// 告诉中心缓存需要分配的确切内存大小。虽然可以通过 index 反推出大小，但直接传递 alignSize高效，避免重复计算。
		return FetchFromCentralCache(index, alignSize);
	}
}

// 释放内存对象
void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// 将 ptr 放回对应的自由链表中
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);

	// 如果自由链表的大小超过阈值，可以考虑将内存块返回给中央缓存
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLong(_freeLists[index], size);
	}
	
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	// 批量取出要进行回收的对象
	void* start = nullptr;
	void* end = nullptr;

	// 取出 MaxSize 个对象进行批量回收
	//既释放了本地过多的内存，又保持了一定的缓存
	list.PopRange(start, end, list.MaxSize());

	// 将取出的对象链表还给中央缓存
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}