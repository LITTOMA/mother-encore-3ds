#pragma once
#include <cstdlib>
#include <iostream>
// Manual drivers must still perform every check in Release/NDEBUG builds.
#define MANUAL_REQUIRE(expression)                                             \
  do {                                                                         \
    if (!(expression)) {                                                       \
      std::cerr << __FILE__ << ':' << __LINE__ << ": " << #expression          \
                << " failed\n";                                                \
      std::exit(1);                                                            \
    }                                                                          \
  } while (false)
