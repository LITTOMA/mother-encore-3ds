#pragma once
#include <cstdint>
// Clock injection for compiling the UNMODIFIED official RandomPCG source.
class OS {
public:
    static OS* get_singleton() { static OS instance; return &instance; }
    uint64_t get_unix_time() { ++unix_calls; return unix_seconds; }
    uint64_t get_ticks_usec() { ++tick_calls; return ticks_usec; }
    uint64_t unix_seconds = 0, ticks_usec = 0;
    unsigned unix_calls = 0, tick_calls = 0;
};
