#include "encore/field_global_data.hpp"
#include "encore/global_data_constructor.hpp"
#include <cassert>
using namespace encore::upstream;
// Manual negative cases; not registered or automatically executed.
int main() {
  FieldGlobalDataRuntime core;
  GlobalDataConstructorData data;
  FieldGlobalExternalBinding binding;
  FieldGlobalExternalState state;
  std::string error, flavor = "unchanged";
  assert(!core.constructor_complete() && !core.ready_complete() &&
         !core.load_complete());
  assert(!core.initialize_constructor(data, error));
  assert(!core.source_stage_parent(binding, 1, error));
  assert(!core.source_enter(binding, 1, error));
  assert(!core.source_begin_ready(binding, 1, error));
  assert(!core.source_finish_ready(binding, 1, error));
  assert(!core.source_exit(binding, 1, error));
  state.name = "unchanged";
  assert(!core.source_state(state, error) && state.name == "unchanged");
  assert(!core.menu_flavor(flavor, error) && flavor == "unchanged");
  assert(!core.constructed_body_alive(1));
}
