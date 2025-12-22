#pragma once
#include <donut/core/object/Object.h>
#include <donut/core/object/DebugUtilities.h>

namespace donut {

template <typename T>
class WeakPtr;

// The main advantage of AutoPtr over the std::shared_ptr is that you can
// attach the same raw pointer to different smart pointers.
//
// For instance, the following code will crash since p will be released twice:
//
// auto *p = new char;
// std::shared_ptr<char> pTmp1(p);
// std::shared_ptr<char> pTmp2(p);
// ...

// This code, in contrast, works perfectly fine:
//
// ObjectImpl *pRawPtr(new ObjectImpl);
// AutoPtr<ObjectImpl> pSmartPtr1(pRawPtr);
// AutoPtr<ObjectImpl> pSmartPtr2(pRawPtr);
// ...

// Other advantage is that weak pointers remain valid until the
// object is alive, even if all smart pointers were destroyed:
//
// WeakPtr<ObjectImpl> pWeakPtr(pSmartPtr1);
// pSmartPtr1.Release();
// auto pSmartPtr3 = pWeakPtr.Lock();
// ..

// Weak pointers can also be attached directly to the object:
// WeakPtr<ObjectImpl> pWeakPtr(pRawPtr);
//

namespace details {

// T should be the AutoPtr<T> or a derived type of it, not just the interface
template <typename T>
class AutoPtrRef {
    using InterfaceType = typename T::InterfaceType;

public:
    AutoPtrRef(T* ptr) throw() { this->ptr_ = ptr; }

    // Conversion operators
    operator void**() const throw() { return reinterpret_cast<void**>(this->ptr_->ReleaseAndGetAddressOf()); }

    // This is our operator AutoPtr<U> (or the latest derived class from AutoPtr (e.g. WeakRef))
    operator T*() throw() {
        *this->ptr_ = nullptr;
        return this->ptr_;
    }

    // We define operator InterfaceType**() here instead of on ComPtrRefBase<T>, since
    // if InterfaceType is IObject or IInspectable, having it on the base will collide.
    operator InterfaceType**() throw() { return this->ptr_->ReleaseAndGetAddressOf(); }

    // This is used for FIID_PPV_ARGS in order to do __uuid_of(**(ppType)).
    // It does not need to clear  ptr_ at this point, it is done at WIID_PPV_ARGS_Helper(AutoPtrRef&) later in this
    // file.
    InterfaceType* operator*() throw() { return this->ptr_->Get(); }

    // Explicit functions
    InterfaceType* const* GetAddressOf() const throw() { return this->ptr_->GetAddressOf(); }

    InterfaceType** ReleaseAndGetAddressOf() throw() { return this->ptr_->ReleaseAndGetAddressOf(); }

private:
    T* ptr_;
};

template <bool Test, typename T = void>
using EnableIf = std::enable_if<Test, T>;

template <typename Tp, typename Ty>
using IsConvertible = std::is_convertible<Tp, Ty>;

template <typename Base, typename Derived>
using IsBaseOf = std::is_base_of<Base, Derived>;

template <typename Tp, typename Ty>
using IsSame = std::is_same<Tp, Ty>;

using nullptr_t = std::nullptr_t;

using BoolType = bool;

template <typename T>
struct WeakRefTypeTrait {
    // SFINEA overload when implement of IWeakReferenceSource is visible to compiling unit.
    template <typename Tp>
    static typename Tp::WeakRefImplType* GetWeakRef(typename Tp::WeakRefImplType*);
    // SFINEA overload when only IWeakReference is used.
    template <typename Tp>
    static IWeakReference* GetWeakRef(Tp*);

    using Type = T;
    using WeakRefType = typename std::remove_pointer<decltype(GetWeakRef<T>(0))>::type;
};

}  // namespace details

template <typename T>
FRESULT AsWeak(T* p, WeakPtr<T>* pWeak) throw();

template <typename T>
class AutoPtr {
public:
    typedef T InterfaceType;

protected:
    InterfaceType* ptr_;
    template <class U>
    friend class AutoPtr;

    void InternalAddRef() const throw() {
        if (ptr_ != nullptr) {
            ptr_->AddRef();
        }
    }

    FLONG InternalRelease() throw() {
        FLONG ref = 0;
        T* temp = ptr_;

        if (temp != nullptr) {
            ptr_ = nullptr;
            ref = temp->Release();
        }

        return ref;
    }

public:
#pragma region constructors
    AutoPtr() throw()
        : ptr_(nullptr) {}

