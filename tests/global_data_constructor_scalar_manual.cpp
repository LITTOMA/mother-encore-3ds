#include "encore/field_global_data.hpp"
#include "encore/global_yaml_caches.hpp"
#include <cassert>
using namespace encore::upstream;
// Manual source-contract negative cases; no game, test runner or RNG runs here.
int main() {
  FieldGlobalDataRuntime core;
  GlobalYamlValue value;
  FieldGlobalDataMemberState out;
  std::string error;
  out.kind = 123;
  assert(!core.read_global_member("", out, error) && out.kind == 123);
  for (uint32_t kind = 0; kind <= 9; ++kind) {
    value.kind = kind;
    assert(!core.write_global_scalar("", value, error));
  }
  assert(!core.constructor_complete() && !core.ready_complete());
}
