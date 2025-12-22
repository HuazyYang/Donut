#ifndef MEMORYALLOCATORS
#define MEMORYALLOCATORS
#include <donut/core/object/IMemoryAllocator.h>
#include <type_traits>
#include <limits>

namespace donut {

class DefaultMemoryAllocator : public IMemoryAllocator
{
public:
    DefaultMemoryAllocator();

    /// Allocates block of memory
    virtual void* Allocate(size_t Size, const char* dbgFileName, const int32_t dbgLineNumber) override;

    /// Releases memory
    virtual void Free(void* Ptr) override;

    /// Allocates block of memory with specified alignment
    virtual void* AllocateAligned(size_t Size, size_t Alignment, const char* dbgFileName, const int32_t dbgLineNumber) override;

    /// Releases memory allocated with AllocateAligned
    virtual void FreeAligned(void* Ptr) override;

    static DefaultMemoryAllocator& Get();

private:
    DefaultMemoryAllocator(const DefaultMemoryAllocator&) = delete;
    DefaultMemoryAllocator(DefaultMemoryAllocator&&)      = delete;
    DefaultMemoryAllocator& operator=(const DefaultMemoryAllocator&) = delete;
    DefaultMemoryAllocator& operator=(DefaultMemoryAllocator&&) = delete;
};


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

    STDAllocator(
        AllocatorType& Allocator) noexcept
        :  
        m_Allocator     {Allocator}
    {
    }

    template <class U>
    STDAllocator(const STDAllocator<U, AllocatorType>& other) noexcept
        :  
        m_Allocator     {other.m_Allocator}
    {
    }

    template <class U>
    STDAllocator(STDAllocator<U, AllocatorType>&& other) noexcept
        :
        m_Allocator     {other.m_Allocator}
    {
    }

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
        return reinterpret_cast<T*>(m_Allocator.AllocateAligned(
            count * sizeof(T), alignof(T), nullptr, 0));
    }

    pointer address(reference r) { return &r; }
    const_pointer address(const_reference r) { return &r; }

    void deallocate(T* p, std::size_t count) { m_Allocator.FreeAligned(p); }

    inline size_type max_size() const { return (std::numeric_limits<size_type>::max)() / sizeof(T); }

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

template <class T, typename AllocatorType = DefaultMemoryAllocator>
struct STDDeleter {
    STDDeleter() noexcept {}

    STDDeleter(AllocatorType& Allocator) noexcept
        : m_Allocator{ &Allocator } {}

    STDDeleter(const STDDeleter&) = default;
    STDDeleter& operator=(const STDDeleter&) = default;

    STDDeleter(STDDeleter&& rhs) noexcept
        : m_Allocator{ rhs.m_Allocator } {
        rhs.m_Allocator = nullptr;
    }

    STDDeleter& operator=(STDDeleter&& rhs) noexcept {
        m_Allocator = rhs.m_Allocator;
        rhs.m_Allocator = nullptr;
        return *this;
    }

    void operator()(T* ptr) noexcept {
        DONUT_VERIFY(
            m_Allocator != nullptr, "The deleter has been moved away or never initialized, and can't be used");
        Destruct(ptr);
        m_Allocator->Free(ptr);
    }

private:
    AllocatorType* m_Allocator = nullptr;
};

}


#endif /* MEMORYALLOCATORS */