    AutoPtr(details::nullptr_t) throw()
        : ptr_(nullptr) {}

    template <class U>
    AutoPtr(U* other) throw()
        : ptr_(other) {
        InternalAddRef();
    }

    AutoPtr(const AutoPtr& other) throw()
        : ptr_(other.ptr_) {
        InternalAddRef();
    }

    // copy constructor that allows to instantiate class when U* is convertible to T*
    template <class U>
    AutoPtr(
        const AutoPtr<U>& other,
        typename details::EnableIf<details::IsConvertible<U*, T*>::value, void*>::type* = 0) throw()
        : ptr_(other.ptr_) {
        InternalAddRef();
    }

    AutoPtr(AutoPtr&& other) throw()
        : ptr_(nullptr) {
        if (this != reinterpret_cast<AutoPtr*>(&reinterpret_cast<unsigned char&>(other))) {
            Swap(other);
        }
    }

    // Move constructor that allows instantiation of a class when U* is convertible to T*
    template <class U>
    AutoPtr(
        AutoPtr<U>&& other, typename details::EnableIf<details::IsConvertible<U*, T*>::value, void*>::type* = 0) throw()
        : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
#pragma endregion

#pragma region destructor
    ~AutoPtr() throw() { InternalRelease(); }
#pragma endregion

#pragma region assignment
    AutoPtr& operator=(details::nullptr_t) throw() {
        InternalRelease();
        return *this;
    }

    AutoPtr& operator=(T* other) throw() {
        if (ptr_ != other) {
            AutoPtr(other).Swap(*this);
        }
        return *this;
    }

    template <typename U>
    AutoPtr& operator=(U* other) throw() {
        AutoPtr(other).Swap(*this);
        return *this;
    }

    AutoPtr& operator=(const AutoPtr& other) throw() {
        if (ptr_ != other.ptr_) {
            AutoPtr(other).Swap(*this);
        }
        return *this;
    }

    template <class U>
    AutoPtr& operator=(const AutoPtr<U>& other) throw() {
        AutoPtr(other).Swap(*this);
        return *this;
    }

    AutoPtr& operator=(AutoPtr&& other) throw() {
        AutoPtr(static_cast<AutoPtr&&>(other)).Swap(*this);
        return *this;
    }

    template <class U>
    AutoPtr& operator=(AutoPtr<U>&& other) throw() {
        AutoPtr(static_cast<AutoPtr<U>&&>(other)).Swap(*this);
        return *this;
    }
#pragma endregion

#pragma region modifiers
    void Swap(AutoPtr&& r) throw() {
        T* tmp = ptr_;
        ptr_ = r.ptr_;
        r.ptr_ = tmp;
    }

    void Swap(AutoPtr& r) throw() {
        T* tmp = ptr_;
        ptr_ = r.ptr_;
        r.ptr_ = tmp;
    }
#pragma endregion

    explicit operator details::BoolType() const noexcept { return ptr_ != nullptr; }

    T* Get() const throw() { return ptr_; }

    operator T*() const noexcept { return ptr_; }

    InterfaceType* operator->() const throw() { return ptr_; }

    details::AutoPtrRef<AutoPtr<T>> operator&() throw() { return details::AutoPtrRef<AutoPtr<T>>(this); }

    const details::AutoPtrRef<const AutoPtr<T>> operator&() const throw() {
        return details::AutoPtrRef<const AutoPtr<T>>(this);
    }

    T* const* GetAddressOf() const throw() { return &ptr_; }

    T** GetAddressOf() throw() { return &ptr_; }

    T** ReleaseAndGetAddressOf() throw() {
        InternalRelease();
        return &ptr_;
    }

    T* Detach() throw() {
        T* ptr = ptr_;
        ptr_ = nullptr;
        return ptr;
    }

    void Attach(InterfaceType* other) throw() {
        if (ptr_ != nullptr) {
            auto ref = ptr_->Release();
            (void)ref;
            // Attaching to the same object only works if duplicate references are being coalesced. Otherwise
            // re-attaching will cause the pointer to be released and may cause a crash on a subsequent dereference.
            DONUT_ASSERT(ref != 0 || ptr_ != other);
        }

        ptr_ = other;
    }

