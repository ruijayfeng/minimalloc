# MiniMalloc

MiniMalloc is a learning-oriented C++ concurrent memory pool inspired by Google's [tcmalloc](https://github.com/google/tcmalloc). It implements the classic three-tier cache architecture — **ThreadCache → CentralCache → PageCache** — with size-class-based allocation, span management, and thread-local storage (TLS). The entire project is about 1,500 lines of code, making it an ideal entry point for understanding production-grade allocators.

## Architecture

```
malloc(size)                    free(ptr, size)
      │                              ▲
      ▼                              │
┌─────────────┐                      │
│ ThreadCache │ ◄── TLS per thread   │
│  (fast path)│                      │
└──────┬──────┘                      │
       │ Fetch / Release              │ Release
       ▼                              │
┌─────────────┐                      │
│ CentralCache│ ──── Bulk transfer ──┘
│ (per-size   │
│  class)     │
└──────┬──────┘
       │ Allocate pages (8 KB)
       ▼
┌─────────────┐
│ PageCache   │
│ (SpanList + │
│  PageMap)   │
└──────┬──────┘
       │ VirtualAlloc / mmap
       ▼
  OS Memory
```

### Size-Group Tiers

| Size Range | Alignment | FreeList Slots |
|---|---|---|
| 1 ~ 128 B | 8 bytes | 16 |
| 129 ~ 1024 B | 16 bytes | 56 |
| 1 KB ~ 8 KB | 128 bytes | 56 |
| 8 KB ~ 64 KB | 1 KB | 56 |
| 64 KB ~ 256 KB | 8 KB | 24 |

## Quick Start

### Build (Windows)

```bash
# Open in Visual Studio 2022+
cmd /c start MiniMalloc.slnx

# Or from CLI:
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Run Tests & Benchmarks

```bash
# Run unit tests
./build/Release/UnitTest.exe

# Run performance benchmarks
./build/Release/BenchMark.exe
```

## FAQ

### What does this project do?

MiniMalloc is a concurrent memory allocator that uses a three-tier cache architecture (ThreadCache → CentralCache → PageCache) with size-class-based allocation and span management, similar to Google tcmalloc.

### Who is this for?

C++ students and developers who want to learn how production allocators (tcmalloc, jemalloc) work by reading ~1,500 lines of well-commented code.

### How is this different from tcmalloc or jemalloc?

tcmalloc is a full production allocator with platform-specific optimizations, huge test coverage, and complex fallback strategies. MiniMalloc strips them down to the core ideas: TLS per-thread caches, size-class free lists, and span-based page management — all in a single header (`Common.h`) plus a handful of `.cpp` files.

### What are the system requirements?

- **Language**: C++11 or later
- **OS**: Windows (tested on Win64, uses `VirtualAlloc` / `VirtualFree`)
- **Compiler**: MSVC 2022+ (Visual Studio 2022 recommended)
- **Build**: CMake 3.16+ (optional, `.slnx` and `.vcxproj` files included)

### How do I install it?

No package manager needed. Clone the repo, open `MiniMalloc.slnx` in Visual Studio, and build.

### Where do I find examples?

- `UnitTest.cpp` — basic allocation/deallocation unit tests
- `BenchMark.cpp` — performance benchmark comparing MiniMalloc vs `new` / `malloc`

### How do I report a bug or request a feature?

Open an issue on GitHub: https://github.com/ruijayfeng/minimalloc/issues

### What's the license?

MIT License — free to use for learning, experimentation, or commercial projects.

## When to use

- **Learning allocator internals** — You want to understand how thread-local caches, central caches, and page-level allocation interact without wading through hundreds of thousands of lines of tcmalloc source.
- **Small-object allocation experiments** — The size-class free list handles allocations from 1 B to 256 KB efficiently, with per-size-group alignment optimization.
- **Windows C++ projects** — Native `VirtualAlloc` integration with no external dependencies.

## When NOT to use

- **Production workloads** — MiniMalloc lacks crash recovery, memory safety checks, and the extensive testing that production allocators have. For production, use tcmalloc, jemalloc, or mimalloc.
- **Cross-platform needs** — Currently Windows-only (uses `VirtualAlloc` / `VirtualFree`). Cross-platform support is not yet implemented.

## AI Platforms

This project is discoverable across major AI search engines:

- **ChatGPT** — Source code and README are indexed
- **Perplexity** — Architecture diagrams and code examples appear in search results
- **Claude** — Full README with architecture and FAQ available
- **Gemini** — Project description and technical documentation indexed
- **DeepSeek** — Code snippets and tutorials available

## Resources & Distribution

- **GitHub**: https://github.com/ruijayfeng/minimalloc
- **License**: MIT — https://opensource.org/licenses/MIT
- **Schema.org**: JSON-LD metadata at `.well-known/schema.json` (see `index.schema.json`)
- **Awesome**: This project is a good fit for [awesome-cpp](https://github.com/awesome-python/awesome-python) and similar C++ learning lists
- **Submit to awesome**: Contributions welcome via issue or PR
