#include "encore/field_canvas_art.hpp"
#include <algorithm>
#include <cmath>
#include <set>
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
Vec2 point(const FieldTransform &t, Vec2 p) {
  return {t[0].x * p.x + t[1].x * p.y + t[2].x,
          t[0].y * p.x + t[1].y * p.y + t[2].y};
}
FieldTransform multiply(const FieldTransform &a, const FieldTransform &b) {
  auto origin = point(a, b[2]);
  return {Vec2{a[0].x * b[0].x + a[1].x * b[0].y,
               a[0].y * b[0].x + a[1].y * b[0].y},
          Vec2{a[0].x * b[1].x + a[1].x * b[1].y,
               a[0].y * b[1].x + a[1].y * b[1].y},
          origin};
}
bool delegated(const FieldCanvasRecord &r) {
  return r.shader != FieldCanvasShader::Default ||
         r.owner == FieldCanvasOwner::Grass ||
         r.owner == FieldCanvasOwner::Character ||
         r.owner == FieldCanvasOwner::Prompt ||
         r.owner == FieldCanvasOwner::Melody;
}
} // namespace
void FieldCanvasArtRuntime::clear() {
  data_ = nullptr;
  source_ = nullptr;
  tree_ = nullptr;
  host_ = {};
  foreign_ = nullptr;
  native_ = nullptr;
  owners_.clear();
  slots_.clear();
}
bool FieldCanvasArtRuntime::bind(const FieldCanvasRecord &r,
                                 FieldObjectId object, FieldObjectId &owner,
                                 std::string &e) {
  owner = 0;
  if (r.owner == FieldCanvasOwner::Native)
    return true;
  auto cached = owners_.find(object);
  if (cached != owners_.end()) {
    owner = cached->second;
    const auto *s = tree_->state(owner);
    if (s && s->alive && s->source == r.owner_id)
      return true;
    return fail(e, "Canvas typed owner was released/replaced");
  }
  auto node = object;
  std::set<FieldObjectId> seen;
  while (node) {
    if (!seen.insert(node).second)
      return fail(e, "Canvas owner ancestor cycle");
    auto *s = tree_->state(node);
    if (!s || !s->alive)
      return fail(e, "Canvas owner ancestry unavailable");
    if (s->source == r.owner_id) {
      owner = node;
      break;
    }
    node = s->parent;
  }
  if (!owner)
    return fail(e, "Canvas typed appearance actual owner missing");
  auto *d = tree_->descriptor(owner);
  FieldIdentity identity{};
  if (!d || d->script != r.owner_script || d->script_sha != r.owner_sha ||
      !tree_->object_identity(owner, identity) ||
      !same(identity, data_->identity()))
    return fail(e, "Canvas owner actual source identity/digest mismatch");
  if (!host_.bind_owner || !host_.appearance ||
      !host_.bind_owner(r, object, owner, e))
    return fail(e, "Canvas typed appearance host unbound/rejected");
  owners_.emplace(object, owner);
  return true;
}
bool FieldCanvasArtRuntime::initialize(const FieldCanvasArtData &d,
                                       const FieldNodeTreeData &s,
                                       FieldNodeTreeRuntime &t,
                                       FieldCanvasArtHost h, std::string &e) {
  clear();
  if (!d.valid() || !s.valid() || !t.root() ||
      !same(d.identity(), s.identity()) || d.source_scene() != s.source_scene())
    return fail(e, "Canvas actual checked NodeTree identity required");
  for (const auto &r : d.records()) {
    auto *n = s.record(r.id);
    if (!n || n->path != r.node || n->flags != r.flags ||
        n->class_index >= s.classes().size() ||
        s.classes()[n->class_index] != (r.kind ? "TextureRect" : "Sprite"))
      return fail(e, "Canvas source node/native class/flags mismatch");
    if (r.owner != FieldCanvasOwner::Native) {
      auto *o = s.record(r.owner_id);
      if (!o || o->script != r.owner_script || o->script_sha != r.owner_sha ||
          !h.bind_owner || !h.appearance)
        return fail(e, "Canvas source typed owner/script binding missing");
    }
  }
  for (const auto &n : s.records()) {
    std::array<uint8_t, 32> a{}, b{};
    if (!n.script.empty()) {
      auto p = n.script.substr(0, n.script.find("::"));
      if (!s.source_hash(p, a) || !d.source_hash(p, b) || a != b)
        return fail(e, "Canvas source script inventory cross-binding rejected");
    }
  }
  data_ = &d;
  source_ = &s;
  tree_ = &t;
  host_ = std::move(h);
  for (const auto &r : d.records()) {
    auto object = t.source_object(r.id);
    auto *live = t.state(object);
    if (live && live->alive) {
      FieldObjectId owner;
      if (!bind(r, object, owner, e)) {
        clear();
        return false;
      }
    }
  }
  e.clear();
  return true;
}
bool FieldCanvasArtRuntime::bind_native(FieldCanvasNativeOwner &owner,
                                        std::string &e) {
  if (!data_ || native_ || owner.canvas_data() != data_ ||
      owner.canvas_tree() != tree_)
    return fail(e, "Canvas native Sprite owner differs from actual data/Tree");
  native_ = &owner;
  return true;
}
bool FieldCanvasArtRuntime::command(FieldObjectId object,
                                    std::vector<FieldCanvasDraw> &out,
                                    std::string &e) {
  auto *s = tree_->state(object);
  if (!s || !s->alive || !s->inside)
    return true;
  FieldIdentity identity{};
  if (!tree_->object_identity(object, identity))
    return fail(e, "Canvas command actual identity unavailable");
  // Foreign nodes have already been admitted by their actual typed owner;
  // they emit their own real GPU draw at the same slot in the compositor.
  if (!same(identity, data_->identity()))
    return true;
  const auto *r = data_->record(s->source);
  if (!r)
    return true;
  if (!tree_->visible_in_tree(object))
    return true;
  FieldObjectId owner = 0;
  if (!bind(*r, object, owner, e))
    return false;
  FieldCanvasAppearance a;
  a.texture = r->texture;
  a.hframes = r->hframes;
  a.vframes = r->vframes;
  a.frame = r->frame;
  a.offset = r->offset;
  a.size = r->size;
  a.centered = r->centered;
  a.flip_h = r->flip_h;
  a.flip_v = r->flip_v;
  if (!r->kind && native_ && !native_->sprite_snapshot(object, a, e))
    return false;
  if (r->owner != FieldCanvasOwner::Native &&
      !host_.appearance(*r, object, owner, a, e))
    return false;
  if (uint32_t(a.action) > uint32_t(FieldCanvasAction::Delegate))
    return fail(e, "Canvas unknown typed appearance action");
  if (a.action == FieldCanvasAction::Hidden)
    return true;
  // Original texture-null Sprite genuinely has no draw command. Its known
  // source owner is still bound; hidden/nonexistent pictures are not fixtures.
  if (!a.texture && a.action != FieldCanvasAction::Delegate)
    return true;
  if (delegated(*r) && a.action != FieldCanvasAction::Delegate)
    return fail(
        e, "Canvas shader/dynamic owner cannot use default texture pipeline");
  const auto *texture = data_->texture(a.texture);
  if ((a.action == FieldCanvasAction::Default && !texture) || !a.hframes ||
      !a.vframes || a.hframes > 1024 || a.vframes > 1024 ||
      a.frame >= a.hframes * a.vframes || !std::isfinite(a.offset.x) ||
      !std::isfinite(a.offset.y) || !std::isfinite(a.size.x) ||
      !std::isfinite(a.size.y) || a.size.x < 0 || a.size.y < 0)
    return fail(e, "Canvas appearance texture/frame/rect rejected");
  FieldTransform world;
  FieldCanvasDraw v;
  if (!tree_->world_transform(object, world, e) ||
      !tree_->effective_color(object, v.color, e) ||
      !tree_->effective_z(object, v.z, e))
    return false;
  for (float c : v.color)
    if (!std::isfinite(c) || c < 0 || c > 1)
      return fail(e, "Canvas inherited color unsupported");
  // A typed consumer binds its own checked dynamic texture. Character Ready
  // can replace the exported initial woman texture without a fake atlas alias.
  float w = texture ? float(texture->width) / a.hframes : 0,
        h = texture ? float(texture->height) / a.vframes : 0;
  Vec2 offset = a.offset;
  v.source_rect = {float(a.frame % a.hframes) * w,
                   float(a.frame / a.hframes) * h, w, h};
  if (r->kind == 0) {
    if (a.centered) {
      offset.x -= w / 2;
      offset.y -= h / 2;
    }
    if (data_->pixel_snap()) {
      offset.x = std::floor(offset.x);
      offset.y = std::floor(offset.y);
    }
  } else {
    if (a.hframes != 1 || a.vframes != 1 || a.frame || a.centered)
      return fail(e, "Canvas native TextureRect frame mutation rejected");
    offset = {0, 0};
    if (r->stretch == 2) {
      w = a.size.x;
      h = a.size.y;
    }
  }
  std::array<Vec2, 4> local{offset, Vec2{offset.x + w, offset.y},
                            Vec2{offset.x + w, offset.y + h},
                            Vec2{offset.x, offset.y + h}};
  for (size_t i = 0; i < 4; ++i) {
    v.vertices[i] = point(world, local[i]);
    if (!std::isfinite(v.vertices[i].x) || !std::isfinite(v.vertices[i].y))
      return fail(e, "Canvas actual affine vertex nonfinite");
  }
  v.object = object;
  v.world = world;
  v.owner_object = owner;
  v.source = r->id;
  v.owner_source = r->owner_id;
  v.texture = a.texture;
  v.frame = a.frame;
  v.owner = r->owner;
  v.shader = r->shader;
  v.action = a.action;
  v.flip_h = a.flip_h;
  v.flip_v = a.flip_v;
  v.pixel_snap = data_->pixel_snap();
  v.order = out.size();
  out.push_back(v);
  return true;
}
bool FieldCanvasArtRuntime::collect(std::vector<FieldCanvasDraw> &out,
                                    std::string &e) {
  if (!ready())
    return fail(e, "Canvas runtime not initialized");
  std::vector<FieldCanvasDraw> next;
  std::vector<FieldCanvasOrderSlot> slots;
  std::map<FieldObjectId, std::vector<FieldObjectId>> children;
  std::vector<FieldObjectId> roots;
  std::set<FieldObjectId> seen;
  std::vector<FieldObjectId> ordered;
  std::map<FieldObjectId, std::pair<FieldIdentity, bool>> foreign_nodes;
  std::function<bool(FieldObjectId)> visit = [&](FieldObjectId id) {
    auto *s = tree_->state(id);
    if (!s || !s->alive)
      return true;
    if (!seen.insert(id).second || seen.size() > 1000000)
      return fail(e, "Canvas actual tree duplicate/cycle/limit rejected");
    ordered.push_back(id);
    for (auto child : s->children)
      if (!visit(child))
        return false;
    return true;
  };
  if (!visit(tree_->root()))
    return false;
  for (auto id : ordered) {
    const auto *s = tree_->state(id);
    if (!(s->flags & 1) || !s->inside)
      continue;
    FieldIdentity identity{};
    if (!tree_->object_identity(id, identity))
      return fail(e, "Canvas dynamic instance scene identity unavailable");
    if (!same(identity, data_->identity())) {
      const auto *d = tree_->descriptor(id); bool drawable = false;
      if (!d || !foreign_ ||
          !foreign_->admit(id, *d, identity, *tree_, drawable, e))
        return fail(e, "Canvas dynamic instance actual foreign owner rejected");
      foreign_nodes.emplace(id, std::make_pair(identity, drawable));
    }
    if (s->canvas_parent && seen.count(s->canvas_parent))
      children[s->canvas_parent].push_back(id);
    else
      roots.push_back(id);
  }
  auto visible = [&](FieldObjectId id) {
    auto *s = tree_->state(id);
    if (!s || !s->alive || !s->inside || !(s->flags & 2))
      return false;
    float alpha = 1;
    std::set<FieldObjectId> chain;
    for (auto at = id; at;) {
      auto *q = tree_->state(at);
      if (!q || !chain.insert(at).second)
        return false;
      alpha *= q->modulate[3];
      at = q->canvas_parent;
    }
    return alpha >= data_->alpha_prune();
  };
  struct Sorted {
    FieldObjectId id;
    float y;
    size_t index;
  };
  std::function<bool(FieldObjectId, const FieldTransform &,
                     std::vector<Sorted> &)>
      gather = [&](FieldObjectId id, const FieldTransform &transform,
                   std::vector<Sorted> &list) {
        for (auto c : children[id]) {
          auto *s = tree_->state(c);
          if (!visible(c))
            continue;
          list.push_back({c, point(transform, s->local[2]).y, list.size()});
          if (s->flags & 128) {
            if (s->z)
              return fail(e, "Canvas live nested YSort z unsupported");
            if (!gather(c, multiply(transform, s->local), list))
              return false;
          }
        }
        return true;
      };
  std::function<bool(FieldObjectId)> draw = [&](FieldObjectId id) {
    if (!visible(id))
      return true;
    auto *s = tree_->state(id);
    std::vector<FieldObjectId> list = children[id];
    bool ysort = (s->flags & 128) != 0;
    if (ysort) {
      std::vector<Sorted> sorted;
      FieldTransform identity{Vec2{1, 0}, Vec2{0, 1}, Vec2{0, 0}};
      if (!gather(id, identity, sorted))
        return false;
      auto less = [&](const Sorted &a, const Sorted &b) {
        float tolerance =
            std::max(data_->y_epsilon(), data_->y_epsilon() * std::abs(a.y));
        if (a.y == b.y || std::abs(a.y - b.y) < tolerance)
          return a.index < b.index;
        return a.y < b.y;
      };
      std::stable_sort(sorted.begin(), sorted.end(), less);
      list.clear();
      for (const auto &v : sorted)
        list.push_back(v.id);
    }
    for (auto c : list) {
      auto *q = tree_->state(c);
      if ((q->flags & 8) && !(ysort && (q->flags & 128)))
        if (!draw(c))
          return false;
    }
    FieldCanvasOrderSlot slot;
    const auto *descriptor = tree_->descriptor(id);
    if (!descriptor)
      return fail(e, "Canvas ordered native descriptor missing");
    slot.object = id;
    auto foreign = foreign_nodes.find(id);
    if (foreign != foreign_nodes.end()) {
      slot.foreign = true; slot.identity = foreign->second.first;
      slot.foreign_drawable = foreign->second.second;
    } else slot.identity = data_->identity();
    slot.source = s->source;
    slot.class_index = descriptor->class_index;
    slot.flags = s->flags;
    slot.order = slots.size();
    slot.native_order = slot.order;
    if (!tree_->world_transform(id, slot.world, e) ||
        !tree_->effective_color(id, slot.color, e) ||
        !tree_->effective_z(id, slot.z, e))
      return false;
    slots.push_back(slot);
    if (!command(id, next, e))
      return false;
    for (auto c : list) {
      auto *q = tree_->state(c);
      if (!(q->flags & 8) && !(ysort && (q->flags & 128)))
        if (!draw(c))
          return false;
    }
    return true;
  };
  for (auto root : roots)
    if (!draw(root))
      return false;
  std::stable_sort(slots.begin(), slots.end(),
                   [](const auto &a, const auto &b) { return a.z < b.z; });
  std::map<FieldObjectId, uint64_t> order;
  for (size_t i = 0; i < slots.size(); ++i) {
    slots[i].order = i;
    order.emplace(slots[i].object, i);
  }
  for (auto &v : next)
    v.order = order.at(v.object);
  std::stable_sort(next.begin(), next.end(), [](const auto &a, const auto &b) {
    return a.order < b.order;
  });
  slots_ = std::move(slots);
  out = std::move(next);
  e.clear();
  return true;
}
bool FieldCanvasArtRuntime::bind_foreign(FieldCanvasForeignOwner &owner,
                                       std::string &e) {
  if (!ready() || foreign_) return fail(e, "Canvas foreign owner binding state rejected");
  foreign_ = &owner; e.clear(); return true;
}
} // namespace encore::upstream
