#include <donut/core/object/UserAllocated.h>
#if defined(_WIN32) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define DONUT_MEMORY_LEAKS_CHECK
#endif

void *operator new(size_t sz, const char *file, int line, donut::DonutOverload) {
#ifdef DONUT_MEMORY_LEAKS_CHECK
    return ::operator new(sz, 1, file, line);
#else
    return ::operator new(sz);
#endif
}

void * operator new[](size_t sz, const char *file, int line, donut::DonutOverload) {
#ifdef DONUT_MEMORY_LEAKS_CHECK
    return ::operator new[](sz, 1, file, line);
#else
    return ::operator new[](sz);
#endif
}

void operator delete(void *p, const char *file, int line, donut::DonutOverload) {
#ifdef DONUT_MEMORY_LEAKS_CHECK
    return ::operator delete(p, 1, file, line);
#else
    return ::delete (p);
#endif
}

void operator delete[](void *p, const char *file, int line, donut::DonutOverload) {
#ifdef DONUT_MEMORY_LEAKS_CHECK
    return ::operator delete[](p, 1, file, line);
#else
    return ::delete[] (p);
#endif
}

void donut::EnableCrtDumpHeapLeaks() {
#ifdef DONUT_MEMORY_LEAKS_CHECK
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
#endif
}
