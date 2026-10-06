#include "podunk_player_animation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace encore::ctr {
using namespace upstream;
namespace {
using Raw = std::shared_ptr<const GlobalYamlValue>;
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
Raw get(const Raw &v, std::string_view k) {
  return v && v->kind == 6 ? v->get(k) : nullptr;
}
bool text(const Raw &v, std::string &s) {
  if (!v || v->kind != 4)
    return false;
  s = v->string;
  return true;
}
bool boolean(const Raw &v, bool &b) {
  if (!v || v->kind != 1)
    return false;
  b = v->boolean;
  return true;
}
bool number(const Raw &v, double &n) {
  if (!v)
    return false;
  if (v->kind == 2) {
    n = double(v->integer);
    return true;
  }
  std::string t, s;
  if (!text(get(v, "type"), t) || (t != "real" && t != "int64") ||
      !text(get(v, "value"), s) || s.empty())
    return false;
  char *end = nullptr;
  n = std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size() && std::isfinite(n);
}
bool uint(const Raw &v, uint32_t &n) {
  double d = 0;
  if (!number(v, d) || d < 0 || d > UINT32_MAX || std::floor(d) != d)
    return false;
  n = uint32_t(d);
  return true;
}
bool vector(const Raw &v, Vec2 &p) {
  std::string t;
  double x = 0, y = 0;
  if (!text(get(v, "type"), t) || t != "Vector2" || !number(get(v, "x"), x) ||
      !number(get(v, "y"), y) || !std::isfinite(float(x)) ||
      !std::isfinite(float(y)))
    return false;
  p = {float(x), float(y)};
  return true;
}
bool reference(const Raw &v, uint32_t &id) {
  std::string t;
  return text(get(v, "type"), t) &&
         (t == "ResourceReference" || t == "Resource") &&
         uint(get(v, "id"), id) && id;
}
Raw array(const Raw &v, std::string_view tag) {
  std::string t;
  if (!text(get(v, "type"), t) || t != tag)
    return nullptr;
  auto a = get(v, "value");
  return a && a->kind == 5 ? a : nullptr;
}
Raw pair(const Raw &v, std::string_view key) {
  std::string t;
  if (!text(get(v, "type"), t) || t != "Dictionary")
    return nullptr;
  auto a = get(v, "pairs");
  if (!a || a->kind != 5)
    return nullptr;
  Raw out;
  for (const auto &p : a->array) {
    if (!p || p->kind != 5 || p->array.size() != 2)
      return nullptr;
    std::string k;
    if (!text(p->array[0], k))
      return nullptr;
    if (k == key) {
      if (out)
        return nullptr;
      out = p->array[1];
    }
  }
  return out;
}
float fpos(float t, float l) {
  float v = std::fmod(t, l);
  return v < 0 ? v + l : v;
}
bool resource_owner(const PlayerInitializationData &d,
                    const FieldGlobalRegistry &registry, uint32_t source_id,
                    FieldObjectId actual_id, uint32_t kind, std::string &e) {
  auto resources = get(d.native_source(), "resources");
  Raw record;
  if (!resources || resources->kind != 5)
    return fail(e, "Player native resource projection unavailable");
  for (const auto &r : resources->array) {
    uint32_t id = 0;
    if (!uint(get(r, "id"), id))
      return fail(e, "Player native resource identity malformed");
    if (id == source_id)
      record = r;
  }
  std::string cls, path;
  if (!record || !text(get(record, "class"), cls) ||
      !text(get(record, "path"), path) ||
      (kind == 1   ? cls != "StreamTexture"
       : kind == 0 ? cls != "ShaderMaterial"
                   : cls != "AudioStreamMP3"))
    return fail(e, "Player native resource class unsupported");
  const auto *owner = registry.source_resource(actual_id);
  if (!owner || !registry.object_exists(actual_id) ||
      cls != owner->resource_class())
    return fail(e, "Player native resource actual owning class differs");
  auto binding = owner->binding();
  if (binding.object != actual_id ||
      binding.source.identity.upstream_commit != d.identity().upstream_commit)
    return fail(e, "Player native resource owning pin differs");
  if (path.empty())
    path = d.recipe().source_scene();
  if (path.rfind("res://", 0) == 0)
    path.erase(0, 6);
  auto internal = path.find("::");
  if (internal != std::string::npos)
    path.resize(internal);
  std::array<uint8_t, 32> sha{};
  if (!d.source_hash(path, sha) || binding.source.source != path ||
      binding.source.source_sha != sha)
    return fail(e, "Player native resource source ownership differs");
  return true;
}
} // namespace
bool PodunkPlayerAnimation::live(std::string &e) const {
  auto s = tree_ ? tree_->state(animation_) : nullptr;
  if (!data_ || poisoned_ || !registry_ || registry_->poisoned() || !s ||
      !s->alive || s->queued || !registry_->object_exists(animation_) ||
      registry_->tree_owner(animation_).get() != tree_ ||
      tree_->object_domain() != registry_->kernel())
    return fail(e, "Player AnimationPlayer actual owner expired");
  return true;
}
bool PodunkPlayerAnimation::checked_resource(uint32_t source,
                                             FieldObjectId actual, bool texture,
                                             std::string &e) const {
  return data_ && registry_ &&
         resource_owner(*data_, *registry_, source, actual, texture, e);
}
bool PodunkPlayerAnimation::parse_value(const Raw &r, Value &v,
                                        std::string &e) const {
  if (!r)
    return fail(e, "Player animation value absent");
  if (r->kind == 0) {
    v.kind = 0;
    return true;
  }
  if (r->kind == 1) {
    v.kind = 1;
    v.boolean = r->boolean;
    return true;
  }
  if (r->kind == 4) {
    v.kind = 4;
    v.string = r->string;
    return true;
  }
  std::string t;
  if (!text(get(r, "type"), t))
    return fail(e, "Player animation value type unsupported");
  if (t == "int64" || t == "real") {
    v.kind = t == "int64" ? 2 : 3;
    if (!number(r, v.number))
      return fail(e, "Player animation number malformed");
    return true;
  }
  if (t == "Vector2") {
    v.kind = 5;
    if (!vector(r, v.vector))
      return fail(e, "Player animation vector malformed");
    return true;
  }
  if (t == "Color") {
    v.kind = 7;
    const char *fields[] = {"r", "g", "b", "a"};
    for (size_t i = 0; i < 4; ++i) {
      double n = 0;
      if (!number(get(r, fields[i]), n) || !std::isfinite(float(n)))
        return fail(e, "Player animation Color malformed");
      v.color[i] = float(n);
    }
    return true;
  }
  if (t == "ResourceReference") {
    v.kind = 6;
    if (!reference(r, v.resource))
      return fail(e, "Player animation resource malformed");
    return true;
  }
  return fail(e, "Player animation new value type requires native consumer");
}
bool PodunkPlayerAnimation::construct(
    const PlayerInitializationData &d, const PlayerReadyData &g,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, FieldObjectId player,
    PodunkPlayerAnimationEndpoints &port, std::string &e) {
  FieldObjectId main = 0;
  if (!t.get_node(player, g.binding(PlayerReadyBinding::AnimationPath), main,
                  e))
    return false;
  sprite_owner_ = this;
  if (!construct_one(d, g, t, r, player, main, true, port, e))
    return false;
  for (const auto &record : d.recipe().records()) {
    if (record.native_class != "AnimationPlayer")
      continue;
    FieldObjectId id = 0;
    if (!t.get_node(player, record.path, id, e)) {
      poisoned_ = true;
      return false;
    }
    if (id == main)
      continue;
    auto child = std::make_unique<PodunkPlayerAnimation>();
    child->sprite_owner_ = this;
    if (!child->construct_one(d, g, t, r, player, id, false, port, e)) {
      poisoned_ = true;
      return false;
    }
    children_.emplace(id, std::move(child));
  }
  return true;
}
bool PodunkPlayerAnimation::construct_one(
    const PlayerInitializationData &d, const PlayerReadyData &g,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, FieldObjectId player,
    FieldObjectId id, bool main, PodunkPlayerAnimationEndpoints &port,
    std::string &e) {
  FieldIdentity actual{};
  if (data_ || !d.valid() || !g.valid() ||
      g.initialization_ir_sha256() != d.ir_sha256() ||
      g.identity().upstream_commit != d.identity().upstream_commit ||
      g.identity().source_sha256 != d.identity().source_sha256 ||
      !t.object_identity(player, actual) ||
      actual.upstream_commit != d.identity().upstream_commit ||
      actual.source_sha256 != d.identity().source_sha256 ||
      r.tree_owner(player).get() != &t || t.object_domain() != r.kernel())
    return fail(e, "Player AnimationPlayer source ObjectDB binding rejected");
  auto desc = t.descriptor(id);
  if (!desc || desc->native_class != "AnimationPlayer" || !desc->script.empty())
    return fail(e, "Player AnimationPlayer native class/script rejected");
  auto nodes = get(d.native_source(), "nodes"),
       resources = get(d.native_source(), "resources");
  if (!nodes || nodes->kind != 5 || !resources || resources->kind != 5)
    return fail(e, "Player animation native snapshot unavailable");
  std::map<uint32_t, Raw> source_resources;
  std::map<std::string, Raw> source_nodes;
  for (auto &v : resources->array) {
    uint32_t n = 0;
    if (!uint(get(v, "id"), n) || !source_resources.emplace(n, v).second)
      return fail(e, "Player animation native resource identity rejected");
  }
  for (auto &v : nodes->array) {
    std::string p;
    if (!text(get(v, "path"), p) || !source_nodes.emplace(p, v).second)
      return fail(e, "Player animation native node identity rejected");
  }
  auto rec = d.recipe().record(desc->id);
  if (!rec || !source_nodes.count(rec->path))
    return fail(e, "Player animation source node absent");
  auto props = get(source_nodes.at(rec->path), "properties");
  std::string root_path;
  uint32_t mode = 0, method_mode = 0;
  double speed = 0, blend_time = 0;
  if (!text(get(get(props, "root_node"), "value"), root_path) ||
      !uint(get(props, "playback_process_mode"), mode) || mode > 1 ||
      !uint(get(props, "method_call_mode"), method_mode) || method_mode != 0 ||
      !number(get(props, "playback_speed"), speed) || speed != 1 ||
      !number(get(props, "playback_default_blend_time"), blend_time) ||
      blend_time != 0 || !text(get(props, "autoplay"), autoplay_))
    return fail(
        e, "Player AnimationPlayer native mode requires additional consumer");
  FieldObjectId root = 0;
  if (!t.get_node(id, root_path, root, e))
    return false;
  auto native_array = get(get(props, "blend_times"), "value");
  if (!native_array || native_array->kind != 5 || !native_array->array.empty())
    return fail(e, "Player AnimationPlayer blend-time source unsupported");
  // Build a candidate completely before publishing the owning service.
  std::map<std::string, Clip> clips;
  auto &sprites = sprite_owner_->sprites_;
  for (const auto &entry : props->dictionary) {
    if (entry.first.rfind("anims/", 0) != 0)
      continue;
    Clip c;
    std::string cls;
    uint32_t n = 0;
    double length = 0;
    if (!reference(entry.second, n) || !source_resources.count(n) ||
        !text(get(source_resources.at(n), "class"), cls) || cls != "Animation")
      return fail(e, "Player Animation clip resource rejected");
    auto p = get(source_resources.at(n), "properties");
    if (!number(get(p, "length"), length) || length <= 0 ||
        !std::isfinite(float(length)) || !boolean(get(p, "loop"), c.loop))
      return fail(e, "Player Animation clip length/loop rejected");
    c.resource = n;
    c.length = float(length);
    for (uint32_t index = 0;; ++index) {
      std::string prefix = "tracks/" + std::to_string(index) + "/", type, path;
      auto kind = get(p, prefix + "type");
      if (!kind)
        break;
      Track track;
      track.index = index;
      if (!text(kind, type) || (type != "value" && type != "method") ||
          !text(get(get(p, prefix + "path"), "value"), path) ||
          !uint(get(p, prefix + "interp"), track.interpolation) ||
          track.interpolation > 1 ||
          !boolean(get(p, prefix + "loop_wrap"), track.wrap) ||
          !boolean(get(p, prefix + "enabled"), track.enabled))
        return fail(e, "Player animation track schema unsupported");
      track.source_path = path;
      track.method = type == "method";
      size_t colon = path.find(':');
      std::string node = path.substr(0, colon);
      if (!t.get_node(root, node, track.target, e))
        return false;
      if (!track.method) {
        if (colon == std::string::npos)
          return fail(e, "Player animation property subpath unsupported");
        track.member = path.substr(colon + 1);
      } else if (colon != std::string::npos)
        return fail(e, "Player animation method subpath unsupported");
      auto keys = get(p, prefix + "keys");
      auto times = array(pair(keys, "times"), "PoolRealArray"),
           transitions = array(pair(keys, "transitions"), "PoolRealArray"),
           values = array(pair(keys, "values"), "Array");
      if (!times || !transitions || !values ||
          times->array.size() != values->array.size() ||
          times->array.size() != transitions->array.size() ||
          times->array.empty() ||
          (!track.method &&
           (!uint(pair(keys, "update"), track.update) || track.update > 2)))
        return fail(e, "Player animation key schema unsupported");
      for (size_t i = 0; i < times->array.size(); ++i) {
        Key k;
        double time = 0, tr = 0;
        if (!number(times->array[i], time) ||
            !number(transitions->array[i], tr) || time < 0 ||
            !std::isfinite(float(time)) || !std::isfinite(float(tr)) ||
            tr < 0 || (i && time < track.keys.back().time))
          return fail(e, "Player animation key time/ease rejected");
        k.time = float(time);
        k.transition = float(tr);
        if (track.method) {
          std::string m;
          if (!text(pair(values->array[i], "method"), m))
            return fail(e, "Player animation method name absent");
          auto args = array(pair(values->array[i], "args"), "Array");
          if (!args || !args->array.empty() ||
              !port.admit(track.target, m, true, e))
            return fail(e, "Player animation actual method owner unavailable");
          k.value.kind = 4;
          k.value.string = m;
        } else if (!parse_value(values->array[i], k.value, e))
          return false;
        if (k.value.kind == 6) {
          FieldObjectId resource = 0;
          bool is_stream = track.member == "stream";
          if (!(is_stream ? port.stream(k.value.resource, resource, e)
                          : port.texture(k.value.resource, resource, e)) ||
              !resource ||
              !resource_owner(d, r, k.value.resource, resource,
                              is_stream ? 2u : 1u, e))
            return fail(
                e, "Player animation Texture actual Resource owner absent");
        }
        track.keys.push_back(std::move(k));
      }
      auto target = t.descriptor(track.target);
      if (!target)
        return fail(e, "Player animation actual target descriptor absent");
      bool canvas = track.member == "visible" || track.member == "position" ||
                    track.member == "rotation_degrees" ||
                    track.member == "show_behind_parent";
      bool shader = track.member.rfind("material:shader_param/", 0) == 0;
      bool sprite_property =
          target->native_class == "Sprite" &&
          (track.member == "frame" || track.member == "texture" ||
           track.member == "offset" || shader);
      bool animated = target->native_class == "AnimatedSprite" &&
                      (track.member == "frame" || track.member == "playing" ||
                       track.member == "offset");
      if (canvas && !(t.state(track.target)->flags & 1u))
        return fail(e, "Player animation Canvas target class rejected");
      if (!track.method && !canvas && !sprite_property &&
          track.member != "disabled" && track.member != "playing" &&
          track.member != "offset" && track.member != "modulate" &&
          track.member != "stream" && !animated && !shader)
        return fail(e, "Player animation unsupported property target");
      if (!track.method &&
          (track.member == "disabled" || track.member == "playing" ||
           track.member == "stream" ||
           (!sprite_property && track.member == "offset") || animated ||
           shader) &&
          !port.admit(track.target, track.member, false, e))
        return false;
      if (sprite_property) {
        if (target->native_class != "Sprite")
          return fail(e, "Player animation Sprite property class rejected");
        if (!port.visual(track.target) && !sprites.count(track.target)) {
          auto rr = d.recipe().record(target->id);
          if (!rr || !source_nodes.count(rr->path))
            return fail(e, "Player Sprite checked source absent");
          auto sp = get(source_nodes.at(rr->path), "properties");
          PodunkPlayerSpriteState s;
          s.object = track.target;
          bool region = false;
          if (!reference(get(sp, "texture"), s.texture_source) ||
              !uint(get(sp, "hframes"), s.columns) ||
              !uint(get(sp, "vframes"), s.rows) || !s.columns || !s.rows ||
              uint64_t(s.columns) * s.rows > UINT32_MAX ||
              !uint(get(sp, "frame"), s.frame) ||
              s.frame >= s.columns * s.rows ||
              !vector(get(sp, "offset"), s.offset) ||
              !boolean(get(sp, "centered"), s.centered) ||
              !boolean(get(sp, "flip_h"), s.flip_h) ||
              !boolean(get(sp, "flip_v"), s.flip_v) ||
              !boolean(get(sp, "region_enabled"), region) || region ||
              !port.texture(s.texture_source, s.texture, e) || !s.texture ||
              !resource_owner(d, r, s.texture_source, s.texture, true, e))
            return fail(e, "Player Sprite source state/resource rejected");
          auto material = get(sp, "material");
          if (material && material->kind != 0 &&
              !reference(material, s.material_source))
            return fail(e, "Player Sprite material source rejected");
          if (s.material_source &&
              (!port.material(s.material_source, s.material, e) ||
               !s.material ||
               !resource_owner(d, r, s.material_source, s.material, false, e)))
            return fail(e, "Player Sprite actual ShaderMaterial owner absent");
          sprites.emplace(s.object, s);
        }
      }
      if (!track.method)
        for (const auto &key : track.keys) {
          auto &v = key.value;
          bool ok = false;
          if (track.member == "visible" ||
              track.member == "show_behind_parent" ||
              track.member == "disabled" || track.member == "playing")
            ok = v.kind == 1;
          else if (track.member == "position" || track.member == "offset")
            ok = v.kind == 5;
          else if (track.member == "rotation_degrees")
            ok = v.kind == 2 || v.kind == 3;
          else if (track.member == "texture")
            ok = v.kind == 6;
          else if (track.member == "stream")
            ok = v.kind == 0 || v.kind == 6;
          else if (track.member == "modulate")
            ok = v.kind == 7;
          else if (shader)
            ok = v.kind == 2 || v.kind == 3 || v.kind == 7;
          else if (track.member == "frame")
            ok = v.kind == 2 && v.number >= 0 && v.number <= UINT32_MAX &&
                 std::floor(v.number) == v.number;
          if (!ok)
            return fail(e,
                        "Player animation native member/key type unsupported");
        }
      c.tracks.push_back(std::move(track));
    }
    size_t source_tracks = 0;
    for (const auto &property : p->dictionary)
      if (property.first.rfind("tracks/", 0) == 0 &&
          property.first.size() >= 5 &&
          property.first.compare(property.first.size() - 5, 5, "/type") == 0)
        ++source_tracks;
    if (source_tracks != c.tracks.size())
      return fail(e, "Player animation sparse/unknown track indices rejected");
    if (!clips.emplace(entry.first.substr(6), std::move(c)).second)
      return fail(e, "Player animation duplicate clip");
  }
  if (clips.empty() || (!autoplay_.empty() && !clips.count(autoplay_)))
    return fail(e, "Player AnimationPlayer autoplay clip unavailable");
  if (main)
    for (auto &s : g.states())
      for (auto &p : s.points) {
        auto c = clips.find(p.clip);
        if (c == clips.end() || c->second.resource != p.clip_id ||
            c->second.length != p.length || c->second.loop != p.loop)
          return fail(e, "Player graph/source clip cross-binding differs");
      }
  // Native NodePath hash, then native HashMap bucket traversal. This audited
  // source fits the engine's initial eight buckets (no resize). Game target
  // strings and insertion order are read from the complete source resource.
  std::array<std::vector<std::pair<FieldObjectId, std::string>>, 8> buckets;
  std::map<std::string, bool> paths;
  for (const auto &clip : clips)
    for (const auto &track : clip.second.tracks) {
      if (!paths.emplace(track.source_path, true).second)
        continue;
      uint32_t hash = 0;
      size_t start = 0;
      bool subpath = false;
      for (size_t i = 0; i <= track.source_path.size(); ++i) {
        if (i != track.source_path.size() &&
            (subpath || track.source_path[i] != '/') &&
            track.source_path[i] != ':')
          continue;
        uint32_t h = 5381;
        for (size_t j = start; j < i; ++j) {
          unsigned char ch = static_cast<unsigned char>(track.source_path[j]);
          if (ch >= 128)
            return fail(e,
                        "Player animation non-ASCII native hash unsupported");
          h = h * 33 + ch;
        }
        hash ^= h;
        if (i < track.source_path.size() && track.source_path[i] == ':')
          subpath = true;
        start = i + 1;
      }
      auto &bucket = buckets[hash & 7];
      bucket.insert(bucket.begin(), {track.target, track.member});
    }
  if (paths.size() > 64)
    return fail(e, "Player animation native hash resize requires adapter");
  for (const auto &bucket : buckets)
    cache_order_.insert(cache_order_.end(), bucket.begin(), bucket.end());
  process_mode_ = mode;
  data_ = &d;
  tree_ = &t;
  registry_ = &r;
  player_ = player;
  animation_ = id;
  endpoints_ = &port;
  clips_ = std::move(clips);

  speed_ = float(speed);
  e.clear();
  return true;
}
const PodunkPlayerSpriteState *
PodunkPlayerAnimation::sprite(FieldObjectId id) const {
  auto &sprites = sprite_owner_ ? sprite_owner_->sprites_ : sprites_;
  auto i = sprites.find(id);
  return i == sprites.end() ? nullptr : &i->second;
}
bool PodunkPlayerAnimation::sprite_frame(FieldObjectId id, uint32_t f,
                                         std::string &e) {
  if (!live(e))
    return false;
  auto &sprites = sprite_owner_ ? sprite_owner_->sprites_ : sprites_;
  auto i = sprites.find(id);
  if (i == sprites.end()) {
    auto v = endpoints_->visual(id);
    return v ? v->set_frame(f, e)
             : fail(e, "Player Sprite actual frame owner absent");
  }
  if (f >= i->second.columns * i->second.rows)
    return fail(e, "Player Sprite frame outside source atlas");
  i->second.frame = f;
  return true;
}
bool PodunkPlayerAnimation::sprite_texture(FieldObjectId id, uint32_t source,
                                           std::string &e) {
  if (!live(e))
    return false;
  auto &sprites = sprite_owner_ ? sprite_owner_->sprites_ : sprites_;
  auto i = sprites.find(id);
  FieldObjectId resource = 0;
  if (i == sprites.end() || !endpoints_->texture(source, resource, e) ||
      !resource || !checked_resource(source, resource, true, e))
    return fail(e, "Player Sprite actual texture owner absent");
  i->second.texture = resource;
  i->second.texture_source = source;
  return true;
}
bool PodunkPlayerAnimation::sprite_offset(FieldObjectId id, Vec2 v,
                                          std::string &e) {
  if (!live(e))
    return false;
  auto &sprites = sprite_owner_ ? sprite_owner_->sprites_ : sprites_;
  auto i = sprites.find(id);
  if (i == sprites.end() || !std::isfinite(v.x) || !std::isfinite(v.y))
    return fail(e, "Player Sprite native offset owner/value rejected");
  i->second.offset = v;
  return true;
}
bool PodunkPlayerAnimation::assign(const Track &t, const Value &v,
                                   std::string &e) {
  auto s = tree_->state(t.target);
  if (!s || !s->alive || s->queued ||
      registry_->tree_owner(t.target).get() != tree_)
    return fail(e, "Player animation live target expired");
  if (t.member == "modulate" && v.kind == 7)
    return tree_->set_modulate(t.target, v.color, false, e);
  if (t.member.rfind("material:shader_param/", 0) == 0) {
    auto param = t.member.substr(22);
    if (param.empty() || param.find(':') != std::string::npos)
      return fail(e, "Player shader parameter subpath unsupported");
    if (v.kind == 7)
      return endpoints_->shader_color(t.target, param, v.color, e);
    if (v.kind == 2 || v.kind == 3)
      return endpoints_->shader_number(t.target, param, v.number, e);
  }
  if (t.member == "offset" && v.kind == 5) {
    auto owned = sprite(t.target);
    return owned ? sprite_offset(t.target, v.vector, e)
                 : endpoints_->native_offset(t.target, v.vector, e);
  }
  if (t.member == "stream" && (v.kind == 0 || v.kind == 6))
    return endpoints_->audio_stream(t.target, v.kind == 6 ? v.resource : 0, e);
  auto desc = tree_->descriptor(t.target);
  if (desc && desc->native_class == "AnimatedSprite") {
    if (t.member == "frame" && (v.kind == 2 || v.kind == 3) && v.number >= 0 &&
        v.number <= UINT32_MAX) {
      auto visual = endpoints_->visual(t.target);
      return visual
                 ? visual->set_frame(uint32_t(v.number), e)
                 : endpoints_->animated_frame(t.target, uint32_t(v.number), e);
    }
    if (t.member == "playing" && v.kind == 1)
      return endpoints_->animated_playing(t.target, v.boolean, e);
  }
  if (t.member == "visible" && v.kind == 1)
    return tree_->set_visible(t.target, v.boolean, e);
  if (t.member == "show_behind_parent" && v.kind == 1) {
    auto own = endpoints_->visual(t.target);
    return own ? own->set_behind_parent(v.boolean, e)
               : tree_->set_behind_parent(t.target, v.boolean, e);
  }
  if (t.member == "position" && v.kind == 5) {
    auto local = s->local;
    local[2] = v.vector;
    return tree_->set_local(t.target, local, e);
  }
  if (t.member == "rotation_degrees" && (v.kind == 2 || v.kind == 3)) {
    auto local = s->local;
    float old = std::atan2(local[0].y, local[0].x),
          a = float(v.number * (3.14159265358979323846 / 180.0)) - old;
    float c = std::cos(a), sn = std::sin(a);
    for (size_t i = 0; i < 2; ++i) {
      Vec2 q = local[i];
      local[i] = {c * q.x - sn * q.y, sn * q.x + c * q.y};
    }
    return tree_->set_local(t.target, local, e);
  }
  if (t.member == "frame" && (v.kind == 2 || v.kind == 3) && v.number >= 0 &&
      v.number <= UINT32_MAX)
    return sprite_frame(t.target, uint32_t(v.number), e);
  if (t.member == "texture" && v.kind == 6)
    return sprite_texture(t.target, v.resource, e);
  if (t.member == "disabled" && v.kind == 1)
    return endpoints_->disabled(t.target, v.boolean, e);
  if (t.member == "playing" && v.kind == 1)
    return endpoints_->audio_playing(t.target, v.boolean, e);
  return fail(e, "Player animation property/value schema differs");
}
std::vector<size_t> PodunkPlayerAnimation::events(const Clip &c, const Track &t,
                                                  float time,
                                                  float delta) const {
  std::vector<size_t> out;
  float from = time - delta, to = time;
  if (from > to)
    std::swap(from, to);
  auto range = [&](float a, float b) {
    if (a != c.length && b == c.length)
      b = c.length * (t.method ? 1.01f : 1.001f);
    for (size_t i = 0; i < t.keys.size(); ++i) {
      float k = t.keys[i].time;
      if ((!t.method && a == b && k == a) || (k >= a && k < b))
        out.push_back(i);
    }
  };
  if (c.loop) {
    if (!t.method || from < 0 || from > c.length)
      from = fpos(from, c.length);
    if (!t.method || to < 0 || to > c.length)
      to = fpos(to, c.length);
    if (from > to) {
      range(from, c.length);
      range(0, to);
      return out;
    }
  } else {
    from = std::clamp(from, 0.f, c.length);
    to = std::clamp(to, 0.f, c.length);
  }
  range(from, to);
  return out;
}
bool PodunkPlayerAnimation::sample(const Clip &c, const Track &t, float time,
                                   Value &out, bool &has,
                                   std::string &e) const {
  has = false;
  size_t len = 0;
  while (len < t.keys.size() && t.keys[len].time <= c.length)
    ++len;
  if (!len)
    return true;
  if (len == 1) {
    out = t.keys[0].value;
    has = true;
    return true;
  }
  int idx = -1;
  for (size_t i = 0; i < len && t.keys[i].time <= time; ++i)
    idx = int(i);
  int next = idx;
  float ratio = 0;
  if (c.loop && t.wrap) {
    if (idx < 0) {
      idx = int(len) - 1;
      next = 0;
      float end = std::max(0.f, c.length - t.keys[size_t(idx)].time),
            span = end + t.keys[0].time;
      ratio = std::abs(span) < 0.00001f ? 0 : (end + time) / span;
    } else {
      next = idx + 1 < int(len) ? idx + 1 : 0;
      float span = next ? t.keys[size_t(next)].time - t.keys[size_t(idx)].time
                        : c.length - t.keys[size_t(idx)].time + t.keys[0].time;
      ratio = std::abs(span) < 0.00001f
                  ? 0
                  : (time - t.keys[size_t(idx)].time) / span;
    }
  } else if (idx < 0) {
    if (!c.loop)
      return true;
    idx = next = 0;
  } else if (idx + 1 < int(len)) {
    next = idx + 1;
    float span = t.keys[size_t(next)].time - t.keys[size_t(idx)].time;
    ratio = std::abs(span) < 0.00001f
                ? 0
                : (time - t.keys[size_t(idx)].time) / span;
  }
  auto &a = t.keys[size_t(idx)];
  out = a.value;
  has = true;
  if (a.transition == 0 || idx == next || !t.interpolation || t.update == 1)
    return true;
  if (a.transition != 1)
    ratio = a.transition < 1 ? 1 - std::pow(1 - ratio, 1 / a.transition)
                             : std::pow(ratio, a.transition);
  auto &b = t.keys[size_t(next)].value;
  if (a.value.kind != b.kind)
    return fail(e, "Player animation continuous value types differ");
  if (out.kind == 2 || out.kind == 3) {
    out.number = out.kind == 3
                     ? double(float(a.value.number) +
                              (float(b.number) - float(a.value.number)) * ratio)
                     : double(float(a.value.number) +
                              float(b.number - a.value.number) * ratio);
    if (out.kind == 2)
      out.number = std::trunc(out.number);
  } else if (out.kind == 5)
    out.vector = {a.value.vector.x + (b.vector.x - a.value.vector.x) * ratio,
                  a.value.vector.y + (b.vector.y - a.value.vector.y) * ratio};
  else if (out.kind == 7) {
    for (size_t i = 0; i < 4; ++i)
      out.color[i] = a.value.color[i] + (b.color[i] - a.value.color[i]) * ratio;
  } else if (out.kind != 1 && out.kind != 6)
    return fail(
        e, "Player animation continuous nonnumeric interpolation unsupported");
  return true;
}
bool PodunkPlayerAnimation::evaluate(const Clip &c, float time, float step,
                                     bool seek, bool graph, float weight,
                                     std::string &e) {
  if (!std::isfinite(time) || !std::isfinite(step) || time < 0 || step < 0 ||
      !std::isfinite(weight) || weight < 0 || weight > 1)
    return fail(e, "Player animation clock/blend rejected");
  if (graph && weight < 0.00001f)
    return true;
  for (const auto &t : c.tracks) {
    if (!t.enabled)
      continue;
    if (t.method) {
      if (step == 0 || seek || !tree_->state(animation_)->inside)
        continue;
      for (size_t i : events(c, t, time, step)) {
        FieldDeferredMessage m;
        m.object = t.target;
        m.member = t.keys[i].value.string;
        if (!tree_->enqueue(std::move(m), e))
          return false;
      }
      continue;
    }
    if (t.update != 0 && (graph || step != 0)) {
      for (size_t i : events(c, t, time, step))
        if (!assign(t, t.keys[i].value, e))
          return false;
      continue;
    }
    if (t.update == 2)
      continue;
    Value value;
    bool has = false;
    if (!sample(c, t, time, value, has, e))
      return false;
    if (!has)
      continue;
    if (!graph || t.update != 0) {
      if (!assign(t, value, e))
        return false;
    } else {
      auto key = std::make_pair(t.target, t.member);
      auto old = pending_.find(key);
      if (old != pending_.end()) {
        auto &a = old->second.second;
        if (a.kind != value.kind)
          return fail(e, "Player graph blend value types differ");
        if (a.kind == 2 || a.kind == 3) {
          value.number =
              a.kind == 3
                  ? double(float(a.number) +
                           (float(value.number) - float(a.number)) * weight)
                  : double(float(a.number) +
                           float(value.number - a.number) * weight);
          if (a.kind == 2)
            value.number = std::trunc(value.number);
        } else if (a.kind == 5)
          value.vector = {a.vector.x + (value.vector.x - a.vector.x) * weight,
                          a.vector.y + (value.vector.y - a.vector.y) * weight};
        else if (a.kind == 7) {
          for (size_t i = 0; i < 4; ++i)
            value.color[i] =
                a.color[i] + (value.color[i] - a.color[i]) * weight;
        } else if (a.kind == 1 || a.kind == 6)
          value = a;
        else if (weight < 1)
          return fail(e, "Player graph nonnumeric blend unsupported");
      }
      pending_[key] = {&t, std::move(value)};
    }
  }
  return true;
}
bool PodunkPlayerAnimation::begin(std::string &e) {
  if (!live(e) || !ready_ || frame_open_ || playing_)
    return fail(
        e,
        "Player graph frame/order or competing AnimationPlayer clock rejected");
  pending_.clear();
  frame_open_ = true;
  return true;
}
bool PodunkPlayerAnimation::blend(const PlayerGraphPoint &p, float time,
                                  float step, bool seek, float weight,
                                  std::string &e) {
  if (!live(e) || !frame_open_)
    return fail(e, "Player graph blend outside frame");
  auto c = clips_.find(p.clip);
  if (c == clips_.end() || c->second.resource != p.clip_id ||
      c->second.length != p.length || c->second.loop != p.loop)
    return fail(e, "Player graph clip source binding differs");
  if (!evaluate(c->second, time, step, seek, true, weight, e)) {
    poisoned_ = true;
    return false;
  }
  return true;
}
bool PodunkPlayerAnimation::apply(std::string &e) {
  if (!live(e) || !frame_open_)
    return fail(e, "Player graph apply outside frame");
  for (auto &key : cache_order_) {
    auto p = pending_.find(key);
    if (p != pending_.end() && !assign(*p->second.first, p->second.second, e)) {
      poisoned_ = true;
      return false;
    }
  }
  pending_.clear();
  frame_open_ = false;
  return true;
}
PlayerGraphHost PodunkPlayerAnimation::graph_host() {
  return {[this](std::string &e) { return begin(e); },
          [this](std::string &e) { return apply(e); },
          [this](const PlayerGraphPoint &p, float t, float d, bool s, float w,
                 std::string &e) { return blend(p, t, d, s, w, e); }};
}
bool PodunkPlayerAnimation::ready(FieldTreePhase phase,
                                  const FieldNodeBinding &binding,
                                  std::string &e) {
  if (!live(e) || phase != FieldTreePhase::ReadyNative || ready_ ||
      !tree_->state(animation_)->inside ||
      binding.stable_id != tree_->descriptor(animation_)->id ||
      binding.native_class != "AnimationPlayer" ||
      binding.class_index != tree_->descriptor(animation_)->class_index ||
      binding.script_sha != tree_->descriptor(animation_)->script_sha ||
      binding.identity.scene_id != data_->identity().scene_id ||
      binding.identity.source_sha256 != data_->identity().source_sha256 ||
      binding.identity.upstream_commit != data_->identity().upstream_commit)
    return fail(e,
                "Player AnimationPlayer actual Native Ready cursor rejected");
  ready_ = true;
  if (!autoplay_.empty())
    return play(autoplay_, e);
  return true;
}
bool PodunkPlayerAnimation::play(std::string_view name, std::string &e) {
  return play(name, 1, false, e);
}
bool PodunkPlayerAnimation::play(std::string_view name, float custom_speed,
                                 bool from_end, std::string &e) {
  if (!live(e) || !ready_ || frame_open_ || !std::isfinite(custom_speed) ||
      !clips_.count(std::string(name)))
    return fail(e, "Player AnimationPlayer actual play rejected");
  const auto &clip = clips_.at(std::string(name));
  if (current_ != name)
    position_ = from_end ? clip.length : 0;
  else if (from_end && position_ == 0)
    position_ = clip.length;
  else if (!from_end && position_ == clip.length)
    position_ = 0;
  current_ = std::string(name);
  custom_speed_ = custom_speed;
  playing_ = true;
  if (!tree_->add_group(animation_,
                        process_mode_ ? "idle_process_internal"
                                      : "physics_process_internal",
                        e)) {
    poisoned_ = true;
    return false;
  }
  if (!endpoints_->signal(animation_, "animation_started", current_, e)) {
    poisoned_ = true;
    return false;
  }
  return true;
}
bool PodunkPlayerAnimation::stop(std::string &e) {
  if (!live(e) || frame_open_)
    return false;
  playing_ = false;
  position_ = 0;
  return tree_->remove_group(
      animation_,
      process_mode_ ? "idle_process_internal" : "physics_process_internal", e);
}
bool PodunkPlayerAnimation::advance(float delta, bool paused, std::string &e) {
  if (!live(e) || !ready_ || !std::isfinite(delta) || delta < 0 || frame_open_)
    return fail(e,
                "Player AnimationPlayer actual process delta/order rejected");
  if (!playing_ || !tree_->can_process(animation_, paused))
    return true;
  auto &c = clips_.at(current_);
  float step = delta * speed_ * custom_speed_, next = position_ + step;
  const bool backwards = std::signbit(step);
  const float previous = position_;
  if (!std::isfinite(next))
    return fail(e, "Player AnimationPlayer clock overflow");
  if (c.loop) {
    float looped = fpos(next, c.length);
    next = looped == 0 && next != 0 ? c.length : looped;
  } else {
    next = std::max(0.0f, std::min(next, c.length));
    step = next - position_;
  }
  if (!evaluate(c, next, step, false, false, 1, e)) {
    poisoned_ = true;
    return false;
  }
  position_ = next;
  if (!c.loop &&
      ((!backwards && next == c.length) || (backwards && next == 0))) {
    playing_ = false;
    if (!tree_->remove_group(animation_,
                             process_mode_ ? "idle_process_internal"
                                           : "physics_process_internal",
                             e)) {
      poisoned_ = true;
      return false;
    }
    if ((!backwards && previous < c.length) || (backwards && previous > 0))
      return endpoints_->signal(animation_, "animation_finished", current_, e);
    return true;
  }
  return true;
}
bool PodunkPlayerAnimation::process(FieldTreePhase phase, float delta,
                                    bool paused, std::string &e) {
  if (!live(e) || phase != (process_mode_ ? FieldTreePhase::IdleInternal
                                          : FieldTreePhase::PhysicsInternal))
    return fail(e,
                "Player AnimationPlayer source process notification rejected");
  return advance(delta, paused, e);
}
PodunkPlayerAnimation *PodunkPlayerAnimation::for_animation(FieldObjectId id) {
  if (id == animation_)
    return this;
  auto i = children_.find(id);
  return i == children_.end() ? nullptr : i->second.get();
}
std::vector<FieldObjectId> PodunkPlayerAnimation::animation_objects() const {
  std::vector<FieldObjectId> out{animation_};
  for (const auto &p : children_)
    out.push_back(p.first);
  return out;
}
bool PodunkPlayerAnimation::ready(FieldObjectId id, FieldTreePhase phase,
                                  const FieldNodeBinding &binding,
                                  std::string &e) {
  auto own = for_animation(id);
  return own ? own->ready(phase, binding, e)
             : fail(e, "Player native AnimationPlayer actual owner absent");
}
bool PodunkPlayerAnimation::play(FieldObjectId id, std::string_view clip,
                                 std::string &e) {
  auto own = for_animation(id);
  return own ? own->play(clip, e)
             : fail(e, "Player native AnimationPlayer actual owner absent");
}
bool PodunkPlayerAnimation::play(FieldObjectId id, std::string_view clip,
                                 float speed, bool from_end, std::string &e) {
  auto own = for_animation(id);
  return own ? own->play(clip, speed, from_end, e)
             : fail(e, "Player native AnimationPlayer actual owner absent");
}
bool PodunkPlayerAnimation::assigned(FieldObjectId id, std::string &name,
                                     std::string &e) const {
  const auto i = children_.find(id);
  const auto *own = id == animation_       ? this
                    : i == children_.end() ? nullptr
                                           : i->second.get();
  if (!own || !own->live(e))
    return fail(e, "Player native AnimationPlayer assigned owner absent");
  name = own->current_;
  return true;
}
bool PodunkPlayerAnimation::stop(FieldObjectId id, std::string &e) {
  auto own = for_animation(id);
  return own ? own->stop(e)
             : fail(e, "Player native AnimationPlayer actual owner absent");
}
bool PodunkPlayerAnimation::process(FieldObjectId id, FieldTreePhase phase,
                                    float delta, bool paused, std::string &e) {
  auto own = for_animation(id);
  return own ? own->process(phase, delta, paused, e)
             : fail(e, "Player native AnimationPlayer actual owner absent");
}
} // namespace encore::ctr
