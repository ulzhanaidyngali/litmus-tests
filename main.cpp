

#include "harness.h"
#include "tests.h"
#include <iostream>
#include <string>

static void print_header() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║    MEMORY CONSISTENCY LITMUS TEST HARNESS                  ║
║    Platform: x86-64   Model: x86-TSO                       ║
║    CompArch Final Project — Team Litmus                    ║
╚════════════════════════════════════════════════════════════╝
)";
}

static void print_tso_intro() {
    std::cout << R"(
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  x86-TSO Memory Model (quick reminder)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  • Each core has a private FIFO store buffer
  • Stores go into the buffer first, then to shared memory
  • Loads bypass the store buffer (can read own pending writes)
  • Loads from OTHER cores read from shared memory
  • Store→Store order: PRESERVED (FIFO buffer)
  • Load→Load order:  PRESERVED
  • Store→Load order: CAN BE REORDERED  ← this is the key!
  • mfence flushes the store buffer, restoring SC for that point

  Legend:  [ALLOWED]  = this outcome can appear on x86
           [FORBIDDEN] = this outcome cannot appear on x86
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
)";
}

static void print_menu() {
    std::cout << R"(
Select tests to run:
  1  SB  (Store Buffering)        — classic x86 relaxation
  2  MP  (Message Passing)        — safe on x86, broken on ARM
  3  LB  (Load Buffering)         — safe on x86
  4  IRIW (Indep. Reads/Writes)   — 4 threads, safe on x86
  5  CoRR (Cache Coherence)       — must hold everywhere
  6  WRC (Write-Read Causality)
  7  RWC (Read-Write Causality)
  8  SB+fence (SB with mfence)    — shows barriers fix SB
  9  DEKKER (mutual exclusion)    — classic lock pattern
  10 2+2W (Two writes each)
  11 ISA2 (causality chain)
  12 N6  (3-variable test)
  0  Run ALL tests
  q  Quit

Enter choice: )";
}

int main() {
    print_header();
    print_tso_intro();

    std::string choice;
    while (true) {
        print_menu();
        std::cin >> choice;

        if (choice == "q" || choice == "Q") break;

        int sel = -1;
        try { sel = std::stoi(choice); } catch (...) {}

        switch (sel) {
            case 1:  run_SB();          break;
            case 2:  run_MP();          break;
            case 3:  run_LB();          break;
            case 4:  run_IRIW();        break;
            case 5:  run_CoRR();        break;
            case 6:  run_WRC();         break;
            case 7:  run_RWC();         break;
            case 8:  run_SB_with_fence(); break;
            case 9:  run_DEKKER();      break;
            case 10: run_2plus2W();     break;
            case 11: run_ISA2();        break;
            case 12: run_N6();          break;
            case 0:
                std::cout << "\n>>> Running all 12 tests...\n\n";
                run_SB();
                run_MP();
                run_LB();
                run_IRIW();
                run_CoRR();
                run_WRC();
                run_RWC();
                run_SB_with_fence();
                run_DEKKER();
                run_2plus2W();
                run_ISA2();
                run_N6();
                std::cout << "\n✓ All tests complete.\n";
                break;
            default:
                std::cout << "Unknown option.\n";
        }
    }
    return 0;
}
