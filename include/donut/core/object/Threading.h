#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <donut/core/object/DebugUtilities.h>

namespace donut {

/// Spin lock implementation
class SpinLock {
public:
    // See https://rigtorp.se/spinlock/
    SpinLock() noexcept {}

    // clang-format off
    SpinLock             (const SpinLock&)  = delete;
    SpinLock& operator = (const SpinLock&)  = delete;
    SpinLock             (      SpinLock&&) = delete;
    SpinLock& operator = (      SpinLock&&) = delete;
    // clang-format on

    void lock() noexcept {
        while (true) {
            // Assume that lock is free on the first try.
            const auto WasLocked = m_IsLocked.exchange(true, std::memory_order_acquire);
            if (!WasLocked)
                return;  // The lock was not acquired when this thread performed the exchange

            Wait();
        }
    }

    bool try_lock() noexcept {
        // First do a relaxed load to check if lock is free in order to prevent
        // unnecessary cache misses if someone does while (!try_lock()).
        if (is_locked())
            return false;

        const auto WasLocked = m_IsLocked.exchange(true, std::memory_order_acquire);
        return !WasLocked;
    }

    void unlock() noexcept {
        DONUT_VERIFY(
            is_locked(),
            "Attempting to unlock a spin lock that is not locked. This is a strong indication of a flawed logic.");
        m_IsLocked.store(false, std::memory_order_release);
    }

    bool is_locked() const noexcept {
        // Use relaxed load as we only want to check the value.
        // To impose ordering, lock()/try_lock() must be used.
        return m_IsLocked.load(std::memory_order_relaxed);
    }

private:
    void Wait() noexcept;

private:
    std::atomic<bool> m_IsLocked{ false };
};

using SpinLockGuard = std::lock_guard<SpinLock>;

class Signal {
public:
    Signal() {
        m_SignaledValue.store(0);
        m_NumThreadsAwaken.store(0);
    }

    // clang-format off
    Signal           (const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal           (Signal&&)      = delete;
    Signal& operator=(Signal&&)      = delete;
    // clang-format on

    // http://en.cppreference.com/w/cpp/thread/condition_variable
    void Trigger(bool NotifyAll = false, int SignalValue = 1) {
        DONUT_VERIFY(SignalValue != 0, "Signal value must not be zero");

        //  The thread that intends to modify the variable has to
        //  * acquire a std::mutex (typically via std::lock_guard)
        //  * perform the modification while the lock is held
        //  * execute notify_one or notify_all on the std::condition_variable (the lock does not need to be held for
        //  notification)
        {
            // std::condition_variable works only with std::unique_lock<std::mutex>
            std::lock_guard<std::mutex> Lock{ m_Mutex };
            DONUT_VERIFY(SignalValue != 0, "Signal value must not be 0");
            DONUT_VERIFY(
                m_SignaledValue.load() == 0 && m_NumThreadsAwaken.load() == 0,
                "Not all threads have been awaken since the signal was triggered last time, or the signal has not been "
                "reset");
            m_SignaledValue.store(SignalValue);
        }
        // Unlocking is done before notifying, to avoid waking up the waiting
        // thread only to block again (see notify_one for details)
        if (NotifyAll)
            m_CondVar.notify_all();
        else
            m_CondVar.notify_one();
    }

    // WARNING!
    // If multiple threads are waiting for a signal in an infinite loop,
    // autoresetting the signal does not guarantee that one thread cannot
    // go through the loop twice. In this case, every thread must wait for its
    // own auto-reset signal or the threads must be blocked by another signal

    int Wait(bool AutoReset = false, int NumThreadsWaiting = 0) {
        //  Any thread that intends to wait on std::condition_variable has to
        //  * acquire a std::unique_lock<std::mutex>, on the SAME MUTEX as used to protect the shared variable
        //  * execute wait, wait_for, or wait_until. The wait operations atomically release the mutex
        //    and suspend the execution of the thread.
        //  * When the condition variable is notified, a timeout expires, or a spurious wakeup occurs,
        //    the thread is awakened, and the mutex is atomically reacquired:
        //    - The thread should then check the condition and resume waiting if the wake-up was spurious.
        std::unique_lock<std::mutex> Lock(m_Mutex);
        // It is safe to check m_SignaledValue since we are holding
        // the mutex
        if (m_SignaledValue.load() == 0) {
            m_CondVar.wait(Lock, [&] { return m_SignaledValue.load() != 0; });
        }
        auto SignaledValue = m_SignaledValue.load();
        // Update the number of threads awaken while holding the mutex
        const auto NumThreadsAwaken = m_NumThreadsAwaken.fetch_add(1) + 1;
        // fetch_add returns the original value immediately preceding the addition.
        if (AutoReset) {
            DONUT_VERIFY(
                NumThreadsWaiting > 0, "Number of waiting threads must not be 0 when auto resetting the signal");
            // Reset the signal while holding the mutex. If Trigger() is executed by another
            // thread, it will wait until we release the mutex
            if (NumThreadsAwaken == NumThreadsWaiting) {
                m_SignaledValue.store(0);
                m_NumThreadsAwaken.store(0);
            }
        }
        return SignaledValue;
    }

    void Reset() {
        std::lock_guard<std::mutex> Lock{ m_Mutex };
        m_SignaledValue.store(0);
        m_NumThreadsAwaken.store(0);
    }

    bool IsTriggered() const { return m_SignaledValue.load() != 0; }

private:
    std::mutex m_Mutex;
    std::condition_variable m_CondVar;
    std::atomic_int m_SignaledValue{ 0 };
    std::atomic_int m_NumThreadsAwaken{ 0 };
};

} // namespace donut
