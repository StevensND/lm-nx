#pragma once
// Keep the caller's CFG identical in GENERATE and USE. Outside training, LTO removes the empty function without
// changing the source contract.
extern "C" void lmnx_pgo_volcar();
// Dump and reset PGO counters about every 1,800 main-loop iterations (60 seconds); GCC adds them to the file.
// Training does not depend on exiting through the menu. Empty outside training.
extern "C" void lmnx_pgo_tick();
