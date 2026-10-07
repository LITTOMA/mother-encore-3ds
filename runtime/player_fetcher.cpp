#include "encore/player_tree_rebind.hpp"
#include "encore/player_fetcher.hpp"
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
// Native get_node_or_null inside this actual branch. Missing names yield nil;
// corrupt ownership, ambiguous aliases and unsupported path syntax reject.
bool nullable(const FieldNodeTreeRuntime &t, FieldObjectId from,
              std::string_view path, FieldObjectId &out, std::string &e) {
  out = 0;
  if (path.empty() || path.front() == '/' || path.find(':') != path.npos)
    return fail(e, "Fetcher relative native NodePath unsupported");
  auto at = from;
  size_t begin = 0;
  while (begin <= path.size()) {
    auto end = path.find('/', begin);
    if (end == path.npos)
      end = path.size();
    auto part = path.substr(begin, end - begin);
    auto *s = t.state(at);
    if (!s || !s->alive)
      return fail(e, "Fetcher NodePath actual base is absent");
    if (part == "..") {
      at = s->parent;
      if (!at) {
        out = 0;
        e.clear();
        return true;
      }
    } else if (!part.empty() && part != ".") {
      FieldObjectId found = 0;
      for (auto child : s->children) {
        auto *c = t.state(child);
        if (!c || !c->alive || c->parent != at)
          return fail(e, "Fetcher native child ownership corrupt");
        if (c->name == part) {
          if (found)
            return fail(e, "Fetcher NodePath ambiguous actual names");
          found = child;
        }
      }
      if (!found) {
        out = 0;
        e.clear();
        return true;
      }
      at = found;
    }
    if (end == path.size())
      break;
    begin = end + 1;
  }
  out = at;
  e.clear();
  return true;
}
} // namespace
bool PlayerFetcherRuntime::prepare(const PlayerFetcherData &d,
                                   const PlayerInitializationData &p,
                                   FieldNodeTreeRuntime &t,
                                   FieldGlobalRegistry &r,
                                   FieldGlobalConstructorRuntime &global,
                                   PlayerFetcherSpriteReader &sprites,
                                   std::string &e) {
  if (data_ || !d.valid() || !p.valid() ||
      d.player_ir_sha256() != p.ir_sha256() || !global.data() ||
      global.data()->ir_sha256() != d.global_ir_sha256() || !global.owner() ||
      !r.object_exists(global.owner()) || t.object_domain() != r.kernel() ||
      sprites.registry() != &r || sprites.tree() != &t)
    return fail(e, "Fetcher actual Player/global/native dependencies differ");
  data_ = &d;
  player_data_ = &p;
  tree_ = &t;
  registry_ = &r;
  global_ = &global;
  sprites_ = &sprites;
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::construct(FieldObjectId fetcher,
                                     const FieldNodeDescriptor &descriptor,
                                     std::string &e) {
  if (!data_ || object_ || !fetcher)
    return fail(e, "Fetcher source constructor already run/not prepared");
  const auto *actual = tree_->descriptor(fetcher);
  const auto *state = tree_->state(fetcher);
  FieldIdentity identity;
  auto found =
      std::find_if(data_->records().begin(), data_->records().end(),
                   [&](const auto &x) { return x.id == descriptor.id; });
  if (!actual || !state || !state->alive || state->inside ||
      state->ready_notified || found == data_->records().end() ||
      actual->id != descriptor.id || actual->script != data_->script() ||
      actual->script_sha != found->script_sha ||
      actual->native_class != "Node" || actual->path != found->path ||
      descriptor.script != actual->script ||
      descriptor.script_sha != actual->script_sha ||
      descriptor.native_class != actual->native_class ||
      !tree_->object_identity(fetcher, identity) ||
      !same(identity, data_->identity()))
    return fail(e, "Fetcher actual source attachment rejected");
  // Allocation and insertion in the actual Tree happen before construct_source;
  // Registry publication/parent/name happen later. No synthetic live ID is
  // used.
  row_ = &*found;
  object_ = fetcher;
  state_.has_reflection = data_->exports().initial_has;
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::initialize(
    const PlayerFetcherData &d, const PlayerInitializationData &p,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    FieldGlobalConstructorRuntime &global, FieldObjectId player,
    FieldObjectId fetcher, PlayerFetcherSpriteReader &sprites, std::string &e) {
  if (r.tree_owner(player).get() != &t || r.tree_owner(fetcher).get() != &t)
    return fail(e, "Fetcher actual published Player branch owners differ");
  auto *root = t.descriptor(player);
  auto *actual = t.descriptor(fetcher);
  FieldIdentity identity;
  if (!root || root->id != p.recipe().identity().scene_id || !actual ||
      !t.object_identity(player, identity) || !same(identity, p.identity()))
    return fail(e, "Fetcher actual source Player root rejected");
  if (!prepare(d, p, t, r, global, sprites, e) ||
      !construct(fetcher, *actual, e))
    return false;
  FieldObjectId relative = 0;
  if (!t.get_node(player, row_->path, relative, e) || relative != fetcher)
    return fail(e, "Fetcher actual Player child reference differs");
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::live(bool inside, std::string &e) const {
  if (!data_ || !row_ || !tree_ || !registry_ || !global_ ||
      !registry_->object_exists(object_) ||
      registry_->tree_owner(object_).get() != tree_)
    return fail(e, "Fetcher actual ObjectDB owner expired");
  auto *s = tree_->state(object_);
  auto *d = tree_->descriptor(object_);
  FieldIdentity id;
  if (!s || !s->alive || !d || d->id != row_->id ||
      d->script != data_->script() || d->script_sha != row_->script_sha ||
      d->native_class != "Node" || !tree_->object_identity(object_, id) ||
      !same(id, data_->identity()) || (inside && !s->inside))
    return fail(e, "Fetcher source native instance mismatch");
  return true;
}
bool PlayerFetcherRuntime::apply_exports(std::string &e) {
  if (!data_ || !row_ || !tree_->state(object_) ||
      !tree_->state(object_)->alive || tree_->state(object_)->inside ||
      state_.export_applied || state_.onready)
    return fail(e, "Fetcher source export cursor rejected");
  // The checked record was cross-compared with the actual Player SceneState.
  // These exports have no setter; no lifecycle or native frame is changed.
  state_.export_applied = true;
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::reflector(FieldObjectId &out, std::string &e) const {
  FieldObjectId scene = 0;
  if (!global_->object(FieldGlobalMemberRole::CurrentScene, scene, e) ||
      !scene || !registry_->object_exists(scene))
    return fail(e, "Fetcher global.currentScene actual Node is absent");
  auto owner = registry_->tree_owner(scene);
  FieldIdentity identity;
  auto *node = owner ? owner->descriptor(scene) : nullptr;
  // This is a native lookup on the actual current scene, not approval of its
  // script/Ready. Other same-pin checked roots are valid lookup receivers;
  // the static Podunk absence proof never hides dynamically created children.
  if (!owner || owner->object_domain() != registry_->kernel() || !node ||
      !owner->object_identity(scene, identity) || !identity.scene_id ||
      identity.upstream_commit != data_->identity().upstream_commit ||
      node->id != identity.scene_id)
    return fail(e,
                "Fetcher current scene actual checked Node owner unsupported");
  return nullable(*owner, scene, data_->reflector_path(), out, e);
}
bool PlayerFetcherRuntime::ready(FieldTreePhase phase,
                                 const FieldNodeBinding &binding,
                                 std::string &e) {
  if (phase != FieldTreePhase::ReadyScript || !live(true, e) ||
      !state_.export_applied || state_.onready)
    return fail(e, "Fetcher implicit onready lifecycle cursor rejected");
  const auto *s = tree_->state(object_);
  const auto *d = tree_->descriptor(object_);
  if (!s->ready_notified || s->ready_first || !s->bound ||
      binding.stable_id != d->id || binding.class_index != d->class_index ||
      binding.native_class != d->native_class ||
      binding.script_sha != row_->script_sha ||
      !same(binding.identity, data_->identity()))
    return fail(e, "Fetcher actual native ReadyScript receipt rejected");
  FieldObjectId sprite = 0;
  if (!nullable(*tree_, object_, row_->sprite_path, sprite, e))
    return false;
  if (sprite) {
    auto *target = tree_->descriptor(sprite);
    FieldIdentity identity;
    if (!target || target->id != row_->target ||
        target->native_class != "Sprite" ||
        registry_->tree_owner(sprite).get() != tree_ ||
        !tree_->object_identity(sprite, identity) ||
        !same(identity, data_->identity()))
      return fail(e, "Fetcher actual Sprite target/source differs");
  }
  // Keep native onready assignment order, including nil target semantics.
  state_.sprite = sprite;
  state_.parent = s->parent;
  if (!reflector(state_.reflector, e))
    return false;
  state_.onready = true;
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::process(FieldTreePhase phase, bool paused,
                                   std::string &e) {
  if (phase != FieldTreePhase::Idle || !live(true, e) || !state_.onready)
    return fail(e, "Fetcher actual source process cursor rejected");
  if (!tree_->can_process(object_, paused)) {
    e.clear();
    return true;
  }
  if (!reflector(state_.reflector, e))
    return false;
  if (!row_->ignore && state_.reflector && !state_.has_reflection)
    return generate_reflection(e);
  if (!state_.reflector)
    return delete_reflection(e);
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::generate_reflection(std::string &e) {
  if (!live(true, e) || !state_.onready || !state_.reflector)
    return fail(e, "Fetcher generate_reflection invalid source receiver");
  // Do not manufacture CharacterReflection, consume an unrelated prototype,
  // or set has_reflection when its real factory/native owners are unavailable.
  return fail(
      e, "Fetcher actual FloorReflector.create_reflection factory pending");
}
bool PlayerFetcherRuntime::delete_reflection(std::string &e) {
  if (!live(false, e) || !state_.onready)
    return fail(e, "Fetcher delete_reflection source body unavailable");
  if (state_.reflection && registry_->object_exists(state_.reflection)) {
    auto owner = registry_->tree_owner(state_.reflection);
    if (!owner || !owner->queue_free(state_.reflection, e))
      return fail(e, "Fetcher reflection actual deferred deletion unavailable");
    state_.has_reflection = false;
  }
  // Source leaves _obj_reflection intact, and leaves has unchanged if invalid.
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::sprite_object(FieldObjectId &out,
                                         std::string &e) const {
  if (!live(false, e) || !state_.onready)
    return fail(e, "Fetcher source getter before onready");
  out = state_.sprite;
  if (out && !registry_->object_exists(out))
    return fail(e, "Fetcher source Sprite reference was freed");
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::sample(PlayerFetcherSpriteState &out,
                                  std::string &e) const {
  FieldObjectId sprite = 0;
  if (!sprite_object(sprite, e) || !sprite || !sprites_ ||
      sprites_->registry() != registry_ || sprites_->tree() != tree_)
    return fail(e, "Fetcher source native Sprite receiver absent");
  if (registry_->tree_owner(sprite).get() != tree_ ||
      !sprites_->read(sprite, out, e))
    return false;
  if (!out.columns || !out.rows ||
      uint64_t(out.frame) >= uint64_t(out.columns) * out.rows)
    return fail(e, "Fetcher live native Sprite frame dimensions rejected");
  if (out.texture && !registry_->source_resource(out.texture))
    return fail(e, "Fetcher actual native Texture Resource owner absent");
  e.clear();
  return true;
}
bool PlayerFetcherRuntime::frame(uint32_t &out, std::string &e) const {
  PlayerFetcherSpriteState s;
  if (!sample(s, e))
    return false;
  out = s.frame;
  return true;
}
bool PlayerFetcherRuntime::texture(FieldObjectId &out, std::string &e) const {
  PlayerFetcherSpriteState s;
  if (!sample(s, e))
    return false;
  out = s.texture;
  return true;
}
bool PlayerFetcherRuntime::hframes(uint32_t &out, std::string &e) const {
  PlayerFetcherSpriteState s;
  if (!sample(s, e))
    return false;
  out = s.columns;
  return true;
}
bool PlayerFetcherRuntime::vframes(uint32_t &out, std::string &e) const {
  PlayerFetcherSpriteState s;
  if (!sample(s, e))
    return false;
  out = s.rows;
  return true;
}
bool PlayerFetcherRuntime::visibility(bool &out, std::string &e) const {
  PlayerFetcherSpriteState s;
  if (!sample(s, e))
    return false;
  out = s.visible;
  return true;
}
bool PlayerFetcherRuntime::rebind_tree(FieldNodeTreeRuntime &next, std::string &e) {
  if (!data_ || !player_data_ || !row_ || !registry_ || !sprites_ ||
      sprites_->registry() != registry_ || sprites_->tree() != &next ||
      !state_.export_applied || !state_.onready ||
      !player_rebind_node(*player_data_, *registry_, next, object_, row_->id, e))
    return fail(e, "Player Fetcher rebind requires preserved source/native owner");
  for (auto id : {state_.sprite, state_.parent}) {
    if (!id) continue;
    auto *descriptor = next.descriptor(id);
    if (!descriptor || !player_rebind_node(*player_data_, *registry_, next, id, descriptor->id, e))
      return false;
  }
  // Reflector/reflection may belong to the prior scene. Source _process
  // performs that lookup/deletion; transfer must not replay its onready.
  tree_ = &next;
  e.clear();
  return true;
}
} // namespace encore::upstream
