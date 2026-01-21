#include <donut/core/object/Foundation.h>
#include <deque>
#include <algorithm>
#include <stdio.h>
#include <vector>

#if defined(_WIN32) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define DONUT_MEMORY_LEAKS_CHECK
#endif

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
inline typename std::conditional<sizeof(T1) >= sizeof(T2), T1, T2>::type AlignUp(
    T1 val, T2 alignment) {
    static_assert(std::is_unsigned<T1>::value == std::is_unsigned<T2>::value,
                  "both types must be signed or unsigned");
    static_assert(!std::is_pointer<T1>::value && !std::is_pointer<T2>::value,
                  "types must not be pointers");
    DONUT_VERIFY(IsPowerOfTwo(alignment), "Alignment (", alignment,
                 ") must be a power of 2");

    using T = typename std::conditional<sizeof(T1) >= sizeof(T2), T1, T2>::type;
    return (static_cast<T>(val) + static_cast<T>(alignment - 1)) &
           ~static_cast<T>(alignment - 1);
}
}  // namespace details

#ifdef USE_ALIGNED_MALLOC_FALLBACK
namespace {
void *AllocateAlignedFallback(size_t Size, size_t Alignment) {
    constexpr size_t PointerSize = sizeof(void *);
    const size_t AdjustedAlignment = (std::max)(Alignment, PointerSize);

    void *Pointer = malloc(Size + AdjustedAlignment + PointerSize);
    void *AlignedPointer = AlignUp(
        reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(Pointer) + PointerSize),
        AdjustedAlignment);

    void **StoredPointer = reinterpret_cast<void **>(AlignedPointer) - 1;
    DONUT_VERIFY(StoredPointer >= Pointer);
    *StoredPointer = Pointer;

    return AlignedPointer;
}

void FreeAlignedFallback(void *Ptr) {
    if (Ptr != nullptr) {
        void *OriginalPointer = *(reinterpret_cast<void **>(Ptr) - 1);
        free(OriginalPointer);
    }
}
}  // namespace
#endif

DefaultMemoryAllocator::DefaultMemoryAllocator() {}

void *DefaultMemoryAllocator::Allocate(size_t Size, const char *dbgFileName,
                                       const int32_t dbgLineNumber) {
    DONUT_VERIFY(Size > 0);
#ifdef USE_CRT_MALLOC_DBG
    return _malloc_dbg(Size, _NORMAL_BLOCK, dbgFileName, dbgLineNumber);
#else
    return malloc(Size);
#endif
}

void DefaultMemoryAllocator::Free(void *Ptr) { free(Ptr); }

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
#define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) \
    _aligned_malloc(Size, Alignment)
#define ALIGNED_FREE(Ptr) _aligned_free(Ptr)
#elif defined(USE_ALIGNED_MALLOC_FALLBACK)
#define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) \
    AllocateAlignedFallback(Size, Alignment)
#define ALIGNED_FREE(Ptr) FreeAlignedFallback(Ptr)
#else
#define ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber) \
    aligned_alloc(Alignment, Size)
#define ALIGNED_FREE(Ptr) free(Ptr)
#endif

void *DefaultMemoryAllocator::AllocateAligned(size_t Size, size_t Alignment,
                                              const char *dbgFileName,
                                              const int32_t dbgLineNumber) {
    DONUT_VERIFY(Size > 0 && Alignment > 0);
    Size = details::AlignUp(Size, Alignment);
    return ALIGNED_MALLOC(Size, Alignment, dbgFileName, dbgLineNumber);
}

void DefaultMemoryAllocator::FreeAligned(void *Ptr) { ALIGNED_FREE(Ptr); }

DefaultMemoryAllocator *GetDefaultMemAllocator() noexcept {
    static DefaultMemoryAllocator Allocator;
    return &Allocator;
}

void EnableCrtDumpHeapLeaks() {
#ifdef DONUT_MEMORY_LEAKS_CHECK
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
#endif
}

namespace details {
FRESULT InterfaceTableQueryInterface(void *pThis, const INTERFACE_ENTRY *pTable, FREFIID riid, void **ppv) {
    if (riid == IID_IObject) {
        // first entry must be an offset
        if(ppv) {
            *ppv = (char *)pThis + pTable->data;
            ((IObject *)(*ppv))->AddRef();
        }
        return FS_OK;
    } else {
        FRESULT hr = FE_NOINTERFACE;

        while (pTable->pfnFinder) {
            if (!pTable->pIID || riid == *pTable->pIID) {
                if (pTable->pfnFinder == DONUT_ENTRY_IS_OFFSET) {
                    if(ppv) {
                        *ppv = (char *)pThis + pTable->data;
                        ((IObject *)(*ppv))->AddRef();
                    }
                    hr = FS_OK;
                    break;
                } else {
                    hr = pTable->pfnFinder(pThis, pTable->data, riid, ppv);

                    if (hr == FS_OK)
                        break;
                }
            }
            pTable++;
        }
        if (hr != FS_OK) {
            if (ppv) *ppv = 0;
        }
        return hr;
    }
}

class ObjectTracker {
public:
    static ObjectTracker s_Instance;

    void AddObject(IObject *pObj);

    bool RemoveObject(IObject *pObj);

    void Dump();

private:
    ObjectTracker();
    ~ObjectTracker();

    ObjectTracker(const ObjectTracker &) = delete;
    ObjectTracker &operator=(const ObjectTracker &) = delete;

