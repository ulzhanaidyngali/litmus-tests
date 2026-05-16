#pragma once
// =============================================================================
// harness.h  —  Litmus Test Harness for Memory Consistency
// Platform:  x86-64, Linux (pthreads) or Windows
// Compiler:  g++ -O2 -std=c++17  /  MSVC /O2 /std:c++17
// =============================================================================
//
// HOW TO USE:
//   1. Include this header
//   2. Define a struct with your shared variables (LitmusState)
//   3. Write thread1_body() and thread2_body() functions
//   4. Call run_test(config) — it returns a ResultMap with outcome counts
//
// =============================================================================

#include <atomic>
#include <thread>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <cstring>
#include <cassert>
#include <chrono>

// ── Platform-specific CPU affinity ──────────────────────────────────────────
#if defined(__linux__)
#  include <pthread.h>
#  include <sched.h>
   static void pin_thread(int cpu_id) {
       cpu_set_t cpuset;
       CPU_ZERO(&cpuset);
       CPU_SET(cpu_id, &cpuset);
       pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
   }
#elif defined(_WIN32)
#  include <windows.h>
   static void pin_thread(int cpu_id) {
       SetThreadAffinityMask(GetCurrentThread(), 1ULL << cpu_id);
   }
#else
   static void pin_thread(int) {} // no-op on unsupported platforms
#endif

// ── Memory barriers ─────────────────────────────────────────────────────────
#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#  define MFENCE()   _mm_mfence()
#  define SFENCE()   _mm_sfence()
#  define LFENCE()   _mm_lfence()
#else
#  define MFENCE()   std::atomic_thread_fence(std::memory_order_seq_cst)
#  define SFENCE()   std::atomic_thread_fence(std::memory_order_release)
#  define LFENCE()   std::atomic_thread_fence(std::memory_order_acquire)
#endif

// ── Outcome key ─────────────────────────────────────────────────────────────
// A result is identified by the values of all observed registers.
// We encode them as "r1=V1,r2=V2,..."
using OutcomeKey    = std::string;
using ResultMap     = std::unordered_map<OutcomeKey, uint64_t>;

static OutcomeKey make_key(std::initializer_list<std::pair<const char*, int>> regs) {
    std::ostringstream oss;
    bool first = true;
    for (auto& [name, val] : regs) {
        if (!first) oss << ' ';
        oss << name << '=' << val;
        first = false;
    }
    return oss.str();
}
// Convenience macro:  OUTCOME(r1, r2)  →  "r1=0 r2=1"
#define OUTCOME(...)  make_key({ __VA_ARGS__ })
#define REG(name, val) { #name, (val) }

// ── Test configuration ───────────────────────────────────────────────────────
struct TestConfig {
    std::string name;           // Human-readable test name, e.g. "SB"
    std::string description;    // What it tests
    int         iterations  = 100'000; // How many times to run
    int         cpu0        = 0;         // Core for thread 1
    int         cpu1        = 1;         // Core for thread 2
    bool        use_barriers = false;    // Insert mfence barriers (demo)
};

// ── Generic 2-thread test runner ────────────────────────────────────────────
//
// State:   a plain struct you define — must have a reset() method
//          that sets all shared vars to initial values.
//
// Thread bodies:  void(State&, bool use_barriers)
//                 Write r1, r2, etc. into the State struct.
//
template<typename State>
ResultMap run_test(
    const TestConfig& cfg,
    std::function<void(State&, bool)> thread1_body,
    std::function<void(State&, bool)> thread2_body,
    std::function<OutcomeKey(const State&)> observe   // read result registers
) {
    ResultMap results;
    State state;

    // Barrier to start both threads simultaneously
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    auto worker = [&](int cpu_id, std::function<void(State&, bool)> body) {
        pin_thread(cpu_id);
        for (int i = 0; i < cfg.iterations; ++i) {
            // Wait for main thread to reset state
            while (ready.load(std::memory_order_acquire) != 1) { /* spin */ }
            // Signal ready
            ready.fetch_add(1, std::memory_order_release);
            // Wait for go
            while (!go.load(std::memory_order_acquire)) { /* spin */ }
            // Run the litmus body
            body(state, cfg.use_barriers);
        }
    };

    auto start_time = std::chrono::steady_clock::now();

    std::thread t2(worker, cfg.cpu1, thread2_body);
    std::thread t1(worker, cfg.cpu0, thread1_body);

    for (int i = 0; i < cfg.iterations; ++i) {
        // Reset shared state
        state.reset();
        ready.store(0, std::memory_order_release);
        go.store(false, std::memory_order_release);

        // Signal threads to get ready
        ready.store(1, std::memory_order_release);

        // Wait until both threads are ready
        while (ready.load(std::memory_order_acquire) < 3) { /* spin */ }

        // Release both threads at once
        go.store(true, std::memory_order_release);

        // Wait for them to finish this iteration
        // (threads spin on ready==1 at start of next iter, so after go=true
        //  they run and then wait for the next ready==1 reset)
        // Give threads time to finish
        while (ready.load(std::memory_order_acquire) > 1) { /* spin */ }

        // Wait for iteration to actually complete
        // A small pause to let threads write results
        std::atomic_thread_fence(std::memory_order_seq_cst);

        OutcomeKey key = observe(state);
        results[key]++;
    }

    t1.join();
    t2.join();

    auto end_time = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Print results
    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Test: " << std::left << std::setw(42) << cfg.name << "║\n";
    std::cout << "║  " << std::left << std::setw(48) << cfg.description << "║\n";
    std::cout << "╠══════════════════════════════════════════════════╣\n";
    std::cout << "║  Iterations: " << std::left << std::setw(36) << cfg.iterations << "║\n";
    std::cout << "║  Time:       " << std::left << std::setw(33) << (std::to_string(ms) + " ms") << "║\n";
    std::cout << "║  Barriers:   " << std::left << std::setw(36) << (cfg.use_barriers ? "YES (mfence)" : "NO") << "║\n";
    std::cout << "╠══════════════════════════════════════════════════╣\n";
    std::cout << "║  OUTCOMES:                                       ║\n";

    uint64_t total = 0;
    for (auto& [k, v] : results) total += v;

    for (auto& [key, count] : results) {
        double pct = 100.0 * count / total;
        std::ostringstream line;
        line << "  " << std::left << std::setw(20) << key
             << std::right << std::setw(10) << count
             << "  (" << std::fixed << std::setprecision(3) << pct << "%)";
        std::cout << "║" << std::left << std::setw(50) << line.str() << "║\n";
    }
    std::cout << "╚══════════════════════════════════════════════════╝\n";

    return results;
}

// ── Helper: print TSO explanation ───────────────────────────────────────────
static void explain_tso(const std::string& test_name,
                         const std::string& forbidden_outcome,
                         const std::string& explanation) {
    std::cout << "\n[x86-TSO Analysis for " << test_name << "]\n";
    std::cout << "  Forbidden outcome: " << forbidden_outcome << "\n";
    std::cout << "  Explanation: " << explanation << "\n\n";
}
