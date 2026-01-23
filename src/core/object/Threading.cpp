#include <donut/core/object/Threading.h>

#include <thread>

#if defined(_MSC_VER) && ((_M_IX86_FP >= 2) || defined(_M_X64))
#include <emmintrin.h>
#define PAUSE() _mm_pause()
#elif (defined(__clang__) || defined(__GNUC__)) && (defined(__i386__) || defined(__x86_64__))
#define PAUSE() __builtin_ia32_pause()
#elif (defined(__clang__) || defined(__GNUC__)) && (defined(__arm__) || defined(__aarch64__))
#define PAUSE() asm volatile("yield")
#else
#define PAUSE()
#endif

namespace donut {

void SpinLock::Wait() noexcept {
    // Wait for the lock to be released without generating cache misses.
    constexpr size_t NumAttemptsToYield = 64;
    for (size_t Attempt = 0; Attempt < NumAttemptsToYield; ++Attempt) {
        if (!is_locked())
            return;

        // Issue X86 PAUSE or ARM YIELD instruction to reduce contention
        // between hyper-threads.
        PAUSE();
    }

    std::this_thread::yield();
}

namespace {
#if defined(_WIN64) || (defined(__linux__) && defined(__x86_64__))
static constexpr uint64_t kSListHeaderCounterBits = 17;
static constexpr uint64_t kSListHeaderPtrMask = (~0ull) >> kSListHeaderCounterBits;
static constexpr uint64_t kSListHeaderCounterMask = ~kSListHeaderPtrMask;
static constexpr uint64_t kSListHeaderCounterInc = kSListHeaderPtrMask + 1;
#else
static constexpr uint64_t kSListHeaderCounterBits = 32;
static constexpr uint64_t kSListHeaderPtrMask = (-1ULL) >> kSListHeaderCounterBits;
static constexpr uint64_t kSListHeaderCounterMask = ~kSListHeaderPtrMask;
static constexpr uint64_t kSListHeaderCounterInc = kSListHeaderPtrMask + 1;
#endif

static constexpr uint64_t kSharedSlimLockExclusiveMask = 1ULL << 63;
static constexpr uint64_t kSharedSlimLockSharedMask = ~kSharedSlimLockExclusiveMask;
}  // namespace

LFStack::LFStack() : head_{0} {}

LFStack::~LFStack() {}

bool LFStack::Empty() const {
    uint64_t cmp = head_.load(std::memory_order_relaxed);
    return (cmp & kSListHeaderPtrMask) == 0;
}

LFStackEntry *LFStack::Top() const {
    uint64_t cmp = head_.load(std::memory_order_relaxed);
    return (LFStackEntry *)(uintptr_t)(cmp & kSListHeaderPtrMask);
}

void LFStack::Push(LFStackEntry *p) {
    uint64_t cmp = head_.load(std::memory_order_relaxed);
    for (;;) {
        uint64_t cnt = (cmp & kSListHeaderCounterMask) + kSListHeaderCounterInc;
        uint64_t xch = (uint64_t)(uintptr_t)p | cnt;
        p->next = (LFStackEntry *)(uintptr_t)(cmp & kSListHeaderPtrMask);
        if (head_.compare_exchange_weak(cmp, xch, std::memory_order_release)) break;
    }
}

void LFStack::Push(LFStackEntry *slice, LFStackEntry *slice_end) {
    uint64_t cmp = head_.load(std::memory_order_relaxed);
    for (;;) {
        uint64_t cnt = (cmp & kSListHeaderCounterMask) + kSListHeaderCounterInc;
        uint64_t xch = (uint64_t)(uintptr_t)slice | cnt;
        slice_end->next = (LFStackEntry *)(uintptr_t)(cmp & kSListHeaderPtrMask);
        if (head_.compare_exchange_weak(cmp, xch, std::memory_order_release)) break;
    }
}

LFStackEntry *LFStack::Pop() {
    uint64_t cmp = head_.load(std::memory_order_acquire);
    for (;;) {
        LFStackEntry *cur = (LFStackEntry *)(uintptr_t)(cmp & kSListHeaderPtrMask);
        if (cur == nullptr) return nullptr;

        LFStackEntry *nxt = cur->next;
        uint64_t cnt = (cmp & kSListHeaderCounterMask);
        uint64_t xch = (uint64_t)(uintptr_t)nxt | cnt;
        if (head_.compare_exchange_weak(cmp, xch, std::memory_order_acquire)) return cur;
    }
}

LFStackEntry *LFStack::Flush() {
    uint64_t cmp = head_.load(std::memory_order_acquire);
    for (;;) {
        LFStackEntry *cur = (LFStackEntry *)(uintptr_t)(cmp & kSListHeaderPtrMask);
        if (cur == nullptr) return nullptr;

        uint64_t cnt = (cmp & kSListHeaderCounterMask);
        uint64_t xch = cnt;
        if (head_.compare_exchange_weak(cmp, xch, std::memory_order_acquire)) return cur;
    }
}

bool SharedSpinLock::is_locked() noexcept {
    uint64_t cnt = shared_cnt_.load(std::memory_order_relaxed);
    return cnt == kSharedSlimLockExclusiveMask;
}

bool SharedSpinLock::is_locked_shared() noexcept {
    uint64_t cnt = shared_cnt_.load(std::memory_order_relaxed);
    return (kSharedSlimLockSharedMask & cnt) == cnt;
}

void SharedSpinLock::Wait() {
    // Wait for the lock to be released without generating cache misses.
    constexpr size_t NumAttemptsToYield = 64;
    for (size_t Attempt = 0; Attempt < NumAttemptsToYield; ++Attempt) {
        if (!is_locked()) return;

        // Issue X86 PAUSE or ARM YIELD instruction to reduce contention
        // between hyper-threads.
        PAUSE();
    }

    std::this_thread::yield();
}

void SharedSpinLock::WaitShared() {
    // Wait for the lock to be released without generating cache misses.
    constexpr size_t NumAttemptsToYield = 64;
    for (size_t Attempt = 0; Attempt < NumAttemptsToYield; ++Attempt) {
        if (!is_locked_shared()) return;

        // Issue X86 PAUSE or ARM YIELD instruction to reduce contention
        // between hyper-threads.
        PAUSE();
    }

    std::this_thread::yield();
}

SharedSpinLock::SharedSpinLock() : shared_cnt_{} {}

SharedSpinLock::~SharedSpinLock() {}

void SharedSpinLock::lock() noexcept {
    while (true) {
        uint64_t cmp = 0;
        uint64_t xch = kSharedSlimLockExclusiveMask;
        if (shared_cnt_.compare_exchange_weak(cmp, xch, std::memory_order_acquire)) break;

        Wait();
    }
}

bool SharedSpinLock::try_lock() noexcept {
    if (is_locked()) return false;

    uint64_t cmp = 0;
    uint64_t xch = kSharedSlimLockExclusiveMask;
    return shared_cnt_.compare_exchange_weak(cmp, xch, std::memory_order_acquire);
}

void SharedSpinLock::unlock() noexcept { shared_cnt_.store(0, std::memory_order_relaxed); }

void SharedSpinLock::lock_shared() noexcept {
    uint64_t cmp = shared_cnt_.load(std::memory_order_release);
    while (true) {
        cmp &= kSharedSlimLockSharedMask;
        uint64_t xch = cmp + 1;
        if (shared_cnt_.compare_exchange_weak(cmp, xch)) break;

        WaitShared();
    }
}

bool SharedSpinLock::try_lock_shared() noexcept {
    if (is_locked()) return false;
    uint64_t cmp = shared_cnt_.load(std::memory_order_release);
    cmp &= kSharedSlimLockSharedMask;
    uint64_t xch = cmp + 1;
    return shared_cnt_.compare_exchange_weak(cmp, xch);
}

void SharedSpinLock::unlock_shared() noexcept {
    shared_cnt_.fetch_add(-1, std::memory_order_release);
}

} // namespace donut
