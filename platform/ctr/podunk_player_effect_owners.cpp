#include "podunk_player_effect_owners.hpp"
#include <cmath>
#include <cstdlib>

namespace encore::ctr {
using namespace upstream;
namespace {
using Raw = std::shared_ptr<const GlobalYamlValue>;
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.source_sha256 == b.source_sha256 &&
         a.upstream_commit == b.upstream_commit;
}
Raw get(const Raw &v, std::string_view k) { return v ? v->get(k) : nullptr; }
bool text(const Raw &v, std::string &s) {
  if (!v || v->kind != 4)
    return false;
  s = v->string;
  return true;
}
bool number(const Raw &v, double &n) {
  if (!v)
    return false;
  if (v->kind == 2) {
    n = double(v->integer);
    return true;
  }
  std::string type, raw;
  if (!text(get(v, "type"), type) || (type != "real" && type != "int64") ||
      !text(get(v, "value"), raw) || raw.empty())
    return false;
  char *end = nullptr;
  n = std::strtod(raw.c_str(), &end);
  return end == raw.c_str() + raw.size() && std::isfinite(n);
}
bool integer(const Raw &v, uint32_t &n) {
  double d = 0;
  if (!number(v, d) || d < 0 || d > UINT32_MAX || std::floor(d) != d)
    return false;
  n = uint32_t(d);
  return true;
}
bool boolean(const Raw &v, bool &b) {
  if (!v || v->kind != 1)
    return false;
  b = v->boolean;
  return true;
}
bool reference(const Raw &v, uint32_t &n) {
  std::string type;
  return text(get(v, "type"), type) && type == "ResourceReference" &&
         integer(get(v, "id"), n);
}
bool vector(const Raw &v, Vec2 &p) {
  std::string type;
  double x = 0, y = 0;
  if (!text(get(v, "type"), type) || type != "Vector2" ||
      !number(get(v, "x"), x) || !number(get(v, "y"), y) ||
      !std::isfinite(float(x)) || !std::isfinite(float(y)))
    return false;
  p = {float(x), float(y)};
  return true;
}
Raw source_node(const Raw &native, std::string_view path) {
  auto nodes = get(native, "nodes");
  if (!nodes || nodes->kind != 5)
    return nullptr;
  for (const auto &n : nodes->array) {
    std::string p;
    if (text(get(n, "path"), p) && p == path)
      return n;
  }
  return nullptr;
}
class PackedSceneOwner final : public FieldGlobalSourceResource {
  const PlayerEffectsData *data_;
  const FieldNodeRecipeData *recipe_;
  Raw native_;
  FieldGlobalRegistry *registry_;
  FieldGlobalExternalBinding binding_;

public:
  PackedSceneOwner(const PlayerEffectsData &d, const FieldNodeRecipeData &r,
                   Raw n, FieldGlobalRegistry &g, FieldGlobalExternalBinding b)
      : data_(&d), recipe_(&r), native_(std::move(n)), registry_(&g),
        binding_(b) {}
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *resource_class() const override { return "PackedScene"; }
  bool state(FieldGlobalExternalState &out, std::string &e) const override {
    if (!data_->valid() || !recipe_->valid() || !native_ ||
        registry_->poisoned())
      return fail(e, "Effect PackedScene source backing retired");
    out = {};
    out.name = binding_.source.name;
    e.clear();
    return true;
  }
  bool deferred(const FieldDeferredMessage &, std::string &e) override {
    return fail(e, "PackedScene Resource cannot execute Node callbacks");
  }
  bool persist_append(FieldObjectId, std::string &e) override {
    return fail(e, "PackedScene Resource has no persistent Node array");
  }
  bool assign_stable_canvas(FieldObjectId, std::string &e) override {
    return fail(e, "PackedScene Resource cannot own a Canvas Node");
  }
};
} // namespace
struct PodunkConcretePlayerEffectOwners::Endpoint final
    : PodunkPlayerAnimationEndpoints {
  PodunkConcretePlayerEffectOwners *owner = nullptr;
  FieldObjectId root = 0;
  uint32_t kind = 0;
  bool admit(FieldObjectId id, std::string_view member, bool method,
             std::string &e) const override {
    auto n = owner->node(id, e);
    if (!n || n->root != root || method ||
        n->descriptor.native_class != "Sprite")
      return fail(e, "Effect animation target native endpoint rejected");
    if (member == "frame" || member == "texture" || member == "offset" ||
        member == "visible" || member == "position" ||
        member == "rotation_degrees" || member == "modulate" ||
        member == "show_behind_parent") {
      e.clear();
      return true;
    }
    const std::string prefix = "material:shader_param/";
    if (member.substr(0, prefix.size()) == prefix) {
      auto s = owner->animation_->sprite(id);
      std::vector<PlayerResourceParameter> parameters;
      if (!s || !s->material ||
          !owner->resources_->shader_parameters(s->material, parameters, e))
        return false;
      for (const auto &p : parameters)
        if (p.name == member.substr(prefix.size())) {
          e.clear();
          return true;
        }
    }
    return fail(e, "Effect animation member has no actual native owner");
  }
  bool disabled(FieldObjectId, bool, std::string &e) override {
    return fail(e, "Effect has no collision disabled endpoint");
  }
  bool audio_playing(FieldObjectId, bool, std::string &e) override {
    return fail(e, "Effect has no AudioStreamPlayer endpoint");
  }
  bool audio_stream(FieldObjectId, uint32_t, std::string &e) override {
    return fail(e, "Effect has no audio stream endpoint");
  }
  bool animated_frame(FieldObjectId, uint32_t, std::string &e) override {
    return fail(e, "Effect uses ordinary Sprite, not AnimatedSprite");
  }
  bool animated_playing(FieldObjectId, bool, std::string &e) override {
    return fail(e, "Effect has no AnimatedSprite clock");
  }
  bool native_offset(FieldObjectId id, Vec2 value, std::string &e) override {
    return owner->animation_->sprite_offset(id, value, e);
  }
  bool shader_number(FieldObjectId id, std::string_view name, double value,
                     std::string &e) override {
    auto s = owner->animation_->sprite(id);
    if (!s || !std::isfinite(float(value)))
      return fail(e, "Effect material number rejected");
    return owner->resources_->set_shader_parameter(s->material, name, 1,
                                                   {float(value), 0, 0, 0}, e);
  }
  bool shader_color(FieldObjectId id, std::string_view name, FieldColor value,
                    std::string &e) override {
    auto s = owner->animation_->sprite(id);
    return s ? owner->resources_->set_shader_parameter(s->material, name, 2,
                                                       value, e)
             : fail(e, "Effect actual material Sprite absent");
  }
  PodunkPlayerVisualNative *visual(FieldObjectId) override { return nullptr; }
  bool texture(uint32_t source, FieldObjectId &out, std::string &e) override {
    return owner->resources_->construct_effect_resource(kind, source, root, out,
                                                        e);
  }
  bool material(uint32_t source, FieldObjectId &out, std::string &e) override {
    return owner->resources_->construct_effect_resource(kind, source, root, out,
                                                        e);
  }
  bool stream(uint32_t, FieldObjectId &, std::string &e) override {
    return fail(e, "Effect source has no stream resources");
  }
  bool signal(FieldObjectId id, std::string_view name, std::string_view clip,
              std::string &e) override {
    return owner->signals_->emit(id, name, {std::string(clip)}, e);
  }
};
struct PodunkConcretePlayerEffectOwners::Instance {
  Endpoint endpoint;
  FieldObjectId animation = 0;
};
PodunkConcretePlayerEffectOwners::PodunkConcretePlayerEffectOwners() = default;
PodunkConcretePlayerEffectOwners::~PodunkConcretePlayerEffectOwners() = default;
bool PodunkConcretePlayerEffectOwners::initialize(
    const PlayerEffectsData &d, const PlayerInitializationData &p,
    const PlayerResourcesData &rd, FieldGlobalRegistry &r,
    FieldGlobalConstructorRuntime &global, FieldObjectSignals &signals,
    PodunkPlayerResources &resources, PodunkPlayerAnimation &animation,
    std::string &e) {
  if (data_ || !d.valid() || !p.valid() || !rd.valid() || !rd.effects_bound() ||
      r.poisoned() || signals.registry() != &r || !global.data() ||
      d.player_ir_sha256() != p.ir_sha256() ||
      rd.initialization_ir_sha256() != p.ir_sha256())
    return fail(e,
                "Effect owning assembly source/ObjectDB prerequisites absent");
  data_ = &d;
  player_ = &p;
  resource_data_ = &rd;
  registry_ = &r;
  global_ = &global;
  signals_ = &signals;
  resources_ = &resources;
  animation_ = &animation;
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::bind_scripts(PodunkPlayerEffectsHost &s,
                                                    std::string &e) {
  if (!live(e) || scripts_)
    return fail(e, "Effect script owner absent/rebound");
  for (uint32_t k = 0; k < 2; ++k) {
    FieldObjectId actual = 0;
    if (!scenes_[k] ||
        !s.preload(k, *data_->recipe(k), *data_->native(k), actual, e) ||
        actual != scenes_[k])
      return fail(
          e, "Effect script preload belongs to another actual source owner");
  }
  scripts_ = &s;
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::live(std::string &e) const {
  return data_ && data_->valid() && resource_data_->valid() &&
                 resource_data_->effects_bound() && registry_ &&
                 !registry_->poisoned()
             ? true
             : fail(e, "Effect actual owning assembly unavailable");
}
bool PodunkConcretePlayerEffectOwners::kind(const FieldNodeRecipeData &r,
                                            const GlobalYamlValue &n,
                                            uint32_t &k, std::string &e) const {
  if (!live(e))
    return false;
  for (uint32_t i = 0; i < 2; ++i)
    if (data_->recipe(i) == &r && data_->native(i).get() == &n) {
      k = i;
      return true;
    }
  return fail(e, "Effect factory is not the exact checked source owner");
}
bool PodunkConcretePlayerEffectOwners::accepts(const FieldIdentity &id) const {
  if (!data_)
    return false;
  for (uint32_t i = 0; i < 2; ++i)
    if (same(id, data_->recipe(i)->identity()))
      return true;
  return false;
}
bool PodunkConcretePlayerEffectOwners::owns(FieldObjectId id) const {
  auto i = nodes_.find(id);
  return i != nodes_.end() && registry_ &&
         registry_->tree_owner(id).get() == i->second.tree;
}
const PodunkConcretePlayerEffectOwners::Node *
PodunkConcretePlayerEffectOwners::node(FieldObjectId id, std::string &e) const {
  auto i = nodes_.find(id);
  if (!live(e) || i == nodes_.end() || !owns(id) ||
      !registry_->object_exists(id)) {
    fail(e, "Effect actual Node owner absent/retired");
    return nullptr;
  }
  return &i->second;
}
PodunkConcretePlayerEffectOwners::Node *
PodunkConcretePlayerEffectOwners::node(FieldObjectId id, std::string &e) {
  return const_cast<Node *>(
      static_cast<const PodunkConcretePlayerEffectOwners *>(this)->node(id, e));
}
bool PodunkConcretePlayerEffectOwners::admit_factory(
    const FieldNodeRecipeData &r, const GlobalYamlValue &native,
    std::string &e) const {
  uint32_t k = 0;
  if (!kind(r, native, k, e) || resources_->registry() != registry_)
    return false;
  for (const auto &d : r.records())
    if (d.native_generated ||
        (d.native_class != "Sprite" && d.native_class != "Node" &&
         d.native_class != "AnimationPlayer"))
      return fail(e, "Effect native subclass factory is not implemented");
  for (const auto &d : r.records()) {
    if (!d.script.empty() &&
        !((k == 0 && d.id == r.identity().scene_id) ||
          (k == 1 && d.script == data_->policy().tint_script)))
      return fail(e, "Effect source script constructor not implemented");
  }
  for (const auto &m : resource_data_->effect_resources())
    if (m.effect == k) {
      auto resource = resource_data_->effect_resource(k, m.source_id);
      if (!resource)
        return fail(e, "Effect exact resource binding absent");
      if (resource->kind < 3) {
        auto im = resource_data_->image(resource->texture);
        C2D_Image actual{};
        if (!im || !resources_->image(im->source, im->source_sha, actual, e) ||
            !actual.tex || !actual.tex->data || !actual.subtex)
          return fail(e, "Effect actual GPU texture closure absent");
      }
    }
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::load_packed_scene(
    const FieldNodeRecipeData &r, const GlobalYamlValue &native,
    FieldObjectId &out, std::string &e) {
  uint32_t k = 0;
  if (!kind(r, native, k, e) || !admit_factory(r, native, e))
    return false;
  if (scenes_[k]) {
    if (!registry_->source_resource(scenes_[k]))
      return fail(e, "Effect PackedScene owner retired");
    out = scenes_[k];
    e.clear();
    return true;
  }
  FieldGlobalExternalSpec spec;
  spec.identity = r.identity();
  spec.stable_id = r.identity().scene_id;
  spec.role = 4;
  spec.native_class = "PackedScene";
  spec.source = r.source_scene();
  spec.source_sha = r.identity().source_sha256;
  spec.name = spec.source;
  FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  FieldGlobalExternalBinding b{id, spec, 0x454e0059, 1};
  if (!registry_->publish_source_resource(
          spec, id,
          std::make_unique<PackedSceneOwner>(*data_, r, data_->native(k),
                                             *registry_, b),
          e)) {
    std::string ignored;
    registry_->retire_object(id, ignored);
    return false;
  }
  scenes_[k] = id;
  out = id;
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::construct(FieldNodeTreeRuntime &tree,
                                                 FieldObjectId id,
                                                 const FieldNodeDescriptor &d,
                                                 const FieldIdentity &identity,
                                                 std::string &e) {
  if (!live(e) || !accepts(identity) || nodes_.count(id) ||
      registry_->tree_owner(id).get() != &tree || !registry_->object_exists(id))
    return fail(e, "Effect source constructor actual Node unavailable");
  uint32_t k = same(identity, data_->recipe(0)->identity()) ? 0 : 1;
  auto recipe = data_->recipe(k);
  auto expected = recipe->record(d.id);
  auto state = tree.state(id);
  if (!expected || expected->path != d.path ||
      expected->native_class != d.native_class ||
      expected->script_sha != d.script_sha || !state || state->inside ||
      state->ready_notified)
    return fail(e, "Effect source constructor descriptor/lifecycle differs");
  auto key = std::make_pair(&tree, k);
  FieldObjectId root = 0;
  if (d.path == ".") {
    root = id;
    constructing_[key] = root;
    auto instance = std::make_unique<Instance>();
    instance->endpoint.owner = this;
    instance->endpoint.root = root;
    instance->endpoint.kind = k;
    instances_.emplace(root, std::move(instance));
  } else {
    auto i = constructing_.find(key);
    if (i == constructing_.end())
      return fail(e, "Effect constructor source root order differs");
    root = i->second;
  }
  nodes_.emplace(id, Node{k, root, &tree, d, false});
  if (d.native_class == "Sprite") {
    auto props = get(source_node(data_->native(k), d.path), "properties");
    PodunkPlayerSpriteState sprite;
    sprite.object = id;
    bool region = false;
    if (!props || !integer(get(props, "hframes"), sprite.columns) ||
        !integer(get(props, "vframes"), sprite.rows) ||
        !integer(get(props, "frame"), sprite.frame) ||
        !vector(get(props, "offset"), sprite.offset) ||
        !boolean(get(props, "centered"), sprite.centered) ||
        !boolean(get(props, "flip_h"), sprite.flip_h) ||
        !boolean(get(props, "flip_v"), sprite.flip_v) ||
        !boolean(get(props, "region_enabled"), region) || region)
      return fail(e, "Effect actual native Sprite defaults rejected");
    auto texture = get(props, "texture"), material = get(props, "material");
    if (!texture || !material)
      return fail(e, "Effect Sprite native resources missing");
    if (texture->kind != 0 &&
        (!reference(texture, sprite.texture_source) ||
         !resources_->construct_effect_resource(k, sprite.texture_source, root,
                                                sprite.texture, e)))
      return false;
    if (material->kind != 0 &&
        (!reference(material, sprite.material_source) ||
         !resources_->construct_effect_resource(k, sprite.material_source, root,
                                                sprite.material, e)))
      return false;
    if (sprite.texture) {
      C2D_Image image{};
      if (!resources_->texture(sprite.texture, image, e) || !sprite.columns ||
          !sprite.rows || image.subtex->width % sprite.columns ||
          image.subtex->height % sprite.rows)
        return fail(e, "Effect actual GPU Sprite frame grid invalid");
    }
    if (!animation_->construct_native_sprite(sprite, e))
      return false;
  } else if (d.native_class != "Node" && d.native_class != "AnimationPlayer")
    return fail(e, "Effect native constructor subclass unsupported");
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::bind(FieldObjectId id,
                                            const FieldNodeDescriptor &d,
                                            FieldNodeBinding &out,
                                            std::string &e) {
  auto n = node(id, e);
  if (!n || n->descriptor.id != d.id ||
      n->descriptor.script_sha != d.script_sha)
    return false;
  if (d.native_class == "AnimationPlayer" && !n->animation_constructed) {
    auto instance = instances_.find(n->root);
    if (instance == instances_.end() ||
        !animation_->construct_effect(*data_, n->kind, *n->tree, *registry_,
                                      n->root, id, instance->second->endpoint,
                                      e))
      return false;
    instance->second->animation = id;
    n->animation_constructed = true;
  }
  out = {data_->recipe(n->kind)->identity(),
         d.id,
         d.class_index,
         0x454e0059,
         1,
         d.script_sha,
         d.native_class};
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::phase(FieldObjectId id,
                                             const FieldNodeBinding &b,
                                             FieldTreePhase p, float delta,
                                             bool paused, std::string &e) {
  auto n = node(id, e);
  if (!n || !same(b.identity, data_->recipe(n->kind)->identity()) ||
      b.stable_id != n->descriptor.id ||
      b.script_sha != n->descriptor.script_sha)
    return fail(e, "Effect actual lifecycle binding differs");
  if (p == FieldTreePhase::ReadyScript && !n->descriptor.script.empty()) {
    if (!scripts_)
      return fail(e, "Effect actual script consumer not bound");
    return scripts_->dynamic_script_ready(id, e);
  }
  if (n->descriptor.native_class == "AnimationPlayer") {
    if (p == FieldTreePhase::ReadyNative)
      return animation_->ready(id, p, b, e);
    if (p == FieldTreePhase::IdleInternal ||
        p == FieldTreePhase::PhysicsInternal)
      return animation_->process(id, p, delta, paused, e);
  }
  switch (p) {
  case FieldTreePhase::Input:
  case FieldTreePhase::UnhandledInput:
  case FieldTreePhase::UnhandledKeyInput:
    return fail(e, "Effect source has no input callbacks");
  default:
    break;
  }
  // Node/Sprite native notifications are already applied by the one Tree.
  // Their source scripts have no other implemented process/input methods.
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::signal_declared(FieldObjectId id,
                                                       std::string_view name,
                                                       uint32_t &arguments,
                                                       std::string &e) const {
  auto n = node(id, e);
  if (!n || n->descriptor.native_class != "AnimationPlayer" ||
      (name != "animation_started" && name != "animation_finished"))
    return fail(e, "Effect signal native declaration absent");
  arguments = 1;
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::connect_finished(
    FieldObjectId animation, std::string_view signal, FieldObjectId receiver,
    std::string_view method, FieldObjectId bind, bool one_shot,
    std::function<bool(std::string_view, FieldObjectId, std::string &)> call,
    std::string &e) {
  uint32_t argc = 0;
  if (!signal_declared(animation, signal, argc, e) || !call ||
      !registry_->object_exists(receiver) ||
      (bind && !registry_->object_exists(bind)))
    return false;
  bool source = false;
  if (!bind && !one_shot) {
    auto n = node(receiver, e);
    source = n && n->kind == 0 &&
             signal == data_->policy().after_finished_signal &&
             method == data_->policy().after_finished_method;
  } else
    for (const auto &c : data_->creators())
      if (c.kind == 1 && signal == c.finished_signal &&
          method == c.finished_method && bind && one_shot) {
        auto tree = registry_->tree_owner(receiver);
        auto d = tree ? tree->descriptor(receiver) : nullptr;
        FieldIdentity identity{};
        auto dust = node(bind, e);
        source = d && d->id == c.id && d->script_sha == c.script_sha &&
                 tree->object_identity(receiver, identity) &&
                 same(identity, player_->recipe().identity()) && dust &&
                 dust->kind == 1 && dust->root == bind;
      }
  auto key = std::make_tuple(receiver, std::string(method), bind);
  if (!source || finished_.count(key))
    return fail(e, "Effect finished connection source/duplicate rejected");
  std::vector<FieldDeferredValue> binds;
  if (bind)
    binds.emplace_back(FieldObjectRef{bind});
  if (!signals_->connect(animation, signal, receiver, method,
                         one_shot ? uint32_t(FieldSignalOneShot) : 0u,
                         std::move(binds), e))
    return false;
  finished_.emplace(key, Finished{bind, one_shot, std::move(call)});
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::handles_callback(
    const FieldDeferredMessage &m) const {
  if (!data_ || m.kind != FieldDeferredKind::Call || m.args.empty())
    return false;
  auto clip = std::get_if<std::string>(&m.args[0]);
  auto bind =
      m.args.size() == 2 ? std::get_if<FieldObjectRef>(&m.args[1]) : nullptr;
  if (!clip || (m.args.size() != 1 && m.args.size() != 2) ||
      (m.args.size() == 2 && !bind))
    return false;
  return finished_.count({m.object, m.member, bind ? bind->id : 0}) != 0;
}
bool PodunkConcretePlayerEffectOwners::deferred(const FieldDeferredMessage &m,
                                                std::string &e) {
  if (!live(e) || m.kind != FieldDeferredKind::Call)
    return fail(e, "Effect callback kind rejected");
  auto clip = m.args.empty() ? nullptr : std::get_if<std::string>(&m.args[0]);
  auto bind =
      m.args.size() == 2 ? std::get_if<FieldObjectRef>(&m.args[1]) : nullptr;
  auto i = finished_.find({m.object, m.member, bind ? bind->id : 0});
  if (i == finished_.end())
    return fail(e, "Effect callback has no actual source connection");
  if (!clip || m.args.size() != (i->second.bind ? 2u : 1u) ||
      (i->second.bind && (!bind || bind->id != i->second.bind)))
    return fail(e, "Effect actual signal payload differs");
  auto slot = i->second;
  bool ok = slot.call(*clip, slot.bind, e);
  if (slot.one_shot)
    finished_.erase(i);
  return ok;
}
bool PodunkConcretePlayerEffectOwners::party_size(size_t &out,
                                                  std::string &e) const {
  if (!live(e))
    return false;
  const PlayerEffectCreator *dust = nullptr;
  for (const auto &c : data_->creators())
    if (c.kind == 1)
      dust = &c;
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!dust || !global_->party_array(dust->party_member, party, e) || !party)
    return false;
  out = party->values.size();
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::play(FieldObjectId id,
                                            std::string_view clip,
                                            std::string &e) {
  auto n = node(id, e);
  return n && n->descriptor.native_class == "AnimationPlayer"
             ? animation_->play(id, clip, e)
             : fail(e, "Effect animation actual clock absent");
}
bool PodunkConcretePlayerEffectOwners::duplicate_sprite(FieldObjectId,
                                                        FieldObjectId &,
                                                        std::string &e) {
  return fail(e,
              "Player Sprite duplicate requires actual Emotes subtree owner");
}
bool PodunkConcretePlayerEffectOwners::copy_sprite(
    FieldObjectId, FieldObjectId, const std::vector<std::string> &,
    std::string &e) {
  return fail(e, "Effect Sprite copy requires full actual source duplicate");
}
bool PodunkConcretePlayerEffectOwners::draw(FieldObjectId id, Vec2 camera,
                                            std::string &e) const {
  auto n = node(id, e);
  if (!n || n->descriptor.native_class != "Sprite")
    return fail(e, "Effect draw actual Sprite absent");
  auto sprite = animation_->sprite(id);
  if (!sprite)
    return fail(e, "Effect native Sprite state absent");
  if (!sprite->texture) {
    e.clear();
    return true;
  } // Original null-texture Sprite draws nothing.
  PlayerEffectGpuSample sample;
  sample.object = id;
  if (!resources_->texture(sprite->texture, sample.image, e))
    return false;
  auto resource =
      resource_data_->effect_resource(n->kind, sprite->texture_source);
  auto im = resource ? resource_data_->image(resource->texture) : nullptr;
  if (!im)
    return fail(e, "Effect actual texture source projection absent");
  sample.source = im->source;
  sample.source_sha = im->source_sha;
  sample.columns = sprite->columns;
  sample.rows = sprite->rows;
  sample.frame = sprite->frame;
  sample.offset = sprite->offset;
  sample.centered = sprite->centered;
  sample.flip_h = sprite->flip_h;
  sample.flip_v = sprite->flip_v;
  sample.visible = (n->tree->state(id)->flags & 2) != 0;
  if (sprite->material) {
    auto mat =
        resource_data_->effect_resource(n->kind, sprite->material_source);
    if (!mat)
      return fail(e, "Effect actual material source projection absent");
    if (mat->kind == 4) {
      if (mat->parameters.size() != 2)
        return fail(e, "Effect native blend policy absent");
      sample.additive = mat->parameters[0].value[0] == 1;
    } else if (mat->kind == 3) {
      std::vector<PlayerResourceParameter> parameters;
      if (!resources_->shader_parameters(sprite->material, parameters, e) ||
          parameters.size() != 4)
        return fail(e, "Effect actual Flash source parameters absent");
      sample.flash_material = true;
      sample.flash_color = parameters[0].value;
      sample.flash_modifier = parameters[2].value[0];
      sample.glow_modifier = parameters[3].value[0];
    } else
      return fail(e, "Effect actual material kernel unsupported");
  }
  return PlayerEffectsRenderer{}.draw(*data_, *registry_, sample, camera, e);
}
bool PodunkConcretePlayerEffectOwners::release(FieldObjectId id,
                                               std::string &e) {
  if (!live(e))
    return false;
  auto i = nodes_.find(id);
  if (i == nodes_.end() || !i->second.tree->state(id) ||
      i->second.tree->state(id)->inside ||
      !i->second.tree->state(id)->children.empty())
    return fail(e, "Effect owner release precedes actual Node PREDELETE");
  if (i->second.descriptor.native_class == "AnimationPlayer" &&
      i->second.animation_constructed && !animation_->release_effect(id, e))
    return false;
  if (scripts_ && !scripts_->core().release(id, e))
    return false;
  if (id == i->second.root)
    retired_roots_.push_back(id);
  retired_.push_back(id);
  for (auto f = finished_.begin(); f != finished_.end();) {
    if (std::get<0>(f->first) == id || f->second.bind == id)
      f = finished_.erase(f);
    else
      ++f;
  }
  nodes_.erase(i);
  instances_.erase(id);
  e.clear();
  return true;
}
bool PodunkConcretePlayerEffectOwners::collect_retired(std::string &e) {
  if (!live(e))
    return false;
  for (auto i = retired_.begin(); i != retired_.end();) {
    if (registry_->object_exists(*i)) {
      ++i;
      continue;
    }
    if (!signals_->release(*i, e))
      return false;
    i = retired_.erase(i);
  }
  for (auto i = retired_roots_.begin(); i != retired_roots_.end();) {
    if (registry_->object_exists(*i)) {
      ++i;
      continue;
    }
    if (!resources_->release_player(*i, e))
      return false;
    i = retired_roots_.erase(i);
  }
  e.clear();
  return true;
}
} // namespace encore::ctr
