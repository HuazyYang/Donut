#include <donut/core/object/MemoryAllocators.h>
#include <donut/core/object/DebugUtilities.h>

#include <stdlib.h>

#if defined(_DEBUG) && defined(_MSC_VER)
#include <crtdbg.h>
#define USE_CRT_MALLOC_DBG 1
#endif

#if PLATFORM_ANDROID && __ANDROID_API__ < 28
#define USE_ALIGNED_MALLOC_FALLBACK 1
#endif

namespace donut {

namespace details {
template <typename T>
bool IsPowerOfTwo(T val) {
    return val > 0 && (val & (val - 1)) == 0;
}

template <typename T1, typename T2>
inline typename std::conditional<sizeof(T1) >= sizeof(T2), T1, T2>::type AlignUp(T1 val, T2 alignment) {
    static_assert(std::is_unsigned<T1>::value == std::is_unsigned<T2>::value, "both types must be signed or unsigned");
    static_assert(!std::is_pointer<T1>::value && !std::is_pointer<T2>::value, "types must not be pointers");
    DONUT_VERIFY(IsPowerOfTwo(alignment), "Alignment (", alignment, ") must be a power of 2");

    using T = typename std::conditional<sizeof(T1) >= sizeof(T2), T1, T2>::type;
    return (static_cast<T>(val) + static_cast<T>(alignment - 1)) & ~static_cast<T>(alignment - 1);
}
}


#ifdef USE_ALIGNED_MALLOC_FALLBACK
namespace {
void* AllocateAlignedFallback(size_t Size, size_t Alignment) {
    constexpr size_t PointerSize = sizeof(void*);
    const size_t AdjustedAlignment = (std::max)(Alignment, PointerSize);

    void* Pointer = malloc(Size + AdjustedAlignment + PointerSize);
    void* AlignedPointer =
        AlignUp(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(Pointer) + PointerSize), AdjustedAlignment);

    void** StoredPointer = reinterpret_cast<void**>(AlignedPointer) - 1;
    DONUT_VERIFY(StoredPointer >= Pointer);
    *StoredPointer = Pointer;

    return AlignedPointer;
}

void FreeAlignedFallback(void* Ptr) {
    if (Ptr != nullptr) {
        void* OriginalPointer = *(reinterpret_cast<void**>(Ptr) - 1);
        free(OriginalPointer);
    }
}
}  // namespace
#endif

DefaultMemoryAllocator::DefaultMemoryAllocator() {}

void* DefaultMemoryAllocator::Allocate(
    size_t Size, const char* dbgFileName, const int32_t dbgLineNumber) {
    DONUT_VERIFY(Size > 0);
#ifdef USE_CRT_MALLOC_DBG
    return _malloc_dbg(Size, _NORMAL_BLOCK, dbgFileName, dbgLineNumber);
#else
    return malloc(Size);
#endif
}

void DefaultMemoryAllocator::Free(void* Ptr) { free(Ptr); }

#ifdef ALIGNED_MALLOC
#undef ALIGNED_MALLOC
#endif
#ifdef ALIGNED_FREE
#undef ALIGNED_FREE
#endif

#ifdef USE_CRT_MALLOC_DBG
#define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) \
    _aligned_malloc_dbg(Size, Alignment, dbgFileName, dbgLineNumber)
#define ALIGNED_FREE(Ptr) _aligned_free_dbg(Ptr)
#elif defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
#define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) _aligned_malloc(Size, Alignment)
#define ALIGNED_FREE(Ptr) _aligned_free(Ptr)
#elif defined(USE_ALIGNED_MALLOC_FALLBACK)
#define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) AllocateAlignedFallback(Size, Alignment)
#define ALIGNED_FREE(Ptr) FreeAlignedFallback(Ptr)
#else
#define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) aligned_alloc(Alignment, Size)
#define ALIGNED_FREE(Ptr) free(Ptr)
#endif

void* DefaultMemoryAllocator::AllocateAligned(
    size_t Size, size_t Alignment,const char* dbgFileName, const int32_t dbgLineNumber) {
    DONUT_VERIFY(Size > 0 && Alignment > 0);
    Size = details::AlignUp(Size, Alignment);
    return ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber);
}

void DefaultMemoryAllocator::FreeAligned(void* Ptr) { ALIGNED_FREE(Ptr); }

DefaultMemoryAllocator& DefaultMemoryAllocator::Get() {
    static DefaultMemoryAllocator Allocator;
    return Allocator;
}

}  // namespace donut
