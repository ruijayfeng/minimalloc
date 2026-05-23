#include "ObjectPool.h"
#include "ConcurrentAlloc.h"

void Alloc1()
{
	for (int i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(6);
	}
}

void Alloc2()
{
	for (int i = 0; i < 5; ++i)
	{
		void* ptr = ConcurrentAlloc(8);
	}
}

void TLSTest()
{
	std::thread t1(Alloc1);
	t1.join();

	std::thread t2(Alloc2);
	t2.join();
}

void TestConcurrentAlloc1()
{
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);

	cout << p1 << endl;
	cout << p2 << endl;
	cout << p3 << endl;
	cout << p4 << endl;
	cout << p5 << endl;

	//ConcurrentFree(p1, 6);
	//ConcurrentFree(p2, 8);
	//ConcurrentFree(p3, 1);
	//ConcurrentFree(p4, 7);
	//ConcurrentFree(p5, 8);
}


void TestMemoryReuse()
{
	cout << "=== 测试内存复用 ===" << endl;

	// 分配相同大小的内存多次
	void* p1 = ConcurrentAlloc(8);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(8);

	cout << "第一次分配:" << endl;
	cout << "p1: " << p1 << endl;
	cout << "p2: " << p2 << endl;
	cout << "p3: " << p3 << endl;

	// 释放内存
	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);

	cout << "\n释放后再次分配:" << endl;

	// 再次分配相同大小，应该复用之前的地址
	void* p4 = ConcurrentAlloc(8);
	void* p5 = ConcurrentAlloc(8);
	void* p6 = ConcurrentAlloc(8);

	cout << "p4: " << p4 << endl;
	cout << "p5: " << p5 << endl;
	cout << "p6: " << p6 << endl;

	// 检查是否复用了之前的地址(顺序可能不同，但地址应该相同)
	cout << "\n验证内存复用:" << endl;
	cout << "p4是否复用: " << (p4 == p1 || p4 == p2 || p4 == p3 ? "是" : "否") << endl;
	cout << "p5是否复用: " << (p5 == p1 || p5 == p2 || p5 == p3 ? "是" : "否") << endl;
	cout << "p6是否复用: " << (p6 == p1 || p6 == p2 || p6 == p3 ? "是" : "否") << endl;
}


//
//int main()
//{
//	//TestObjectPoll();
//
//	//TLSTest();
//
//	//TestConcurrentAlloc1();
//
//	TestMemoryReuse();
//
//	return 0;
//}