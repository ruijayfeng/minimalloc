#include "ConcurrentAlloc.h"
#include <vector>
#include <thread>
#include <atomic>
#include <cstdio>
#include <iostream>

using std::cout;
using std::endl;

// ntimes: number of alloc/free operations per thread
// rounds: number of test rounds
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(malloc(16));
					v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%zu threads, %zu rounds, malloc %zu objects per round: cost %zu ms\n",
		nworks, rounds, ntimes, (size_t)malloc_costtime);

	printf("%zu threads, %zu rounds, free %zu objects per round: cost %zu ms\n",
		nworks, rounds, ntimes, (size_t)free_costtime);

	printf("%zu threads malloc&free %zu times, total cost: %zu ms\n",
		nworks, nworks * rounds * ntimes, (size_t)malloc_costtime + (size_t)free_costtime);
}


// Test concurrent malloc/free with multiple threads and rounds
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(ConcurrentAlloc(16));
					v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%zu threads, %zu rounds, concurrent alloc %zu objects per round: cost %zu ms\n",
		nworks, rounds, ntimes, (size_t)malloc_costtime);

	printf("%zu threads, %zu rounds, concurrent dealloc %zu objects per round: cost %zu ms\n",
		nworks, rounds, ntimes, (size_t)free_costtime);

	printf("%zu threads concurrent alloc&dealloc %zu times, total cost: %zu ms\n",
		nworks, nworks * rounds * ntimes, (size_t)malloc_costtime + (size_t)free_costtime);
}

int main()
{
	size_t n = 10000;
	cout << "========== Concurrent Memory Pool Test ==========" << endl;
	BenchmarkConcurrentMalloc(n, 4, 10);
	cout << endl;

	cout << "========== System malloc Test ==========" << endl;
	BenchmarkMalloc(n, 4, 10);
	cout << "==========================================" << endl;

	return 0;
}