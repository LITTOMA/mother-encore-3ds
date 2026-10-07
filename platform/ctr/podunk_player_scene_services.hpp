#pragma once
#include "podunk_named_sfx.hpp"
#include "podunk_player_preload_scenes.hpp"
#include "podunk_scene_consumers.hpp"

namespace encore::ctr {
// The caller supplies the actual input sample and the existing native owners.
// Business services below read/write the SAME continuation and player body.
struct PodunkPlayerSceneInput {
  PodunkHouseContinuation *continuation = nullptr;
  const PodunkPlayerSources *sources = nullptr;
  PodunkPlayerHost *player = nullptr;
  upstream::FieldNodeTreeRuntime *tree = nullptr;
  PodunkSceneConsumers *consumers = nullptr;
  PodunkSceneScripts *scripts = nullptr;
  const upstream::FieldSceneData *scene_data = nullptr;
  PodunkPlayerPreloadScenes *preloads = nullptr;
  PodunkNamedSfx *named_sfx = nullptr;
  PodunkPlayerServices native;
  std::function<bool(upstream::Vec2 &, std::string &)> controls;
  std::function<bool(std::string_view, upstream::PlayerInputQuery, bool &,
                     std::string &)>
      input;
};
class PodunkPlayerSceneServices {
public:
  // Preparation does not evaluate onready, allocate nodes, draw RNG or demand
  // scene Ready. Each operation checks its actual owner at the source call.
  bool prepare(PodunkPlayerSceneInput, std::string &);
  PodunkPlayerServices services() const;
  bool pause(bool stop_running, bool start_idle, bool emit_signal,
             std::string &);
  bool unpause(bool emit_signal, std::string &);
  bool collisions(bool, std::string &);
  bool direction_and_input(upstream::Vec2, std::string &);
  bool update_party_member(std::string &);
  bool exit_camera(std::string &);
  bool respawn(std::string &);
  bool collider_info(upstream::FieldObjectId, upstream::PlayerColliderInfo &,
                     std::string &) const;
  bool interact(upstream::FieldObjectId, bool thoughts, std::string &);
  bool press_prompt(upstream::FieldObjectId, std::string &);

private:
  bool live(std::string &) const;
  bool source(upstream::FieldObjectId, upstream::FieldSceneScriptAdmission &,
              std::string &) const;
  bool singleton_party(std::string &) const;
  bool party_call(std::string_view,
                  const std::vector<upstream::FieldDeferredValue> &,
                  std::string &);
  bool current_scene_area(bool &, std::string &) const;
  bool turn_player(upstream::FieldObjectId, bool party, std::string &);
  bool climbing(upstream::FieldObjectId, bool &, std::string &) const;
  bool damage_effects(upstream::FieldObjectId, std::string_view,
                      std::vector<upstream::PlayerDamageEffect> &,
                      std::string &) const;
  bool character_name(upstream::FieldObjectId, std::string &,
                      std::string &) const;
  bool has_field_skill(upstream::FieldObjectId, std::string_view, bool &,
                       std::string &) const;
  bool battle_skill(upstream::FieldObjectId, std::string_view, bool &,
                    std::string &) const;
  bool button_skills(upstream::FieldObjectId, std::vector<std::string> &,
                     std::string &) const;
  PodunkPlayerSceneInput input_;
  PodunkPlayerServices services_;
};
} // namespace encore::ctr
