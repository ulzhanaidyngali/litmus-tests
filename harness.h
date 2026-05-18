#pragma once
#include <atomic>
#include <thread>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <chrono>

#if defined(__linux__)
#  include <pthread.h>
#  include <sched.h>
   static void pin_thread(int cpu_id) {
       cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(cpu_id, &cpuset);
       pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
   }
#else
   static void pin_thread(int) {}
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#  define MFENCE() _mm_mfence()
#  define SFENCE() _mm_sfence()
#  define LFENCE() _mm_lfence()
#else
#  define MFENCE() std::atomic_thread_fence(std::memory_order_seq_cst)
#  define SFENCE() std::atomic_thread_fence(std::memory_order_release)
#  define LFENCE() std::atomic_thread_fence(std::memory_order_acquire)
#endif

using OutcomeKey = std::string;
using ResultMap  = std::unordered_map<OutcomeKey, uint64_t>;

static OutcomeKey make_key(std::initializer_list<std::pair<const char*, int>> regs) {
    std::ostringstream oss; bool first = true;
    for (auto& [n, v] : regs) { if (!first) oss << ' '; oss << n << '=' << v; first = false; }
    return oss.str();
}
#define OUTCOME(...) make_key({ __VA_ARGS__ })
#define REG(name, val) { #name, (val) }

struct TestConfig {
    std::string name, description;
    int  iterations  = 10000;
    int  cpu0 = 0, cpu1 = 1;
    bool use_barriers = false;
};

template<typename State>
ResultMap run_test(
    const TestConfig& cfg,
    std::function<void(State&, bool)> t1_body,
    std::function<void(State&, bool)> t2_body,
    std::function<OutcomeKey(const State&)> observe)
{
    ResultMap results;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < cfg.iterations; ++i) {
        State s;
        s.reset();

        // Run both threads truly in parallel
        std::thread ta([&]{ pin_thread(cfg.cpu0); t1_body(s, cfg.use_barriers); });
        std::thread tb([&]{ pin_thread(cfg.cpu1); t2_body(s, cfg.use_barriers); });
        ta.join(); tb.join();

        results[observe(s)]++;
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Test: " << std::left << std::setw(42) << cfg.name << "║\n";
    std::cout << "║  " << std::left << std::setw(48) << cfg.description << "║\n";
    std::cout << "╠══════════════════════════════════════════════════╣\n";
    std::cout << "║  Iterations: " << std::left << std::setw(36) << cfg.iterations << "║\n";
    std::cout << "║  Time: " << std::left << std::setw(42) << (std::to_string(ms)+" ms") << "║\n";
    std::cout << "║  Barriers: " << std::left << std::setw(38) << (cfg.use_barriers?"YES (mfence)":"NO") << "║\n";
    std::cout << "╠══════════════════════════════════════════════════╣\n";
    std::cout << "║  OUTCOMES:                                       ║\n";

    uint64_t total = 0;
    for (auto& [k,v]: results) total += v;
    for (auto& [key,count]: results) {
        double pct = 100.0*count/total;
        std::ostringstream line;
        line << "  " << std::left << std::setw(20) << key
             << std::right << std::setw(10) << count
             << "  (" << std::fixed << std::setprecision(3) << pct << "%)";
        std::cout << "║" << std::left << std::setw(50) << line.str() << "║\n";
    }
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    return results;
}

static void explain_tso(const std::string& name, const std::string& forbidden, const std::string& expl) {
    std::cout << "\n[x86-TSO: " << name << "]\n  Forbidden: " << forbidden << "\n  " << expl << "\n\n";
}
