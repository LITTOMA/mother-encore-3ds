#include "podunk_scene_consumers.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}

} // namespace
struct PodunkSceneConsumers::State {
  PodunkSceneConsumerInput input;
  PodunkSceneMechanismOwners owners;
  FieldSceneConsumers consumers;
  FieldRuntime grass;
  FieldNpcRuntime npc;
  FieldEnemyRuntime enemy;
  FieldTintRuntime tint;
  FieldSpriteRuntime sprite;
  FieldEmoteRuntime emote;
  FieldDandelionRuntime dandelion;
  FieldDoorRuntime door;
  FieldPromptRuntime prompt;
  FieldBushRuntime bush;
  FieldInteractRuntime interact;
  FieldPresentRuntime present;
  FieldDroppedRuntime dropped;
  FieldSparklesRuntime sparkles;
  FieldOpenableDoorRuntime openable;
  FieldPayphoneRuntime payphone;
  FieldButterflyRuntime butterfly;
  FieldCutsceneAreaRuntime cutscene;
  FieldBirdRuntime birds;
  FieldCameraAreaRuntime camera_area;
  FieldMusicChangerRuntime music;
  FieldCameraArrowsRuntime arrows;
  FieldSceneActionsRuntime actions;
  FieldSteppingSoundsRuntime stepping;
  FieldPlayerTransitionsRuntime transitions;
  FieldGameCameraRuntime camera;
  FieldDoorNpcRuntime door_npc;
  FieldMelodyBackgroundRuntime melody;
  FieldVendingRuntime vending;
  PodunkSceneScripts *scripts = nullptr;
  std::vector<std::string> failures;
  bool attempted = false, initialized = false;
  bool source(uint32_t stable, FieldObjectId &out, std::string &e) const {
    auto id = input.tree->source_object(stable);
    auto node = id ? input.tree->state(id) : nullptr;
    auto descriptor = id ? input.tree->descriptor(id) : nullptr;
    FieldIdentity identity;
    if (!node || !descriptor || !node->alive || node->queued ||
        descriptor->id != stable ||
        input.continuation->registry()->tree_owner(id).get() != input.tree ||
        !input.tree->object_identity(id, identity) ||
        identity.upstream_commit !=
            input.sources->tree().identity().upstream_commit ||
        identity.scene_id != input.sources->tree().identity().scene_id ||
        identity.source_sha256 !=
            input.sources->tree().identity().source_sha256)
      return fail(
          e, "Actual scene source node not constructed/alive in same ObjectDB");
    out = id;
    e.clear();
    return true;
  }
  bool visible(uint32_t stable, bool value, std::string &e) {
    FieldObjectId id = 0;
    return source(stable, id, e) && input.tree->set_visible(id, value, e);
  }
  bool queued(uint32_t stable, std::string &e) {
    FieldObjectId id = 0;
    return source(stable, id, e) && input.tree->queue_free(id, e);
  }
  bool position(uint32_t stable, Vec2 value, std::string &e) {
    FieldObjectId id = 0;
    if (!source(stable, id, e))
      return false;
    auto transform = input.tree->state(id)->local;
    transform[2] = value;
    return input.tree->set_local(id, transform, e);
  }
  bool read_flag(bool object, std::string_view name, bool &present, bool &value,
                 std::string &e) const {
    return input.continuation->characters()->flags().read(object, name, present,
                                                          value, e);
  }
  bool flag(std::string_view name, bool &value, std::string &e) const {
    bool present = false;
    return read_flag(false, name, present, value, e);
  }
  bool body(uint64_t value, bool &yes, std::string &e) const {
    const auto id = input.player->body().object();
    if (!id || !input.player->body().constructed() ||
        input.player->registry() != input.continuation->registry() ||
        input.player->tree() != input.tree)
      return fail(e, "Actual Player source body not constructed in scene");
    yes = value == id;
    e.clear();
    return true;
  }
  bool player_position(Vec2 &out, std::string &e) const {
    bool yes = false;
    if (!body(input.player->body().object(), yes, e))
      return false;
    FieldTransform world;
    if (!input.tree->world_transform(input.player->body().object(), world, e))
      return false;
    out = world[2];
    return true;
  }

