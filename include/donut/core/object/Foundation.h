#pragma once
#include <donut/core/object/Threading.h>
#include <donut/core/object/Types.h>
#include <donut/core/object/MemoryAllocator.h>
#include <cstddef>
#include <cstdint>
#include <atomic>

#if defined(_MSC_VER)
#define donut_likely(x) (x)
#define donut_unlikely(x) (x)
#else
#define donut_likely(x) __builtin_expect(!!(x), 1)
#define donut_unlikely(x) __builtin_expect(!!(x), 0)
#endif

// packing reference control block(WeakReferenceImpl) and object memory together
// so as to optimize memory allocation and cache missing.
// Default memory allocation strategy is packing them all together.
#ifndef DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
#define DONUT_PACK_CONTROL_BLOCK_AND_OBJECT 1
#endif

#ifdef __clang__
#define DONUT_CLSID(Class, StrCLSID) \
    static constexpr donut::GUID IID_##Class = StrCLSID##_donut_guid;
#else
#define DONUT_CLSID(Class, StrCLSID)                                          \
    static constexpr donut::GUID IID_##Class = StrCLSID##_donut_guid;      \
    template <>                                                                  \
    struct ::UUIDTraits<class Class> {                                           \
        static constexpr const donut::GUID& uuid_of() { return IID_##Class; } \
    };
#endif

#define DONUT_BEGIN_INTERFACE_TABLE(ClassName)                                     \
    donut::FRESULT ClassName::QueryInterface(donut::FREFIID riid, void** ppv) { \
        typedef ClassName _ITCls;                                                     \
        static const donut::details::INTERFACE_ENTRY inttable[] = {
#define DONUT_BEGIN_INTERFACE_TABLE_INLINE(ClassName)                            \
    donut::FRESULT QueryInterface(donut::FREFIID riid, void** ppv) override { \
        typedef ClassName _ITCls;                                                   \
        static const donut::details::INTERFACE_ENTRY inttable[] = {
#define DONUT_BEGIN_NON_DELEGATING_INTERFACE_TABLE(ClassName)                     \
    donut::FRESULT ClassName::NonDelegatingQueryInterface(donut::FREFIID riid, \
                                                             void** ppv) {           \
        typedef ClassName _ITCls;                                                    \
        static const donut::details::INTERFACE_ENTRY inttable[] = {
#define DONUT_BEGIN_NON_DELEGATING_INTERFACE_TABLE_INLINE(ClassName)               \
    donut::FRESULT NonDelegatingQueryInterface(donut::FREFIID riid, void** ppv) \
        override {                                                                    \
        typedef ClassName _ITCls;                                                     \
        static const donut::details::INTERFACE_ENTRY inttable[] = {
#define DONUT_IMPLEMENTS_ROUTE_PARENT(BaseClass)                       \
    {                                                                     \
        nullptr,                                                          \
        &donut::details::RouteParentQueryInterface<_ITCls, BaseClass>, \
        DONUT_BASE_OFFSET(_ITCls, BaseClass),                          \
    },

#define DONUT_IMPLEMENTS_ROUTE_MEMBER(Member)                                        \
    {nullptr,                                                                           \
     &donut::details::RouteMemberQueryInterface<std::decay<decltype(Member)>::type>, \
     uint32_t(size_t(std::addressof(this->Member)) -                                    \
              size_t(this))}, /* Note: we can not use offsetof here */

#define DONUT_IMPLEMENTS_INTERFACE(Itf) \
    {&__uuid_of<Itf>(), DONUT_ENTRY_IS_OFFSET, DONUT_BASE_OFFSET(_ITCls, Itf)},

#define DONUT_IMPLEMENTS_INTERFACE_AS(req, Itf) \
    {&__uuid_of<req>(), DONUT_ENTRY_IS_OFFSET, DONUT_BASE_OFFSET(_ITCls, Itf)},

#define DONUT_END_INTERFACE_TABLE()                                                 \
    { 0, (donut::details::INTERFACE_FINDER)0, 0 }                                   \
    }                                                                                  \
    ;                                                                                  \
    return donut::details::InterfaceTableQueryInterface(this, inttable, riid, ppv); \
    }

#define DONUT_DECLARE_INTERFACE_TABLE() \
    donut::FRESULT QueryInterface(const FIID& riid, void** ppv) override;

namespace donut {

namespace details {
typedef FRESULT (*INTERFACE_FINDER)(void* pThis, uint32_t data, FREFIID riid, void** ppv);

struct INTERFACE_ENTRY {
    const FIID* pIID;
    INTERFACE_FINDER pfnFinder;
    uint32_t data;
};

extern FRESULT InterfaceTableQueryInterface(void* pThis, const INTERFACE_ENTRY* pTable,
                                            FREFIID riid, void** ppv);

#define DONUT_ENTRY_IS_OFFSET donut::details::INTERFACE_FINDER(-1)
#define DONUT_ENTRY_ROUTE_BASECLASS uint32_t(-1)
#define DONUT_BASE_OFFSET(ClassName, BaseName)                    \
    uint32_t(reinterpret_cast<char*>(static_cast<BaseName*>(         \
                 reinterpret_cast<ClassName*>(sizeof(ClassName)))) - \
             reinterpret_cast<char*>(sizeof(ClassName)))

template <typename T, typename TBase>
FRESULT RouteParentQueryInterface(void* pThis, uint32_t data, FREFIID riid, void** ppv) {
    static_assert(std::is_base_of<TBase, T>::value,
                  "Can not route query interface implement");
    return reinterpret_cast<TBase*>(static_cast<char*>(pThis) + data)
        ->TBase::QueryInterface(riid, ppv);
}

template <typename T, typename std::enable_if<!std::is_pointer<T>::value, int>::type = 0>
FRESULT RouteMemberQueryInterface(void* pThis, uint32_t data, FREFIID riid, void** ppv) {
    return reinterpret_cast<T*>(static_cast<char*>(pThis) + data)
        ->T::NonDelegatingQueryInterface(riid, ppv);
}

template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
FRESULT RouteMemberQueryInterface(void* pThis, uint32_t data, FREFIID riid, void** ppv) {
    using Ty = typename std::remove_pointer<T>::type;
    return (*reinterpret_cast<T*>(static_cast<char*>(pThis) + data))
        ->Ty::NonDelegatingQueryInterface(riid, ppv);
}

extern void ObjectTrackerAddObject(IObject* pObj);

extern bool ObjectTrackerRemoveObject(IObject* pObj);

class ObjectWrapperBase {
 public:
    virtual void DestroyObject() = 0;
    virtual FRESULT QueryInterface(const FIID& iid, void** ppInterface) = 0;
#if DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
    virtual void DeletePackedStorage(void* pWeakRef) noexcept {};
#endif
};

template <typename ObjectType, typename AllocatorType>
class ObjectWrapper : public ObjectWrapperBase {
 public:
    ObjectWrapper(ObjectType* pObject, AllocatorType* pAllocator) noexcept
        : m_pObject{pObject}, m_pAllocator{pAllocator} {}
    virtual void DestroyObject() override final {
        if (m_pAllocator) {
            m_pObject->~ObjectType();
            m_pAllocator->Free(m_pObject);
        } else {
            delete m_pObject;
        }
#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerRemoveObject(m_pObject);
#endif
    }
    virtual FRESULT QueryInterface(const FIID& iid, void** ppInterface) override final {
        return m_pObject->QueryInterface(iid, ppInterface);
    }

 private:
    // It is crucially important that the type of the pointer
    // is ObjectType and not IObject, since the latter
    // does not have virtual dtor.
    ObjectType* const m_pObject;
    AllocatorType* const m_pAllocator;
};

template <typename ObjectType, typename AllocatorType>
class PackedObjectWrapper : public ObjectWrapperBase {
 public:
    PackedObjectWrapper(ObjectType* pObject, AllocatorType* pAllocator) noexcept
        : m_pObject{pObject}, m_pAllocator{pAllocator} {}
    virtual void DestroyObject() override final {
        m_pObject->~ObjectType();
#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerRemoveObject(m_pObject);
#endif
    }
    virtual FRESULT QueryInterface(const FIID& iid, void** ppInterface) override final {
        return m_pObject->QueryInterface(iid, ppInterface);
    }

    virtual void DeletePackedStorage(void* pWeakRef) noexcept;

 private:
    // It is crucially important that the type of the pointer
    // is ObjectType and not IObject, since the latter
    // does not have virtual dtor.
    ObjectType* const m_pObject;
    AllocatorType* const m_pAllocator;
};

// MSVC starting with 19.25.28610.4 fails to compile sizeof(ObjectWrapper<IObject,
// IMemoryAllocator>) because IObject does not have virtual destructor. The compiler is
// technically right, so we use IObjectStub, which does have virtual destructor.
struct IObjectStub : public IObject {
    virtual ~IObjectStub() = 0;
};

struct ObjectWrapperStorage {
    using ObjectWrapperStub = ObjectWrapper<IObjectStub, IMemoryAllocator>;
    static constexpr size_t ObjectWrapperBufferSize =
        sizeof(ObjectWrapperStub) / sizeof(size_t);
    alignas(ObjectWrapperStub) size_t val[ObjectWrapperBufferSize];
};

template <typename ObjectType>
struct IsWeakReferenceSource {
    static constexpr bool value =
        std::is_base_of<IWeakReferenceSource, ObjectType>::value ||
        std::is_same<IWeakReferenceSource, ObjectType>::value;
};

template <typename ObjectType>
struct PackedCtrlBlock;

template <typename TInterface>
struct WeakRefTypeTrait;

}  // namespace details

template <class Allocator = IMemoryAllocator>
class MakeNewRCObj;

template <typename ObjectType, typename AllocatorType = IMemoryAllocator>
class MakeNewRCDelegating;

// This class controls the lifetime of a refcounted object
class WeakReferenceImpl final : public IWeakReference {
 public:
    FLONG AddRef() override final { return AddWeakRef(); }

    FLONG Release() override final { return ReleaseWeakRef(); }

    FRESULT QueryInterface(FREFIID riid, void** ppv) override final {
        if (!ppv) return FE_INVALID_ARGS;

        if (riid == IID_IObject || riid == IID_IWeakReference) {
            *ppv = this;
            this->AddRef();
            return FS_OK;
        } else {
            *ppv = nullptr;
            return FE_NOINTERFACE;
        }
    }

    FRESULT Resolve(FREFIID riid, void** ppv) override final {
        return QueryObject(riid, ppv);
    }

    FLONG AddStrongRef() {
        DONUT_VERIFY(m_ObjectState.load() == ObjectState::Alive,
                        "Attempting to increment strong reference counter for a destroyed "
                        "or not initialized object!");
        DONUT_VERIFY(
            m_ObjectWrapperBuffer.val[0] != 0 && m_ObjectWrapperBuffer.val[1] != 0,
            "Object wrapper is not initialized");
        return m_NumStrongReferences.fetch_add(+1, std::memory_order_relaxed) + 1;
    }

    template <class TPreObjectDestroy>
    FLONG ReleaseStrongRef(TPreObjectDestroy&& PreObjectDestroy) {
        DONUT_VERIFY(m_ObjectState.load() == ObjectState::Alive,
                        "Attempting to decrement strong reference counter for an object "
                        "that is not alive");
        DONUT_VERIFY(
            m_ObjectWrapperBuffer.val[0] != 0 && m_ObjectWrapperBuffer.val[1] != 0,
            "Object wrapper is not initialized");

        // Decrement strong reference counter without acquiring the lock.
        const auto RefCount = m_NumStrongReferences.fetch_add(-1) - 1;
        DONUT_VERIFY(RefCount >= 0, "Inconsistent call to ReleaseStrongRef()");
        if (RefCount == 0) {
            PreObjectDestroy();
            TryDestroyObject();
        }

        return RefCount;
    }

    FLONG ReleaseStrongRef() {
        return ReleaseStrongRef([]() {});
    }

    FLONG AddWeakRef() {
        return m_NumWeakReferences.fetch_add(+1, std::memory_order_relaxed) + 1;
    }

    FLONG ReleaseWeakRef() {
        // The method must be serialized!
        std::unique_lock<SpinLock> Guard{m_Lock};

        // It is essentially important to check the number of weak references
        // while holding the lock. Otherwise reference counters object
        // may be destroyed twice if ReleaseStrongRef() is executed by other
        // thread.
        const auto NumWeakReferences = m_NumWeakReferences.fetch_add(-1) - 1;
        DONUT_VERIFY(NumWeakReferences >= 0, "Inconsistent call to ReleaseWeakRef()");

        // clang-format off
        // There are two special case when we must not destroy the ref counters object even
        // when NumWeakReferences == 0 && m_NumStrongReferences == 0 :
        //
        //             This thread             |    Another thread - ReleaseStrongRef()
        //                                     |
        // 1. Lock the object                  |
        //                                     |
        // 2. Decrement m_NumWeakReferences,   |   1. Decrement m_NumStrongReferences,
        //    m_NumWeakReferences==0           |      RefCount == 0
        //                                     |
        //                                     |   2. Start waiting for the lock to destroy
        //                                     |      the object, m_ObjectState != ObjectState::Destroyed
        // 3. Do not destroy reference         |
        //    counters, unlock                 |
        //                                     |   3. Acquire the lock,
        //                                     |      destroy the object,
        //                                     |      read m_NumWeakReferences==0
        //                                     |      destroy the reference counters
        //

        // If an exception is thrown during the object construction and there is a weak pointer to the object itself,
        // we may get to this point, but should not destroy the reference counters, because it will be destroyed by MakeNewRCObj
        // Consider this example:
        //
        //   A ==sp==> B ---wp---> A
        //
        //   MakeNewRCObj::operator()
        //    try
        //    {
        //     A.ctor()
        //       B.ctor()
        //        wp.ctor m_NumWeakReferences==1
        //        throw
        //        wp.dtor m_NumWeakReferences==0, destroy this
        //    }
        //    catch(...)
        //    {
        //       Destroy ref counters second time
        //    }
        //
        // clang-format on
        if (NumWeakReferences == 0 &&
            /*m_NumStrongReferences == 0 &&*/ m_ObjectState.load() ==
                ObjectState::Destroyed) {
            DONUT_VERIFY(m_NumStrongReferences.load() == 0);
#if !DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
            DONUT_VERIFY(
                m_ObjectWrapperBuffer.val[0] == 0 && m_ObjectWrapperBuffer.val[1] == 0,
                "Object wrapper must be null");
#endif
            // m_ObjectState is set to ObjectState::Destroyed under the lock. If the state
            // is not Destroyed, ReleaseStrongRef() will take care of it. Access to Object
            // wrapper and decrementing m_NumWeakReferences is atomic. Since we acquired the
            // lock, no other thread can access either of them. Access to
            // m_NumStrongReferences is NOT PROTECTED by lock.

            // There are no more references to the ref counters object and the object itself
            // is already destroyed.
            // We can safely unlock it and destroy.
            // If we do not unlock it, this->m_LockFlag will expire,
            // which will cause Lock.~LockHelper() to crash.
            Guard.unlock();
            SelfDestroy();
        }
        return NumWeakReferences;
    }

    void ReleaseWeakRefLockFree() {
        const auto NumWeakReferences = m_NumWeakReferences.fetch_add(-1) - 1;
        DONUT_VERIFY(NumWeakReferences >= 0, "Inconsistent call to ReleaseWeakRef()");

        if (NumWeakReferences == 0) {
            SelfDestroy();
        }
    }

    FRESULT
    QueryObject(FREFIID riid, void** ppv) {
        if (m_ObjectState.load() != ObjectState::Alive)
            return FE_NOT_ALIVE_OBJECT;  // Early exit

        FRESULT hr = FS_OK;
        if (ppv) *ppv = nullptr;

        // It is essential to INCREMENT REF COUNTER while HOLDING THE LOCK to make sure that
        // StrongRefCnt > 1 guarantees that the object is alive.

        // If other thread started deleting the object in ReleaseStrongRef(), then
        // m_NumStrongReferences==0 We must make sure only one thread is allowed to
        // increment the counter to guarantee that if StrongRefCnt > 1, there is at least
        // one real strong reference left. Otherwise the following scenario may occur:
        //
        //                                      m_NumStrongReferences == 1
        //
        //    Thread 1 - ReleaseStrongRef()    |    Thread 2 - QueryObject()       | Thread
        //    3 - QueryObject()
        //                                     |                                   |
        //  - Decrement m_NumStrongReferences  | -Increment m_NumStrongReferences  |
        //  -Increment m_NumStrongReferences
        //  - Read RefCount == 0               | -Read StrongRefCnt==1             | -Read
        //  StrongRefCnt==2
        //    Destroy the object               |                                   | -Return
        //    reference to the soon
        //                                     |                                   |  to
        //                                     expire object
        //
        SpinLockGuard Guard{m_Lock};

        const auto StrongRefCnt = m_NumStrongReferences.fetch_add(+1) + 1;

        // Checking if m_ObjectState == ObjectState::Alive only is not reliable:
        //
        //           This thread                    |          Another thread
        //                                          |
        //   1. Acquire the lock                    |
        //                                          |    1. Decrement m_NumStrongReferences
        //   2. Increment m_NumStrongReferences     |    2. Test RefCount==0
        //   3. Read StrongRefCnt == 1              |    3. Start destroying the object
        //      m_ObjectState == ObjectState::Alive |
        //   4. DO NOT return the reference to      |    4. Wait for the lock, m_ObjectState
        //   == ObjectState::Alive
        //      the object                          |
        //   5. Decrement m_NumStrongReferences     |
        //                                          |    5. Destroy the object

        if (m_ObjectState == ObjectState::Alive && StrongRefCnt > 1) {
            DONUT_VERIFY(
                m_ObjectWrapperBuffer.val[0] != 0 && m_ObjectWrapperBuffer.val[1] != 0,
                "Object wrapper is not initialized");
            // QueryInterface() must not lock the object, or a deadlock happens.
            // The only other two methods that lock the object are ReleaseStrongRef()
            // and ReleaseWeakRef(), which are never called by QueryInterface()
            auto* pWrapper =
                reinterpret_cast<details::ObjectWrapperBase*>(&m_ObjectWrapperBuffer);
            hr = pWrapper->QueryInterface(riid, ppv);
        }
        m_NumStrongReferences.fetch_add(-1);

        return hr;
    }

    FLONG GetNumStrongRefs() const override { return m_NumStrongReferences.load(); }

    FBOOL IsExpired() const override {
        return m_NumStrongReferences.load() > 0 &&
               m_ObjectState.load() == ObjectState::Alive;
    }

    // FLONG GetNumWeakRefs() const { return m_NumWeakReferences.load(); }

 private:
    template <typename AllocatorType>
    friend class MakeNewRCObj;
#if DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
    template <typename ObjectType>
    friend struct details::PackedCtrlBlock;
#endif

    WeakReferenceImpl() noexcept {}

    template <typename ObjectType, typename AllocatorType>
    void Attach(ObjectType* pObject, AllocatorType* pAllocator) throw() {
        DONUT_VERIFY(m_ObjectState.load() == ObjectState::NotInitialized,
                        "Object has already been attached");
#if DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
        static_assert(sizeof(details::ObjectWrapper<ObjectType, AllocatorType>) ==
                          sizeof(m_ObjectWrapperBuffer),
                      "Unexpected object wrapper size");

        new (&m_ObjectWrapperBuffer)
            details::PackedObjectWrapper<ObjectType, AllocatorType>{pObject, pAllocator};
        m_ObjectState.store(ObjectState::Alive);
#else
        static_assert(sizeof(details::ObjectWrapper<ObjectType, AllocatorType>) ==
                          sizeof(m_ObjectWrapperBuffer),
                      "Unexpected object wrapper size");

        new (&m_ObjectWrapperBuffer)
            details::ObjectWrapper<ObjectType, AllocatorType>{pObject, pAllocator};
        m_ObjectState.store(ObjectState::Alive);
#endif
    }

    void TryDestroyObject() {
        // clang-format off
        // Since RefCount==0, there are no more strong references and the only place
        // where strong ref counter can be incremented is from QueryObject().

        // If several threads were allowed to get to this point, there would
        // be serious risk that <this> had already been destroyed and m_LockFlag expired.
        // Consider the following scenario:
        //                                      |
        //             This thread              |             Another thread
        //                                      |
        //                      m_NumStrongReferences == 1
        //                      m_NumWeakReferences == 1
        //                                      |
        // 1. Decrement m_NumStrongReferences   |
        //    Read RefCount==0, no lock acquired|
        //                                      |   1. Run QueryObject()
        //                                      |      - acquire the lock
        //                                      |      - increment m_NumStrongReferences
        //                                      |      - release the lock
        //                                      |
        //                                      |   2. Run ReleaseWeakRef()
        //                                      |      - decrement m_NumWeakReferences
        //                                      |
        //                                      |   3. Run ReleaseStrongRef()
        //                                      |      - decrement m_NumStrongReferences
        //                                      |      - read RefCount==0
        //
        //         Both threads will get to this point. The first one will destroy <this>
        //         The second one will read expired m_LockFlag

        //  IT IS CRUCIALLY IMPORTANT TO ASSURE THAT ONLY ONE THREAD WILL EVER
        //  EXECUTE THIS CODE

        // The solution is to atomically increment strong ref counter in QueryObject().
        // There are two possible scenarios depending on who first increments the counter:


        //                                                     Scenario I
        //
        //             This thread              |     Another thread - QueryObject()        |  One more thread - QueryObject()
        //                                      |                                           |
        //                        m_NumStrongReferences == 1                                |
        //                                      |                                           |
        //                                      |   1. Acquire the lock                     |
        // 1. Decrement mlNumStrongReferences   |                                           |   1. Wait for the lock
        // 2. Read RefCount==0                  |   2. Increment m_NumStrongReferences      |
        // 3. Start destroying the object       |   3. Read StrongRefCnt == 1               |
        // 4. Wait for the lock                 |   4. DO NOT return the reference          |
        //                                      |      to the object                        |
        //                                      |   5. Decrement m_NumStrongReferences      |
        // _  _  _  _  _  _  _  _  _  _  _  _  _|   6. Release the lock _  _  _  _  _  _  _ |_  _  _  _  _  _  _  _  _  _  _  _  _  _
        //                                      |                                           |   2. Acquire the lock
        //                                      |                                           |   3. Increment m_NumStrongReferences
        //                                      |                                           |   4. Read StrongRefCnt == 1
        //                                      |                                           |   5. DO NOT return the reference
        //                                      |                                           |      to the object
        //                                      |                                           |   6. Decrement m_NumStrongReferences
        //  _  _  _  _  _  _  _  _  _  _  _  _  | _  _  _  _  _  _  _  _  _  _  _  _  _  _  | _ 7. Release the lock _  _  _  _  _  _
        // 5. Acquire the lock                  |                                           |
        //   - m_NumStrongReferences==0         |                                           |
        // 6. DESTROY the object                |                                           |
        //                                      |                                           |

        //  QueryObject() MUST BE SERIALIZED for this to work properly!


        //                                   Scenario II
        //
        //             This thread              |     Another thread - QueryObject()
        //                                      |
        //                       m_NumStrongReferences == 1
        //                                      |
        //                                      |   1. Acquire the lock
        //                                      |   2. Increment m_NumStrongReferences
        // 1. Decrement m_NumStrongReferences   |
        // 2. Read RefCount>0                   |
        // 3. DO NOT destroy the object         |   3. Read StrongRefCnt > 1 (while m_NumStrongReferences == 1)
        //                                      |   4. Return the reference to the object
        //                                      |       - Increment m_NumStrongReferences
        //                                      |   5. Decrement m_NumStrongReferences
        // clang-format on
#ifdef _DEBUG
        {
            auto NumStrongRefs = m_NumStrongReferences.load();
            DONUT_VERIFY(NumStrongRefs == 0 || NumStrongRefs == 1,
                            "Num strong references (", NumStrongRefs,
                            ") is expected to be 0 or 1");
        }
#endif

        // Acquire the lock.
        std::unique_lock<SpinLock> Guard{m_Lock};

        // QueryObject() first acquires the lock, and only then increments and
        // decrements the ref counter. If it reads 1 after incrementing the counter,
        // it does not return the reference to the object and decrements the counter.
        // If we acquired the lock, QueryObject() will not start until we are done
        DONUT_VERIFY(m_NumStrongReferences.load() == 0 &&
                        m_ObjectState.load() == ObjectState::Alive);

        // Extra caution
        if (m_NumStrongReferences.load() == 0 &&
            m_ObjectState.load() == ObjectState::Alive) {
            DONUT_VERIFY(
                m_ObjectWrapperBuffer.val[0] != 0 && m_ObjectWrapperBuffer.val[1] != 0,
                "Object wrapper is not initialized");
            // We cannot destroy the object while reference counters are locked as this will
            // cause a deadlock in cases like this:
            //
            //    A ==sp==> B ---wp---> A
            //
            //    RefCounters_A.Lock();
            //    delete A{
            //      A.~dtor(){
            //          B.~dtor(){
            //              wpA.ReleaseWeakRef(){
            //                  RefCounters_A.Lock(); // Deadlock
            //

#if DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
            auto* const pWrapper =
                reinterpret_cast<details::ObjectWrapperBase*>(&m_ObjectWrapperBuffer);
#else
            // So we copy the object wrapper and destroy the object after unlocking the
            // reference counters
            details::ObjectWrapperStorage ObjectWrapperBufferCopy;
            memcpy(&ObjectWrapperBufferCopy, &m_ObjectWrapperBuffer,
                   sizeof(m_ObjectWrapperBuffer));
            memset(&m_ObjectWrapperBuffer, 0, sizeof(m_ObjectWrapperBuffer));

            auto* pWrapper =
                reinterpret_cast<details::ObjectWrapperBase*>(&ObjectWrapperBufferCopy);
#endif

            // In a multithreaded environment, reference counters object may
            // be destroyed at any time while m_pObject->~dtor() is running.
            // NOTE: m_pObject may not be the only object referencing m_pWeakRef.
            //       All objects that are owned by m_pObject will point to the same
            //       reference counters object.

            // Note that this is the only place where m_ObjectState is
            // modified after the ref counters object has been created
            m_ObjectState.store(ObjectState::Destroyed);

            // The object is now detached from the reference counters, and it is if
            // it was destroyed since no one can obtain access to it.

            // It is essentially important to check the number of weak references
            // while the object is locked. Otherwise reference counters object
            // may be destroyed twice if ReleaseWeakRef() is executed by other thread:
            //
            //             This thread             |    Another thread - ReleaseWeakRef()
            //                                     |
            // 1. Decrement m_NumStrongReferences, |
            //    m_NumStrongReferences==0,        |
            //    acquire the lock, destroy        |
            //    the obj, release the lock        |
            //    m_NumWeakReferences == 1         |
            //                                     |   1. Acquire the lock,
            //                                     |      decrement m_NumWeakReferences,
            //                                     |      m_NumWeakReferences == 0,
            //                                     |      m_ObjectState ==
            //                                     ObjectState::Destroyed
            //                                     |
            // 2. Read m_NumWeakReferences == 0    |
            // 3. Destroy the ref counters obj     |   2. Destroy the ref counters obj
            //
            const auto bMayDestroyThis = m_NumWeakReferences.load() == 0;
            // ReleaseWeakRef() decrements m_NumWeakReferences, and checks it for
            // zero only after acquiring the lock. So if m_NumWeakReferences==0, no
            // weak reference-related code may be running

            // There may be scenario that object dtor decrement weak reference, so we
            // increment weak reference for this
            // Another reason we add weak reference here is for implement of packing
            // reference control block(WeakReferenceImpl) and object memory together
            // so as to optimize memory allocation and cache missing.
            if (donut_unlikely(!bMayDestroyThis)) AddWeakRef();

            // We must explicitly unlock the object now to avoid deadlocks. Also,
            // if this is deleted, this->m_LockFlag will expire, which will cause
            // Lock.~LockHelper() to crash
            Guard.unlock();

            // Destroy referenced object
            // Note: Object dtor may release weak reference, so we need to re-evaluate weak
            // reference
            //      count
            pWrapper->DestroyObject();

            // // Note that <this> may be destroyed here already,
            // // see comments in ~ControlledObjectType()
            if (donut_likely(bMayDestroyThis))
                SelfDestroy();
            else
                ReleaseWeakRefLockFree();
        }
    }

    void SelfDestroy() {
#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerRemoveObject(this);
#endif

#if DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
        auto ObjWrappStorageCopy = m_ObjectWrapperBuffer;
        auto pWrapper = reinterpret_cast<details::ObjectWrapperBase*>(&ObjWrappStorageCopy);
        pWrapper->DeletePackedStorage(this);
#else
        delete this;
#endif
    }

    ~WeakReferenceImpl() {
        DONUT_VERIFY(
            m_NumStrongReferences.load() == 0 && m_NumWeakReferences.load() == 0,
            "There exist outstanding references to the object being destroyed");
    }

    // No copies/moves
    // clang-format off
    WeakReferenceImpl             (const WeakReferenceImpl&)  = delete;
    WeakReferenceImpl             (      WeakReferenceImpl&&) = delete;
    WeakReferenceImpl& operator = (const WeakReferenceImpl&)  = delete;
    WeakReferenceImpl& operator = (      WeakReferenceImpl&&) = delete;
    // clang-format on

    std::atomic<FLONG> m_NumStrongReferences{1};
    std::atomic<FLONG> m_NumWeakReferences{0};

    SpinLock m_Lock;

    enum class ObjectState : uint32_t { NotInitialized = 0, Alive = 1, Destroyed = 2 };
    std::atomic<ObjectState> m_ObjectState{ObjectState::NotInitialized};

    details::ObjectWrapperStorage m_ObjectWrapperBuffer{};
};

/// Base class for all reference counting objects, must be one of IWeakReferenceSource
template <typename BaseItf>
class RefCountedObject : public BaseItf {
 public:
    // Constructor with weak reference syntax
    RefCountedObject(IWeakReference* pWeakRef) noexcept
        : m_pWeakRef{pWeakRef ? ClassPtrCast<WeakReferenceImpl>(pWeakRef) : nullptr} {
        // If object is allocated on stack, ref counters will be null
        // DONUT_VERIFY(pRefCounters != nullptr, "Reference counters must not be null")
    }

    // Virtual destructor makes sure all derived classes can be destroyed
    // through the pointer to the base class
    virtual ~RefCountedObject() {
        // WARNING! m_pWeakRef may be expired in scenarios like this:
        //
        //    A ==sp==> B ---wp---> A
        //
        //    RefCounters_A.ReleaseStrongRef(){ // NumStrongRef == 0, NumWeakRef == 1
        //      bMayDestroyThis = (m_NumWeakReferences == 0) == false;
        //      delete A{
        //        A.~dtor(){
        //            B.~dtor(){
        //                wpA.ReleaseWeakRef(){ // NumStrongRef == 0, NumWeakRef == 0,
        //                m_pObject==nullptr
        //                    delete RefCounters_A;
        //        ...
        //        DONUT_VERIFY( m_pWeakRef->GetNumStrongRefs() == 0 // Access violation!

        // This also may happen if one thread is executing ReleaseStrongRef(), while
        // another one is simultaneously running ReleaseWeakRef().

        // DONUT_VERIFY( m_pWeakRef->GetNumStrongRefs() == 0,
        //         "There remain strong references to the object being destroyed" );
    }

    inline virtual FLONG AddRef() override final {
        // Since type of m_pWeakRef is WeakReference,
        // this call will not be virtual and should be inlined
        DONUT_VERIFY(m_pWeakRef != nullptr);
        return m_pWeakRef->AddStrongRef();
    }

    inline virtual FLONG Release() override {
        // Since type of m_pWeakRef is WeakReference,
        // this call will not be virtual and should be inlined
        DONUT_VERIFY(m_pWeakRef != nullptr);
        return m_pWeakRef->ReleaseStrongRef();
    }

    template <class TPreObjectDestroy>
    inline FLONG Release(TPreObjectDestroy&& PreObjectDestroy) {
        DONUT_VERIFY(m_pWeakRef != nullptr);
        return m_pWeakRef->ReleaseStrongRef(
            std::forward<TPreObjectDestroy>(PreObjectDestroy));
    }

    IWeakReference* GetWeakReference() override final { return m_pWeakRef; }

 protected:
    template <typename AllocatorType>
    friend class MakeNewRCObj;

    template <typename ObjectType, typename AllocatorType>
    friend class details::ObjectWrapper;

    friend class WeakReferenceImpl;

    template <typename ObjectType>
    friend struct details::WeakRefTypeTrait;  // Used for get implement object type of
                                              // IWeakReference.

    using WeakRefImplType = WeakReferenceImpl;

    // Operator delete can only be called from MakeNewRCObj if an exception is thrown,
    // or from WeakReference when object is destroyed
    // It needs to be protected (not private!) to allow generation of destructors in derived
    // classes

    void operator delete(void* ptr) { GetDefaultMemAllocator()->Free(ptr); }

    template <typename ObjectAllocatorType>
    void operator delete(void* ptr, ObjectAllocatorType& Allocator,
                         const char* dbgDescription, const char* dbgFileName,
                         const int32_t dbgLineNumber) {
        return Allocator.Free(ptr);
    }

 private:
    // Operator new is private, and can only be called by MakeNewRCObj

    void* operator new(size_t Size) {
        GetDefaultMemAllocator()->Allocate(Size, nullptr, nullptr, 0);
    }

    template <typename ObjectAllocatorType>
    void* operator new(size_t Size, ObjectAllocatorType& Allocator,
                       const char* dbgDescription, const char* dbgFileName,
                       const int32_t dbgLineNumber) {
        return Allocator.Allocate(Size, dbgDescription, dbgFileName, dbgLineNumber);
    }

    // Note that the type of the reference counters is WeakReference,
    // not IWeakReference. This avoids virtual calls from
    // AddRef() and Release() methods
    WeakReferenceImpl* const m_pWeakRef;
};

template <typename BaseItf>
struct WeakObjectImpl : public RefCountedObject<BaseItf> {
 public:
    WeakObjectImpl(IWeakReference* pWeakRef) : RefCountedObject<BaseItf>(pWeakRef) {}

    FRESULT QueryInterface(FREFIID riid, void** ppv) override {
        if (riid == IID_IObject) {
            if (ppv) {
                *ppv = static_cast<IObject*>(this);
                this->AddRef();
            }
            return FS_OK;
        } else {
            if (ppv) *ppv = nullptr;

            return FE_NOINTERFACE;
        }
    }
};

template <typename BaseItf>
struct ObjectImpl: public BaseItf {
 public:
    ObjectImpl() {}

    FRESULT QueryInterface(FREFIID riid, void** ppv) override {
        if (riid == IID_IObject) {
            if (ppv) {
                *ppv = static_cast<IObject*>(this);
                this->AddRef();
            }
            return FS_OK;
        } else {
            if (ppv) *ppv = nullptr;

            return FE_NOINTERFACE;
        }
    }

    FLONG AddRef() override final {
        FLONG RefCount = m_NumStrongReferences.fetch_add(+1, std::memory_order_relaxed) + 1;
        return RefCount;
    }

    FLONG Release() override final {
        FLONG RefCount = m_NumStrongReferences.fetch_add(-1) - 1;
        if (RefCount == 0) DestroyObject();
        return RefCount;
    }

    void DestroyObject() {
        auto ObjWrapperStorageCopy = m_ObjWrapperStorage;
        auto pWrapper =
            reinterpret_cast<details::ObjectWrapperBase*>(&ObjWrapperStorageCopy);
        pWrapper->DestroyObject();
    }

 protected:
    template <typename AllocatorType>
    friend class MakeNewRCObj;

    template <typename ObjectType, typename AllocatorType>
    friend class details::ObjectWrapper;
    // Operator delete can only be called from MakeNewRCObj if an exception is thrown,
    // or from WeakReference when object is destroyed
    // It needs to be protected (not private!) to allow generation of destructors in derived
    // classes

    void operator delete(void* ptr) { GetDefaultMemAllocator()->Free(ptr); }

    template <typename ObjectAllocatorType>
    void operator delete(void* ptr, ObjectAllocatorType& Allocator,
                         const char* dbgDescription, const char* dbgFileName,
                         const int32_t dbgLineNumber) {
        return Allocator.Free(ptr);
    }

 private:
    template <typename ObjectType, typename AllocatorType>
    void Attach(ObjectType* pObject, AllocatorType* pAllocator) throw() {
        static_assert(sizeof(details::ObjectWrapper<ObjectType, AllocatorType>) ==
                          sizeof(m_ObjWrapperStorage),
                      "Unexpected object wrapper size");
        new (&m_ObjWrapperStorage)
            details::ObjectWrapper<ObjectType, AllocatorType>{pObject, pAllocator};
    }

    // Operator new is private, and can only be called by MakeNewRCObj

    void* operator new(size_t Size) {
        return GetDefaultMemAllocator()->Allocate(Size, nullptr, 0);
    }

    template <typename ObjectAllocatorType>
    void* operator new(size_t Size, ObjectAllocatorType& Allocator,
                       const char* dbgDescription, const char* dbgFileName,
                       const int32_t dbgLineNumber) {
        return Allocator.Allocate(Size, dbgFileName, dbgLineNumber);
    }

    std::atomic<FLONG> m_NumStrongReferences{1};
    details::ObjectWrapperStorage m_ObjWrapperStorage{};
};

template <
    typename BaseItf,
    typename std::enable_if<!details::IsWeakReferenceSource<BaseItf>::value, int>::type = 0>
class DelegatingObjectImpl : public BaseItf {
 public:
    DelegatingObjectImpl(IObject* pOwner) : m_pOwner(pOwner) {}

    FLONG AddRef() override final { return m_pOwner->AddRef(); }

    FLONG Release() override final { return m_pOwner->Release(); }

    FRESULT QueryInterface(FREFIID riid, void** ppv) override {
        return m_pOwner->QueryInterface(riid, ppv);
    }

    virtual FRESULT NonDelegatingQueryInterface(FREFIID riid, void** ppv) {
        return FE_NOINTERFACE;
    }

    void DestroyObject() {
        auto ObjWrapperStorageCopy = m_ObjWrapperStorage;
        auto pWrapper =
            reinterpret_cast<details::ObjectWrapperBase*>(&ObjWrapperStorageCopy);
        pWrapper->DestroyObject();
    }

 protected:
    template <typename ObjectType, typename AllocatorType>
    friend class MakeNewRCDelegating;

    template <typename ObjectType, typename AllocatorType>
    friend class details::ObjectWrapper;
    // Operator delete can only be called from MakeNewRCObj if an exception is thrown,
    // or from WeakReference when object is destroyed
    // It needs to be protected (not private!) to allow generation of destructors in derived
    // classes

    void operator delete(void* ptr) { GetDefaultMemAllocator()->Free(ptr); }

    template <typename ObjectAllocatorType>
    void operator delete(void* ptr, ObjectAllocatorType& Allocator,
                         const char* dbgDescription, const char* dbgFileName,
                         const int32_t dbgLineNumber) {
        return Allocator.Free(ptr);
    }

    IObject* m_pOwner;

 private:
    template <typename ObjectType, typename AllocatorType>
    void Attach(ObjectType* pObject, AllocatorType* pAllocator) throw() {
        static_assert(sizeof(details::ObjectWrapper<ObjectType, AllocatorType>) ==
                          sizeof(m_ObjWrapperStorage),
                      "Unexpected object wrapper size");
        new (&m_ObjWrapperStorage)
            details::ObjectWrapper<ObjectType, AllocatorType>{pObject, pAllocator};
    }

    // Operator new is private, and can only be called by MakeNewRCObj

    void* operator new(size_t Size) {
        return GetDefaultMemAllocator()->Allocate(Size, nullptr, 0);
    }

    template <typename ObjectAllocatorType>
    void* operator new(size_t Size, ObjectAllocatorType& Allocator,
                       const char* dbgDescription, const char* dbgFileName,
                       const int32_t dbgLineNumber) {
        return Allocator.Allocate(Size, dbgFileName, dbgLineNumber);
    }

    details::ObjectWrapperStorage m_ObjWrapperStorage{};
};

#if DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
namespace details {

template <typename ObjectType>
struct PackedCtrlBlock {
    WeakReferenceImpl WeakRef;
    typename std::aligned_storage<sizeof(ObjectType), alignof(ObjectType)>::type Storage;
    PackedCtrlBlock() {}
    ~PackedCtrlBlock() {}

    void operator delete(void* ptr) { GetDefaultMemAllocator()->Free(ptr); }

    template <typename ObjectAllocatorType>
    void operator delete(void* ptr, ObjectAllocatorType& Allocator,
                         const char* dbgDescription, const char* dbgFileName,
                         const int32_t dbgLineNumber) {
        return Allocator.Free(ptr);
    }

    void* operator new(size_t Size) {
        return GetDefaultMemAllocator()->Allocate(Size, nullptr, 0);
    }

    template <typename ObjectAllocatorType>
    void* operator new(size_t Size, ObjectAllocatorType& Allocator,
                       const char* dbgDescription, const char* dbgFileName,
                       const int32_t dbgLineNumber) {
        return Allocator.Allocate(Size, dbgFileName, dbgLineNumber);
    }
};

template <typename ObjectType, typename AllocatorType>
inline void PackedObjectWrapper<ObjectType, AllocatorType>::DeletePackedStorage(
    void* pWeakRef) noexcept {
    using Tpcb = PackedCtrlBlock<ObjectType>;
    Tpcb* pCtrlBlock = reinterpret_cast<Tpcb*>(pWeakRef);
    if (m_pAllocator) {
        pCtrlBlock->~Tpcb();
        m_pAllocator->Free(pCtrlBlock);
    } else
        delete pCtrlBlock;
}
}
#endif

template <typename AllocatorType>
class MakeNewRCObj {
 public:
    MakeNewRCObj(AllocatorType& Allocator, const char* Description, const char* FileName,
                 const int32_t LineNumber) noexcept
        :  // clang-format off
        m_pAllocator{&Allocator}
#ifdef DONUT_DEVELOPMENT
      , m_dvpDescription{Description}
      , m_dvpFileName   {FileName   }
      , m_dvpLineNumber {LineNumber }
    // clang-format on
#endif
    {
    }

    MakeNewRCObj() noexcept
        :  // clang-format off
        m_pAllocator    {nullptr}
#ifdef DONUT_DEVELOPMENT
      , m_dvpDescription{nullptr}
      , m_dvpFileName   {nullptr}
      , m_dvpLineNumber {0      }
#endif
    // clang-format on
    {
    }

    // clang-format off
    MakeNewRCObj           (const MakeNewRCObj&)  = delete;
    MakeNewRCObj           (      MakeNewRCObj&&) = delete;
    MakeNewRCObj& operator=(const MakeNewRCObj&)  = delete;
    MakeNewRCObj& operator=(      MakeNewRCObj&&) = delete;
    // clang-format on

    template <typename ObjectType, typename... CtorArgTypes>
    ObjectType* RcNew(CtorArgTypes&&... CtorArgs) const {
        return RcNewImpl<ObjectType>(0, std::forward<CtorArgTypes>(CtorArgs)...);
    }

 private:
    // SFINEA overload for IWeakReferenceSoure kind object type
    template <typename Tp, typename... CtorArgTypes>
    Tp* RcNewImpl(
        typename std::enable_if<details::IsWeakReferenceSource<Tp>::value, int>::type,
        CtorArgTypes&&... CtorArgs) const {
#ifndef DONUT_DEVELOPMENT
        static constexpr const char* m_dvpDescription = "<Unavailable in release build>";
        static constexpr const char* m_dvpFileName = "<Unavailable in release build>";
        static constexpr int32_t m_dvpLineNumber = -1;
#endif

#if DONUT_PACK_CONTROL_BLOCK_AND_OBJECT
        using Tpcb = details::PackedCtrlBlock<Tp>;
        Tpcb* pCBlock = nullptr;
        WeakReferenceImpl* pWeakRef = nullptr;
        Tp* pObj = nullptr;

        try {
            if (m_pAllocator) {
                pCBlock = new (*m_pAllocator, m_dvpDescription, m_dvpFileName,
                               m_dvpLineNumber) Tpcb{};
            } else
                pCBlock = new Tpcb{};

            pWeakRef = &pCBlock->WeakRef;

            pObj = ::new (&pCBlock->Storage)
                Tp(pWeakRef, std::forward<CtorArgTypes>(CtorArgs)...);
            pWeakRef->Attach(pObj, m_pAllocator);
        } catch (...) {
            if (pWeakRef) {
                pWeakRef->m_NumStrongReferences = 0;
                // Obviously, control block is initialized.
                if (m_pAllocator) {
                    pCBlock->~Tpcb();
                    m_pAllocator->Free(pCBlock);
                } else
                    delete pCBlock;
            }
            throw;
        }

#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerAddObject(pWeakRef);
        details::ObjectTrackerRemoveObject(pObj);
#endif

        return pObj;
#else
        WeakReferenceImpl* pWeakRef = new WeakReferenceImpl;
#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerAddObject(pWeakRef);
#endif

        Tp* pObj = nullptr;
        try {
            // Operators new and delete of RefCountedObject are private and only accessible
            // by methods of MakeNewRCObj
            if (m_pAllocator)
                pObj = new (*m_pAllocator, m_dvpDescription, m_dvpFileName, m_dvpLineNumber)
                    Tp{pWeakRef, std::forward<CtorArgTypes>(CtorArgs)...};
            else
                pObj = new Tp{pWeakRef, std::forward<CtorArgTypes>(CtorArgs)...};

            pWeakRef->Attach<Tp, AllocatorType>(pObj, m_pAllocator);
        } catch (...) {
            pWeakRef->m_NumStrongReferences = 0;
            pWeakRef->SelfDestroy();
#ifdef DONUT_DUMP_ALIVE_OBJECTS
            details::ObjectTrackerRemoveObject(pWeakRef);
#endif
            throw;
        }

#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerAddObject(pObj);
#endif

        return pObj;
#endif
    }

    // SFINEA overload for IObject (but non-IWeakReferenceSource) kind object type
    template <typename Tp, typename... CtorArgTypes>
    Tp* RcNewImpl(
        typename std::enable_if<!details::IsWeakReferenceSource<Tp>::value, int>::type,
        CtorArgTypes&&... CtorArgs) const {
#ifndef DONUT_DEVELOPMENT
        static constexpr const char* m_dvpDescription = "<Unavailable in release build>";
        static constexpr const char* m_dvpFileName = "<Unavailable in release build>";
        static constexpr int32_t m_dvpLineNumber = -1;
#endif

        Tp* pObj = nullptr;
        try {
            // Operators new and delete of RefCountedObject are private and only accessible
            // by methods of MakeNewRCObj
            if (m_pAllocator)
                pObj = new (*m_pAllocator, m_dvpDescription, m_dvpFileName, m_dvpLineNumber)
                    Tp{std::forward<CtorArgTypes>(CtorArgs)...};
            else
                pObj = new Tp{std::forward<CtorArgTypes>(CtorArgs)...};

            pObj->template Attach<Tp, AllocatorType>(pObj, m_pAllocator);
        } catch (...) {
#ifdef DONUT_DUMP_ALIVE_OBJECTS
            details::ObjectTrackerRemoveObject(pObj);
#endif
            throw;
        }

#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerAddObject(pObj);
#endif

        return pObj;
    }

    AllocatorType* m_pAllocator;

#ifdef DONUT_DEVELOPMENT
    const char* const m_dvpDescription;
    const char* const m_dvpFileName;
    int32_t const m_dvpLineNumber;
#endif
};

template <typename ObjectType, typename AllocatorType>
class MakeNewRCDelegating {
 public:
    MakeNewRCDelegating(AllocatorType& Allocator, IObject* pOwner, const char* Description,
                        const char* FileName,
                        const int32_t LineNumber) noexcept
        :  // clang-format off
        m_pAllocator{&Allocator}
        , m_pOwner(pOwner)
#ifdef DONUT_DEVELOPMENT
      , m_dvpDescription{Description}
      , m_dvpFileName   {FileName   }
      , m_dvpLineNumber {LineNumber }
    // clang-format on
#endif
    {
    }

    MakeNewRCDelegating(IObject* pOwner) noexcept
        :  // clang-format off
        m_pAllocator    {nullptr}
        , m_pOwner{pOwner}
#ifdef DONUT_DEVELOPMENT
      , m_dvpDescription{nullptr}
      , m_dvpFileName   {nullptr}
      , m_dvpLineNumber {0      }
#endif
    // clang-format on
    {
    }

    // clang-format off
    MakeNewRCDelegating           (const MakeNewRCDelegating&)  = delete;
    MakeNewRCDelegating           (      MakeNewRCDelegating&&) = delete;
    MakeNewRCDelegating& operator=(const MakeNewRCDelegating&)  = delete;
    MakeNewRCDelegating& operator=(      MakeNewRCDelegating&&) = delete;
    // clang-format on

    template <typename... CtorArgTypes>
    ObjectType* operator()(CtorArgTypes&&... CtorArgs) const {
#ifndef DONUT_DEVELOPMENT
        static constexpr const char* m_dvpDescription = "<Unavailable in release build>";
        static constexpr const char* m_dvpFileName = "<Unavailable in release build>";
        static constexpr int32_t m_dvpLineNumber = -1;
#endif

        ObjectType* pObj = nullptr;
        try {
            // Operators new and delete of RefCountedObject are private and only accessible
            // by methods of MakeNewRCObj
            if (m_pAllocator)
                pObj = new (*m_pAllocator, m_dvpDescription, m_dvpFileName, m_dvpLineNumber)
                    ObjectType{m_pOwner, std::forward<CtorArgTypes>(CtorArgs)...};
            else
                pObj = new ObjectType{m_pOwner, std::forward<CtorArgTypes>(CtorArgs)...};

            pObj->template Attach<ObjectType, AllocatorType>(pObj, m_pAllocator);
        } catch (...) {
#ifdef DONUT_DUMP_ALIVE_OBJECTS
            details::ObjectTrackerRemoveObject(pObj);
#endif
            throw;
        }

#ifdef DONUT_DUMP_ALIVE_OBJECTS
        details::ObjectTrackerAddObject(pObj);
#endif

        return pObj;
    }

 private:
    AllocatorType* m_pAllocator;
    IObject* m_pOwner;

#ifdef DONUT_DEVELOPMENT
    const char* const m_dvpDescription;
    const char* const m_dvpFileName;
    int32_t const m_dvpLineNumber;
#endif
};

#define MAKE_RC_OBJ(Allocator, Type)                                                   \
    donut::MakeNewRCObj<typename std::remove_reference<decltype(Allocator)>::type>( \
        Allocator, #Type, __FILE__, __LINE__)                                          \
        .RcNew<Type>

#define MAKE_RC_OBJ0(Type)                                                  \
    donut::MakeNewRCObj<donut::DefaultMemoryAllocator>(               \
        *donut::GetDefaultMemAllocator(), #Type, __FILE__, __LINE__) \
        .RcNew<Type>

#define MAKE_RC_DELEGATING(Allocator, Type, Owner)                        \
    donut::MakeNewRCDelegating<                                        \
        Type, typename std::remove_reference<decltype(Allocator)>::type>( \
        Allocator, Owner, #Type, __FILE__, __LINE__)

#define MAKE_RC_DELEGATING0(Type, Owner)                             \
    donut::MakeNewRCDelegating<Type, donut::DefaultMemoryAllocator>( \
        *donut::GetDefaultMemAllocator(), Owner, #Type, __FILE__, __LINE__)

}  // namespace donut
