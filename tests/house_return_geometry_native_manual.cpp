#include "../platform/ctr/house_return_geometry_native.hpp"
#include "manual_require.hpp"

using namespace encore::upstream;
using encore::ctr::HouseReturnGeometryNative;
// Manual only. The external driver must construct/enter the actual full House
// through its real native and script owners, replace the SAME live Space after
// old scene deletion, transfer the original Player and activate its world.
// No fake Registry, RID, Ready callback, test main or automatic hook is added.
void house_return_geometry_native_manual(HouseReturnGeometryNative&native,
    FieldObjectId area,FieldObjectId shape,FieldObjectId static_body,
    const FieldNodeDescriptor&actual_external_kinematic){
  std::string error;
  MANUAL_REQUIRE(native.physics_admitted(error));
  MANUAL_REQUIRE(native.owns(area)&&native.owns(shape)&&native.owns(static_body));
  MANUAL_REQUIRE(actual_external_kinematic.native_class=="KinematicBody2D"&&
                 !native.owns(actual_external_kinematic));
  FieldPhysicsRid original;
  MANUAL_REQUIRE(native.owner_rid(static_body,original,error));
  MANUAL_REQUIRE(original.space&&original.handle);
  auto retained=original;
  MANUAL_REQUIRE(!native.owner_rid(shape,retained,error));
  MANUAL_REQUIRE(retained.space==original.space&&retained.handle==original.handle);
  MANUAL_REQUIRE(!native.owner_rid(0,retained,error));
  MANUAL_REQUIRE(retained.space==original.space&&retained.handle==original.handle);

  bool disabled=false;
  MANUAL_REQUIRE(native.shape_disabled(shape,disabled,error));
  auto retained_disabled=disabled;
  MANUAL_REQUIRE(!native.shape_disabled(static_body,retained_disabled,error));
  MANUAL_REQUIRE(retained_disabled==disabled);
  MANUAL_REQUIRE(!native.set_disabled(static_body,!disabled,error));
  MANUAL_REQUIRE(native.shape_disabled(shape,retained_disabled,error)&&retained_disabled==disabled);
  MANUAL_REQUIRE(!native.set_collision(shape,0,0,error));

  uint32_t arguments=0;
  MANUAL_REQUIRE(native.declaration(area,"body_entered",arguments,error)&&arguments==1);
  MANUAL_REQUIRE(native.declaration(area,"body_shape_exited",arguments,error)&&arguments==4);
  MANUAL_REQUIRE(!native.declaration(area,"unknown_native_signal",arguments,error)&&arguments==4);
  MANUAL_REQUIRE(!native.declaration(shape,"body_entered",arguments,error)&&arguments==4);
  MANUAL_REQUIRE(!native.phase(area,FieldTreePhase::ReadyScript,error));
  MANUAL_REQUIRE(!native.phase(area,FieldTreePhase::Input,error));
  MANUAL_REQUIRE(!native.phase(area,FieldTreePhase::EnterNative,error));
  MANUAL_REQUIRE(!native.phase(area,FieldTreePhase::Deleting,error));
  MANUAL_REQUIRE(!native.observe_deleted(area,error));
  MANUAL_REQUIRE(!native.release_deleted(area,error));
  MANUAL_REQUIRE(native.physics_admitted(error));
  FieldPhysicsRid after;
  MANUAL_REQUIRE(native.owner_rid(static_body,after,error)&&
      after.space==original.space&&after.handle==original.handle);

  FieldDeferredMessage invented;invented.object=area;invented.kind=FieldDeferredKind::Call;
  invented.member="unknown_source_method";invented.args={FieldObjectRef{static_body}};
  MANUAL_REQUIRE(!native.dispatch_source(invented,error));
  MANUAL_REQUIRE(native.physics_admitted(error));
}