  bool current_scene(std::string &e) const {
    FieldObjectId actual = 0;
    if (!input.continuation->global()->core().object(
            FieldGlobalMemberRole::CurrentScene, actual, e))
      return false;
    if (!actual || actual != input.tree->root() ||
        input.continuation->registry()->tree_owner(actual).get() != input.tree)
      return fail(
          e, "Actual global currentScene not same destination source tree");
    e.clear();
    return true;
  }
  bool current_camera(uint32_t &stable, std::string &e) const {
    FieldObjectId actual = 0;
    if (!input.continuation->global()->core().object(
            FieldGlobalMemberRole::CurrentCamera, actual, e))
      return false;
    const auto *descriptor = input.tree->descriptor(actual);
    if (!actual || !descriptor ||
        input.continuation->registry()->tree_owner(actual).get() !=
            input.tree ||
        !input.sources->camera().record(descriptor->id) ||
        !camera.state(descriptor->id) || !camera.state(descriptor->id)->ready)
      return fail(e,
                  "Actual global currentCamera has no same-scene Camera owner");
    stable = descriptor->id;
    e.clear();
    return true;
  }
  template <class Init> void initialize_one(const char *label, Init init) {
    std::string e;
    if (!init(e))
      failures.push_back(std::string(label) + ": " + e);
  }
};
PodunkSceneConsumers::PodunkSceneConsumers() = default;
PodunkSceneConsumers::~PodunkSceneConsumers() = default;

