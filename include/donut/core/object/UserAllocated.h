#ifndef CORE_OBJECT_ALLOCABLE_H
#define CORE_OBJECT_ALLOCABLE_H
#include <donut/core/object/MemoryAllocators.h>
#include <new>
#include <cstdlib>

namespace donut {

struct DonutOverload {};

void EnableCrtDumpHeapLeaks();

// heap allocable object without reference
class UserAllocated {
 public:
    void* operator new(size_t sz, const char* file, int line, DonutOverload overload) {
        void *p = DefaultMemoryAllocator::Get().Allocate(sz, file, line);
        if(p) return p;
        throw std::bad_alloc{};
    }

    void* operator new[](size_t sz, const char* file, int line, DonutOverload overload) {
        void *p = DefaultMemoryAllocator::Get().Allocate(sz, file, line);
        if(p) return p;
        throw std::bad_alloc{};
    }

    void operator delete(void* ptr, const char* file, int line, DonutOverload overload) {
        DefaultMemoryAllocator::Get().Free(ptr);
    }

    void operator delete[](void* ptr, const char* file, int line,
                           DonutOverload overload) {
        DefaultMemoryAllocator::Get().Free(ptr);
    }

    void operator delete(void* ptr) {
        if (ptr) DefaultMemoryAllocator::Get().Free(ptr);
    }

    void operator delete[](void* ptr) {
        if (ptr) DefaultMemoryAllocator::Get().Free(ptr);
    }
};

}  // namespace donut

void* operator new(size_t sz, const char* file, int line, donut::DonutOverload);
void* operator new[](size_t sz, const char* file, int line, donut::DonutOverload);
void operator delete(void* p, const char* file, int line, donut::DonutOverload);
void operator delete[](void* p, const char* file, int line, donut::DonutOverload);

#define DONUT_NEW(Tp) new(__FILE__, __LINE__, donut::DonutOverload{}) Tp
#define DONUT_DELETE(x) delete x 

#endif /* CORE_OBJECT_ALLOCABLE_H */
