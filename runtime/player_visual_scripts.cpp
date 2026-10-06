#include "encore/player_visual_scripts.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool same_frames(const std::vector<PlayerVisualAnimation> &a,
                 const std::vector<PlayerVisualAnimation> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].name != b[i].name || a[i].speed != b[i].speed ||
        a[i].loop != b[i].loop || a[i].frames.size() != b[i].frames.size())
      return false;
    for (size_t j = 0; j < a[i].frames.size(); ++j) {
      const auto &x = a[i].frames[j];
      const auto &y = b[i].frames[j];
      if (x.resource != y.resource || x.texture != y.texture ||
          x.source != y.source || x.source_sha != y.source_sha ||
          x.rect != y.rect)
        return false;
    }
  }
  return true;
}
} // namespace
bool PlayerVisualScriptsRuntime::initialize(
    const PlayerVisualScriptsData &d, const PlayerInitializationData &p,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    FieldGlobalConstructorRuntime &global, PlayerInitializationBody &body,
    PlayerVisualNativeOwner &shadow, PlayerVisualNativeOwner &bat,
    PlayerVisualFetcherOwner &fetcher, std::string &e) {
  if (data_ || !d.valid() || !p.valid() ||
      d.player_ir_sha256() != p.ir_sha256() ||
      t.object_domain() != r.kernel() || !body.constructed() ||
      !body.object() || !r.object_exists(body.object()) || !global.data() ||
      global.data()->identity().upstream_commit !=
          d.identity().upstream_commit ||
      shadow.registry() != &r || bat.registry() != &r ||
      fetcher.registry() != &r || shadow.tree() != &t || bat.tree() != &t ||
      fetcher.tree() != &t)
    return fail(e, "Player visual actual same native/source owners required");
  data_ = &d;
  player_data_ = &p;
  tree_ = &t;
  registry_ = &r;
  global_ = &global;
  player_ = &body;
  shadow_ = &shadow;
  bat_ = &bat;
  fetcher_ = &fetcher;
  const auto *root = p.recipe().record(d.player_id());
  if (!root || !node(body.object(), root->id, root->script, root->script_sha,
                     root->native_class, false, e)) {
    data_ = nullptr;
    return false;
  }
  return true;
}
bool PlayerVisualScriptsRuntime::node(FieldObjectId object, uint32_t source,
                                      std::string_view script,
                                      const std::array<uint8_t, 32> &sha,
                                      std::string_view native_class,
                                      bool inside, std::string &e) const {
  if (!data_ || !tree_ || !registry_ || !object ||
      !registry_->object_exists(object))
    return fail(e, "Player visual actual ObjectDB node absent");
  const auto *s = tree_->state(object);
  const auto *d = tree_->descriptor(object);
  FieldIdentity id;
  if (!s || !d || !s->alive || !tree_->object_identity(object, id) ||
      !same(id, data_->identity()) || d->id != source || d->script != script ||
      d->script_sha != sha || d->native_class != native_class ||
      (inside && !s->inside))
    return fail(e, "Player visual native/script source identity differs");
  return true;
}
bool PlayerVisualScriptsRuntime::native(PlayerVisualNativeOwner &owner,
                                        uint32_t source, std::string_view cls,
                                        bool ready,
                                        PlayerVisualNativeState &state,
                                        std::string &e) const {
  const auto *d = player_data_->recipe().record(source);
  if (!d || owner.registry() != registry_ || owner.tree() != tree_ ||
      !node(owner.object(), source, d->script, d->script_sha, cls, false, e) ||
      !owner.state(state, e) || !state.constructed ||
      (ready && !state.native_ready))
    return fail(
        e, "Player visual actual constructed/nativeReady Sprite owner missing");
  return true;
}
bool PlayerVisualScriptsRuntime::construct(
    FieldObjectId object, const FieldNodeDescriptor &descriptor,
    std::string &e) {
  if (!data_ || states_.count(object))
    return fail(e, "Player visual script already constructed/not bound");
  PlayerVisualScriptState script;
  PlayerVisualNativeState state;
  if (descriptor.id == data_->shadow().id) {
    const auto &s = data_->shadow();
    if (object != shadow_->object() ||
        !native(*shadow_, s.id, "AnimatedSprite", false, state, e) ||
        state.frames_resource != s.frames_resource)
      return fail(e, "Shadow actual SpriteFrames constructor owner differs");
    const std::vector<PlayerVisualAnimation> *frames = nullptr;
    if (!shadow_->sprite_frames(frames, e) || !frames ||
        !same_frames(*frames, s.animations))
      return fail(e, "Shadow actual source SpriteFrames Resource differs");
    script.start_anim = s.start_default;
    script.front_anims = s.front;
  } else if (descriptor.id == data_->bat().id) {
    const auto &b = data_->bat();
    if (object != bat_->object() ||
        !native(*bat_, b.id, "Sprite", false, state, e) ||
        state.columns != b.columns || state.rows != b.rows ||
        state.texture != b.texture || state.frame != b.initial_frame)
      return fail(e, "Bat actual Sprite native constructor differs");
  } else
    return fail(e, "Player visual source script constructor unknown");
  const auto *actual = tree_->descriptor(object);
  if (!actual || actual->id != descriptor.id ||
      actual->script_sha != descriptor.script_sha ||
      actual->script != descriptor.script)
    return fail(e, "Player visual script attachment descriptor differs");
  script.constructed = true;
  states_.emplace(object, std::move(script));
  return true;
}
bool PlayerVisualScriptsRuntime::apply_shadow_export(FieldObjectId object,
                                                     std::string_view member,
                                                     std::string &e) {
  auto f = states_.find(object);
  if (!data_ || object != shadow_->object() ||
      member != data_->shadow().start_member || f == states_.end() ||
      f->second.export_applied)
    return fail(e, "Shadow actual source setget property cursor rejected");
  if (!set_shadow_anim(object, data_->shadow().start, e))
    return false;
  f->second.export_applied = true;
  return true;
}
bool PlayerVisualScriptsRuntime::set_shadow_anim(FieldObjectId object,
                                                 std::string_view animation,
                                                 std::string &e) {
  auto f = states_.find(object);
  if (!data_ || object != shadow_->object() || f == states_.end())
    return fail(e, "Shadow actual source body absent");
  const auto &clips = data_->shadow().animations;
  if (!animation.empty() &&
      std::none_of(clips.begin(), clips.end(),
                   [&](const auto &a) { return a.name == animation; }))
    return fail(e, "Shadow unreviewed animation rejected");
  PlayerVisualNativeState before;
  if (!native(*shadow_, data_->shadow().id, "AnimatedSprite", false, before, e))
    return false;
  // Original setter assigns the script field BEFORE the two native calls.
  f->second.start_anim = std::string(animation);
  if (!shadow_->play(animation, e))
    return false;
  bool behind =
      std::find(f->second.front_anims.begin(), f->second.front_anims.end(),
                animation) == f->second.front_anims.end();
  if (!shadow_->set_behind_parent(behind, e))
    return false;
  PlayerVisualNativeState after;
  if (!shadow_->state(after, e) || !after.playing ||
      after.behind_parent != behind ||
      (!animation.empty() && after.animation != animation))
    return fail(
        e, "Shadow native source play/behind-parent side effect not observed");
  return true;
}
bool PlayerVisualScriptsRuntime::ready(FieldObjectId object, std::string &e) {
  auto f = states_.find(object);
  if (!data_ || f == states_.end() || !f->second.constructed ||
      f->second.onready_complete)
    return fail(e, "Player visual source Ready body/cursor rejected");
  PlayerVisualNativeState state;
  if (object == shadow_->object()) {
    if (!native(*shadow_, data_->shadow().id, "AnimatedSprite", true, state,
                e) ||
        !f->second.export_applied)
      return fail(e, "Shadow source constructor setter/nativeReady incomplete");
  } else if (object == bat_->object()) {
    const auto &b = data_->bat();
    if (!native(*bat_, b.id, "Sprite", true, state, e))
      return false;
    FieldObjectId actual = 0;
    if (!tree_->get_node(object, b.special_path, actual, e) ||
        actual != fetcher_->object() || fetcher_->registry() != registry_ ||
        fetcher_->tree() != tree_ ||
        !node(actual, b.fetcher, b.fetcher_script, b.fetcher_sha, "Node", true,
              e))
      return fail(
          e, "Bat implicit onready actual SpriteDataFetcher owner missing");
    f->second.special = actual;
  } else
    return fail(e, "Player visual source Ready object unknown");
  f->second.onready_complete = true;
  return true;
}
bool PlayerVisualScriptsRuntime::process(FieldObjectId object, std::string &e) {
  auto f = states_.find(object);
  if (!data_ || object != bat_->object() || f == states_.end() ||
      !f->second.onready_complete || !f->second.special)
    return fail(e, "Bat original implicit onready not executed");
  const auto &b = data_->bat();
  if (!node(object, b.id, b.script, b.script_sha, "Sprite", true, e))
    return false;
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!global_->array(FieldGlobalMemberRole::PartyObjects, party, e) ||
      !party || party->values.empty() ||
      party->values[0] != player_->object() || !player_->constructed())
    return fail(e, "Bat global.get_player actual partyObjects owner differs");
  PlayerInitializationMember action;
  if (!player_->member(b.action_member, action, e) || !action.value ||
      action.value->kind != 4)
    return fail(e, "Bat actual Player skill-action String missing");
  if (!bat_->set_visible(action.value->string == b.visible_action, e))
    return false;
  if (f->second.special != fetcher_->object() || !fetcher_->onready_complete())
    return fail(e, "Bat source fetcher onready receiver not available");
  FieldObjectId target = 0, source_target = 0;
  if (!fetcher_->sprite_object(target, e) ||
      !tree_->get_node(fetcher_->object(), b.fetcher_path, source_target, e) ||
      target != source_target)
    return fail(e, "Bat actual source fetcher target reference differs");
  const auto *td = tree_->descriptor(target);
  FieldIdentity identity;
  if (!td || td->id != b.target || td->native_class != "Sprite" ||
      !tree_->object_identity(target, identity) ||
      !same(identity, data_->identity()))
    return fail(e, "Bat fetcher actual native Sprite source differs");
  uint32_t frame = 0;
  if (!fetcher_->frame(frame, e) ||
      uint64_t(frame) >= uint64_t(b.columns) * b.rows ||
      !bat_->set_frame(frame, e))
    return fail(e, "Bat source frame getter/native Sprite frame rejected");
  PlayerVisualNativeState after;
  if (!bat_->state(after, e) || after.frame != frame ||
      after.visible != (action.value->string == b.visible_action))
    return fail(e, "Bat source native visible/frame writes not observed");
  return true;
}
const PlayerVisualScriptState *
PlayerVisualScriptsRuntime::state(FieldObjectId id) const {
  auto f = states_.find(id);
  return f == states_.end() ? nullptr : &f->second;
}
} // namespace encore::upstream
