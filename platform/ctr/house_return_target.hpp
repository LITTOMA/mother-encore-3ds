#pragma once
#include "podunk_outdoor_door.hpp"
#include "house_return_scripts.hpp"
#include "house_return_ladder.hpp"
#include "house_return_npc_presentation.hpp"
#include "house_return_ui_context.hpp"
#include "house_return_tint_runtime.hpp"
#include "house_return_controls.hpp"
#include "house_return_gui_native.hpp"
#include "house_return_prompt_material.hpp"
#include "house_return_interact_programme.hpp"
#include "house_return_inventory.hpp"
#include "house_return_camera_control_effects.hpp"
#include "podunk_scene_visibility.hpp"
#include "podunk_scene_audio.hpp"
#include "podunk_scene_scripts.hpp"
#include "house_return_player_scene_owner.hpp"

namespace encore::ctr {
// All borrows are actual retained owners. No callback in this input approves a
// source binding, supplies native Ready, creates a replacement Player or LOAD.
// Keep the target and the old tree alive through every real deletion receipt.
struct HouseReturnTargetInput {
  const upstream::HouseReturnSources *sources=nullptr;
  const upstream::HouseReturnLadderData *ladder=nullptr;
  const upstream::FieldDoorData *doors=nullptr;
  const upstream::FieldSceneData *old_scene=nullptr;
  const upstream::FieldSceneSignalCallbacksData *callbacks=nullptr;
  const upstream::NativeSessionData *session_data=nullptr;
  upstream::RoomView room{};
  upstream::HouseView text{};
  upstream::DrawerProgramView drawer{};
  upstream::FreshHouseState *house=nullptr;
  PodunkHouseContinuation *continuation=nullptr;
  PodunkMickSession *session=nullptr;
  upstream::FieldGlobalConstructorRuntime *global=nullptr;
  PodunkGlobalDataHost *characters=nullptr;
  PodunkInventoryHost *inventory=nullptr;
  upstream::FieldMapSpace *map=nullptr;
  upstream::FieldGeometrySpace *space=nullptr;
  PodunkPlayerPhysicsWorld *world=nullptr;
  PodunkPlayerHost *player=nullptr;
  PodunkPlayerCamera *camera=nullptr;
  PodunkPlayerSources player_sources;
  upstream::FieldSceneTreeTimers *scene_timers=nullptr;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> old_tree;
  PodunkSceneAudio *retained_audio=nullptr;
  upstream::NativeInputAdapter *retained_input=nullptr;
  PodunkSceneNative *retained_native=nullptr;
  PodunkSceneScripts *retained_scripts=nullptr;
  PodunkOutdoorDoor *retained_door=nullptr;
  // The SAME existing owner whose closures PlayerMotion already captures.
  PodunkPlayerSceneServices *retained_player_services=nullptr;
  SourceFontRenderer *font=nullptr;
  upstream::FieldEquipmentView equipment{};
  std::string asset_root;
  upstream::FieldCanvasArtHost canvas;
  upstream::FieldSpriteHost sprite;
  PodunkSceneMaterialOwner *materials=nullptr;
  const HouseButtonPromptSourceOwner *prompt_source_calls=nullptr;
  // Only source signal registration/provenance remain external. UI, Sprite
  // and Timer operations are replaced by the concrete owners in this target.
  HouseReturnNpcPorts npc;
  bool actual_debug_build=false;
};
struct HouseReturnTargetGap {
  uint32_t source=0;
  std::string node,script,native_class,reason;
};
// A concrete detached House factory and Door target. It runs the existing
// consumers, not an interpreter or a list of success callbacks. The current
// incomplete source roster rejects prepare BEFORE ObjectDB allocation; stage
// and a node count are never proof of a usable or Ready destination.
class HouseReturnTarget final : public PodunkDoorTargetOwner {
public:
  HouseReturnTarget();
  ~HouseReturnTarget();
  HouseReturnTarget(const HouseReturnTarget&)=delete;
  HouseReturnTarget&operator=(const HouseReturnTarget&)=delete;
  bool stage(HouseReturnTargetInput,std::string&);
  const std::vector<HouseReturnTargetGap>&missing()const;
  const upstream::HouseReentryData*data()const override;
  const upstream::FieldGlobalRegistry*registry()const override;
  bool prepare(const upstream::FieldDoorDescriptor&,
               upstream::FieldDoorCandidate&,std::string&)override;
  bool release(const upstream::FieldDoorCandidate&,bool,std::string&)override;
  bool instantiate(const upstream::FieldDoorCandidate&,PodunkDoorScene&,
                   std::string&)override;
  bool leave_old(upstream::FieldObjectId,upstream::FieldSceneHost&,
                 std::string&)override;
  bool free_old(std::shared_ptr<upstream::FieldNodeTreeRuntime>,
                upstream::FieldObjectId,std::string&)override;
  bool attach_root(const PodunkDoorScene&,std::string&)override;
  bool attach_player(const PodunkDoorScene&,PodunkPlayerHost&,
                    std::string&)override;
  bool reparent_persistent(const PodunkDoorScene&,upstream::FieldObjectId,
                           std::string&)override;
  PodunkPlayerSceneServices*player_services()override;
  bool update_key_indicator(std::string&)override;
  bool make_player_camera_current(upstream::FieldObjectId,std::string&)override;
  // Called only after the real retained Player has entered. No physics_step,
  // collect, global timer idle or GPU begin is duplicated by this composition.
  bool activate_after_player(std::string&);
  bool begin_frame(uint64_t,float idle_delta,float physics_delta,bool paused,
                   bool actual_update_pending,std::string&);
  // The caller supplies the real physics epoch immediately before the one
  // SceneTree physics traversal; this does not collect Area pairs or timers.
  bool begin_physics(uint64_t actual_epoch,float actual_delta,std::string&);
  bool deferred(const upstream::FieldDeferredMessage&,std::string&);
  bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,
                   std::string&)const;
  bool collect_deleted(std::string&);
  // After the sole global SceneTreeTimers idle tail; never advances a timer.
  bool finish_idle_tail(std::string&);
  // Keep World/driver/UI alive through exact unbind. Release the Sample only
  // after real House source/native deletion and collect_deleted.
  bool unbind_room_shaker(std::string&);
  bool release_room_shaker_resources(std::string&);
  bool draw(uint64_t,float delta,float global_shader_time,std::string&);
  bool ready()const;
  // FreshHouse's return-specific Ready adapter may call this validator after
  // the real complete Tree traversal. It executes no source body or RNG.
  bool source_factory_ready(std::string&)const;
  const std::shared_ptr<upstream::FieldNodeTreeRuntime>&tree()const;
  HouseReturnNpcRuntime&npcs();
  HouseReturnDialogue&dialogue();
  HouseReturnDialogueNativeOwner&dialogue_native();
  HouseReturnInventoryOwner&house_inventory();
  PodunkSceneNative&canvas();
  HouseReturnGeometryNative&geometry();
  HouseReturnKinematicNative&kinematic();
  HouseReturnRayNative&rays();
  PodunkSceneTimers&timers();
  HouseReturnControlsNative&controls();
  HouseReturnCameraControl&room_shaker();
private:
  struct State;
  std::unique_ptr<State>state_;
};
} // namespace encore::ctr
