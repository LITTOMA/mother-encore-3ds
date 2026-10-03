#pragma once
// Minimal compile fixture only: no RNG or OS algorithm is replaced here.
#include <cstdint>
#define _FORCE_INLINE_ inline
#define unlikely(value) (value)
#define _llvm_has_builtin(value) 0
using real_t = float;
