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

} // namespace donut