    unsigned long Reset() { return InternalRelease(); }

    // Previously, unsafe behavior could be triggered when 'this' is AutoPtr<IInspectable> or
    // AutoPtr<IObject> and CopyTo is used to copy to another type U. The user will use operator& to convert the
    // destination into a AutoPtrRef, which can then implicit cast to IInspectable** and IObject**. If this
    // overload of CopyTo is not present, it will implicitly cast to IInspectable or IObject and match
    // CopyTo(InterfaceType**) instead. A valid polymoprhic downcast requires run-time type checking via QueryInterface,
    // so CopyTo(InterfaceType**) will break type safety. This overload matches AutoPtrRef before the implicit
    // cast takes place, preventing the unsafe downcast.
    template <typename U>
    FRESULT CopyTo(
        details::AutoPtrRef<AutoPtr<U>> ptr,
        typename details::EnableIf<details::IsSame<T, IObject>::value && !details::IsSame<U*, T*>::value, void*>::
            type* = 0) const throw() {
        return ptr_->QueryInterface(__uuid_of<U>(), ptr);
    }

    FRESULT CopyTo(InterfaceType** ptr) const throw() {
        InternalAddRef();
        *ptr = ptr_;
        return FS_OK;
    }

    FRESULT CopyTo(FREFIID riid, void** ptr) const throw() { return ptr_->QueryInterface(riid, ptr); }

    template <typename U>
    FRESULT CopyTo(U** ptr) const throw() {
        return ptr_->QueryInterface(__uuid_of<U>(), reinterpret_cast<void**>(ptr));
    }

    // query for U interface
    template <typename U>
    FRESULT As(details::AutoPtrRef<AutoPtr<U>> p) const throw() {
        return ptr_->QueryInterface(__uuid_of<U>(), p);
    }

    // query for U interface
    template <typename U>
    FRESULT As(AutoPtr<U>* p) const throw() {
        return ptr_->QueryInterface(__uuid_of<U>(), reinterpret_cast<void**>(p->ReleaseAndGetAddressOf()));
    }

    // query for riid interface and return as IObject
    FRESULT AsIID(FREFIID riid, AutoPtr<IObject>* p) const throw() {
        return ptr_->QueryInterface(riid, reinterpret_cast<void**>(p->ReleaseAndGetAddressOf()));
    }

    FRESULT AsWeak(WeakPtr<T>* pWeakRef) const throw() { return ::donut::AsWeak(ptr_, pWeakRef); }
};  // AutoPtr

/// Implementation of weak pointers
template <typename T>
class WeakPtr {
public:
    using WeakRefType = typename details::WeakRefTypeTrait<T>::WeakRefType;

    WeakPtr() noexcept {}

    explicit WeakPtr(T* pObj) noexcept
        : m_pWeakRef{ nullptr },
          m_pObject{ pObj } {
        if (m_pObject) {
            m_pWeakRef = WeakRefTypeCast(m_pObject->GetWeakReference());
            m_pWeakRef->AddRef();
        }
    }

    ~WeakPtr() { Reset(); }

    WeakPtr(const WeakPtr& WeakPtr) noexcept
        : m_pWeakRef{ WeakPtr.m_pWeakRef },
          m_pObject{ WeakPtr.m_pObject } {
        if (m_pWeakRef)
            m_pWeakRef->AddRef();
    }

    WeakPtr(WeakPtr&& other) noexcept
        : m_pWeakRef{ other.m_pWeakRef },
          m_pObject{ other.m_pObject } {
        other.m_pWeakRef = nullptr;
        other.m_pObject = nullptr;
    }

    explicit WeakPtr(AutoPtr<T>& AutoPtr) noexcept
        : m_pWeakRef{ AutoPtr ? WeakRefTypeCast(AutoPtr->GetWeakReference()) : nullptr },
          m_pObject{ static_cast<T*>(AutoPtr.Get()) } {
        if (m_pWeakRef)
            m_pWeakRef->AddRef();
    }

    WeakPtr& operator=(const WeakPtr& WeakPtr) noexcept {
        if (*this == WeakPtr)
            return *this;

        Reset();
        m_pObject = WeakPtr.m_pObject;
        m_pWeakRef = WeakPtr.m_pWeakRef;
        if (m_pWeakRef)
            m_pWeakRef->AddRef();
        return *this;
    }

