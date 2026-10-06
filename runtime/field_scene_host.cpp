#include "encore/field_scene_host.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool identity(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
template <class Data> bool same_pack(const FieldSceneData &s, const Data *d) {
  return d && d->valid() && d->scene_id() == s.identity().scene_id &&
         d->source_pin() == s.identity().upstream_commit;
}
} // namespace
bool FieldSceneHost::configure(const FieldSceneData &d, FieldSceneConsumers c,
                               FieldSceneHostOps h, std::string &e) {
  if (data_) {
    e = "Field SceneHost already connected; construct a new host after "
        "disconnecting the old source instance";
    return false;
  }
  if (!d.valid() || !h.read_flag || !h.write_flag || !h.connect_flags ||
      !h.emit_flags || !h.connect_area_left || !h.emit_area_left ||
      !h.connect_switches || !h.visibility || !h.queue_free ||
      !h.map_possessed || !h.flyingman_present || !h.set_flyingman_present ||
      !h.debug_context || !h.teleport_player || !h.current_scene) {
    e = "Field SceneHost actual lifecycle operations incomplete";
    return false;
  }
  if ((c.grass_data || c.grass) &&
      (!c.grass_data || !c.grass || !c.random || !c.grass_data->valid() ||
       !identity(d.identity(), c.grass_data->identity()))) {
    e = "Field SceneHost grass source binding rejected";
    return false;
  }
  if ((c.npc_data || c.npc) && (!c.npc || !same_pack(d, c.npc_data))) {
    e = "Field SceneHost NPC binding rejected";
    return false;
  }
  if ((c.enemy_data || c.enemy) && (!c.enemy || !same_pack(d, c.enemy_data))) {
    e = "Field SceneHost enemy binding rejected";
    return false;
  }
  if ((c.tint_data || c.tint) && (!c.tint || !same_pack(d, c.tint_data))) {
    e = "Field SceneHost tint binding rejected";
    return false;
  }
  if ((c.sprite_data || c.sprite) &&
      (!c.sprite || !same_pack(d, c.sprite_data))) {
    e = "Field SceneHost sprite binding rejected";
    return false;
  }
  if ((c.emote_data || c.emote) && (!c.emote || !same_pack(d, c.emote_data) ||
                                    !c.sprite_data || !c.sprite)) {
    e = "Field SceneHost emotes binding incomplete";
    return false;
  }
  if (c.emote_data) {
    std::array<uint8_t, 32> h{};
    if (!c.emote_data->source_hash(std::string(d.source_scene()), h) ||
        h != d.identity().source_sha256) {
      e = "Field SceneHost emotes scene source differs";
      return false;
    }
  }
  if ((c.dandelion_data || c.dandelion) &&
      (!c.dandelion_data || !c.dandelion || !c.dandelion_data->valid() ||
       !identity(d.identity(), c.dandelion_data->identity()) ||
       d.source_scene() != c.dandelion_data->source_scene())) {
    e = "Field SceneHost dandelion source binding rejected";
    return false;
  }
  if (c.dandelion_data) {
    std::array<uint8_t, 32> h{};
    if (!d.source_hash(c.dandelion_data->spawner_script(), h) ||
        h != c.dandelion_data->spawner_sha()) {
      e = "Field SceneHost dandelion script source differs";
      return false;
    }
  }
  if ((c.door_data || c.door) &&
      (!c.door_data || !c.door || !c.door_data->valid() ||
       !identity(d.identity(), c.door_data->identity()) ||
       d.source_scene() != c.door_data->source_scene())) {
    e = "Field SceneHost Door source binding rejected";
    return false;
  }
  if (c.door_data) {
    std::array<uint8_t, 32> h{};
    if (!d.source_hash(c.door_data->script(), h) ||
        h != c.door_data->script_sha()) {
      e = "Field SceneHost Door script source differs";
      return false;
    }
  }
  if ((c.prompt_data || c.prompt) &&
      (!c.prompt || !same_pack(d, c.prompt_data) ||
       c.prompt_data->scene_hash() != d.identity().source_sha256)) {
    e = "Field SceneHost ButtonPrompt binding rejected";
    return false;
  }
  if ((c.bush_data || c.bush) &&
      (!c.bush || !same_pack(d, c.bush_data) ||
       c.bush_data->scene_hash() != d.identity().source_sha256 ||
       !c.prompt_data || !c.prompt || !c.geometry_data || !c.geometry)) {
    e = "Field SceneHost DeadBush actual prompt/geometry/source binding "
        "incomplete";
    return false;
  }

  if ((c.interact_data || c.interact) &&
      (!c.interact || !same_pack(d, c.interact_data) || !c.prompt_data ||
       !c.prompt)) {
    e = "Field SceneHost InteractDialog source/prompt consumer incomplete";
    return false;
  }
  if (c.interact_data) {
    std::array<uint8_t, 32> h{};
    if (!c.interact_data->source_hash(d.source_scene(), h) ||
        h != d.identity().source_sha256) {
      e = "Field SceneHost InteractDialog scene source differs";
      return false;
    }
  }
  // Retained source closure proves exact scene and script bytes, not just IDs.
  auto closure = [&](const auto *pack) {
    std::array<uint8_t, 32> hash{};
    return pack && pack->valid() && pack->source_hash(d.source_scene(), hash) &&
           hash == d.identity().source_sha256;
  };
  if ((c.arrows_data || c.arrows) && (!c.arrows || !closure(c.arrows_data) ||
                                      c.arrows->data() != c.arrows_data)) {
    e = "Field SceneHost arrows checked runtime/source differs";
    return false;
  }
  if ((c.actions_data || c.actions) &&
      (!c.actions || !closure(c.actions_data) ||
       c.actions->content() != c.actions_data)) {
    e = "Field SceneHost actions checked runtime/source differs";
    return false;
  }
  if ((c.stepping_data || c.stepping) &&
      (!c.stepping || !closure(c.stepping_data) ||
       c.stepping->content() != c.stepping_data)) {
    e = "Field SceneHost stepping checked runtime/source differs";
    return false;
  }
  if ((c.transitions_data || c.transitions) &&
      (!c.transitions || !closure(c.transitions_data) ||
       c.transitions->content() != c.transitions_data)) {
    e = "Field SceneHost transitions checked runtime/source differs";
    return false;
  }
  if ((c.camera_data || c.camera) &&
      (!c.camera || !closure(c.camera_data) ||
       c.camera->data() != c.camera_data ||
       !identity(d.identity(), c.camera_data->identity()))) {
    e = "Field SceneHost camera checked runtime/source differs";
    return false;
  }
  if ((c.door_npc_data || c.door_npc) &&
      (!c.door_npc || !closure(c.door_npc_data) ||
       c.door_npc->content() != c.door_npc_data ||
       c.door_npc_data->source_pin() != d.identity().upstream_commit)) {
    e = "Field SceneHost door_npc checked runtime/source differs";
    return false;
  }
  if ((c.melody_data || c.melody) &&
      (!c.melody || !closure(c.melody_data) ||
       c.melody->content() != c.melody_data ||
       c.melody_data->source_pin() != d.identity().upstream_commit)) {
    e = "Field SceneHost melody checked runtime/source differs";
    return false;
  }
  if (c.camera_data && (!c.random || !c.arrows_data || !c.arrows)) {
    e = "Field SceneHost camera actual scope/shared RNG missing";
    return false;
  }
  if (c.map_space && (!c.map || c.map_space->source() != c.map)) {
    e = "Field SceneHost dynamic map resource differs";
    return false;
  }
  if (c.arrows_data && !identity(d.identity(), c.arrows_data->identity())) {
    e = "Field SceneHost MapArrows source identity differs";
    return false;
  }
  if (c.actions_data &&
      c.actions_data->source_pin() != d.identity().upstream_commit) {
    e = "Field SceneHost actions source pin differs";
    return false;
  }
  if (c.stepping_data &&
      c.stepping_data->source_pin() != d.identity().upstream_commit) {
    e = "Field SceneHost stepping source pin differs";
    return false;
  }
  if (c.transitions_data &&
      c.transitions_data->source_pin() != d.identity().upstream_commit) {
    e = "Field SceneHost transitions source pin differs";
    return false;
  }
  if ((c.present_data || c.present) &&
      (!c.present || !closure(c.present_data) ||
       !(c.present_data->source_pin() == d.identity().upstream_commit))) {
    e = "Field SceneHost present source binding incomplete";
    return false;
  }
  if ((c.dropped_data || c.dropped) &&
      (!c.dropped || !closure(c.dropped_data) ||
       !(c.dropped_data->source_pin() == d.identity().upstream_commit))) {
    e = "Field SceneHost dropped source binding incomplete";
    return false;
  }
  if ((c.sparkles_data || c.sparkles) &&
      (!c.sparkles || !closure(c.sparkles_data) ||
       !(identity(d.identity(), c.sparkles_data->identity())))) {
    e = "Field SceneHost sparkles source binding incomplete";
    return false;
  }
  if ((c.openable_data || c.openable) &&
      (!c.openable || !closure(c.openable_data) ||
       !(identity(d.identity(), c.openable_data->identity())))) {
    e = "Field SceneHost openable source binding incomplete";
    return false;
  }
  if ((c.payphone_data || c.payphone) &&
      (!c.payphone || !closure(c.payphone_data) ||
       !(c.payphone_data->source_pin() == d.identity().upstream_commit))) {
    e = "Field SceneHost payphone source binding incomplete";
    return false;
  }
  if ((c.butterfly_data || c.butterfly) &&
      (!c.butterfly || !closure(c.butterfly_data) ||
       !(c.butterfly_data->source_pin() == d.identity().upstream_commit))) {
    e = "Field SceneHost butterfly source binding incomplete";
    return false;
  }
  if ((c.present_data || c.dropped_data || c.sparkles_data) &&
      (!c.present_data || !c.present || !c.dropped_data || !c.dropped ||
       !c.sparkles_data || !c.sparkles || !c.random || !c.prompt_data ||
       !c.prompt || c.present->content() != c.present_data ||
       c.dropped->content() != c.dropped_data ||
       c.sparkles->data() != c.sparkles_data ||
       !validate_dropped_present_bridge(*c.present_data, *c.dropped_data, e) ||
       !validate_sparkles_owner_bridge(*c.sparkles_data, *c.present_data,
                                       *c.dropped_data, e))) {
    if (e.empty())
      e = "Field SceneHost collected object/Sparkles owner bridge incomplete";
    return false;
  }
  if ((c.openable_data || c.payphone_data) && (!c.prompt_data || !c.prompt)) {
    e = "Field SceneHost object prompt bridge missing";
    return false;
  }
  if (c.butterfly_data && !c.random) {
    e = "Field SceneHost Butterfly shared RNG absent";
    return false;
  }
  if ((c.cutscene_data || c.cutscene) &&
      (!c.cutscene || !closure(c.cutscene_data) ||
       !(c.cutscene_data->source_pin() == d.identity().upstream_commit))) {
    e = "Field SceneHost cutscene typed binding incomplete";
    return false;
  }
  if ((c.birds_data || c.birds) &&
      (!c.birds || !closure(c.birds_data) ||
       !(identity(d.identity(), c.birds_data->identity())))) {
    e = "Field SceneHost birds typed binding incomplete";
    return false;
  }
  if ((c.camera_area_data || c.camera_area) &&
      (!c.camera_area || !closure(c.camera_area_data) ||
       !(identity(d.identity(), c.camera_area_data->identity())))) {
    e = "Field SceneHost camera_area typed binding incomplete";
    return false;
  }
  if ((c.music_data || c.music) &&
      (!c.music || !closure(c.music_data) ||
       !(c.music_data->source_pin() == d.identity().upstream_commit))) {
    e = "Field SceneHost music typed binding incomplete";
    return false;
  }
  if (c.birds_data && !c.random) {
    e = "Field SceneHost Birds shared RNG missing";
    return false;
  }
  if ((c.present_data && c.present->content() != c.present_data) ||
      (c.dropped_data && c.dropped->content() != c.dropped_data) ||
      (c.sparkles_data && c.sparkles->data() != c.sparkles_data) ||
      (c.butterfly_data && c.butterfly->content() != c.butterfly_data) ||
      (c.cutscene_data && c.cutscene->content() != c.cutscene_data) ||
      (c.birds_data && c.birds->data() != c.birds_data) ||
      (c.camera_area_data && c.camera_area->data() != c.camera_area_data) ||
      (c.music_data && c.music->content() != c.music_data)) {
    e = "Field SceneHost runtime belongs to a different checked resource";
    return false;
  }
  if (c.npc_data && c.sprite_data &&
      !field_sprite_npc_binding(*c.sprite_data, *c.npc_data, e))
    return false;
  if (c.map && (!c.map->valid() || !identity(d.identity(), c.map->identity()) ||
                c.map->source_scene() != d.source_scene())) {
    e = "Field SceneHost map binding rejected";
    return false;
  }
  if ((c.geometry_data || c.geometry) &&
      (!c.geometry || !c.geometry_data || !c.geometry_data->valid() ||
       !identity(d.identity(), c.geometry_data->identity()) ||
       c.geometry_data->source_scene() != d.source_scene())) {
    e = "Field SceneHost geometry binding rejected";
    return false;
  }
  // Every pre-existing immutable typed descriptor must match the complete
  // lifecycle roster, including its exact Ready ordinal and relative path.
  auto match = [&](uint32_t id, uint32_t ready, std::string_view node,
                   FieldSceneRole role) {
    for (uint32_t i = 0; i < d.ready_count(); ++i) {
      auto n = d.ready(i);
      if (n.id == id)
        return n.ordinal == ready && d.string(n.node) == node && n.role == role;
    }
    return false;
  };
  if (c.grass_data)
    for (uint32_t i = 0; i < c.grass_data->grass_count(); ++i) {
      auto n = c.grass_data->grass(i);
      if (!match(n.stable_id, n.ready_ordinal,
                 c.grass_data->string(n.node_string), FieldSceneRole::Grass)) {
        e = "Field SceneHost grass Ready differs";
        return false;
      }
    }
  if (c.dandelion_data)
    for (uint32_t i = 0; i < c.dandelion_data->spawner_count(); ++i) {
      auto n = c.dandelion_data->spawner(i);
      if (!match(n.id, n.ready, c.dandelion_data->string(n.node),
                 FieldSceneRole::DandelionSpawner)) {
        e = "Field SceneHost dandelion Ready differs";
        return false;
      }
    }
  if (c.door_data)
    for (uint32_t i = 0; i < c.door_data->door_count(); ++i) {
      auto n = c.door_data->door(i);
      if (!match(n.id, n.ready, c.door_data->string(n.node),
                 FieldSceneRole::Door)) {
        e = "Field SceneHost Door Ready differs";
        return false;
      }
    }
  if (c.prompt_data)
    for (const auto &n : c.prompt_data->records()) {
      if (!match(n.id, n.ready_ordinal, n.node, FieldSceneRole::ButtonPrompt)) {
        e = "Field SceneHost ButtonPrompt Ready differs";
        return false;
      }
      for (uint32_t i = 0; i < d.ready_count(); ++i) {
        auto row = d.ready(i);
        if (row.id == n.id && row.sha != c.prompt_data->script_hash()) {
          e = "Field SceneHost ButtonPrompt script proof differs";
          return false;
        }
      }
    }
  if (c.bush_data)
    for (const auto &n : c.bush_data->records()) {
      if (!match(n.id, n.ready_ordinal, n.node, FieldSceneRole::DeadBush)) {
        e = "Field SceneHost DeadBush Ready differs";
        return false;
      }
      for (uint32_t i = 0; i < d.ready_count(); ++i) {
        auto row = d.ready(i);
        if (row.id == n.id) {
          std::array<uint8_t, 32> hash{};
          if (!c.bush_data->source_hash(d.string(row.script), hash) ||
              hash != row.sha || hash != c.bush_data->script_hash()) {
            e = "Field SceneHost DeadBush roster source differs";
            return false;
          }
        }
      }
      auto prompt = c.prompt_data->record(n.prompt_id);
      if (!prompt || prompt->ready_ordinal >= n.ready_ordinal) {
        e = "Field SceneHost DeadBush child prompt source differs";
        return false;
      }
      // GeometrySpace owns collision ancestors/shapes, not visual Sprite,
      // VisibilityNotifier or the dynamically chosen Roots parent. The actual
      // BushHost resolves these onready identities before its Ready effects.
      for (const auto child : {n.id, n.parent_id, n.body_shape_id,
                               n.hit_shape_id, n.interact_shape_id}) {
        bool found = false;
        for (uint32_t i = 0; i < c.geometry_data->node_count(); ++i)
          if (c.geometry_data->node(i).stable_id == child) {
            found = true;
            break;
          }
        if (!found) {
          e = "Field SceneHost DeadBush collision source node unavailable";
          return false;
        }
      }
    }
  if (c.interact_data) {
    std::array<uint8_t, 32> expected{};
    if (!c.interact_data->source_hash(c.interact_data->script(), expected)) {
      e = "Field SceneHost InteractDialog script receipt absent";
      return false;
    }
    for (const auto &n : c.interact_data->records()) {
      if (!match(n.id, n.ready, n.node, FieldSceneRole::InteractDialog)) {
        e = "Field SceneHost InteractDialog Ready differs";
        return false;
      }
      bool proof = false;
      for (uint32_t i = 0; i < d.ready_count(); ++i) {
        auto row = d.ready(i);
        if (row.id == n.id) {
          proof = d.string(row.script) == c.interact_data->script() &&
                  row.sha == expected;
          break;
        }
      }
      auto child = c.prompt_data->record(n.prompt);
      if (!proof || !child || child->parent_id != n.id ||
          child->ready_ordinal >= n.ready) {
        e = "Field SceneHost InteractDialog script/child prompt source differs";
        return false;
      }
    }
  }
  auto exact_leaf = [&](const auto *pack, uint32_t id, uint32_t ready,
                        std::string_view node, FieldSceneRole role) {
    if (!match(id, ready, node, role))
      return false;
    for (uint32_t i = 0; i < d.ready_count(); ++i) {
      auto row = d.ready(i);
      if (row.id == id) {
        std::array<uint8_t, 32> hash{};
        return pack->source_hash(d.string(row.script), hash) && hash == row.sha;
      }
    }
    return false;
  };
  auto child_prompt = [&](uint32_t child, uint32_t parent, uint32_t ready) {
    const auto *prompt = c.prompt_data ? c.prompt_data->record(child) : nullptr;
    return prompt && prompt->parent_id == parent &&
           prompt->ready_ordinal < ready;
  };
  if (c.present_data)
    for (const auto &n : c.present_data->bindings()) {
      if (!exact_leaf(c.present_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::Present) ||
          !child_prompt(n.prompt_id, n.id, n.ready_ordinal)) {
        e = "Field SceneHost present Ready/script/child identity differs";
        return false;
      }
    }
  if (c.dropped_data)
    for (const auto &n : c.dropped_data->bindings()) {
      if (!exact_leaf(c.dropped_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::DroppedItem) ||
          !child_prompt(n.prompt_id, n.id, n.ready_ordinal)) {
        e = "Field SceneHost dropped Ready/script/child identity differs";
        return false;
      }
    }
  if (c.sparkles_data)
    for (const auto &n : c.sparkles_data->records()) {
      if (!exact_leaf(c.sparkles_data, n.id, n.ready, n.node,
                      FieldSceneRole::Sparkles)) {
        e = "Field SceneHost sparkles Ready/script/child identity differs";
        return false;
      }
    }
  if (c.openable_data)
    for (const auto &n : c.openable_data->records()) {
      if (!exact_leaf(c.openable_data, n.id, n.ready, n.node,
                      FieldSceneRole::OpenableDoor) ||
          !child_prompt(n.children[9], n.id, n.ready)) {
        e = "Field SceneHost openable Ready/script/child identity differs";
        return false;
      }
    }
  if (c.payphone_data)
    for (const auto &n : c.payphone_data->records()) {
      if (!exact_leaf(c.payphone_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::Payphone) ||
          !child_prompt(n.prompt_id, n.id, n.ready_ordinal)) {
        e = "Field SceneHost payphone Ready/script/child identity differs";
        return false;
      }
    }
  if (c.butterfly_data)
    for (const auto &n : c.butterfly_data->bindings()) {
      if (!exact_leaf(c.butterfly_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::Butterfly)) {
        e = "Field SceneHost butterfly Ready/script/child identity differs";
        return false;
      }
    }
  if (c.cutscene_data)
    for (const auto &n : c.cutscene_data->bindings())
      if (!exact_leaf(c.cutscene_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::CutsceneArea)) {
        e = "Field SceneHost cutscene source Ready differs";
        return false;
      }
  if (c.birds_data)
    for (const auto &n : c.birds_data->records())
      if (!exact_leaf(c.birds_data, n.id, n.ready, n.node,
                      FieldSceneRole::Birds)) {
        e = "Field SceneHost birds source Ready differs";
        return false;
      }
  if (c.camera_area_data)
    for (const auto &n : c.camera_area_data->records())
      if (!exact_leaf(c.camera_area_data, n.id, n.ready, n.node,
                      FieldSceneRole::CameraArea)) {
        e = "Field SceneHost camera_area source Ready differs";
        return false;
      }
  if (c.music_data)
    for (const auto &n : c.music_data->bindings()) {
      if (!match(n.id, n.ready_ordinal, n.node, FieldSceneRole::MusicChanger)) {
        e = "Field SceneHost MusicChanger source Ready differs";
        return false;
      }
      bool proof = false;
      for (uint32_t i = 0; i < d.ready_count(); ++i) {
        auto row = d.ready(i);
        if (row.id == n.id) {
          auto script = d.string(row.script);
          auto split = script.find("::");
          std::array<uint8_t, 32> a{}, b{};
          proof = script == c.music_data->script() && split != script.npos &&
                  d.source_hash(script.substr(0, split), a) &&
                  c.music_data->source_hash(script.substr(0, split), b) &&
                  a == b;
          break;
        }
      }
      if (!proof) {
        e = "Field SceneHost MusicChanger embedded source closure differs";
        return false;
      }
    }
  if (c.arrows_data)
    for (const auto &n : c.arrows_data->records())
      if (!exact_leaf(c.arrows_data, n.id, n.ready, n.node,
                      FieldSceneRole::MapArrows)) {
        e = "Field SceneHost arrows source Ready differs";
        return false;
      }
  if (c.stepping_data)
    for (const auto &n : c.stepping_data->bindings())
      if (!exact_leaf(c.stepping_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::SteppingSounds)) {
        e = "Field SceneHost stepping source Ready differs";
        return false;
      }
  if (c.actions_data)
    for (const auto &n : c.actions_data->bindings())
      if (!exact_leaf(c.actions_data, n.id, n.ready_ordinal, n.node,
                      n.kind == 1 ? FieldSceneRole::Reparenter
                                  : FieldSceneRole::EventActivator)) {
        e = "Field SceneHost actions source Ready differs";
        return false;
      }
  if (c.transitions_data)
    for (const auto &n : c.transitions_data->records())
      if (!exact_leaf(c.transitions_data, n.id, n.ready, n.node,
                      n.kind == 1 ? FieldSceneRole::JumpArea
                                  : FieldSceneRole::Stairs)) {
        e = "Field SceneHost transitions source Ready differs";
        return false;
      }
  if (c.camera_data)
    for (const auto &n : c.camera_data->records())
      if (!exact_leaf(c.camera_data, n.id, n.ready, n.node,
                      FieldSceneRole::GameCamera)) {
        e = "Field SceneHost camera source Ready differs";
        return false;
      }
  if (c.door_npc_data)
    for (const auto &n : c.door_npc_data->bindings())
      if (!exact_leaf(c.door_npc_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::DoorNpc)) {
        e = "Field SceneHost door_npc source Ready differs";
        return false;
      }
  if ((c.vending_data || c.vending) &&
      (!c.vending || !c.vending_data || !c.vending_data->valid() ||
       c.vending->data() != c.vending_data || !c.geometry_data ||
       !c.geometry || !c.prompt_data || !c.prompt ||
       c.vending_data->scene_id() != d.identity().scene_id ||
       c.vending_data->source_pin() != d.identity().upstream_commit ||
       c.vending_data->scene_hash() != d.identity().source_sha256)) {
    e = "Field SceneHost Vending actual initialized geometry/prompt/source missing";
    return false;
  }
  if (c.vending_data) {
    const auto &n = c.vending_data->descriptor();
    std::array<uint8_t,32> source{};
    if (!match(n.id,n.ready,n.node,FieldSceneRole::VendingMachine) ||
        !d.source_hash(n.script_source,source) ||
        source != c.vending_data->script_hash() ||
        !c.vending_data->bind_geometry(*c.geometry_data,e)) {
      if (e.empty()) e = "Field SceneHost Vending source roster differs";
      return false;
    }
  }
  if (c.melody_data)
    for (const auto &n : c.melody_data->bindings())
      if (!exact_leaf(c.melody_data, n.id, n.ready_ordinal, n.node,
                      FieldSceneRole::MelodyBackground)) {
        e = "Field SceneHost melody source Ready differs";
        return false;
      }
  if (c.npc_data)
    for (const auto &n : c.npc_data->npcs())
      if (!match(n.id, n.ready_ordinal, n.node, FieldSceneRole::Npc)) {
        e = "Field SceneHost NPC Ready differs";
        return false;
      }
  if (c.enemy_data)
    for (const auto &n : c.enemy_data->spawners())
      if (!match(n.id, n.ready_ordinal, n.node, FieldSceneRole::EnemySpawner)) {
        e = "Field SceneHost enemy Ready differs";
        return false;
      }
  if (c.tint_data)
    for (const auto &n : c.tint_data->records())
      if (n.kind == FieldTintKind::Scene &&
          !match(n.id, n.ready_ordinal, n.node, FieldSceneRole::Tint)) {
        e = "Field SceneHost tint Ready differs";
        return false;
      }
  if (c.sprite_data)
    for (const auto &n : c.sprite_data->records())
      if (!match(n.id, n.ready_ordinal, n.node,
                 n.kind == FieldSpriteKind::Character
                     ? FieldSceneRole::CharacterSprite
                     : FieldSceneRole::SpriteFetcher)) {
        e = "Field SceneHost sprite Ready differs";
        return false;
      }
  if (c.emote_data)
    for (const auto &n : c.emote_data->records()) {
      if (!match(n.id, n.ready_ordinal, n.node, FieldSceneRole::Emotes)) {
        e = "Field SceneHost emotes Ready differs";
        return false;
      }
      FieldSceneReady row{};
      for (uint32_t i = 0; i < d.ready_count(); ++i)
        if (d.ready(i).id == n.id) {
          row = d.ready(i);
          break;
        }
      auto script = d.string(row.script);
      auto split = script.find("::");
      std::array<uint8_t, 32> a{}, b{};
      if (split == script.npos || !d.source_hash(script.substr(0, split), a) ||
          !c.emote_data->source_hash(std::string(script.substr(0, split)), b) ||
          a != b) {
        e = "Field SceneHost emotes embedded source closure differs";
        return false;
      }
      if (n.object_id) {
        auto p = c.sprite_data->record(n.object_id);
        if (!p || p->kind != FieldSpriteKind::Character ||
            p->id != n.parent_id || n.direction_id != p->id ||
            p->node + "/" + std::string(d.string(row.name)) != n.node ||
            p->ready_ordinal <= n.ready_ordinal) {
          e = "Field SceneHost emotes parent sprite source differs";
          return false;
        }
      } else if (n.direction_id || !n.object_path.empty()) {
        e = "Field SceneHost null emotes object differs";
        return false;
      }
    }
  if (c.map)
    for (uint32_t i = 0; i < c.map->gate_count(); ++i) {
      auto g = c.map->gate(i);
      bool found = false;
      for (uint32_t j = 0; j < d.landmark_count(); ++j) {
        auto n = d.landmark(j);
        if (n.id == g.stable_id) {
          found = true;
          if (d.string(n.appear) != c.map->string(g.appear) ||
              d.string(n.disappear) != c.map->string(g.disappear) ||
              n.delete_if_hidden != g.delete_if_hidden) {
            e = "Field SceneHost map gate source differs";
            return false;
          }
        }
      }
      if (!found) {
        e = "Field SceneHost map gate missing";
        return false;
      }
    }
  data_ = &d;
  consumers_ = c;
  host_ = std::move(h);
  cursor_ = 0;
  switches_ = false;
  poisoned_ = false;
  blocked_valid_ = false;
  base_done_ = false;
  gates_.clear();
  for (uint32_t i = 0; i < d.landmark_count(); ++i) {
    auto n = d.landmark(i);
    Gate g;
    g.visible = n.initial_visible;
    gates_.emplace(n.id, g);
  }
  if (!host_.connect_switches(
          d.area().id, [this](bool value) { switches_ = value; }, e)) {
    poisoned_ = true;
    return false;
  }
  e.clear();
  return true;
}
bool FieldSceneHost::read_flag(bool object, std::string_view key, bool &value,
                               std::string &e) const {
  bool present = false;
  if (!host_.read_flag(object, key, present, value, e))
    return false;
  if (!present)
    value = false;
  return true;
}
bool FieldSceneHost::set_flag(bool object, std::string_view key, bool value,
                              bool emit, std::string &e) {
  if (!data_ || poisoned_) {
    e = "Field SceneHost unavailable";
    return false;
  }
  if (!object) {
    if (!data_->normal_flag_exists(key)) {
      e.clear();
      return true;
    }
    bool present = false, old = false;
    if (!host_.read_flag(false, key, present, old, e))
      return false;
    if (!present) {
      e.clear();
      return true;
    }
  }
  if (!host_.write_flag(object, key, value, e))
    return false;
  return !emit || host_.emit_flags(e);
}
FieldMapGateState FieldSceneHost::gate(uint32_t id) const {
  auto n = gates_.find(id);
  if (n == gates_.end() || !n->second.ready)
    return FieldMapGateState::Pending;
  if (n->second.deleted)
    return FieldMapGateState::Deleted;
  return n->second.visible ? FieldMapGateState::Visible
                           : FieldMapGateState::Hidden;
}
bool FieldSceneHost::recheck_landmark(uint32_t id, std::string &e) {
  if (!data_) {
    e = "Field SceneHost unavailable";
    return false;
  }
  auto g = gates_.find(id);
  if (g == gates_.end() || g->second.deleted) {
    e = "Field landmark missing/deleted";
    return false;
  }
  FieldSceneLandmark d;
  bool found = false;
  for (uint32_t i = 0; i < data_->landmark_count(); ++i)
    if (data_->landmark(i).id == id) {
      d = data_->landmark(i);
      found = true;
      break;
    }
  if (!found) {
    e = "Field landmark source missing";
    return false;
  }
  bool shown = true, v = false;
  auto appear = data_->string(d.appear), disappear = data_->string(d.disappear);
  if (!appear.empty()) {
    if (!read_flag(false, appear, v, e))
      return false;
    shown = v;
  }
  if (shown && !disappear.empty()) {
    if (!read_flag(false, disappear, v, e))
      return false;
    shown = shown && !v;
  }
  if (d.delete_if_hidden && !shown) {
    if (!host_.queue_free(id, e))
      return false;
    g->second.queued = true;
  } else {
    if (!host_.visibility(id, shown, e))
      return false;
    g->second.visible = shown;
  }
  return true;
}
bool FieldSceneHost::flag_key(uint32_t id, bool &object, std::string &key,
                              uint32_t &bits, std::string &e) const {
  if (!data_) {
    e = "Field flaggable data missing";
    return false;
  }
  FieldSceneFlaggable f;
  bool found = false;
  for (uint32_t i = 0; i < data_->flaggable_count(); ++i)
    if (data_->flaggable(i).id == id) {
      f = data_->flaggable(i);
      found = true;
      break;
    }
  if (!found) {
    e = "Field flaggable source missing";
    return false;
  }
  bits = f.flags;
  // ItemHolder Ready assigns reset_when_leaving_region=false before any leave.
  for (uint32_t i = 0; i < data_->ready_count(); ++i) {
    auto row = data_->ready(i);
    if (row.id == id && (row.role == FieldSceneRole::Present ||
                         row.role == FieldSceneRole::DroppedItem)) {
      bits &= ~4u;
      break;
    }
  }
  key = std::string(data_->string(f.key));
  object = (bits & 1) != 0;
  if (key.empty()) {
    const FieldSceneData *current = nullptr;
    if (!host_.current_scene(current, e))
      return false;
    if (!current || !current->valid()) {
      e = "Field flaggable current scene unadmitted";
      return false;
    }
    object = true;
    for (uint32_t i = 0; i < data_->ready_count(); ++i) {
      auto n = data_->ready(i);
      if (n.id == id) {
        key = std::string(current->string(current->area().name)) + "/" +
              std::string(data_->string(n.name));
        return true;
      }
    }
    e = "Field flaggable node name missing";
    return false;
  }
  return true;
}
bool FieldSceneHost::flag_status(uint32_t id, bool &value, std::string &e) {
  bool object = false;
  std::string key;
  uint32_t bits = 0;
  if (!flag_key(id, object, key, bits, e))
    return false;
  return read_flag(object, key, value, e);
}
bool FieldSceneHost::set_flag_status(uint32_t id, bool value, std::string &e) {
  bool object = false;
  std::string key;
  uint32_t bits = 0;
  if (!flag_key(id, object, key, bits, e))
    return false;
  return set_flag(object, key, value, (bits & 2) != 0, e);
}
bool FieldSceneHost::leave_area(uint32_t id, bool region_changed,
                                std::string &e) {
  bool object = false;
  std::string key;
  uint32_t bits = 0;
  if (!flag_key(id, object, key, bits, e))
    return false;
  if ((bits & 8) || (region_changed && (bits & 4)))
    return set_flag_status(id, false, e);
  return true;
}
bool FieldSceneHost::area_ready(std::string &e) {
  auto area = data_->area();
  auto region = data_->string(area.region);
  for (uint32_t i = 0; i < data_->visit_count(); ++i) {
    auto kv = data_->visit(i);
    if (kv.first == region) {
      if (!set_flag(false, kv.second, true, true, e))
        return false;
      break;
    }
  }
  bool is_magicant = region == data_->string(area.magicant), flying = false;
  if (is_magicant) {
    bool present = false;
    if (!host_.read_flag(false, data_->string(area.flying_flag), present,
                         flying, e))
      return false;
    if (!present) {
      e = "AreaRoom flyingman flag dictionary incomplete";
      return false;
    }
  }
  if (!is_magicant || flying) {
    bool member = false;
    if (!host_.flyingman_present(member, e))
      return false;
    if (is_magicant != member && !host_.set_flyingman_present(is_magicant, e))
      return false;
  }
  return true;
}
bool FieldSceneHost::map_name(bool only, bool override, std::string &out,
                              std::string &e) const {
  if (!data_) {
    e = "AreaRoom unavailable";
    return false;
  }
  auto a = data_->area();
  std::string_view name = data_->string(a.region);
  bool possessed = false;
  if (!host_.map_possessed(name, possessed, e))
    return false;
  if (!possessed || override) {
    for (uint32_t i = 0; i < data_->override_count(); ++i) {
      auto kv = data_->map_override(i);
      if (kv.first == name) {
        if (!host_.map_possessed(kv.second, possessed, e))
          return false;
        if (possessed)
          name = kv.second;
        break;
      }
    }
  }
  if (only) {
    if (!host_.map_possessed(name, possessed, e))
      return false;
    if (!possessed) {
      out.clear();
      return true;
    }
  }
  out = std::string(name.empty() ? data_->string(a.name) : name);
  return true;
}
bool FieldSceneHost::leave_for(const FieldSceneData &next, std::string &e) {
  if (!data_ || !next.valid()) {
    e = "AreaRoom next source unadmitted";
    return false;
  }
  return host_.emit_area_left(next.string(next.area().region) !=
                                  data_->string(data_->area().region),
                              e);
}
bool FieldSceneHost::bind_geometry(const FieldSceneReady &n, uint32_t family,
                                   uint32_t capability, std::string &e) {
  if (!consumers_.geometry)
    return true;
  for (uint32_t i = 0; i < consumers_.geometry_data->node_count(); ++i) {
    auto g = consumers_.geometry_data->node(i);
    if (g.stable_id == n.id) {
      if (consumers_.geometry_data->string(g.script) !=
              data_->string(n.script) ||
          g.script_sha256 != n.sha) {
        e = "Field SceneHost geometry script source differs";
        return false;
      }
      return consumers_.geometry->bind_script(n.id, n.sha, family, capability,
                                              e);
    }
  }
  return true;
}
bool FieldSceneHost::dispatch(const FieldSceneReady &n, bool &pending,
                              std::string &e) {
  pending = false;
  uint32_t family = 0x454e001c,
           capability = uint32_t(n.role) < 32 ? 1u << uint32_t(n.role) : 0;
  auto unavailable = [&](const char *why) {
    pending = true;
    e = why;
    return false;
  };
  switch (n.role) {
  case FieldSceneRole::OpenableDoor:
    if (!consumers_.openable)
      return unavailable("OpenableDoor actual source consumer missing");
    if (!consumers_.openable->instance(n.id) &&
        !consumers_.openable->create(n.id)) {
      e = consumers_.openable->error();
      return false;
    }
    if (!consumers_.openable->ready(n.id)) {
      e = consumers_.openable->error();
      return false;
    }
    break;
  case FieldSceneRole::Payphone:
    if (!consumers_.payphone)
      return unavailable("Payphone actual source consumer missing");
    if (!consumers_.payphone->instance(n.id) &&
        !consumers_.payphone->create(n.id)) {
      e = consumers_.payphone->error();
      return false;
    }
    if (!consumers_.payphone->ready(n.id)) {
      e = consumers_.payphone->error();
      return false;
    }
    break;
  case FieldSceneRole::Sparkles:
    if (!consumers_.sparkles)
      return unavailable("Sparkles actual source consumer missing");
    if (!consumers_.sparkles->source_constructed(n.id) &&
        !consumers_.sparkles->create(n.id)) {
      e = consumers_.sparkles->error();
      return false;
    }
    if (!consumers_.sparkles->ready(n.id)) {
      e = consumers_.sparkles->error();
      return false;
    }
    break;
  case FieldSceneRole::Present:
    if (!consumers_.present)
      return unavailable("Present actual inventory/programme consumer missing");
    if (!host_.connect_area_left(
            n.id,
            [this, id = n.id](bool region, std::string &error) {
              return leave_area(id, region, error);
            },
            e))
      return false;
    if (!consumers_.present->ready_present(n.id, e))
      return false;
    break;
  case FieldSceneRole::DroppedItem:
    if (!consumers_.dropped)
      return unavailable(
          "DroppedItem actual inventory/programme consumer missing");
    if (!host_.connect_area_left(
            n.id,
            [this, id = n.id](bool region, std::string &error) {
              return leave_area(id, region, error);
            },
            e))
      return false;
    if (!consumers_.dropped->ready_item(n.id, e))
      return false;
    break;
  case FieldSceneRole::Butterfly:
    if (!consumers_.butterfly || !consumers_.random)
      return unavailable("Butterfly actual scene/shared RNG consumer missing");
    if (!consumers_.butterfly->ready(n.id, *consumers_.random, e))
      return false;
    break;
  case FieldSceneRole::GameCamera:
    if (!consumers_.camera || !consumers_.camera->state(n.id))
      return unavailable(
          "GameCamera native descendants not instantiated before Ready");
    if (!consumers_.camera->ready(n.id)) {
      e = consumers_.camera->error();
      return false;
    }
    family = 0x454e0031;
    capability = 3;
    break;
  case FieldSceneRole::VendingMachine:
    if (!consumers_.vending || !consumers_.vending_data ||
        consumers_.vending->data() != consumers_.vending_data)
      return unavailable("Vending actual typed interaction host missing");
    // The original Vending leaf defines interact/_end, no _ready. Its typed
    // runtime was already bound to actual Shop, Player, Area and Prompt.
    family = 0x454e003b;
    capability = 1;
    break;
  case FieldSceneRole::MelodyBackground:
    if (!consumers_.melody)
      return unavailable("MelodyBG actual actor/tree/tween host missing");
    if (!consumers_.melody->ready(n.id, e))
      return false;
    family = 0x454e003a;
    capability = 1;
    break;
  case FieldSceneRole::DoorNpc:
    if (!consumers_.door_npc)
      return unavailable("DoorNPC actual Room/audio/timer host missing");
    if (!consumers_.door_npc->ready(n.id, e))
      return false;
    family = 0x454e0038;
    capability = 1;
    break;
  case FieldSceneRole::MapArrows:
    if (!consumers_.arrows)
      return unavailable("Actual arrows source host missing");
    if (!consumers_.arrows->root_state(n.id))
      return unavailable(
          "MapArrows source native children/signals missing before Ready");
    if (!consumers_.arrows->ready(n.id)) {
      e = consumers_.arrows->error();
      return false;
    }
    family = 0x454e0030;
    capability = 3;
    break;
  case FieldSceneRole::Reparenter:
  case FieldSceneRole::EventActivator:
    if (!consumers_.actions)
      return unavailable("Actual actions source host missing");
    if (!consumers_.actions->ready(n.id, e))
      return false;
    family = 0x454e0036;
    capability = 1;
    break;
  case FieldSceneRole::SteppingSounds:
    if (!consumers_.stepping)
      return unavailable("Actual stepping source host missing");
    if (!consumers_.stepping->ready(n.id, e))
      return false;
    family = 0x454e0037;
    capability = 1;
    break;
  case FieldSceneRole::JumpArea:
  case FieldSceneRole::Stairs:
    if (!consumers_.transitions)
      return unavailable("Actual transitions source host missing");
    if (!consumers_.transitions->ready(n.id)) {
      e = consumers_.transitions->error();
      return false;
    }
    family = 0x454e0035;
    capability = 1;
    break;
  case FieldSceneRole::CutsceneArea:
    if (!consumers_.cutscene)
      return unavailable("CutsceneArea actual source host missing");
    if (!consumers_.cutscene->ready(n.id, e))
      return false;
    break;
  case FieldSceneRole::MusicChanger:
    if (!consumers_.music)
      return unavailable("MusicChanger actual source host missing");
    if (!consumers_.music->ready(n.id, e))
      return false;
    break;
  case FieldSceneRole::Birds:
    if (!consumers_.birds || !consumers_.birds->state(n.id))
      return unavailable(
          "Birds source instantiation/signals not bound before Ready");
    if (!consumers_.birds->ready(n.id)) {
      e = consumers_.birds->error();
      return false;
    }
    break;
  case FieldSceneRole::CameraArea:
    if (!consumers_.camera_area || !consumers_.camera_area->state(n.id))
      return unavailable(
          "CameraArea source instantiation/signals not bound before Ready");
    if (!consumers_.camera_area->ready(n.id)) {
      e = consumers_.camera_area->error();
      return false;
    }
    break;
  case FieldSceneRole::InteractDialog:
    if (!consumers_.interact)
      return unavailable(
          "InteractDialog actual programme/flag/prompt host missing");
    if (!consumers_.interact->ready(n.id)) {
      e = consumers_.interact->error();
      return false;
    }
    break;
  case FieldSceneRole::DeadBush:
    if (!consumers_.bush)
      return unavailable(
          "DeadBush actual animation/roots/prompt/deferred consumer missing");
    if (!consumers_.bush->instance(n.id) && !consumers_.bush->create(n.id)) {
      e = consumers_.bush->error();
      return false;
    }
    if (!consumers_.bush->ready(n.id)) {
      e = consumers_.bush->error();
      return false;
    }
    break;
  case FieldSceneRole::Door:
    if (!consumers_.door)
      return unavailable("Door actual lifecycle/transition consumer missing");
    if (!consumers_.door->ready(n.id, e))
      return false;
    family = 0x454e001f;
    capability = 3;
    break;
  case FieldSceneRole::ButtonPrompt:
    if (!consumers_.prompt)
      return unavailable(
          "ButtonPrompt actual signal/animation consumer missing");
    if (!consumers_.prompt->instance(n.id) &&
        !consumers_.prompt->create(n.id)) {
      e = consumers_.prompt->error();
      return false;
    }
    if (!consumers_.prompt->ready(n.id)) {
      e = consumers_.prompt->error();
      return false;
    }
    break;
  case FieldSceneRole::DandelionSpawner:
    if (!consumers_.dandelion)
      return unavailable("Dandelion actual factory consumer missing");
    if (!consumers_.dandelion->ready(n.id, e))
      return false;
    family = 0x454e001d;
    capability = 3;
    break;
  case FieldSceneRole::Grass:
    if (!consumers_.grass)
      return unavailable("Grass actual consumer missing");
    if (!consumers_.grass->execute_grass_ready(n.ordinal, *consumers_.random,
                                               e))
      return false;
    family = 0x454e0017;
    break;
  case FieldSceneRole::Npc:
    if (!consumers_.npc)
      return unavailable("NPC actual consumer missing");
    if (!consumers_.npc->ready(n.id)) {
      e = consumers_.npc->error();
      return false;
    }
    break;
  case FieldSceneRole::EnemySpawner:
    if (!consumers_.enemy)
      return unavailable("Enemy actual consumer missing");
    if (!consumers_.enemy->ready(n.id)) {
      e = consumers_.enemy->error();
      return false;
    }
    break;
  case FieldSceneRole::Tint:
    if (!consumers_.tint)
      return unavailable("Tint actual consumer missing");
    if (!consumers_.tint->instance(n.id) &&
        !consumers_.tint->create(n.id, n.id)) {
      e = consumers_.tint->error();
      return false;
    }
    if (!consumers_.tint->ready(n.id)) {
      e = consumers_.tint->error();
      return false;
    }
    break;
  case FieldSceneRole::CharacterSprite:
  case FieldSceneRole::SpriteFetcher:
    if (!consumers_.sprite)
      return unavailable("Sprite actual consumer missing");
    if (!consumers_.sprite->instance(n.id) &&
        !consumers_.sprite->create(n.id)) {
      e = consumers_.sprite->error();
      return false;
    }
    if (!consumers_.sprite->ready(n.id)) {
      e = consumers_.sprite->error();
      return false;
    }
    break;
  case FieldSceneRole::Emotes:
    if (!consumers_.emote)
      return unavailable("Emotes actual consumer missing");
    if (!consumers_.emote->instance(n.id) && !consumers_.emote->create(n.id)) {
      e = consumers_.emote->error();
      return false;
    }
    if (!consumers_.emote->ready(n.id)) {
      e = consumers_.emote->error();
      return false;
    }
    break;
  case FieldSceneRole::FlagLandmark: {
    auto &g = gates_.at(n.id);
    if (!recheck_landmark(n.id, e))
      return false;
    if (!host_.connect_flags(
            n.id,
            [this, id = n.id](std::string &error) {
              return recheck_landmark(id, error);
            },
            e))
      return false;
    g.ready = true;
    family = 0x454e001c;
    break;
  }
  case FieldSceneRole::FlaggableDerived:
    if (!base_done_) {
      if (!host_.connect_area_left(
              n.id,
              [this, id = n.id](bool region, std::string &error) {
                return leave_area(id, region, error);
              },
              e))
        return false;
      base_done_ = true;
    }
    return unavailable("FlaggableObject base Ready complete; derived "
                       "ItemHolder/Present/DroppedItem remains pending");
  case FieldSceneRole::AreaRoom:
    if (!area_ready(e))
      return false;
    family = 0x454e001c;
    break;
  case FieldSceneRole::DebugStart: {
    bool debug = false, area = false, paused = false;
    if (!host_.debug_context(debug, area, paused, e))
      return false;
    if (debug && area && !paused &&
        !host_.teleport_player(data_->debug(n.profile).position, e))
      return false;
    family = 0x454e001c;
    break;
  }
  default:
    return unavailable("Unimplemented source script lifecycle");
  }
  if (!bind_geometry(n, family, capability, e))
    return false;
  admissions_.emplace(
      n.id, FieldSceneScriptAdmission{n.id, family, capability, n.sha});
  return true;
}
bool FieldSceneHost::construct_camera_arrows(uint32_t id, std::string &e) {
  if (!data_ || poisoned_ || !consumers_.arrows || !consumers_.arrows_data ||
      consumers_.arrows->data() != consumers_.arrows_data ||
      !consumers_.arrows_data->record(id) ||
      consumers_.arrows->root_state(id)) {
    e = "MapArrows actual source constructor owner rejected";
    return false;
  }
  bool found = false;
  for (uint32_t i = 0; i < data_->ready_count(); ++i) {
    const auto row = data_->ready(i);
    if (row.id == id && row.role == FieldSceneRole::MapArrows &&
        row.sha == consumers_.arrows_data->script_sha()) {
      found = true;
      break;
    }
  }
  if (!found) {
    e = "MapArrows constructor is outside actual source roster";
    return false;
  }
  if (!consumers_.arrows->create_source_constructor(id)) {
    e = consumers_.arrows->error();
    return false;
  }
  return true;
}
bool FieldSceneHost::ready_next(std::string &e) {
  if (!data_ || poisoned_) {
    e = "Field SceneHost unavailable or startup callback failed";
    return false;
  }
  if (cursor_ >= data_->ready_count()) {
    e = "Field SceneHost Ready exhausted";
    return false;
  }
  auto n = data_->ready(cursor_);
  bool pending = false;
  if (!dispatch(n, pending, e)) {
    blocked_ = n;
    blocked_valid_ = true;
    if (!pending)
      poisoned_ = true;
    e += " [" + std::string(data_->string(n.node)) + "; " +
         std::string(data_->string(n.script)) + "]";
    return false;
  }
  ++cursor_;
  base_done_ = false;
  blocked_valid_ = false;
  e.clear();
  return true;
}
bool FieldSceneHost::ready_to_boundary(std::string &e) {
  if (!data_) {
    e = "Field SceneHost unavailable";
    return false;
  }
  while (cursor_ < data_->ready_count())
    if (!ready_next(e))
      return false;
  return scene_ready();
}
bool FieldSceneHost::scene_ready() const {
  return data_ && !poisoned_ && cursor_ == data_->ready_count();
}
bool FieldSceneHost::script_admission(uint32_t id,
                                      FieldSceneScriptAdmission &out) const {
  auto n = admissions_.find(id);
  if (n == admissions_.end())
    return false;
  out = n->second;
  return true;
}
bool FieldSceneHost::commit_deleted(uint32_t id, std::string &e) {
  auto g = gates_.find(id);
  if (g == gates_.end() || !g->second.queued || g->second.deleted) {
    e = "Field landmark deferred delete not queued";
    return false;
  }
  if (consumers_.geometry) {
    FieldGeometryNodeUpdate n;
    n.stable_id = id;
    n.fields = 2;
    n.deleted = true;
    bool found = false;
    for (uint32_t i = 0; i < consumers_.geometry_data->node_count(); ++i)
      if (consumers_.geometry_data->node(i).stable_id == id) {
        found = true;
        break;
      }
    if (found && !consumers_.geometry->apply_updates({n}, e))
      return false;
  }
  g->second.deleted = true;
  e.clear();
  return true;
}
bool FieldSceneHost::emote_object(const FieldEmoteDescriptor &n, bool &exists,
                                  uint32_t &id, std::string &e) const {
  if (!data_ || poisoned_ || !consumers_.emote_data ||
      consumers_.emote_data->record(n.id) != &n) {
    e = "Field emotes descriptor not bound";
    return false;
  }
  exists = n.object_id != 0;
  id = n.object_id;
  e.clear();
  return true;
}
bool FieldSceneHost::emote_direction_source(const FieldEmoteDescriptor &n,
                                            uint32_t &id,
                                            std::string &e) const {
  bool exists = false;
  uint32_t object = 0;
  if (!emote_object(n, exists, object, e))
    return false;
  if (!exists || !n.direction_id ||
      !consumers_.sprite_data->record(n.direction_id)) {
    e = "Field emotes direction ancestor unbound";
    return false;
  }
  id = n.direction_id;
  e.clear();
  return true;
}
bool FieldSceneHost::emote_texture_geometry(uint32_t id, uint32_t &height,
                                            uint32_t &rows,
                                            std::string &e) const {
  if (!data_ || poisoned_ || !consumers_.sprite || !consumers_.sprite_data) {
    e = "Field emotes sprite unavailable";
    return false;
  }
  auto s = consumers_.sprite->instance(id);
  auto d = consumers_.sprite_data->record(id);
  auto t = s ? consumers_.sprite_data->texture(s->texture_id) : nullptr;
  if (!s || !d || d->kind != FieldSpriteKind::Character || !t || !s->rows) {
    e = "Field emotes live sprite texture unbound";
    return false;
  }
  height = t->height;
  rows = s->rows;
  e.clear();
  return true;
}
bool FieldSceneHost::emote_direction(uint32_t id, Vec2 &direction,
                                     std::string &e) const {
  if (!data_ || poisoned_ || !consumers_.sprite) {
    e = "Field emotes direction unavailable";
    return false;
  }
  auto s = consumers_.sprite->instance(id);
  if (!s || !s->ready) {
    e = "Field emotes direction sprite not Ready";
    return false;
  }
  direction = s->direction;
  e.clear();
  return true;
}
bool FieldSceneHost::emote_sprite_changed(uint32_t id, std::string &e) {
  if (!data_ || poisoned_ || !consumers_.sprite_data ||
      !consumers_.emote_data || !consumers_.emote) {
    e = "Field emotes signal consumer unavailable";
    return false;
  }
  if (!consumers_.sprite_data->record(id)) {
    e = "Field emotes signal source unbound";
    return false;
  }
  bool receiver = false;
  for (const auto &n : consumers_.emote_data->records())
    if (n.object_id == id)
      receiver = true;
  if (!receiver) {
    e.clear();
    return true;
  }
  if (!consumers_.emote->sprite_changed(id)) {
    e = consumers_.emote->error();
    return false;
  }
  e.clear();
  return true;
}
bool FieldSceneHost::idle_emotes(float dt, std::string &e) {
  if (!data_ || poisoned_ || !consumers_.emote || !consumers_.emote_data ||
      !std::isfinite(dt) || dt < 0 || dt > 60) {
    e = "Field emotes idle binding/delta rejected";
    return false;
  }
  for (const auto &n : consumers_.emote_data->records()) {
    auto s = consumers_.emote->instance(n.id);
    if (s && s->ready && !consumers_.emote->idle_frame(n.id, dt)) {
      e = consumers_.emote->error();
      return false;
    }
  }
  e.clear();
  return true;
}
bool FieldSceneHost::idle_dead_bushes(float dt, std::string &e) {
  if (!data_ || poisoned_ || !consumers_.bush || !consumers_.bush_data ||
      !std::isfinite(dt) || dt < 0 || dt > 60) {
    e = "Field DeadBush idle binding/delta rejected";
    return false;
  }
  for (const auto &n : consumers_.bush_data->records()) {
    auto state = consumers_.bush->instance(n.id);
    if (state && state->ready && !state->deleted &&
        !consumers_.bush->idle_frame(n.id, dt)) {
      e = consumers_.bush->error();
      return false;
    }
  }
  e.clear();
  return true;
}

} // namespace encore::upstream
