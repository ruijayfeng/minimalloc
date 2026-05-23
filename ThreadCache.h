#pragma once

#include "Common.h"

class ThreadCache
{
public:
	// 申请和释放内存对象
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// 从中央缓存获取内存
	void* FetchFromCentralCache(size_t index, size_t size);
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELIST];
};

// TLS thread local storage
// 用于存储每个线程的 ThreadCache 实例
// 这样每个线程可以独立地管理自己的内存池，避免多线程竞争
// 通过 static _declspec(thread) 声明，确保每个线程都有自己的 ThreadCache 实例
// 这样可以提高多线程环境下的内存分配效率
// 这使得每个线程可以独立地分配和释放内存，而不会干扰其他线程的内存操作
// 这样可以减少锁的使用，提高性能
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;