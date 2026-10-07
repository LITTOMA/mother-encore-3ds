#include "encore/field_scene_sources.hpp"
namespace encore::upstream {
namespace {
template<class T> bool read_plain(const PodunkBundleData &bundle,
    PodunkPackRole role, const std::string &root, T &owner, std::string &e) {
  std::vector<uint8_t> bytes;
  return bundle.read(role, root, bytes, e) &&
         owner.load(bytes.data(), bytes.size(), e);
}
template<class T> bool read_identity(const PodunkBundleData &bundle,
    PodunkPackRole role, const std::string &root, T &owner, std::string &e) {
  const auto *entry = bundle.entry(role);
  if (!entry) { e = "Scene bundle required typed source role absent"; return false; }
  std::vector<uint8_t> bytes;
  return bundle.read(role, root, bytes, e) &&
         owner.load(bytes.data(), bytes.size(), entry->identity, e);
}
} // namespace
bool FieldSceneSources::load(const PodunkBundleData &bundle,
                            const std::string &root, std::string &e) {
  if (loaded_ || failed_ || !bundle.valid() || root.empty()) {
    e = "Scene source owner must be fresh with a valid checked bundle";
    return false;
  }
  if (!read_identity(bundle, PodunkPackRole::Grass, root, grass_, e)) {
    failed_ = true; e = "Scene Grass source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::Npc, root, npc_, e)) {
    failed_ = true; e = "Scene Npc source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::Enemy, root, enemy_, e)) {
    failed_ = true; e = "Scene Enemy source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::CharacterTint, root, tint_, e)) {
    failed_ = true; e = "Scene CharacterTint source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::SpriteBridge, root, sprite_, e)) {
    failed_ = true; e = "Scene SpriteBridge source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::Emotes, root, emote_, e)) {
    failed_ = true; e = "Scene Emotes source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::Dandelion, root, dandelion_, e)) {
    failed_ = true; e = "Scene Dandelion source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::Door, root, door_, e)) {
    failed_ = true; e = "Scene Door source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::Prompt, root, prompt_, e)) {
    failed_ = true; e = "Scene Prompt source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::DeadBush, root, bush_, e)) {
    failed_ = true; e = "Scene DeadBush source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::InteractDialog, root, interact_, e)) {
    failed_ = true; e = "Scene InteractDialog source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::Present, root, present_, e)) {
    failed_ = true; e = "Scene Present source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::DroppedItem, root, dropped_, e)) {
    failed_ = true; e = "Scene DroppedItem source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::Sparkles, root, sparkles_, e)) {
    failed_ = true; e = "Scene Sparkles source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::OpenableDoor, root, openable_, e)) {
    failed_ = true; e = "Scene OpenableDoor source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::Payphone, root, payphone_, e)) {
    failed_ = true; e = "Scene Payphone source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::Butterfly, root, butterfly_, e)) {
    failed_ = true; e = "Scene Butterfly source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::CutsceneArea, root, cutscene_, e)) {
    failed_ = true; e = "Scene CutsceneArea source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::Birds, root, birds_, e)) {
    failed_ = true; e = "Scene Birds source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::CameraArea, root, camera_area_, e)) {
    failed_ = true; e = "Scene CameraArea source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::MusicChanger, root, music_, e)) {
    failed_ = true; e = "Scene MusicChanger source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::CameraArrows, root, arrows_, e)) {
    failed_ = true; e = "Scene CameraArrows source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::SceneActions, root, actions_, e)) {
    failed_ = true; e = "Scene SceneActions source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::SteppingSounds, root, stepping_, e)) {
    failed_ = true; e = "Scene SteppingSounds source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::PlayerTransitions, root, transitions_, e)) {
    failed_ = true; e = "Scene PlayerTransitions source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::GameCamera, root, camera_, e)) {
    failed_ = true; e = "Scene GameCamera source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::DoorNpc, root, door_npc_, e)) {
    failed_ = true; e = "Scene DoorNpc source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::MelodyBackground, root, melody_, e)) {
    failed_ = true; e = "Scene MelodyBackground source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::VendingMachine, root, vending_, e)) {
    failed_ = true; e = "Scene VendingMachine source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::Map, root, map_, e)) {
    failed_ = true; e = "Scene Map source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::Geometry, root, geometry_, e)) {
    failed_ = true; e = "Scene Geometry source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::SceneLifecycle, root, lifecycle_, e)) {
    failed_ = true; e = "Scene SceneLifecycle source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::NodeTree, root, tree_, e)) {
    failed_ = true; e = "Scene NodeTree source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::CanvasArt, root, canvas_, e)) {
    failed_ = true; e = "Scene CanvasArt source: " + e; return false;
  }
  if (!read_plain(bundle, PodunkPackRole::NativeTimers, root, timers_, e)) {
    failed_ = true; e = "Scene NativeTimers source: " + e; return false;
  }
  std::vector<uint8_t> audio_bytes;
  if (!bundle.read(PodunkPackRole::SceneNativeAudio, root, audio_bytes, e) ||
      !audio_.load(audio_bytes.data(), audio_bytes.size(), tree_, e)) {
    failed_ = true; e = "Scene native Audio source: " + e; return false;
  }
  if (!read_identity(bundle, PodunkPackRole::SceneSignalCallbacks, root,
                     signals_, e)) {
    failed_ = true; e = "Scene signal callback source: " + e; return false;
  }
  std::vector<uint8_t> visibility_bytes;
  if (!bundle.read(PodunkPackRole::SceneVisibility, root, visibility_bytes, e) ||
      !visibility_.load(visibility_bytes.data(), visibility_bytes.size(), tree_, e)) {
    failed_ = true; e = "Scene native visibility source: " + e; return false;
  }
  std::vector<uint8_t> npc_world_bytes;
  if (!bundle.read(PodunkPackRole::SceneNpcWorld, root, npc_world_bytes, e) ||
      !npc_world_.load(npc_world_bytes.data(), npc_world_bytes.size(),
                       tree_, npc_, geometry_, e)) {
    failed_ = true; e = "Scene NPC native world source: " + e; return false;
  }
  std::vector<uint8_t> prompt_native_bytes;
  if (!bundle.read(PodunkPackRole::ScenePromptNative, root, prompt_native_bytes, e) ||
      !prompt_native_.load(prompt_native_bytes.data(), prompt_native_bytes.size(),
                          tree_, prompt_, e)) {
    failed_ = true; e = "Scene Prompt native source: " + e; return false;
  }
  const auto expected = bundle.identity();
  auto same = [&](const FieldIdentity &id) {
    return id.scene_id == expected.scene_id &&
           id.source_sha256 == expected.source_sha256 &&
           id.upstream_commit == expected.upstream_commit;
  };
  if (!same(map_.identity()) || !same(geometry_.identity()) ||
      !same(lifecycle_.identity()) || !same(tree_.identity()) ||
      !same(canvas_.identity()) || !same(audio_.identity()) ||
      !same(signals_.identity()) || !same(visibility_.identity()) ||
      !same(npc_world_.identity()) || !same(prompt_native_.identity()) ||
      map_.source_scene() != bundle.source_scene() ||
      geometry_.source_scene() != bundle.source_scene() ||
      lifecycle_.source_scene() != bundle.source_scene() ||
      tree_.source_scene() != bundle.source_scene() ||
      canvas_.source_scene() != bundle.source_scene()) {
    failed_ = true;
    e = "Complete scene sources belong to different destination identities";
    return false;
  }
  loaded_ = true;
  e.clear();
  return true;
}
bool FieldSceneSources::bind_sources(FieldSceneConsumers &c, std::string &e) const {
  if (!valid()) { e = "Complete scene sources not loaded"; return false; }
  c.grass_data = &grass_;
  c.npc_data = &npc_;
  c.enemy_data = &enemy_;
  c.tint_data = &tint_;
  c.sprite_data = &sprite_;
  c.emote_data = &emote_;
  c.dandelion_data = &dandelion_;
  c.door_data = &door_;
  c.prompt_data = &prompt_;
  c.bush_data = &bush_;
  c.interact_data = &interact_;
  c.present_data = &present_;
  c.dropped_data = &dropped_;
  c.sparkles_data = &sparkles_;
  c.openable_data = &openable_;
  c.payphone_data = &payphone_;
  c.butterfly_data = &butterfly_;
  c.cutscene_data = &cutscene_;
  c.birds_data = &birds_;
  c.camera_area_data = &camera_area_;
  c.music_data = &music_;
  c.arrows_data = &arrows_;
  c.actions_data = &actions_;
  c.stepping_data = &stepping_;
  c.transitions_data = &transitions_;
  c.camera_data = &camera_;
  c.door_npc_data = &door_npc_;
  c.melody_data = &melody_;
  c.vending_data = &vending_;
  c.map = &map_;
  c.geometry_data = &geometry_;
  e.clear();
  return true;
}
} // namespace encore::upstream
