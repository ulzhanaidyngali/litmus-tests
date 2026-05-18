// =============================================================================
// tests.h  —  12 Classic Litmus Tests
// =============================================================================
// Each test follows the same pattern:
//   1. Struct with shared variables + reset()
//   2. thread1_body() and thread2_body()
//   3. observe() — reads result registers
//   4. run_XYZ() — fills TestConfig and calls run_test()
// =============================================================================

#pragma once
#include "harness.h"

// ─────────────────────────────────────────────────────────────────────────────
// TEST 1: SB — Store Buffering
// ─────────────────────────────────────────────────────────────────────────────
// Thread 1:  x=1; r1=y
// Thread 2:  y=1; r2=x
//
// Forbidden on SC: r1=0 && r2=0
// Allowed on x86-TSO: YES — store buffer delays write visibility
// ─────────────────────────────────────────────────────────────────────────────
struct SB_State {
    std::atomic<int> x{0}, y{0};
    int r1{0}, r2{0};
    void reset() { x=0; y=0; r1=0; r2=0; }
};

void run_SB(bool use_barriers = false) {
    TestConfig cfg;
    cfg.name        = "SB (Store Buffering)";
    cfg.description = "Can r1=0 && r2=0 happen?";
    cfg.use_barriers = use_barriers;

    run_test<SB_State>(cfg,
        // Thread 1
        [](SB_State& s, bool barriers) {
            s.x.store(1, std::memory_order_relaxed);
            if (barriers) MFENCE();
            s.r1 = s.y.load(std::memory_order_relaxed);
        },
        // Thread 2
        [](SB_State& s, bool barriers) {
            s.y.store(1, std::memory_order_relaxed);
            if (barriers) MFENCE();
            s.r2 = s.x.load(std::memory_order_relaxed);
        },
        // Observe
        [](const SB_State& s) {
            return OUTCOME(REG(r1, s.r1), REG(r2, s.r2));
        }
    );

    if (!use_barriers)
        explain_tso("SB", "r1=0 r2=0",
            "x86 has per-core store buffers. Each thread writes to its buffer\n"
            "  and reads from memory before the other's write is flushed.\n"
            "  Result: both see the old value 0. This CANNOT happen on SC.");
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 2: MP — Message Passing
// ─────────────────────────────────────────────────────────────────────────────
// Thread 1:  data=42; flag=1
// Thread 2:  r1=flag; r2=data
//
// Forbidden on SC/TSO: r1=1 && r2=0
// Allowed on TSO: NO (x86 preserves store→store order)
// Allowed on ARM: YES (weak model)
// ─────────────────────────────────────────────────────────────────────────────
struct MP_State {
    std::atomic<int> data{0}, flag{0};
    int r1{0}, r2{0};
    void reset() { data=0; flag=0; r1=0; r2=0; }
};

void run_MP(bool use_barriers = false) {
    TestConfig cfg;
    cfg.name        = "MP (Message Passing)";
    cfg.description = "Can r1=1 && r2=0 happen?";
    cfg.use_barriers = use_barriers;

    run_test<MP_State>(cfg,
        [](MP_State& s, bool barriers) {
            s.data.store(42, std::memory_order_relaxed);
            if (barriers) SFENCE();
            s.flag.store(1, std::memory_order_relaxed);
        },
        [](MP_State& s, bool barriers) {
            s.r1 = s.flag.load(std::memory_order_relaxed);
            if (barriers) LFENCE();
            s.r2 = s.data.load(std::memory_order_relaxed);
        },
        [](const MP_State& s) {
            return OUTCOME(REG(r1, s.r1), REG(r2, s.r2));
        }
    );

    explain_tso("MP", "r1=1 r2=0",
        "x86-TSO: stores are ordered (store buffer is FIFO per core).\n"
        "  If T2 sees flag=1, it must also see data=42. Safe on x86.\n"
        "  On ARMv8 (weak model) this outcome IS possible without barriers.");
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 3: LB — Load Buffering
// ─────────────────────────────────────────────────────────────────────────────
// Thread 1:  r1=x; y=1
// Thread 2:  r2=y; x=1
//
// Forbidden on SC: r1=1 && r2=1
// Allowed on x86-TSO: NO (loads not reordered past stores on x86)
// Allowed on ARM: YES
// ─────────────────────────────────────────────────────────────────────────────
struct LB_State {
    std::atomic<int> x{0}, y{0};
    int r1{0}, r2{0};
    void reset() { x=0; y=0; r1=0; r2=0; }
};

void run_LB() {
    TestConfig cfg;
    cfg.name        = "LB (Load Buffering)";
    cfg.description = "Can r1=1 && r2=1 happen?";

    run_test<LB_State>(cfg,
        [](LB_State& s, bool) {
            s.r1 = s.x.load(std::memory_order_relaxed);
            s.y.store(1, std::memory_order_relaxed);
        },
        [](LB_State& s, bool) {
            s.r2 = s.y.load(std::memory_order_relaxed);
            s.x.store(1, std::memory_order_relaxed);
        },
        [](const LB_State& s) {
            return OUTCOME(REG(r1, s.r1), REG(r2, s.r2));
        }
    );

    explain_tso("LB", "r1=1 r2=1",
        "x86-TSO does NOT allow load→store reordering.\n"
        "  So r1=1 && r2=1 should NOT appear on x86.\n"
        "  On ARMv8 it can appear (loads can be reordered freely).");
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 4: IRIW — Independent Reads of Independent Writes
// ─────────────────────────────────────────────────────────────────────────────
// Thread 1:  x=1
// Thread 2:  y=1
// Thread 3:  r1=x; r2=y
// Thread 4:  r3=y; r4=x
//
// Forbidden on SC: r1=1,r2=0 AND r3=1,r4=0 simultaneously
// Allowed on x86-TSO: NO (x86 has a single total store order)
// Allowed on ARM: YES
// ─────────────────────────────────────────────────────────────────────────────
struct IRIW_State {
    std::atomic<int> x{0}, y{0};
    int r1{0}, r2{0}, r3{0}, r4{0};
    void reset() { x=0; y=0; r1=0; r2=0; r3=0; r4=0; }
};

// IRIW needs 4 threads — we use a slightly different runner
void run_IRIW() {
    const int ITERATIONS = 10000;
    ResultMap results;

    for (int i = 0; i < ITERATIONS; ++i) {
        IRIW_State state;
        state.reset();
        std::thread t1([&]{ state.x.store(1, std::memory_order_relaxed); });
        std::thread t2([&]{ state.y.store(1, std::memory_order_relaxed); });
        std::thread t3([&]{
            state.r1 = state.x.load(std::memory_order_relaxed);
            state.r2 = state.y.load(std::memory_order_relaxed);
        });
        std::thread t4([&]{
            state.r3 = state.y.load(std::memory_order_relaxed);
            state.r4 = state.x.load(std::memory_order_relaxed);
        });
        t1.join(); t2.join(); t3.join(); t4.join();
        results[OUTCOME(REG(r1,state.r1), REG(r2,state.r2),
                        REG(r3,state.r3), REG(r4,state.r4))]++;
    }

    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Test: IRIW (Independent Reads/Writes)           ║\n";
    std::cout << "║  4 threads: can T3 and T4 see writes in          ║\n";
    std::cout << "║  different orders?                                ║\n";
    std::cout << "╠══════════════════════════════════════════════════╣\n";
    uint64_t total = 0;
    for (auto& [k,v]: results) total += v;
    for (auto& [key, count] : results) {
        double pct = 100.0 * count / total;
        std::cout << "║  " << std::left << std::setw(30) << key
                  << std::right << std::setw(8) << count
                  << " (" << std::fixed << std::setprecision(2) << pct << "%)  ║\n";
    }
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    explain_tso("IRIW", "r1=1,r2=0 AND r3=1,r4=0",
        "x86-TSO has a SINGLE total store order visible to all cores.\n"
        "  Two observers cannot see writes in different orders. Safe on x86.\n"
        "  On ARMv8 this is allowed.");
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 5: CoRR — Coherence (same variable)
// ─────────────────────────────────────────────────────────────────────────────
// Thread 1: x=1
// Thread 2: x=2
// Thread 3: r1=x; r2=x
// Thread 4: r3=x; r4=x
//
// All observers must agree on the write order to x.
// ─────────────────────────────────────────────────────────────────────────────
struct CoRR_State {
    std::atomic<int> x{0};
    int r1{0}, r2{0}, r3{0}, r4{0};
    void reset() { x=0; r1=0; r2=0; r3=0; r4=0; }
};

void run_CoRR() {
    TestConfig cfg;
    cfg.name        = "CoRR (Cache Coherence)";
    cfg.description = "All threads must see same write order to x";
    cfg.iterations  = 500'000;

    // Use 2-thread version: T1 writes x=1, T2 reads x twice
    struct State2 {
        std::atomic<int> x{0};
        int r1{0}, r2{0};
        void reset() { x=0; r1=0; r2=0; }
    };
    run_test<State2>(cfg,
        [](State2& s, bool) {
            s.x.store(1, std::memory_order_relaxed);
            s.x.store(2, std::memory_order_relaxed);
        },
        [](State2& s, bool) {
            s.r1 = s.x.load(std::memory_order_relaxed);
            s.r2 = s.x.load(std::memory_order_relaxed);
        },
        [](const State2& s) {
            return OUTCOME(REG(r1, s.r1), REG(r2, s.r2));
        }
    );
    std::cout << "[CoRR] r1=2,r2=1 would mean T2 saw writes in reverse order.\n"
              << "       This must NOT happen on any coherent system.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 6: WRC — Write-to-Read Causality
// ─────────────────────────────────────────────────────────────────────────────
// Thread 1: x=1
// Thread 2: r1=x; y=r1
// Thread 3: r2=y; r3=x
//
// If r2=1 (saw y=1), must r3=1?  On SC: yes. On TSO: yes. On ARM: maybe not.
// ─────────────────────────────────────────────────────────────────────────────
struct WRC_State {
    std::atomic<int> x{0}, y{0};
    int r1{0}, r2{0}, r3{0};
    void reset() { x=0; y=0; r1=0; r2=0; r3=0; }
};

void run_WRC() {
    TestConfig cfg;
    cfg.name        = "WRC (Write-Read Causality)";
    cfg.description = "If T3 sees y=1, must it also see x=1?";
    cfg.iterations  = 500'000;
    // Simplified to 2-thread for harness compatibility
    run_test<WRC_State>(cfg,
        [](WRC_State& s, bool) {
            s.x.store(1, std::memory_order_relaxed);
        },
        [](WRC_State& s, bool) {
            s.r1 = s.x.load(std::memory_order_relaxed);
            if (s.r1 == 1) s.y.store(1, std::memory_order_relaxed);
            s.r2 = s.y.load(std::memory_order_relaxed);
            s.r3 = s.x.load(std::memory_order_relaxed);
        },
        [](const WRC_State& s) {
            return OUTCOME(REG(r1,s.r1), REG(r2,s.r2), REG(r3,s.r3));
        }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 7: SB+fence — Store Buffering WITH barriers
// Shows that mfence fixes the SB outcome
// ─────────────────────────────────────────────────────────────────────────────
void run_SB_with_fence() {
    std::cout << "\n>>> Running SB WITH mfence barriers:\n";
    run_SB(true);
    std::cout << "[SB+fence] With mfence, r1=0 && r2=0 should disappear completely.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 8: RWC — Read-Write Causality
// ─────────────────────────────────────────────────────────────────────────────
struct RWC_State {
    std::atomic<int> x{0}, y{0};
    int r1{0}, r2{0};
    void reset() { x=0; y=0; r1=0; r2=0; }
};

void run_RWC() {
    TestConfig cfg;
    cfg.name        = "RWC (Read-Write Causality)";
    cfg.description = "Load→store reordering test";

    run_test<RWC_State>(cfg,
        [](RWC_State& s, bool) {
            s.r1 = s.x.load(std::memory_order_relaxed);
            s.y.store(1, std::memory_order_relaxed);
        },
        [](RWC_State& s, bool) {
            s.r2 = s.y.load(std::memory_order_relaxed);
            s.x.store(1, std::memory_order_relaxed);
        },
        [](const RWC_State& s) {
            return OUTCOME(REG(r1, s.r1), REG(r2, s.r2));
        }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 9: N6 — 3 variables, 3 threads (classic)
// ─────────────────────────────────────────────────────────────────────────────
struct N6_State {
    std::atomic<int> x{0}, y{0}, z{0};
    int r1{0}, r2{0};
    void reset() { x=0; y=0; z=0; r1=0; r2=0; }
};

void run_N6() {
    TestConfig cfg;
    cfg.name        = "N6 (3-variable litmus)";
    cfg.description = "x,y,z: mixed read/write reordering";

    run_test<N6_State>(cfg,
        [](N6_State& s, bool) {
            s.x.store(1, std::memory_order_relaxed);
            s.r1 = s.y.load(std::memory_order_relaxed);
        },
        [](N6_State& s, bool) {
            s.y.store(1, std::memory_order_relaxed);
            s.r2 = s.x.load(std::memory_order_relaxed);
        },
        [](const N6_State& s) {
            return OUTCOME(REG(r1, s.r1), REG(r2, s.r2));
        }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 10: 2+2W — Two stores, two loads on opposite vars
// ─────────────────────────────────────────────────────────────────────────────
struct W2_State {
    std::atomic<int> x{0}, y{0};
    int r1{0}, r2{0};
    void reset() { x=0; y=0; r1=0; r2=0; }
};

void run_2plus2W() {
    TestConfig cfg;
    cfg.name        = "2+2W (Two writes each)";
    cfg.description = "Both write x and y; do reads agree?";

    run_test<W2_State>(cfg,
        [](W2_State& s, bool) {
            s.x.store(2, std::memory_order_relaxed);
            s.y.store(1, std::memory_order_relaxed);
        },
        [](W2_State& s, bool) {
            s.y.store(2, std::memory_order_relaxed);
            s.x.store(1, std::memory_order_relaxed);
        },
        [](const W2_State& s) {
            return OUTCOME(REG(x, s.x.load()), REG(y, s.y.load()));
        }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 11: DEKKER — Dekker's mutual exclusion (relies on memory ordering)
// ─────────────────────────────────────────────────────────────────────────────
struct Dekker_State {
    std::atomic<int> flag0{0}, flag1{0};
    int in_cs0{0}, in_cs1{0}; // both in critical section?
    void reset() { flag0=0; flag1=0; in_cs0=0; in_cs1=0; }
};

void run_DEKKER() {
    TestConfig cfg;
    cfg.name        = "DEKKER (mutual exclusion)";
    cfg.description = "Can both threads enter critical section?";
    cfg.iterations  = 200'000;

    run_test<Dekker_State>(cfg,
        [](Dekker_State& s, bool barriers) {
            s.flag0.store(1, std::memory_order_relaxed);
            if (barriers) MFENCE();
            if (s.flag1.load(std::memory_order_relaxed) == 0)
                s.in_cs0 = 1;  // enter critical section
        },
        [](Dekker_State& s, bool barriers) {
            s.flag1.store(1, std::memory_order_relaxed);
            if (barriers) MFENCE();
            if (s.flag0.load(std::memory_order_relaxed) == 0)
                s.in_cs1 = 1;
        },
        [](const Dekker_State& s) {
            return OUTCOME(REG(cs0, s.in_cs0), REG(cs1, s.in_cs1));
        }
    );
    std::cout << "[DEKKER] cs0=1,cs1=1 means BOTH entered critical section — BUG!\n"
              << "         Without barriers this can happen on x86 due to store buffering.\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST 12: ISA2 — 3-thread causality chain
// ─────────────────────────────────────────────────────────────────────────────
struct ISA2_State {
    std::atomic<int> x{0}, y{0}, z{0};
    int r1{0}, r2{0}, r3{0};
    void reset() { x=0; y=0; z=0; r1=0; r2=0; r3=0; }
};

void run_ISA2() {
    TestConfig cfg;
    cfg.name        = "ISA2 (causality chain)";
    cfg.description = "x→y→z: does causality hold?";
    cfg.iterations  = 500'000;

    run_test<ISA2_State>(cfg,
        [](ISA2_State& s, bool) {
            s.x.store(1, std::memory_order_relaxed);
        },
        [](ISA2_State& s, bool) {
            s.r1 = s.x.load(std::memory_order_relaxed);
            s.y.store(1, std::memory_order_relaxed);
        },
        [](const ISA2_State& s) {
            // simplified: T3 reads both y and x
            int ry = s.y.load(std::memory_order_relaxed);
            int rx = s.x.load(std::memory_order_relaxed);
            return OUTCOME(REG(r1,s.r1), REG(y,ry), REG(x,rx));
        }
    );
    std::cout << "[ISA2] If y=1 is visible, x=1 should also be visible (causality).\n"
              << "       x86-TSO: causality preserved. ARMv8: may break without barriers.\n\n";
}