    WeakPtr& operator=(T* pObj) noexcept { return operator=(WeakPtr(pObj)); }

    WeakPtr& operator=(WeakPtr&& other) noexcept {
        if (*this == other)
            return *this;

        Reset();
        m_pObject = other.m_pObject;
        m_pWeakRef = other.m_pWeakRef;
        other.m_pWeakRef = nullptr;
        other.m_pObject = nullptr;
        return *this;
    }

    WeakPtr& operator=(AutoPtr<T>& AutoPtr) noexcept {
        Reset();
        m_pObject = AutoPtr.Get();
        m_pWeakRef = m_pObject ? WeakRefTypeCast(m_pObject->GetWeakReference()) : nullptr;
        if (m_pWeakRef)
            m_pWeakRef->AddRef();
        return *this;
    }

    void Attach(T* pObj) noexcept {
        Reset();
        m_pObject = pObj;
        m_pWeakRef = pObj ? WeakRefTypeCast(pObj->GetWeakReference()) : nullptr;
    }

    void Reset() noexcept {
        if (m_pWeakRef)
            m_pWeakRef->Release();
        m_pWeakRef = nullptr;
        m_pObject = nullptr;
    }

    /// \note This method may not be reliable in a multithreaded environment.
    ///       However, when false is returned, the strong pointer created from
    ///       this weak pointer will reliably be empty.
    bool IsValid() const noexcept {
        return m_pObject != nullptr && m_pWeakRef != nullptr && m_pWeakRef->GetNumStrongRefs() > 0;
    }

    /// Returns a raw pointer to the managed object.
    /// \note The object may or may not be alive.
    ///       Use Lock() to atomically obtain a strong reference.
    T* UnsafeRawPtr() noexcept { return m_pObject; }
    const T* UnsafeRawPtr() const noexcept { return m_pObject; }

    /// Obtains a strong reference to the object
    AutoPtr<T> Lock() {
        AutoPtr<T> spObj;
        if (m_pWeakRef) {
            // Try to obtain a pointer to the owner object.
            // spOwner is only used to keep the object
            // alive while obtaining strong reference from
            // the raw pointer m_pObject
            AutoPtr<IObject> spOwner;
            m_pWeakRef->Resolve(FIID_PPV_ARGS(&spOwner));
            if (spOwner) {
                // If owner is alive, we can use our RAW pointer to
                // create strong reference
                spObj = m_pObject;
            } else {
                // Owner object has been destroyed. There is no reason
                // to keep this weak reference anymore
                Reset();
            }
        }
        return spObj;
    }

