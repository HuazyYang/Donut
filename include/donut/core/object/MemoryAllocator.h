#ifndef MEMORYALLOCATOR_H
#define MEMORYALLOCATOR_H
#include <type_traits>
#include <limits>
#include <cassert>

#if defined(_MSC_VER)
#define donut_likely(x) (x)
#define donut_unlikely(x) (x)
#else
#define donut_likely(x) __builtin_expect(!!(x), 1)
#define donut_unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifdef _DEBUG

// #define DONUT_DUMP_ALIVE_OBJECTS

#define DONUT_ASSERT(expr) assert(expr)

#define DONUT_ASSERTION_FAILED(...) \
    do {                            \
        assert(0);                  \
    } while (false)

#define DONUT_VERIFY(Expr, ...)                  \
    do {                                         \
        if (!(Expr)) {                           \
            DONUT_ASSERTION_FAILED(__VA_ARGS__); \
        }                                        \
    } while (false)

#define DONUT_UNEXPECTED DONUT_ASSERTION_FAILED

#define DONUT_LOG_WARNING_MESSAGE(Fmt, ...) ((void)0)

template <typename DstType, typename SrcType>
void CheckDynamicType(SrcType* pSrcPtr) {
    DONUT_VERIFY(pSrcPtr == nullptr || dynamic_cast<DstType*>(pSrcPtr) != nullptr,
                 "Dynamic type cast failed. Src typeid: \'", typeid(*pSrcPtr).name(),
                 "\' Dst typeid: \'", typeid(DstType).name(), '\'');
}
#define DONUT_CHECK_DYNAMIC_TYPE(DstType, pSrcPtr) \
    do {                                           \
        CheckDynamicType<DstType>(pSrcPtr);        \
    } while (false)

#else

// #define DONUT_DUMP_ALIVE_OBJECTS

#define DONUT_ASSERT(expr) ((void)0)

// clang-format off
#    define DONUT_CHECK_DYNAMIC_TYPE(...) do{}while(false)
#    define DONUT_VERIFY(...)do{}while(false)
#    define DONUT_UNEXPECTED(...)do{}while(false)
// clang-format on

#endif

template <typename DstType, typename SrcType>
[[nodiscard]] DstType* ClassPtrCast(
    SrcType* Ptr, typename std::enable_if<!std::is_same<DstType, SrcType>::value &&
                                              !std::is_base_of<DstType, SrcType>::value,
                                          void*>::type = 0) {
#ifdef _DEBUG
    if (Ptr != nullptr) {
        DONUT_CHECK_DYNAMIC_TYPE(DstType, Ptr);
    }
#endif
    return static_cast<DstType*>(Ptr);
}

template <typename DstType, typename SrcType>
[[nodiscard]] DstType* ClassPtrCast(
    SrcType* Ptr, typename std::enable_if<std::is_same<DstType, SrcType>::value ||
                                              std::is_base_of<DstType, SrcType>::value,
                                          void*>::type = 0) {
    return static_cast<DstType*>(Ptr);
}

namespace donut {
struct IMemoryAllocator {
    /// Allocates block of memory
    virtual void* Allocate(size_t Size) = 0;

    /// Releases memory
    virtual void Free(void* Ptr) = 0;

    /// Allocates block of memory with specified alignment
    virtual void* AllocateAligned(size_t Size, size_t Alignment) = 0;

    /// Releases memory allocated with AllocateAligned
    virtual void FreeAligned(void* Ptr) = 0;
};

class DefaultMemoryAllocator final: public IMemoryAllocator {
 public:
    DefaultMemoryAllocator();

    /// Allocates block of memory
    virtual void* Allocate(size_t Size) override;

    /// Releases memory
    virtual void Free(void* Ptr) override;

    /// Allocates block of memory with specified alignment
    virtual void* AllocateAligned(size_t Size, size_t Alignment) override;

    /// Releases memory allocated with AllocateAligned
    virtual void FreeAligned(void* Ptr) override;

 private:
    DefaultMemoryAllocator(const DefaultMemoryAllocator&) = delete;
    DefaultMemoryAllocator(DefaultMemoryAllocator&&) = delete;
    DefaultMemoryAllocator& operator=(const DefaultMemoryAllocator&) = delete;
    DefaultMemoryAllocator& operator=(DefaultMemoryAllocator&&) = delete;
};

DefaultMemoryAllocator* GetDefaultMemAllocator() noexcept;

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

// inline void* operator new(size_t, donut::DonutNewOverload, void* where) { return where; }
// inline void operator delete(void*, donut::DonutNewOverload, void*) {
// }  // This is only required so we can use the symmetrical new()
// #define DONUT_NEW(Allocator, Ty) \
//     new (donut::DonutNewOverload, Allocator.Allocate(sizeof(Ty))) Ty
// #define DONUT_NEW0(Ty)            \
//     new (donut::DonutNewOverload, \
//          donut::GetDefaultMemAllocator()->Allocate(sizeof(Ty))) Ty
// #define DONUT_DELETE(Allocator, p) donut::DeleteObject(&Allocator, p)
// #define DONUT_DELETE0(p) donut::DeleteObject(donut::GetDefaultMemAllocator(), p)


#endif /* MEMORYALLOCATOR_H */
