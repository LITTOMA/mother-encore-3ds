#include "encore/field_canvas_art.hpp"
#include "encore/field_map.hpp"
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
  control_ = nullptr;
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
        s.classes()[n->class_index] != (r.kind == 0 ? "Sprite" : r.kind == 1 ? "TextureRect" : r.kind == 2 ? "AnimatedSprite" : "ColorRect"))
      return fail(e, "Canvas source node/native class/flags mismatch");
    if (r.owner != FieldCanvasOwner::Native) {
      auto *o = s.record(r.owner_id);
      if (!o || o->script != r.owner_script || o->script_sha != r.owner_sha ||
          !h.bind_owner || !h.appearance)
        return fail(e, "Canvas source typed owner/script binding missing");
    }
  }
  for(const auto&b:d.control_boundaries()) {
    const auto*n=s.record(b.id);const auto*o=s.record(b.owner_id);
    if(!n||!o||n->path!=b.node||n->flags!=b.flags||n->class_index>=s.classes().size()||
       s.classes()[n->class_index]!=b.native_class||o->script!=b.owner_script||o->script_sha!=b.owner_sha)
      return fail(e,"Canvas actual native Control source/owner mismatch");
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
bool FieldCanvasArtRuntime::bind_control(FieldCanvasControlOwner&owner,std::string&e) {
  if(!ready()||control_||owner.canvas_data()!=data_||owner.canvas_tree()!=tree_)
    return fail(e,"Canvas actual native Control owner binding rejected");
  for(const auto&b:data_->control_boundaries()) {
    auto object=tree_->source_object(b.id),at=object;FieldObjectId actual_owner=0;
    std::set<FieldObjectId>seen;
    while(at) {const auto*s=tree_->state(at);if(!s||!s->alive||!seen.insert(at).second)break;
      if(s->source==b.owner_id){actual_owner=at;break;}at=s->parent;}
    if(!actual_owner||!owner.admit_control(b,object,actual_owner,e))
      return fail(e,"Canvas actual native Control admission unavailable/rejected");
  }
  control_=&owner;e.clear();return true;
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
  if (!r) {
    const auto*b=data_->control_boundary(s->source);
    if(!b)return true;
    auto at=object;FieldObjectId owner=0;std::set<FieldObjectId>seen;
    while(at){const auto*q=tree_->state(at);if(!q||!q->alive||!seen.insert(at).second)break;
      if(q->source==b->owner_id){owner=at;break;}at=q->parent;}
    bool drawable=false;
    if(!control_||!owner||!control_->control_snapshot(*b,object,owner,drawable,e))
      return fail(e,"Canvas actual native Control snapshot unavailable/rejected");
    if(drawable!=(b->native_class=="Label"))return fail(e,"Canvas native Control draw classification rejected");
    if(drawable&&tree_->visible_in_tree(object)) {
      FieldCanvasDraw v;v.object=object;v.owner_object=owner;v.source=b->id;v.owner_source=b->owner_id;
      v.owner=FieldCanvasOwner::Control;v.action=FieldCanvasAction::Delegate;v.primitive=2;
      if(!tree_->world_transform(object,v.world,e)||!tree_->effective_color(object,v.color,e)||!tree_->effective_z(object,v.z,e))return false;
      v.order=out.size();out.push_back(std::move(v));
    }
    return true;
  }
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
  std::array<float,4>animated_rect{};
  if(r->kind==2) {
    std::string animation;uint32_t frame=0;
    if(!native_||!native_->animated_snapshot(object,animation,frame,a,e))
      return fail(e,"Canvas actual AnimatedSprite snapshot unavailable/rejected");
    const FieldCanvasAnimation*catalog=nullptr;
    for(const auto&x:r->animations)if(x.name==animation)catalog=&x;
    if(!catalog||frame>=catalog->frames.size())return fail(e,"Canvas actual AnimatedSprite frame rejected");
    const auto&f=catalog->frames[frame];a.texture=f.texture;a.hframes=a.vframes=1;a.frame=0;
    animated_rect=f.region;
  }
  FieldColor rect_color{};
  if(r->kind==3) {
    if(!native_||!native_->color_rect_snapshot(object,a.size,rect_color,e))
      return fail(e,"Canvas actual ColorRect snapshot unavailable/rejected");
    a.texture=0;a.hframes=a.vframes=1;a.frame=0;
    for(float x:rect_color)if(!std::isfinite(x)||x<0||x>1)return fail(e,"Canvas native ColorRect color rejected");
  }
  if (r->owner != FieldCanvasOwner::Native &&
      !host_.appearance(*r, object, owner, a, e))
    return false;
  if (uint32_t(a.action) > uint32_t(FieldCanvasAction::Delegate))
    return fail(e, "Canvas unknown typed appearance action");
  if (a.action == FieldCanvasAction::Hidden)
    return true;
  // Original texture-null Sprite genuinely has no draw command. Its known
  // source owner is still bound; hidden/nonexistent pictures are not fixtures.
  if (!a.texture && r->kind!=3 && a.action != FieldCanvasAction::Delegate)
    return true;
  if (delegated(*r) && a.action != FieldCanvasAction::Delegate)
    return fail(
        e, "Canvas shader/dynamic owner cannot use default texture pipeline");
  const auto *texture = data_->texture(a.texture);
  if ((a.action == FieldCanvasAction::Default && !texture && r->kind!=3) || !a.hframes ||
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
  if(r->kind==2){v.source_rect=animated_rect;w=animated_rect[2];h=animated_rect[3];}
  if (r->kind == 0 || r->kind==2) {
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
    if (r->stretch == 2 || r->kind==3) {
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
  if(r->kind==3){v.primitive=1;for(size_t i=0;i<4;++i)v.color[i]*=rect_color[i];}
  if(data_->format()==2&&r->material.present) {
    if(!host_.material||!host_.material(*r,object,owner,v.material_uniforms,e)||v.material_uniforms.size()!=r->material.uniforms.size())
      return fail(e,"Canvas actual ShaderMaterial snapshot unavailable/rejected");
    for(size_t i=0;i<v.material_uniforms.size();++i){const auto&a=v.material_uniforms[i];const auto&b=r->material.uniforms[i];
      if(a.name!=b.name||a.type!=b.type||a.proof!=b.proof||(!a.initialized&&b.initialized)||
         (a.type==FieldCanvasUniformType::Sampler2D&&a.initialized))return fail(e,"Canvas actual material uniform schema rejected");
      for(float x:a.value)if(!std::isfinite(x))return fail(e,"Canvas actual material uniform nonfinite");
      if(a.type==FieldCanvasUniformType::Bool&&(a.integer<0||a.integer>1))return fail(e,"Canvas actual bool uniform rejected");}
  }
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
  std::map<FieldObjectId,std::vector<FieldCanvasNativeTileChild>>tile_nodes;
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
    }else if(data_->format()==2&&(s->flags&128)&&tree_->visible_in_tree(id)){
      const auto*d=tree_->descriptor(id);
      if(d&&d->class_index<source_->classes().size()&&source_->classes()[d->class_index]=="TileMap"){
        const auto*map=native_?native_->tile_map_data():nullptr;
        if(!map||!map->valid()||!same(map->identity(),data_->identity())||map->source_scene()!=data_->source_scene())
          return fail(e,"Canvas actual sorted TileMap resource owner unavailable");
        uint32_t index=map->map_count();for(uint32_t i=0;i<map->map_count();++i)if(map->map(i).stable_id==s->source){index=i;break;}
        if(index==map->map_count())return fail(e,"Canvas actual sorted TileMap source absent");
        auto layer=map->map(index);std::vector<FieldCanvasNativeTileChild>items;
        if(!(layer.flags&1)||map->string(layer.node)!=d->path||!native_->tile_sort_children(id,items,e)||items.size()!=layer.draw_count)
          return fail(e,"Canvas actual native quadrant complete source receipt rejected");
        std::set<uint32_t>draws;
        for(const auto&item:items){
          if(item.draw_index<layer.draw_first||item.draw_index-layer.draw_first>=layer.draw_count||!draws.insert(item.draw_index).second)
            return fail(e,"Canvas native quadrant draw identity rejected");
          auto draw=map->draw(item.draw_index);auto cell=map->cell(draw.cell);auto anchor=map->sort_anchor(item.draw_index);
          if(draw.map!=index||item.quadrant_order!=uint32_t(cell.quadrant_ordinal)||item.z!=draw.z||
             item.local_position.x!=anchor.x-layer.position.x||item.local_position.y!=anchor.y-layer.position.y)
            return fail(e,"Canvas native quadrant source anchor/z/order mismatch");
        }
        std::stable_sort(items.begin(),items.end(),[](const auto&a,const auto&b){return a.quadrant_order==b.quadrant_order?a.draw_index<b.draw_index:a.quadrant_order<b.quadrant_order;});
        tile_nodes.emplace(id,std::move(items));
      }
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
    bool tile=false;uint32_t tile_index=0;
  };
  std::function<bool(FieldObjectId, const FieldTransform &,
                     std::vector<Sorted> &)>
      gather = [&](FieldObjectId id, const FieldTransform &transform,
                   std::vector<Sorted> &list) {
        auto tiles=tile_nodes.find(id);
        if(tiles!=tile_nodes.end())for(uint32_t i=0;i<tiles->second.size();++i)
          list.push_back({id,point(transform,tiles->second[i].local_position).y,list.size(),true,i});
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
    std::vector<Sorted>list;for(auto child:children[id])list.push_back({child,0,list.size(),false,0});
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
      list=std::move(sorted);
    }
    for (const auto&entry : list) {
      if(entry.tile)continue;
      auto c=entry.id;
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
    for (const auto&entry : list) {
      if(entry.tile){
        const auto&item=tile_nodes.at(entry.id).at(entry.tile_index);FieldCanvasOrderSlot tile;
        const auto*actual=tree_->state(entry.id);const auto*desc=tree_->descriptor(entry.id);
        if(!actual||!desc)return fail(e,"Canvas actual native quadrant parent expired");
        FieldTransform parent;int32_t parent_z;
        if(!tree_->world_transform(entry.id,parent,e)||!tree_->effective_color(entry.id,tile.color,e)||!tree_->effective_z(entry.id,parent_z,e))return false;
        tile.world=parent;tile.world[2]=point(parent,item.local_position);tile.object=entry.id;tile.source=actual->source;
        tile.class_index=desc->class_index;tile.flags=actual->flags;tile.identity=data_->identity();tile.native_tile=true;
        tile.tile_draw=item.draw_index;tile.tile_quadrant=item.quadrant_order;tile.z=int32_t(std::max<int64_t>(-4096,std::min<int64_t>(4096,int64_t(parent_z)+item.z)));
        tile.order=tile.native_order=slots.size();slots.push_back(std::move(tile));continue;
      }
      auto c=entry.id;
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
    if(!slots[i].native_tile)order.emplace(slots[i].object, i);
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
namespace {
bool animation_valid(const FieldCanvasAnimatedState&s,std::string&e) {
  return (s.source&&s.object&&s.source->kind==2&&s.animation<s.source->animations.size()&&
    !s.source->animations[s.animation].frames.empty()&&s.frame<s.source->animations[s.animation].frames.size()&&
    std::isfinite(s.timeout)&&std::isfinite(s.speed_scale)&&s.speed_scale>=0)||fail(e,"Canvas actual AnimatedSprite state rejected");
}
float duration(const FieldCanvasAnimatedState&s){float speed=s.source->animations[s.animation].speed*s.speed_scale;return speed>0?1.f/speed:0;}
void reset_timeout(FieldCanvasAnimatedState&s){if(s.playing){s.timeout=duration(s);s.is_over=false;}}
bool animation_call(FieldCanvasAnimatedState&s,const FieldCanvasAnimationHost&h,FieldCanvasNativeCall call,std::string&e){
  if(!animation_valid(s,e))return false;
  if(!h.source_call||!h.signal||!h.source_call(s.object,call,e))return fail(e,"Canvas actual animation source call/signal owner rejected");
  return animation_valid(s,e);
}
bool frame_impl(FieldCanvasAnimatedState&s,int32_t frame,const FieldCanvasAnimationHost&h,std::string&e){
  auto count=s.source->animations[s.animation].frames.size();
  frame=std::max(int32_t(0),std::min(frame,int32_t(count)-1));
  if(s.frame==uint32_t(frame))return true;
  s.frame=uint32_t(frame);reset_timeout(s);
  return h.signal(s.object,FieldCanvasNativeSignal::FrameChanged,e)&&animation_valid(s,e);
}
}
bool field_canvas_animation_initialize(const FieldCanvasArtData&d,FieldNodeTreeRuntime&t,
 FieldObjectId object,FieldCanvasAnimatedState&out,std::string&e){
  const auto*state=t.state(object);const auto*descriptor=t.descriptor(object);FieldIdentity identity{};
  const auto*r=state?d.record(state->source):nullptr;
  if(!d.valid()||d.format()!=2||!state||!state->alive||!descriptor||!r||r->kind!=2||
     descriptor->path!=r->node||descriptor->script!=r->owner_script||descriptor->script_sha!=r->owner_sha||
     !t.object_identity(object,identity)||!same(identity,d.identity()))return fail(e,"Canvas animation native constructor source binding rejected");
  FieldCanvasAnimatedState next;next.source=r;next.object=object;next.frame=r->frame;next.playing=r->playing;next.speed_scale=r->speed_scale;
  bool found=false;for(size_t i=0;i<r->animations.size();++i)if(r->animations[i].name==r->animation){next.animation=i;found=true;}
  if(!found)return fail(e,"Canvas animation constructor catalogue absent");
  reset_timeout(next);if(!animation_valid(next,e))return false;out=next;e.clear();return true;
}
bool field_canvas_animation_ready(FieldCanvasAnimatedState&s,const FieldCanvasAnimationHost&h,std::string&e){
  if(!animation_call(s,h,FieldCanvasNativeCall::Ready,e))return false;
  double random=0;
  if(!h.random_range||!h.random_range(s.source->ready_min,s.source->ready_max,random,e)||!std::isfinite(random)||
     random<s.source->ready_min||random>s.source->ready_max||random>INT32_MAX)
    return fail(e,"Canvas Sparkles actual source rand_range receipt rejected");
  return frame_impl(s,int32_t(random),h,e);
}
bool field_canvas_animation_set_frame(FieldCanvasAnimatedState&s,int32_t f,const FieldCanvasAnimationHost&h,std::string&e){
  return animation_call(s,h,FieldCanvasNativeCall::SetProperty,e)&&frame_impl(s,f,h,e);
}
bool field_canvas_animation_set_playing(FieldCanvasAnimatedState&s,bool p,const FieldCanvasAnimationHost&h,std::string&e){
  if(!animation_call(s,h,FieldCanvasNativeCall::SetProperty,e))return false;
  if(s.playing!=p){s.playing=p;reset_timeout(s);}e.clear();return true;
}
bool field_canvas_animation_set_speed(FieldCanvasAnimatedState&s,float speed,const FieldCanvasAnimationHost&h,std::string&e){
  if(!std::isfinite(speed))return fail(e,"Canvas animation speed nonfinite");
  if(!animation_call(s,h,FieldCanvasNativeCall::SetProperty,e))return false;
  float elapsed=duration(s)-s.timeout;s.speed_scale=std::max(speed,0.f);reset_timeout(s);s.timeout-=elapsed;
  return animation_valid(s,e);
}
bool field_canvas_animation_set_animation(FieldCanvasAnimatedState&s,std::string_view name,const FieldCanvasAnimationHost&h,std::string&e){
  if(!animation_call(s,h,FieldCanvasNativeCall::SetProperty,e))return false;
  size_t index=s.source->animations.size();for(size_t i=0;i<index;++i)if(s.source->animations[i].name==name){index=i;break;}
  if(index==s.source->animations.size())return fail(e,"Canvas animation source name absent");
  if(index!=s.animation){s.animation=index;reset_timeout(s);return frame_impl(s,0,h,e);}e.clear();return true;
}
bool field_canvas_animation_idle(FieldCanvasAnimatedState&s,float remaining,bool update_pending,const FieldCanvasAnimationHost&h,std::string&e){
  if(!std::isfinite(remaining)||remaining<0)return fail(e,"Canvas actual native animation delta rejected");
  if(!animation_call(s,h,FieldCanvasNativeCall::IdleInternal,e))return false;
  if(!s.playing||!update_pending)return true;
  while(remaining>0){
    if(!animation_valid(s,e))return false;
    const auto&a=s.source->animations[s.animation];float speed=a.speed*s.speed_scale;
    if(speed==0)return true;
    if(!std::isfinite(speed)||speed<0||duration(s)<=0)return fail(e,"Canvas native animation duration rejected");
    if(s.timeout<=0){
      s.timeout=duration(s);bool finished=false;
      if((!s.backwards&&s.frame==a.frames.size()-1)||(s.backwards&&s.frame==0)){
        if(a.loop){s.frame=s.backwards?uint32_t(a.frames.size()-1):0;finished=true;}
        else if(!s.is_over){s.is_over=true;finished=true;}
      }else if(s.backwards)--s.frame;else ++s.frame;
      if(finished&&(!h.signal(s.object,FieldCanvasNativeSignal::AnimationFinished,e)||!animation_valid(s,e)))return false;
      if(!h.signal(s.object,FieldCanvasNativeSignal::FrameChanged,e)||!animation_valid(s,e))return false;
    }
    float used=std::min(s.timeout,remaining);
    if(used<0||!std::isfinite(used))return fail(e,"Canvas native animation timeout rejected");
    remaining-=used;s.timeout-=used;
  }
  e.clear();return true;
}
} // namespace encore::upstream
