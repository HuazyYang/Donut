#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H

#include <assert.h>
#include <type_traits>

#if defined(_MSC_VER)
#define donut_likely(x) (x)
#define donut_unlikely(x) (x)
#else
#define donut_likely(x) __builtin_expect(!!(x), 1)
#define donut_unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifdef _DEBUG

// #define DONUT_DUMP_ALIVE_OBJECTS

#define DONUT_DEVELOPMENT

#define DONUT_ASSERT(expr) assert(expr)

#include <typeinfo>

#define DONUT_ASSERTION_FAILED(...) \
    do {                               \
        assert(0);                     \
    } while (false)

#define DONUT_VERIFY(Expr, ...)           \
    do {                                              \
        if (!(Expr)) {                                \
            DONUT_ASSERTION_FAILED(__VA_ARGS__); \
        }                                             \
    } while (false)

#define DONUT_UNEXPECTED DONUT_ASSERTION_FAILED

#define DONUT_LOG_WARNING_MESSAGE(Fmt, ...) ((void)0)

template <typename DstType, typename SrcType>
void CheckDynamicType(SrcType* pSrcPtr) {
    DONUT_VERIFY(
        pSrcPtr == nullptr || dynamic_cast<DstType*>(pSrcPtr) != nullptr,
        "Dynamic type cast failed. Src typeid: \'",
        typeid(*pSrcPtr).name(),
        "\' Dst typeid: \'",
        typeid(DstType).name(),
        '\'');
}
#define DONUT_CHECK_DYNAMIC_TYPE(DstType, pSrcPtr) \
    do {                                         \
        CheckDynamicType<DstType>(pSrcPtr);      \
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
    SrcType* Ptr,
    typename std::enable_if<
        !std::is_same<DstType, SrcType>::value && !std::is_base_of<DstType, SrcType>::value,
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
    SrcType* Ptr,
    typename std::enable_if<std::is_same<DstType, SrcType>::value || std::is_base_of<DstType, SrcType>::value, void*>::
        type = 0) {
    return static_cast<DstType*>(Ptr);
}

template<typename T>
void SafeAddRef(T *p) {
    if(p)
        p->AddRef();
}

template<typename T>
void SafeRelease(T *&p) {
    if(p) {
        p->Release();
        p = nullptr;
    }
}

#endif /* DEBUGUTILS_H */