bool PodunkSceneConsumers::prepare(PodunkSceneConsumerInput input,
                                   std::string &e) {
  if (state_)
    return fail(e, "Scene consumer preparation is one-shot");
  if (!input.sources || !input.sources->valid() || !input.continuation ||
      !input.continuation->initialized() || !input.tree || !input.native ||
      !input.player || !input.animated || !input.map || !input.geometry ||
      !input.shop || !input.scene_epoch)
    return fail(e, "Scene consumers actual source/owner inputs incomplete");
  state_ = std::make_unique<State>();
  state_->input = input;
  if (!input.sources->bind_sources(state_->consumers, e))
    return false;
  auto &s = *state_;
  s.consumers.random = input.continuation->random();
  s.consumers.grass = &s.grass;
  s.consumers.map_space = input.map;
  s.consumers.geometry = input.geometry;
  s.consumers.npc = &s.npc;
  s.consumers.enemy = &s.enemy;
  s.consumers.tint = &s.tint;
  s.consumers.sprite = &s.sprite;
  s.consumers.emote = &s.emote;
  s.consumers.dandelion = &s.dandelion;
  s.consumers.door = &s.door;
  s.consumers.prompt = &s.prompt;
  s.consumers.bush = &s.bush;
  s.consumers.interact = &s.interact;
  s.consumers.present = &s.present;
  s.consumers.dropped = &s.dropped;
  s.consumers.sparkles = &s.sparkles;
  s.consumers.openable = &s.openable;
  s.consumers.payphone = &s.payphone;
  s.consumers.butterfly = &s.butterfly;
  s.consumers.cutscene = &s.cutscene;
  s.consumers.birds = &s.birds;
  s.consumers.camera_area = &s.camera_area;
  s.consumers.music = &s.music;
  s.consumers.arrows = &s.arrows;
  s.consumers.actions = &s.actions;
  s.consumers.stepping = &s.stepping;
  s.consumers.transitions = &s.transitions;
  s.consumers.camera = &s.camera;
  s.consumers.door_npc = &s.door_npc;
  s.consumers.melody = &s.melody;
  s.consumers.vending = &s.vending;
  e.clear();
  return true;
}
FieldSceneConsumers PodunkSceneConsumers::runtime_instances() const {
  return state_ ? state_->consumers : FieldSceneConsumers{};
}
bool PodunkSceneConsumers::initialize(PodunkSceneConsumerInput input,
                                      PodunkSceneMechanismOwners owners,
                                      std::string &e) {
  return prepare(input, e) && initialize_owners(std::move(owners), e);
}
bool PodunkSceneConsumers::initialize_owners(PodunkSceneMechanismOwners owners,
                                             std::string &e) {
  if (!state_ || state_->attempted)
    return fail(
        e, "Scene consumer owner initialization missing/duplicate prepare");
  auto &s = *state_;
  s.attempted = true;
  auto input = s.input;
  s.owners = std::move(owners);
  auto &o = s.owners;
  auto &d = *input.sources;
  // These closures borrow actual owners. Child/ObjectID lookup occurs only when
  // the typed source consumer executes that original source operation.
  o.scene.read_flag = [&s](bool object, std::string_view key, bool &p, bool &v,
                           std::string &e) {
    return s.read_flag(object, key, p, v, e);
  };
  o.scene.write_flag = [&s](bool object, std::string_view key, bool value,
                            std::string &e) {
    return s.input.continuation->characters()->flags().write(object, key, value,
                                                             e);
  };
  o.scene.emit_flags = [&s](std::string &e) {
    return s.input.continuation->characters()->flags().emit(e);
  };
  o.scene.visibility = [&s](uint32_t id, bool v, std::string &e) {
    return s.visible(id, v, e);
  };
  o.scene.queue_free = [&s](uint32_t id, std::string &e) {
    return s.queued(id, e);
  };
  o.scene.current_scene = [&s](const FieldSceneData *&out, std::string &e) {
    if (!s.scripts || !s.current_scene(e))
      return false;
    out = &s.input.sources->lifecycle();
    e.clear();
    return true;
  };
  o.tint.resolve = [&s](uint32_t instance, const FieldTintDescriptor &d,
                        const FieldTintTarget &target, bool &exists,
                        uint32_t &result, std::string &e) {
    FieldObjectId actual = 0;
    if (!s.source(instance, actual, e))
      return false;
    if (d.kind != FieldTintKind::Scene || d.id != instance)
      return fail(e, "Dynamic Tint requires its actual instance factory");
    FieldObjectId resolved = 0;
    if (!s.input.tree->get_node(actual, target.node_path, resolved, e))
      return false;
    if (!resolved) {
      if (target.exists)
        return fail(e, "Source Tint target unexpectedly absent");
      exists = false;
      result = 0;
      e.clear();
      return true;
    }
    const auto *descriptor = s.input.tree->descriptor(resolved);
    if (!target.exists || !descriptor || descriptor->id != target.source_id ||
        s.input.continuation->registry()->tree_owner(resolved).get() !=
            s.input.tree)
      return fail(e, "Source Tint resolved child identity differs");
    exists = true;
    result = descriptor->id;
    e.clear();
    return true;
  };
  o.tint.self_modulate = [&s](uint32_t id, FieldTintColor c, std::string &e) {
    FieldObjectId actual = 0;
    return s.source(id, actual, e) &&
           s.input.tree->set_modulate(actual, c, true, e);
  };

  // Native AnimationTree advances the one NPC clip; its original frame setter
  // addresses that CharacterSprite's same native body before notifying peers.
  if (o.npc.present) {
    auto inherited = o.npc.present;
    o.npc.present = [&s, inherited](uint32_t id, FieldNpcPresentation operation,
                                    const FieldNpcDescriptor &d,
                                    const FieldNpcInstance &pose,
                                    std::string &e) {
      if (operation != FieldNpcPresentation::Frame)
        return inherited(id, operation, d, pose, e);
      uint32_t child = 0;
      for (const auto &descriptor : s.input.sources->sprite().records())
        if (descriptor.parent_id == id &&
            descriptor.kind == FieldSpriteKind::Character) {
          if (child)
            return fail(e, "NPC actual CharacterSprite mapping ambiguous");
          child = descriptor.id;
        }
      FieldObjectId actual = 0;
      if (!child || pose.id != id || !s.source(child, actual, e))
        return fail(e, "NPC actual CharacterSprite absent");
      const auto *body = s.sprite.instance(child);
      if (!body || !body->ready)
        return fail(
            e, "NPC CharacterSprite source setter before actual child Ready");
      if (!s.input.native->sprite_set_frame(actual, pose.frame, e))
        return false;
      if (!s.sprite.frame(child, pose.frame)) {
        e = s.sprite.error();
        return false;
      }
      e.clear();
      return true;
    };
  }
  o.npc.flag = [&s](std::string_view key, bool &v, std::string &e) {
    return s.flag(key, v, e);
  };
  o.sprite.current_scene = [&s](const FieldSpriteData *&out, std::string &e) {
    if (!s.scripts || !s.current_scene(e))
      return false;
    out = &s.input.sources->sprite();
    e.clear();
    return true;
  };
  o.sprite.resolve_sprite = [&s](uint32_t id, const FieldSpriteDescriptor &d,
                                 bool &found, uint32_t &out, std::string &e) {
    FieldObjectId actual = 0, target = 0;
    if (d.id != id || !s.source(id, actual, e) ||
        !s.input.tree->get_node(actual, d.target_path, target, e))
      return false;
    if (!target) {
      if (d.target_id)
        return fail(e, "Fetcher checked target absent");
      found = false;
      out = 0;
      e.clear();
      return true;
    }
    const auto *desc = s.input.tree->descriptor(target);
    if (!desc || desc->id != d.target_id)
      return fail(e, "Fetcher actual target source differs");
    FieldCanvasAppearance native;
    if (!s.input.native->sprite_snapshot(target, native, e))
      return false;
    found = true;
    out = desc->id;
    e.clear();
    return true;
  };
  o.sprite.sample = [&s](uint32_t stable, FieldSpriteSample &out,
                         std::string &e) {
    FieldObjectId actual = 0;
    FieldCanvasAppearance native;
    if (!s.source(stable, actual, e) ||
        !s.input.native->sprite_snapshot(actual, native, e))
      return false;
    uint32_t source_texture = 0;
    if (native.texture) {
      const auto *asset =
          s.input.native->canvas_data()->texture(native.texture);
      if (!asset)
        return fail(e, "Sprite actual GPU texture absent");
      for (const auto &r : s.input.sources->sprite().records()) {
        const auto *t = s.input.sources->sprite().texture(r.texture);
        if (t && t->source == asset->source) {
          if (source_texture && source_texture != t->id)
            return fail(e, "Sprite texture source mapping ambiguous");
          source_texture = t->id;
        }
      }
      if (!source_texture)
        return fail(e, "Sprite texture outside checked Sprite definitions");
    }
    const auto *node = s.input.tree->state(actual);
    if (!node)
      return fail(e, "Sprite actual CanvasItem state absent");
    out = {source_texture, native.hframes, native.vframes, native.frame,
           (node->flags & 2) != 0};
    e.clear();
    return true;
  };
  o.sprite.publish = [&s](uint32_t id, const FieldSpriteInstance &pose,
                          std::string &e) {
    FieldObjectId actual = 0;
    FieldCanvasAppearance native;
    if (id != pose.id || !s.source(id, actual, e) ||
        !s.input.native->sprite_snapshot(actual, native, e))
      return false;
    uint32_t texture = 0;
    if (pose.texture_id) {
      const auto *t = s.input.sources->sprite().texture(pose.texture_id);
      if (!t)
        return fail(e, "CharacterSprite unknown source texture");
      for (const auto &asset : s.input.native->canvas_data()->textures())
        if (asset.source == t->source) {
          if (texture || asset.width != t->width || asset.height != t->height)
            return fail(e, "CharacterSprite GPU source mapping "
                           "extent/uniqueness rejected");
          texture = asset.id;
        }
      if (!texture)
        return fail(e, "CharacterSprite actual source GPU texture not loaded");
    }
    native.texture = texture;
    native.hframes = pose.columns;
    native.vframes = pose.rows;
    native.frame = pose.frame;
    native.offset = pose.offset;
    return s.input.native->sprite_publish(actual, native, e) &&
           s.input.tree->set_visible(actual, pose.visible, e);
  };
  o.sprite.sprite_changed = [&s](uint32_t id, const FieldSpriteDescriptor &,
                                 const FieldSpriteInstance &, std::string &e) {
    if (!s.scripts)
      return fail(e, "Source sprite_changed scene lifecycle absent");
    return s.scripts->lifecycle().emote_sprite_changed(id, e);
  };
  o.emote.resolve_object = [&s](const FieldEmoteDescriptor &d, bool &found,
                                uint32_t &id, std::string &e) {
    if (!s.scripts)
      return fail(e, "Source Emotes lifecycle absent");
    return s.scripts->lifecycle().emote_object(d, found, id, e);
  };
  o.emote.resolve_direction = [&s](const FieldEmoteDescriptor &d, uint32_t &id,
                                   std::string &e) {
    if (!s.scripts)
      return fail(e, "Source Emotes lifecycle absent");
    return s.scripts->lifecycle().emote_direction_source(d, id, e);
  };
  o.emote.texture_geometry = [&s](uint32_t id, uint32_t &rows, uint32_t &height,
                                  std::string &e) {
    if (!s.scripts)
      return fail(e, "Source Emotes lifecycle absent");
    return s.scripts->lifecycle().emote_texture_geometry(id, rows, height, e);
  };
  o.emote.direction = [&s](uint32_t id, Vec2 &v, std::string &e) {
    if (!s.scripts)
      return fail(e, "Source Emotes lifecycle absent");
    return s.scripts->lifecycle().emote_direction(id, v, e);
  };
  o.dandelion.queue_source_sprite = [&s](uint32_t id, std::string &e) {
    return s.queued(id, e);
  };
  o.prompt.visibility = [&s](uint32_t id, bool v, std::string &e) {
    return s.visible(id, v, e);
  };
  o.bush.read_flag = [&s](std::string_view key, bool &p, bool &v,
                          std::string &e) {
    return s.read_flag(false, key, p, v, e);
  };
  o.interact.read_flag = [&s](std::string_view key, bool &v, std::string &e) {
    return s.flag(key, v, e);
  };
  o.bush.queue_free = [&s](uint32_t id, std::string &e) {
    return s.queued(id, e);
  };
  o.interact.visible = [&s](uint32_t id, bool v, std::string &e) {
    return s.visible(id, v, e);
  };
  o.interact.queue_free = [&s](uint32_t id, std::string &e) {
    return s.queued(id, e);
  };
  o.melody.root_object = [&s](uint32_t id, FieldMelodyObject &out,
                              std::string &e) { return s.source(id, out, e); };
  o.melody.local_position = [&s](uint32_t id, Vec2 &out, std::string &e) {
    FieldObjectId actual = 0;
    if (!s.source(id, actual, e))
      return false;
    out = s.input.tree->state(actual)->local[2];
    e.clear();
    return true;
  };
  o.melody.set_visible = [&s](uint32_t id, bool value, std::string &e) {
    return s.visible(id, value, e);
  };
  o.melody.write_color = [&s](uint32_t id, FieldMelodyColorRole role,
                              BattleValue c, std::string &e) {
    FieldObjectId actual = 0;
    if (!s.source(id, actual, e))
      return false;
    if (role != FieldMelodyColorRole::Modulate &&
        role != FieldMelodyColorRole::SelfModulate)
      return fail(e, "Melody unknown source CanvasItem color role");
    return s.input.tree->set_modulate(
        actual, FieldColor{c.x, c.y, c.z, c.w},
        role == FieldMelodyColorRole::SelfModulate, e);
  };
  o.melody.set_global_position = [&s](uint32_t id, Vec2 value, std::string &e) {
    FieldObjectId actual = 0;
    FieldTransform transform;
    if (!s.source(id, actual, e) ||
        !s.input.tree->world_transform(actual, transform, e))
      return false;
    const auto local = s.input.tree->state(actual)->local;
    const float det =
        transform[0].x * transform[1].y - transform[0].y * transform[1].x;
    if (!std::isfinite(det) || std::abs(det) < 1e-12f)
      return fail(e, "Source global position parent basis singular");
    const Vec2 delta{value.x - transform[2].x, value.y - transform[2].y};
    const Vec2 normalized{
        (transform[1].y * delta.x - transform[1].x * delta.y) / det,
        (-transform[0].y * delta.x + transform[0].x * delta.y) / det};
    auto result = local;
    result[2] = {
        local[2].x + local[0].x * normalized.x + local[1].x * normalized.y,
        local[2].y + local[0].y * normalized.x + local[1].y * normalized.y};
    return s.input.tree->set_local(actual, result, e);
  };

  o.interact.apply_serialized_offset = [&s](uint32_t id, Vec2 value,
                                            std::string &e) {
    const auto *r = s.input.sources->interact().record(id);
    if (!r || value.x != r->button_offset.x || value.y != r->button_offset.y)
      return fail(e, "Interact source serialized offset differs");
    return s.position(r->prompt, value, e);
  };
  o.payphone.read_flag = [&s](std::string_view key, bool &p, bool &v,
                              std::string &e) {
    return s.read_flag(false, key, p, v, e);
  };
  o.payphone.visibility = [&s](uint32_t id, bool value, std::string &e) {
    return s.visible(id, value, e);
  };
  o.payphone.queue_free = [&s](uint32_t id, std::string &e) {
    return s.queued(id, e);
  };
  o.present.read_flag = [&s](const FieldPresentBinding &b, bool &v,
                             std::string &e) {
    if (s.input.sources->present().binding(b.id) != &b)
      return fail(e, "Present source binding identity differs");
    bool p = false;
    return s.read_flag(b.object_flag(), b.flag, p, v, e);
  };
  o.present.write_flag = [&s](const FieldPresentBinding &b, bool v,
                              std::string &e) {
    if (s.input.sources->present().binding(b.id) != &b)
      return fail(e, "Present source binding identity differs");
    auto &flags = s.input.continuation->characters()->flags();
    return b.object_flag() ? flags.set_object(b.flag, v, b.emit(), e)
                           : flags.set_normal(b.flag, v, b.emit(), e);
  };
  o.dropped.read_flag = [&s](const FieldDroppedBinding &b, bool &v,
                             std::string &e) {
    if (s.input.sources->dropped().binding(b.id) != &b)
      return fail(e, "Dropped source binding identity differs");
    bool p = false;
    return s.read_flag(b.object_flag(), b.flag, p, v, e);
  };
  o.dropped.write_flag = [&s](const FieldDroppedBinding &b, bool v,
                              std::string &e) {
    if (s.input.sources->dropped().binding(b.id) != &b)
      return fail(e, "Dropped source binding identity differs");
    auto &flags = s.input.continuation->characters()->flags();
    return b.object_flag() ? flags.set_object(b.flag, v, b.emit(), e)
                           : flags.set_normal(b.flag, v, b.emit(), e);
  };

  o.present.prompt_enabled = [&s](uint32_t id, bool v, std::string &e) {
    if (!s.input.sources->prompt().record(id))
      return fail(e, "Present unknown source Prompt");
    if (!s.prompt.set_enabled(id, v)) {
      e = s.prompt.error();
      return false;
    }
    e.clear();
    return true;
  };
  o.dropped.prompt_enabled = o.present.prompt_enabled;
  o.dropped.prompt_visible = [&s](uint32_t id, bool &v, std::string &e) {
    const auto *p = s.prompt.instance(id);
    if (!p || !p->ready)
      return fail(e, "Dropped actual Prompt body not Ready");
    v = p->visible();
    e.clear();
    return true;
  };
  o.dropped.prompt_force_show = [&s](uint32_t id, std::string &e) {
    if (!s.prompt.force(id, 1)) {
      e = s.prompt.error();
      return false;
    }
    e.clear();
    return true;
  };
  o.dropped.prompt_press = [&s](uint32_t id, std::string &e) {
    if (!s.prompt.press(id)) {
      e = s.prompt.error();
      return false;
    }
    e.clear();
    return true;
  };
  o.dropped.prompt_queue_free = [&s](uint32_t id, std::string &e) {
    return s.queued(id, e);
  };
  o.dropped.prompt_position = [&s](uint32_t id, Vec2 p, std::string &e) {
    if (!s.input.sources->prompt().record(id))
      return fail(e, "Dropped unknown source Prompt");
    return s.position(id, p, e);
  };
  o.dropped.queue_free = [&s](uint32_t id, std::string &e) {
    return s.queued(id, e);
  };
  o.dropped.player_position = [&s](Vec2 &v, std::string &e) {
    return s.player_position(v, e);
  };
  o.dropped.publish_position = [&s](uint32_t id, Vec2 value, std::string &e) {
    return s.position(id, value, e);
  };

  o.camera_area.is_global_player = [&s](uint64_t actual, bool &v,
                                        std::string &e) {
    return s.body(actual, v, e);
  };
  o.camera_area.adjust_camareas = [&s](int64_t delta, int64_t &result,
                                       std::string &e) {
    uint32_t id = 0;
    if (!s.current_camera(id, e))
      return false;
    if (!s.camera.adjust_camareas(id, delta, result)) {
      e = s.camera.error();
      return false;
    }
    e.clear();
    return true;
  };
  o.camera_area.set_limit = [&s](FieldCameraLimit role, int32_t value,
                                 std::string &e) {
    uint32_t id = 0;
    if (!s.current_camera(id, e))
      return false;
    if (!s.camera.set_limit(id, role, value)) {
      e = s.camera.error();
      return false;
    }
    e.clear();
    return true;
  };
  o.camera_area.current_limits = [&s](std::array<int32_t, 4> &values,
                                      std::string &e) {
    uint32_t id = 0;
    if (!s.current_camera(id, e))
      return false;
    values = s.camera.state(id)->limits;
    e.clear();
    return true;
  };
  o.camera_area.set_camarea_offset = [&s](Vec2 value, std::string &e) {
    uint32_t id = 0;
    if (!s.current_camera(id, e))
      return false;
    if (!s.camera.set_camarea_offset(id, value)) {
      e = s.camera.error();
      return false;
    }
    e.clear();
    return true;
  };
  o.camera_area.resolve_reference = [&s](uint32_t id, std::string_view path,
                                         uint32_t &result, std::string &e) {
    const auto *descriptor = s.input.sources->camera_area().record(id);
    FieldObjectId actual = 0, target = 0;
    if (!descriptor || descriptor->reference_path != path)
      return fail(e, "CameraArea source onready path differs");
    if (!s.source(id, actual, e) ||
        !s.input.tree->get_node(actual, path, target, e))
      return false;
    if (!target) {
      if (descriptor->reference_exists)
        return fail(e, "CameraArea actual checked reference absent");
      result = 0;
      e.clear();
      return true;
    }
    const auto *reference = s.input.tree->descriptor(target);
    if (!descriptor->reference_exists || !reference ||
        reference->id != descriptor->reference_id)
      return fail(e, "CameraArea actual reference source differs");
    result = reference->id;
    e.clear();
    return true;
  };
  o.cutscene.read_flag = [&s](std::string_view key, bool &v, std::string &e) {
    return s.flag(key, v, e);
  };
  o.cutscene.body_is_current_player = [&s](uint32_t actual, bool &v,
                                           std::string &e) {
    return s.body(actual, v, e);
  };
  o.cutscene.query_ui = [&s](FieldCutsceneAreaUi &value, std::string &e) {
    auto *ui = s.input.continuation->ui();
    const auto id = ui->binding().object;
    return ui->source_is_in_cutscene(id, value.cutscene, e) &&
           ui->source_is_in_battle(id, value.battle, e) &&
           ui->source_is_pause_menu_active(id, value.pause, e);
  };
  if (!d.bind_sources(s.consumers, e))
    return false;
  s.consumers.random = input.continuation->random();
  s.consumers.grass = &s.grass;
  s.consumers.map_space = input.map;
  s.consumers.geometry = input.geometry;
  s.consumers.npc = &s.npc;
  s.consumers.enemy = &s.enemy;
  s.consumers.tint = &s.tint;
  s.consumers.sprite = &s.sprite;
  s.consumers.emote = &s.emote;
  s.consumers.dandelion = &s.dandelion;
  s.consumers.door = &s.door;
  s.consumers.prompt = &s.prompt;
  s.consumers.bush = &s.bush;
  s.consumers.interact = &s.interact;
  s.consumers.present = &s.present;
  s.consumers.dropped = &s.dropped;
  s.consumers.sparkles = &s.sparkles;
  s.consumers.openable = &s.openable;
  s.consumers.payphone = &s.payphone;
  s.consumers.butterfly = &s.butterfly;
  s.consumers.cutscene = &s.cutscene;
  s.consumers.birds = &s.birds;
  s.consumers.camera_area = &s.camera_area;
  s.consumers.music = &s.music;
  s.consumers.arrows = &s.arrows;
  s.consumers.actions = &s.actions;
  s.consumers.stepping = &s.stepping;
  s.consumers.transitions = &s.transitions;
  s.consumers.camera = &s.camera;
  s.consumers.door_npc = &s.door_npc;
  s.consumers.melody = &s.melody;
  s.consumers.vending = &s.vending;
  s.initialize_one("grass", [&](std::string &v) {
    return s.grass.prepare_grass_slice(d.grass(), v);
  });
  s.initialize_one("npc", [&](std::string &v) {
    return s.npc.initialize(&d.npc(), input.continuation->random(), o.npc, v);
  });
  s.initialize_one("enemy", [&](std::string &v) {
    return s.enemy.initialize(&d.enemy(), input.continuation->random(), o.enemy,
                              v);
  });
  s.initialize_one("tint", [&](std::string &v) {
    return s.tint.initialize(d.tint(), o.tint, v);
  });
  s.initialize_one("sprite", [&](std::string &v) {
    return s.sprite.initialize(d.sprite(), o.sprite, v);
  });
  s.initialize_one("emote", [&](std::string &v) {
    return s.emote.initialize(d.emote(), o.emote, v);
  });
  s.initialize_one("dandelion", [&](std::string &v) {
    return s.dandelion.initialize(d.dandelion(), o.dandelion, v);
  });
  s.initialize_one("door", [&](std::string &v) {
    return s.door.initialize(d.door(), o.door, v);
  });
  s.initialize_one("prompt", [&](std::string &v) {
    return s.prompt.initialize(d.prompt(), o.prompt, v);
  });
  s.initialize_one("bush", [&](std::string &v) {
    return s.bush.initialize(d.bush(), o.bush, v);
  });
  s.initialize_one("interact", [&](std::string &v) {
    return s.interact.initialize(d.interact(), o.interact, v);
  });
  s.initialize_one("present", [&](std::string &v) {
    return s.present.initialize(d.present(), o.present, v);
  });
  s.initialize_one("dropped", [&](std::string &v) {
    return s.dropped.initialize(d.dropped(), o.dropped, v);
  });
  s.initialize_one("sparkles", [&](std::string &v) {
    return s.sparkles.initialize(d.sparkles(), *input.continuation->random(),
                                 o.sparkles, s.present, s.dropped, v);
  });
  s.initialize_one("openable", [&](std::string &v) {
    return s.openable.initialize(d.openable(), o.openable, v);
  });
  s.initialize_one("payphone", [&](std::string &v) {
    return s.payphone.initialize(d.payphone(), o.payphone, v);
  });
  s.initialize_one("butterfly", [&](std::string &v) {
    return s.butterfly.initialize(d.butterfly(), o.butterfly, v);
  });
  s.initialize_one("cutscene", [&](std::string &v) {
    return s.cutscene.initialize(d.cutscene(), o.cutscene, v);
  });
  s.initialize_one("birds", [&](std::string &v) {
    return s.birds.initialize(d.birds(), *input.continuation->random(), o.birds,
                              v);
  });
  s.initialize_one("camera_area", [&](std::string &v) {
    return s.camera_area.initialize(d.camera_area(), o.camera_area, v);
  });
  s.initialize_one("music", [&](std::string &v) {
    return s.music.initialize(d.music(), input.scene_epoch, o.music, v);
  });
  s.initialize_one("arrows", [&](std::string &v) {
    return s.arrows.initialize(d.arrows(), o.arrows, v);
  });
  s.initialize_one("actions", [&](std::string &v) {
    return s.actions.initialize(d.actions(), o.actions, v);
  });
  s.initialize_one("stepping", [&](std::string &v) {
    return s.stepping.initialize(d.stepping(), o.stepping, v);
  });
  s.initialize_one("transitions", [&](std::string &v) {
    return s.transitions.initialize(d.transitions(), o.transitions, v);
  });
  s.initialize_one("camera", [&](std::string &v) {
    return s.camera.initialize(d.camera(), *input.continuation->random(),
                               o.camera, v);
  });
  s.initialize_one("door_npc", [&](std::string &v) {
    return s.door_npc.initialize(d.door_npc(), o.door_npc, v);
  });
  s.initialize_one("melody", [&](std::string &v) {
    return s.melody.initialize(d.melody(), o.melody, v);
  });
  s.initialize_one("vending", [&](std::string &v) {
    return s.vending.initialize(d.vending(), *input.shop, d.geometry(),
                                o.vending, v);
  });

  if (!s.failures.empty()) {
    e = s.failures.front();
    return false;
  }
  s.initialized = true;
  e.clear();
  return true;
}
bool PodunkSceneConsumers::bind_scripts(PodunkSceneScripts &scripts,
                                        std::string &e) {
  if (!initialized() || state_->scripts)
    return fail(e,
                "SceneScripts binding missing/duplicate initialized consumers");
  state_->scripts = &scripts;
  e.clear();
  return true;
}
bool PodunkSceneConsumers::initialized() const {
  return state_ && state_->initialized;
}
FieldSceneConsumers PodunkSceneConsumers::consumers() const {
  return initialized() ? state_->consumers : FieldSceneConsumers{};
}
FieldSceneHostOps PodunkSceneConsumers::ops() const {
  return initialized() ? state_->owners.scene : FieldSceneHostOps{};
}
bool PodunkSceneConsumers::source_object(uint32_t id, FieldObjectId &out,
                                         std::string &e) const {
  return initialized() ? state_->source(id, out, e)
                       : fail(e, "Scene consumers not initialized");
}
const std::vector<std::string> &
PodunkSceneConsumers::initialization_failures() const {
  static const std::vector<std::string> empty;
  return state_ ? state_->failures : empty;
}
bool PodunkSceneConsumers::sprite_appearance(const FieldCanvasRecord &r,
                                             FieldObjectId actual,
                                             FieldObjectId owner,
                                             FieldCanvasAppearance &out,
                                             std::string &e) const {
  if (!initialized())
    return fail(e, "Scene appearance before actual consumer binding");
  auto &s = *state_;
  FieldObjectId verified = 0;
  if (!s.source(r.id, verified, e) || verified != actual)
    return fail(e, "Scene CanvasArt actual identity differs");
  const auto *parent = s.input.tree->descriptor(owner);
  if (!parent || parent->id != r.owner_id)
    return fail(e, "Scene CanvasArt typed owner differs");
  // The same native body holds dynamic property values. Shader ownership must
  // still be the actual separate admitted GPU consumer, never flattened here.
  if (r.shader != FieldCanvasShader::Default)
    return fail(e, "Scene typed shader requires actual material owner");
  return s.input.native->sprite_snapshot(actual, out, e);
}
} // namespace encore::ctr