    SpinLock m_Lock;
    std::deque<IObject *> m_AliveObjectContainer;
};

ObjectTracker ObjectTracker::s_Instance;

void ObjectTracker::AddObject(IObject *pObj) {
    std::lock_guard<SpinLock> lock(m_Lock);
    m_AliveObjectContainer.push_back(pObj);
}

bool ObjectTracker::RemoveObject(IObject *pObj) {
    std::lock_guard<SpinLock> lock(m_Lock);
    auto it = std::find(m_AliveObjectContainer.begin(), m_AliveObjectContainer.end(), pObj);
    if (it != m_AliveObjectContainer.end()) {
        m_AliveObjectContainer.erase(it);
        return true;
    }
    return false;
}

void ObjectTracker::Dump() {
    std::lock_guard<SpinLock> lock(m_Lock);
    if (!m_AliveObjectContainer.empty()) {
        printf("ObjectTracker::Dump() detect %llu alive objects:\n", m_AliveObjectContainer.size());
    }
}

ObjectTracker::ObjectTracker() {}

ObjectTracker::~ObjectTracker() { Dump(); }

void ObjectTrackerAddObject(IObject *pObj) { return ObjectTracker::s_Instance.AddObject(pObj); }

bool ObjectTrackerRemoveObject(IObject *pObj) {
    return ObjectTracker::s_Instance.RemoveObject(pObj);
}

}

/// Base interface for a data blob
DONUT_CLSID(DataBlobImpl, "405202ca-4daa-459c-9da8-6996ca3fb1d4")
class DataBlobImpl final : public ObjectImpl<IDataBlob> {
 public:
    DONUT_DECLARE_UUID_TRAITS(DataBlobImpl)

    DONUT_BEGIN_INTERFACE_TABLE_INLINE(DataBlobImpl)
    DONUT_IMPLEMENTS_INTERFACE(DataBlobImpl)
    DONUT_IMPLEMENTS_INTERFACE(IDataBlob)
    DONUT_END_INTERFACE_TABLE()

    DataBlobImpl(size_t InitialSize, const void *pData = nullptr)
        : m_DataBuff(InitialSize) {
        if (!m_DataBuff.empty() && pData != nullptr) {
            std::memcpy(m_DataBuff.data(), pData, InitialSize);
        }
    }

    /// Sets the size of the internal data buffer
    void Resize(size_t NewSize) override { m_DataBuff.resize(NewSize); }

    /// Returns the size of the internal data buffer
    size_t GetSize() override { return m_DataBuff.size(); }

    /// Returns the pointer to the internal data buffer
    void *GetDataPtr() override { return m_DataBuff.data(); }

 private:
    std::vector<uint8_t> m_DataBuff;
};

/// String data blob implementation.
DONUT_CLSID(StringDataBlobImpl, "2bf21355-9bf0-4ed4-b2e9-e5a45a25cfa2")
class StringDataBlobImpl : public ObjectImpl<IDataBlob> {
    DONUT_DECLARE_UUID_TRAITS(StringDataBlobImpl)
 public:
    DONUT_BEGIN_INTERFACE_TABLE_INLINE(StringDataBlobImpl)
    DONUT_IMPLEMENTS_INTERFACE(StringDataBlobImpl)
    DONUT_IMPLEMENTS_INTERFACE(IDataBlob)
    DONUT_END_INTERFACE_TABLE()

    /// Sets the size of the internal data buffer
    virtual void Resize(size_t NewSize) override { m_String.resize(NewSize); }

    /// Returns the size of the internal data buffer
    virtual size_t GetSize() override { return m_String.length(); }

    /// Returns the pointer to the internal data buffer
    virtual void *GetDataPtr() override { return &m_String[0]; }

    StringDataBlobImpl(size_t Size, const char *pData = nullptr) {
        m_String.resize(Size);
        if (pData) std::strncpy((char *)m_String.data(), pData, Size);
    }

 private:
    std::string m_String;
};

DONUT_CLSID(ProxyDataBlobImpl, "d1373bc6-c59a-40c5-ac46-56d299206d43")
class ProxyDataBlobImpl : public ObjectImpl<IDataBlob> {
public:
    DONUT_DECLARE_UUID_TRAITS(ProxyDataBlobImpl)

    DONUT_BEGIN_INTERFACE_TABLE_INLINE(ProxyDataBlobImpl)
    DONUT_IMPLEMENTS_INTERFACE(ProxyDataBlobImpl)
    DONUT_IMPLEMENTS_INTERFACE(IDataBlob)
    DONUT_END_INTERFACE_TABLE()

    virtual void Resize(size_t NewSize) override {
        DONUT_VERIFY(false, "Operation forbidden");
    }

    virtual size_t GetSize() override { return m_Size; }

    virtual void *GetDataPtr() override { return const_cast<void *>(m_pData); }

    ProxyDataBlobImpl(size_t Size, const void *pData) : m_pData{pData}, m_Size{Size} {}

 private:
    const void *const m_pData;
    const size_t m_Size;
};

FRESULT CreateBlob(size_t Size, IDataBlob **ppBlob) {
    auto blob = MAKE_RC_OBJ0(DataBlobImpl)(Size);
    if (ppBlob) {
        *ppBlob = blob;
        blob->AddRef();
    }
    blob->Release();
    return FS_OK;
}

FRESULT CreateStringBlob(size_t Size, IDataBlob **ppBlob) {
    auto blob = MAKE_RC_OBJ0(StringDataBlobImpl)(Size);
    if (ppBlob) {
        *ppBlob = blob;
        blob->AddRef();
    }
    blob->Release();
    return FS_OK;
}

FRESULT CreateProxyBlob(size_t Size, const void *pData, IDataBlob **ppBlob) {
    auto blob = MAKE_RC_OBJ0(ProxyDataBlobImpl)(Size, pData);
    if (ppBlob) {
        *ppBlob = blob;
        blob->AddRef();
    }
    blob->Release();
    return FS_OK;
}

}  // namespace donut

