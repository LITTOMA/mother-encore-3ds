#include "encore/field_object_signals.hpp"
#include "podunk_native_root.hpp"
#include <cassert>
using namespace encore::upstream;
// Manual only. No fabricated scene or successful source/native owner.
void field_object_signals_manual_negative_cases() {
  FieldObjectSignals signals;FieldGlobalRegistry registry;std::string error;
  uint32_t arity=99;bool connected=true;
  assert(!signals.initialize(registry,{},error));
  assert(!signals.connect(1,"ready",2,"callback",0,{},error));
  assert(!signals.connect(1,"ready",2,"callback",16,{},error));
  assert(!signals.connected(1,"ready",2,"callback",connected,error));
  assert(!signals.disconnect(1,"ready",2,"callback",error));
  assert(!signals.emit(1,"ready",{},error));
  assert(!signals.block(1,true,error));
  assert(!signals.release(1,error));
  assert(!signals.duplicate_persistent({},error));
  encore::ctr::PodunkNativeRoot native;
  assert(!native.bind_object_signals(signals,error));
  assert(!native.signal_declaration(1,"size_changed",arity,error));
  assert(!native.connect_signal(false,"size_changed",2,"callback",error));
  assert(!native.disconnect_signal(false,"size_changed",2,"callback",error));
}
// Caller provides live objects and real source declarations from its normal
// scene assembly. This exercises the actual deferred queue, not a local mock.
void field_object_signals_manual_connections(
    FieldObjectSignals &signals,FieldGlobalRegistry &registry,
    FieldObjectId emitter,std::string_view signal,FieldObjectId target,
    std::string_view method,const std::vector<FieldDeferredValue> &arguments) {
  std::string error;bool connected=false;
  assert(signals.registry()==&registry);
  assert(signals.connect(emitter,signal,target,method,
                        FieldSignalDeferred|FieldSignalOneShot,{},error));
  assert(signals.connected(emitter,signal,target,method,connected,error)&&connected);
  assert(!signals.connect(emitter,signal,target,method,0,{},error));
  assert(signals.block(emitter,true,error));
  assert(signals.emit(emitter,signal,arguments,error));
  assert(signals.connected(emitter,signal,target,method,connected,error)&&connected);
  assert(signals.block(emitter,false,error));
  assert(signals.emit(emitter,signal,arguments,error));
  assert(signals.connected(emitter,signal,target,method,connected,error)&&!connected);
  assert(registry.flush_messages(error));
  assert(!signals.disconnect(emitter,signal,target,method,error));
}
