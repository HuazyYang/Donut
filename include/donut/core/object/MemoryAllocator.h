#ifndef MEMORYALLOCATOR_H
#define MEMORYALLOCATOR_H
#include <type_traits>
#include <limits>

namespace donut {
struct IMemoryAllocator {
    /// Allocates block of memory
    virtual void* Allocate(size_t Size, const char* dbgFileName,
                           const int dbgLineNumber) = 0;

    /// Releases memory
    virtual void Free(void* Ptr) = 0;

    /// Allocates block of memory with specified alignment
    virtual void* AllocateAligned(size_t Size, size_t Alignment, const char* dbgFileName,
                                  const int dbgLineNumber) = 0;

    /// Releases memory allocated with AllocateAligned
    virtual void FreeAligned(void* Ptr) = 0;
};

class DefaultMemoryAllocator : public IMemoryAllocator {
 public:
    DefaultMemoryAllocator();

    /// Allocates block of memory
    virtual void* Allocate(size_t Size, const char* dbgFileName,
                           const int dbgLineNumber) override;

    /// Releases memory
    virtual void Free(void* Ptr) override;

    /// Allocates block of memory with specified alignment
    virtual void* AllocateAligned(size_t Size, size_t Alignment, const char* dbgFileName,
                                  const int dbgLineNumber) override;

    /// Releases memory allocated with AllocateAligned
    virtual void FreeAligned(void* Ptr) override;

 private:
    DefaultMemoryAllocator(const DefaultMemoryAllocator&) = delete;
    DefaultMemoryAllocator(DefaultMemoryAllocator&&) = delete;
    DefaultMemoryAllocator& operator=(const DefaultMemoryAllocator&) = delete;
    DefaultMemoryAllocator& operator=(DefaultMemoryAllocator&&) = delete;
};

DefaultMemoryAllocator* GetDefaultMemAllocator() noexcept;

void EnableCrtDumpHeapLeaks();

struct DonutNewOverload {};

template <typename AllocatorType, typename Tp>
void DeleteObject(AllocatorType* pAllocator, Tp* p) {
    if (p) {
        p->~Tp();
        pAllocator->Free(p);
    }
}

// std::allocator adapter

template <typename T>
typename std::enable_if<std::is_destructible<T>::value, void>::type Destruct(T* ptr) {
    ptr->~T();
}

template <typename T>
typename std::enable_if<!std::is_destructible<T>::value, void>::type Destruct(T* ptr) {}

template <typename T, typename AllocatorType = DefaultMemoryAllocator>
struct STDAllocator {
    using value_type = T;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    STDAllocator(AllocatorType& Allocator) noexcept : m_Allocator{Allocator} {}

    template <class U>
    STDAllocator(const STDAllocator<U, AllocatorType>& other) noexcept
        : m_Allocator{other.m_Allocator} {}

    template <class U>
    STDAllocator(STDAllocator<U, AllocatorType>&& other) noexcept
        : m_Allocator{other.m_Allocator} {}

    template <class U>
    STDAllocator& operator=(STDAllocator<U, AllocatorType>&& other) noexcept {
        DONUT_VERIFY(&m_Allocator == &other.m_Allocator, "Inconsistent allocators");
        return *this;
    }

    template <class U>
    struct rebind {
        typedef STDAllocator<U, AllocatorType> other;
    };

    T* allocate(std::size_t count) {
        return reinterpret_cast<T*>(
            m_Allocator.AllocateAligned(count * sizeof(T), alignof(T), nullptr, 0));
    }

    pointer address(reference r) { return &r; }
    const_pointer address(const_reference r) { return &r; }

    void deallocate(T* p, std::size_t count) { m_Allocator.FreeAligned(p); }

    inline size_type max_size() const {
        return (std::numeric_limits<size_type>::max)() / sizeof(T);
    }

    //    construction/destruction
    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        ::new (p) U(std::forward<Args>(args)...);
    }

    inline void destroy(pointer p) { Destruct(p); }

    AllocatorType& m_Allocator;
};

template <class T, class U, class A>
bool operator==(const STDAllocator<T, A>& left, const STDAllocator<U, A>& right) noexcept {
    return &left.m_Allocator == &right.m_Allocator;
}

template <class T, class U, class A>
bool operator!=(const STDAllocator<T, A>& left, const STDAllocator<U, A>& right) {
    return !(left == right);
}

}  // namespace dount

inline void* operator new(size_t, donut::DonutNewOverload, void* where) { return where; }
inline void operator delete(void*, donut::DonutNewOverload, void*) {
}  // This is only required so we can use the symmetrical new()
#define DONUT_NEW(Allocator, Ty) \
    new (donut::DonutNewOverload, Allocator.Allocate(sizeof(Ty), __FILE__, __LINE__)) Ty
#define DONUT_NEW0(Ty)            \
    new (donut::DonutNewOverload, \
         donut::GetDefaultMemAllocator()->Allocate(sizeof(Ty), __FILE__, __LINE__)) Ty
#define DONUT_DELETE(Allocator, p) donut::DeleteObject(&Allocator, p)
#define DONUT_DELETE0(p) donut::DeleteObject(donut::GetDefaultMemAllocator(), p)


#endif /* MEMORYALLOCATOR_H */