    bool operator==(const WeakPtr& Ptr) const noexcept { return m_pWeakRef == Ptr.m_pWeakRef; }
    bool operator!=(const WeakPtr& Ptr) const noexcept { return m_pWeakRef != Ptr.m_pWeakRef; }

protected:
    static WeakRefType* WeakRefTypeCast(IWeakReference* p) { return static_cast<WeakRefType*>(p); }
    WeakRefType* m_pWeakRef = nullptr;
    // We need to store raw pointer to object itself,
    // because if the object is owned by another object,
    // m_pRefCounters->QueryObject(&pObj) will return
    // a pointer to the owner, which is not what we need.
    T* m_pObject = nullptr;
};

template <typename T>
AutoPtr<T> TakeOver(T* p) noexcept {
    AutoPtr<T> ret;
    *ret.GetAddressOf() = p;
    return ret;
}

template <typename T>
WeakPtr<T> TakeOverRef(T* p) noexcept {
    WeakPtr<T> ret;
    ret.Attach(p);
    return ret;
}

template <typename T>
FRESULT AsWeak(T* p, WeakPtr<T>* pWeak) throw() {
    static_assert(!details::IsSame<IWeakReference, T>::value, "Cannot get IWeakReference object to IWeakReference.");
    AutoPtr<IWeakReferenceSource> refSource;

    FRESULT hr = p->QueryInterface(FIID_PPV_ARGS(refSource.GetAddressOf()));
    if (FFAILED(hr)) {
        return hr;
    }

    auto weakref = refSource->GetWeakReference();
    if (!weakref) {
        return FE_NOT_IMPLEMENT;
    }

    *pWeak = WeakPtr<T>(p);
    return FS_OK;
}

// Comparison operators - don't compare COM object identity
template <class T, class U>
bool operator==(const AutoPtr<T>& a, const AutoPtr<U>& b) throw() {
    static_assert(
        details::IsBaseOf<T, U>::value || details::IsBaseOf<U, T>::value, "'T' and 'U' pointers must be comparable");
    return a.Get() == b.Get();
}

template <class T>
bool operator==(const AutoPtr<T>& a, details::nullptr_t) throw() {
    return a.Get() == nullptr;
}

template <class T>
bool operator==(details::nullptr_t, const AutoPtr<T>& a) throw() {
    return a.Get() == nullptr;
}

template <class T, class U>
bool operator!=(const AutoPtr<T>& a, const AutoPtr<U>& b) throw() {
    static_assert(
        details::IsBaseOf<T, U>::value || details::IsBaseOf<U, T>::value, "'T' and 'U' pointers must be comparable");
    return a.Get() != b.Get();
}

template <class T>
bool operator!=(const AutoPtr<T>& a, details::nullptr_t) throw() {
    return a.Get() != nullptr;
}

template <class T>
bool operator!=(details::nullptr_t, const AutoPtr<T>& a) throw() {
    return a.Get() != nullptr;
}

template <class T, class U>
bool operator<(const AutoPtr<T>& a, const AutoPtr<U>& b) throw() {
    static_assert(
        details::IsBaseOf<T, U>::value || details::IsBaseOf<U, T>::value, "'T' and 'U' pointers must be comparable");
    return a.Get() < b.Get();
}

//// details::AutoPtrRef comparisons
template <class T, class U>
bool operator==(const details::AutoPtrRef<AutoPtr<T>>& a, const details::AutoPtrRef<AutoPtr<U>>& b) throw() {
    static_assert(
        details::IsBaseOf<T, U>::value || details::IsBaseOf<U, T>::value, "'T' and 'U' pointers must be comparable");
    return a.GetAddressOf() == b.GetAddressOf();
}

template <class T>
bool operator==(const details::AutoPtrRef<AutoPtr<T>>& a, details::nullptr_t) throw() {
    return a.GetAddressOf() == nullptr;
}

template <class T>
bool operator==(details::nullptr_t, const details::AutoPtrRef<AutoPtr<T>>& a) throw() {
    return a.GetAddressOf() == nullptr;
}

template <class T>
bool operator==(const details::AutoPtrRef<AutoPtr<T>>& a, void* b) throw() {
    return a.GetAddressOf() == b;
}

template <class T>
bool operator==(void* b, const details::AutoPtrRef<AutoPtr<T>>& a) throw() {
    return a.GetAddressOf() == b;
}

template <class T, class U>
bool operator!=(const details::AutoPtrRef<AutoPtr<T>>& a, const details::AutoPtrRef<AutoPtr<U>>& b) throw() {
    static_assert(
        details::IsBaseOf<T, U>::value || details::IsBaseOf<U, T>::value, "'T' and 'U' pointers must be comparable");
    return a.GetAddressOf() != b.GetAddressOf();
}

template <class T>
bool operator!=(const details::AutoPtrRef<AutoPtr<T>>& a, details::nullptr_t) throw() {
    return a.GetAddressOf() != nullptr;
}

template <class T>
bool operator!=(details::nullptr_t, const details::AutoPtrRef<AutoPtr<T>>& a) throw() {
    return a.GetAddressOf() != nullptr;
}

template <class T>
bool operator!=(const details::AutoPtrRef<AutoPtr<T>>& a, void* b) throw() {
    return a.GetAddressOf() != b;
}

template <class T>
bool operator!=(void* b, const details::AutoPtrRef<AutoPtr<T>>& a) throw() {
    return a.GetAddressOf() != b;
}

template <class T, class U>
bool operator<(const details::AutoPtrRef<AutoPtr<T>>& a, const details::AutoPtrRef<AutoPtr<U>>& b) throw() {
    static_assert(
        details::IsBaseOf<T, U>::value || details::IsBaseOf<U, T>::value, "'T' and 'U' pointers must be comparable");
    return a.GetAddressOf() < b.GetAddressOf();
}


}  // namespace donut

// Overloaded global function to provide to IID_PPV_ARGS that support details::AutoPtrRef
template <typename T>
void** IID_PPV_ARGS_Helper(donut::details::AutoPtrRef<T> pp) throw() {
    return pp;
}
