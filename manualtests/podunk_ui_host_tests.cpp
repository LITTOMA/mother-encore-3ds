#include "podunk_ui_host.hpp"
#include <cassert>
using namespace encore::ctr;
using namespace encore::upstream;
// Explicit manual invocation only; compile does not execute these cases.
void podunk_ui_host_manual_negative_cases() {
  PodunkUiHost host;
  std::string error;
  FieldGlobalExternalState state;
  FieldNodeDescriptor node;
  FieldNodeBinding binding;
  FieldGlobalExternalSpec spec;
  std::unique_ptr<FieldGlobalExternalObject> object;
  FieldDeferredMessage message;
  assert(!host.ui());
  assert(!host.state(state, error));
  assert(!host.entered(1, error));
  assert(!host.advance_ready(error));
  assert(!host.exited(error));
  assert(!host.construct(1, spec, object, error) && !object);
  assert(!host.bind(1, node, binding, error));
  assert(!host.dispatch(1, binding, FieldTreePhase::ReadyNative, error));
  assert(!host.deferred(message, error));
  assert(!host.release(1, binding, error));
  assert(!host.pending_owners().empty());
}
