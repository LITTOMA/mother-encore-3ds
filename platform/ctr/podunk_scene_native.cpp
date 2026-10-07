#include "podunk_scene_native.hpp"
#include "house_return_geometry_native.hpp"
#include "podunk_scene_animated_leaves.hpp"
#include "podunk_scene_scripts.hpp"
#include "field_canvas_art_renderer.hpp"
#include "podunk_player_effect_owners.hpp"
#include <algorithm>
// This existing primitive's compact statements predate this owner. Keep its
// diagnostics local rather than changing the frozen renderer's source bytes.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "field_map_renderer.hpp"
#pragma GCC diagnostic pop
#include <fstream>
#include <limits>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const std::string &s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool translation(const FieldTransform &t) {
  return t[0].x == 1 && t[0].y == 0 && t[1].x == 0 && t[1].y == 1;
}
} // namespace
// A closed, concrete ownership adapter for the existing two renderers. It is
// not an arbitrary callback that approves a foreign identity or empty draw.
class PodunkPlayerCanvasForeign final : public FieldCanvasForeignOwner {
public:
  PodunkPlayerCanvasForeign(PodunkPlayerHost &player,
                            const PlayerInitializationData &initial,
                            PodunkConcretePlayerEffectOwners &effects,
                            const PlayerEffectsData &effect_data,
                            FieldGlobalRegistry &registry,
                            FieldNodeTreeRuntime &tree)
      : player_(player), initial_(initial), effects_(effects),
        effect_data_(effect_data), registry_(registry), tree_(tree) {}
  void bind_grass(PodunkSceneGrassFactory *owner) { grass_ = owner; }
  bool sources(std::string &e) const {
    if (!initial_.valid() || !effect_data_.valid() ||
        effect_data_.player_ir_sha256() != initial_.ir_sha256() ||
        player_.registry() != &registry_ || player_.tree() != &tree_ ||
        player_.body().data() != &initial_ ||
        effects_.data() != &effect_data_ || effects_.registry() != &registry_)
      return fail(e,
                  "Canvas foreign actual source bodies/IR/Registry mismatch");
    return true;
  }
  bool admit(FieldObjectId id, const FieldNodeDescriptor &d,
             const FieldIdentity &identity, const FieldNodeTreeRuntime &tree,
             bool &drawable, std::string &e) const override {
    if (!sources(e) || &tree != &tree_ || !registry_.object_exists(id))
      return false;
    auto actual_owner = registry_.tree_owner(id);
    const auto *s = tree_.state(id);
    FieldIdentity actual_identity{};
    if (!actual_owner || actual_owner.get() != &tree_ || !s || !s->alive ||
        !s->inside || !s->bound ||
        !tree_.object_identity(id, actual_identity) ||
        !same(identity, actual_identity) ||
        !same(s->binding.identity, identity) || s->binding.stable_id != d.id ||
        s->binding.class_index != d.class_index ||
        s->binding.native_class != d.native_class ||
        s->binding.script_sha != d.script_sha || !s->binding.family ||
        !s->binding.capability)
      return fail(e, "Canvas foreign actual live source binding rejected");
    if (grass_ && grass_->owns(id))
      return grass_->admit_canvas(id, d, identity, tree, drawable, e);
    const FieldNodeRecipeData *recipe = nullptr;
    if (same(identity, initial_.recipe().identity())) {
      if (!player_.owns(id) || !player_.ready_complete() ||
          s->binding.family != 0x454e0056 || s->binding.capability != 1)
        return fail(e, "Canvas foreign Player actual owning Ready incomplete");
      recipe = &initial_.recipe();
    } else {
      if (!effects_.accepts(identity) || !effects_.owns(id) ||
          s->binding.family != 0x454e0059 || s->binding.capability != 1)
        return fail(e, "Canvas foreign effect actual owning instance absent");
      for (const auto &creator : effect_data_.creators()) {
        const auto *candidate = effect_data_.recipe(creator.kind);
        if (candidate && same(candidate->identity(), identity)) {
          recipe = candidate;
          break;
        }
      }
    }
    const auto *source = recipe ? recipe->record(d.id) : nullptr;
    if (!source || source->native_generated || source->path != d.path ||
        source->native_class != d.native_class || source->script != d.script ||
        source->script_sha != d.script_sha ||
        source->class_index != d.class_index)
      return fail(e, "Canvas foreign descriptor differs from actual checked "
                     "source recipe");
    drawable = d.native_class == "Sprite" || d.native_class == "AnimatedSprite";
    e.clear();
    return true;
  }
  bool draw(const FieldCanvasOrderSlot &slot, const FieldTransform &viewport,
            bool snap, std::string &e) {
    const auto *d = tree_.descriptor(slot.object);
    bool drawable = false;
    if (!d || !slot.foreign ||
        !admit(slot.object, *d, slot.identity, tree_, drawable, e) ||
        !drawable || !slot.foreign_drawable)
      return fail(e, "Canvas foreign GPU slot source ownership rejected");
    if (grass_ && grass_->owns(slot.object)) {
      auto *leaf = grass_->canvas_leaf();
      if (!leaf || !leaf->owns_drawable(slot.object))
        return fail(e, "Grass foreign native slot has no actual GPU leaf");
      return leaf->draw_leaf(slot, viewport, snap, e);
    }
    if (same(slot.identity, initial_.recipe().identity()))
      return player_.draw(slot.object, viewport, snap, e);
    if (!translation(viewport))
      return fail(e, "Canvas foreign effect viewport affine unsupported");
    return effects_.draw(slot.object, Vec2{-viewport[2].x, -viewport[2].y}, e);
  }

private:
  PodunkSceneGrassFactory *grass_ = nullptr;
  PodunkPlayerHost &player_;
  const PlayerInitializationData &initial_;
  PodunkConcretePlayerEffectOwners &effects_;
  const PlayerEffectsData &effect_data_;
  FieldGlobalRegistry &registry_;
  FieldNodeTreeRuntime &tree_;
};
PodunkSceneNative::PodunkSceneNative() = default;
PodunkSceneNative::~PodunkSceneNative() {
  if (map_gpu_)
    map_gpu_->free();
}
bool PodunkSceneNative::prepare(
    const FieldNodeTreeData &d, FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
    PodunkNativeRoot &root, FieldMapSpace &map, FieldGeometrySpace &geometry,
    const FieldCanvasArtData &art, const char *prefix, FieldCanvasArtHost host,
    PodunkSceneMaterialOwner *materials, std::string &e) {
  const auto *m = map.source();
  const auto *g = geometry.source();
  if (data_ || !d.valid() || !m || !g || !art.valid() || !prefix ||
      !same(d.identity(), m->identity()) ||
      !same(d.identity(), g->identity()) ||
      !same(d.identity(), art.identity()) ||
      d.source_scene() != m->source_scene() ||
      d.source_scene() != g->source_scene() ||
      d.source_scene() != art.source_scene() ||
      (materials && materials->registry() != &r))
    return fail(e, "Scene native source/owner identity mismatch");
  // Verify the actual converted map bytes before importing GPU textures.
  for (uint32_t i = 0; i < m->texture_count(); ++i) {
    const auto tex = m->texture(i);
    std::ifstream f(std::string(prefix) + std::string(m->string(tex.path)),
                    std::ios::binary);
    if (!f)
      return fail(e, "Scene native map texture unavailable");
    f.seekg(0, std::ios::end);
    auto n = f.tellg();
    if (n <= 0 || n > 16 * 1024 * 1024)
      return fail(e, "Scene native map texture extent rejected");
    std::vector<uint8_t> bytes(static_cast<size_t>(n));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char *>(bytes.data()), n) ||
        field_canvas_art_renderer_detail::sha256(bytes.data(), bytes.size()) !=
            tex.output_sha256)
      return fail(e, "Scene native map texture source receipt rejected");
  }
  auto mg = std::make_unique<FieldMapRenderer>();
  auto ag = std::make_unique<FieldCanvasArtRenderer>();
  if (!mg->load(*m, prefix, e) || !ag->load(art, prefix, e)) {
    mg->free();
    return false;
  }
  std::map<uint32_t, FieldMapAnimationState> clocks;
  for (uint32_t i = 0; i < m->texture_count(); ++i) {
    const auto tex = m->texture(i);
    if (tex.frames > 1 && !clocks.count(tex.group)) {
      FieldMapAnimationState a;
      if (!m->start_animation(i, a, e)) {
        mg->free();
        return false;
      }
      clocks.emplace(tex.group, a);
    }
  }
  // Pack shape state remains source-native. This owner does not query the
  // preindexed space or pretend script ancestors are admitted during factory
  // construction. The real physics owner must check physics_admitted().
  data_ = &d;
  tree_ = &t;
  registry_ = &r;
  root_ = &root;
  map_ = &map;
  geometry_ = &geometry;
  art_ = &art;
  art_host_ = std::move(host);
  materials_ = materials;
  map_gpu_ = std::move(mg);
  art_gpu_ = std::move(ag);
  animations_ = std::move(clocks);
  if (materials_ && !materials_->bind_images(*art_gpu_, e)) return false;
  e.clear();
  return true;
}
bool PodunkSceneNative::prepare_images(const FieldMapView&m,const FieldCanvasArtData&a,
    const char*prefix,std::string&e){
  for(uint32_t i=0;i<m.texture_count();++i){const auto tex=m.texture(i);
    std::ifstream f(std::string(prefix)+std::string(m.string(tex.path)),std::ios::binary);
    if(!f)return fail(e,"House native map GPU texture unavailable");
    f.seekg(0,std::ios::end);const auto n=f.tellg();
    if(n<=0||n>16*1024*1024)return fail(e,"House native map GPU texture extent rejected");
    std::vector<uint8_t>bytes(static_cast<size_t>(n));f.seekg(0);
    if(!f.read(reinterpret_cast<char*>(bytes.data()),n)||
       field_canvas_art_renderer_detail::sha256(bytes.data(),bytes.size())!=tex.output_sha256)
      return fail(e,"House native map GPU texture source receipt differs");
  }
  auto mg=std::make_unique<FieldMapRenderer>();auto ag=std::make_unique<FieldCanvasArtRenderer>();
  if(!mg->load(m,prefix,e)||!ag->load(a,prefix,e)){mg->free();return false;}
  std::map<uint32_t,FieldMapAnimationState>clocks;
  for(uint32_t i=0;i<m.texture_count();++i){const auto tex=m.texture(i);
    if(tex.frames>1&&!clocks.count(tex.group)){FieldMapAnimationState state;
      if(!m.start_animation(i,state,e)){mg->free();return false;}
      clocks.emplace(tex.group,state);
    }
  }
  map_gpu_=std::move(mg);art_gpu_=std::move(ag);animations_=std::move(clocks);
  e.clear();return true;
}
bool PodunkSceneNative::prepare_house(const HouseReturnSources&sources,
    FieldNodeTreeRuntime&tree,FieldGlobalRegistry&registry,PodunkNativeRoot&root,
    FieldMapSpace&map,FieldGeometrySpace&space,HouseReturnGeometryNative&geometry,
    SourceRandom&random,const char*prefix,FieldCanvasArtHost host,
    PodunkSceneMaterialOwner*materials,FieldCanvasControlOwner*controls,std::string&e){
  const auto&d=sources.tree();const auto&m=sources.map();const auto&a=sources.canvas();
  const auto&g=sources.geometry();
  if(data_||!sources.valid()||!d.valid()||!m.valid()||!a.valid()||a.format()!=2||!g.valid()||
     !prefix||registry.poisoned()||root.kernel_object()!=registry.kernel()||
     !registry.object_exists(root.viewport_object())||
     (tree.object_domain()&&tree.object_domain()!=registry.kernel())||
     (!tree.object_domain()&&tree.object_count())||
     !same(d.identity(),m.identity())||!same(d.identity(),a.identity())||
     d.source_scene()!=m.source_scene()||d.source_scene()!=a.source_scene()||
     d.source_scene()!=g.source_scene()||d.identity().upstream_commit!=g.identity().upstream_commit||
     d.identity().source_sha256!=g.identity().source_sha256||
     (materials&&materials->registry()!=&registry)||
     (controls&&(controls->canvas_tree()!=&tree||controls->canvas_data()!=&a)))
    return fail(e,"House native Canvas requires exact loaded Tree/Map/Canvas and independent geometry source closure");
  for(uint32_t i=0;i<g.node_count();++i){const auto n=g.node(i);const auto*actual=d.record(n.stable_id);
    if(!actual||actual->path!=g.string(n.path)||actual->script_sha!=n.script_sha256||
       actual->script!=g.string(n.script)||actual->class_index>=d.classes().size()||
       d.classes()[actual->class_index]!=g.string(n.class_name)||
       actual->local[0].x!=n.local.x.x||actual->local[0].y!=n.local.x.y||
       actual->local[1].x!=n.local.y.x||actual->local[1].y!=n.local.y.y||
       actual->local[2].x!=n.local.origin.x||actual->local[2].y!=n.local.origin.y||
       actual->world[0].x!=n.world.x.x||actual->world[0].y!=n.world.x.y||
       actual->world[1].x!=n.world.y.x||actual->world[1].y!=n.world.y.y||
       actual->world[2].x!=n.world.origin.x||actual->world[2].y!=n.world.origin.y)
      return fail(e,"House native geometry source node namespace differs from full Tree");
  }
  for(uint32_t i=0;i<m.map_count();++i){const auto r=m.map(i);const auto*actual=sources.tilemap_node(r.stable_id);
    if(!actual||actual->class_index>=d.classes().size()||
       d.classes()[actual->class_index]!="TileMap"||!translation(actual->world)||
       actual->world[2].x!=r.position.x||actual->world[2].y!=r.position.y||r.polygon_count)
      return fail(e,"House native TileMap draw/collision source proof differs");
  }
  // Source Sprite data must use these same actual Canvas records/texture bytes.
  for(const auto&r:sources.sprites().records())if(r.kind==FieldSpriteKind::Character){
    const auto*canvas=a.record(r.id);
    if(!canvas||canvas->kind||canvas->node!=r.node||canvas->hframes!=r.columns||
       canvas->vframes!=r.rows||canvas->frame!=r.frame)
      return fail(e,"House NPC native Sprite constructor differs from checked source Sprite pack");
    for(const auto id:{r.texture,r.sprite,r.setup_texture})if(id){const auto*texture=sources.sprites().texture(id);
      bool found=false;for(const auto&asset:a.textures())if(texture&&asset.source==texture->source){
        if(found||asset.width!=texture->width||asset.height!=texture->height)
          return fail(e,"House native source Sprite GPU extent/uniqueness differs");
        found=true;
      }
      if(!found)return fail(e,"House native source Sprite texture is outside complete Canvas closure");
    }
  }
  if(!prepare_images(m,a,prefix,e))return false;
  data_=&d;tree_=&tree;registry_=&registry;root_=&root;map_=&map;geometry_=&space;art_=&a;
  house_sources_=&sources;house_map_=&m;house_geometry_=&geometry;house_random_=&random;
  house_controls_=controls;materials_=materials;art_host_=std::move(host);
  // This owner concretely consumes the source Sparkles _ready and native
  // clock. Other owner classes keep the caller's actual source consumers.
  const auto bind=art_host_.bind_owner;const auto appearance=art_host_.appearance;
  art_host_.bind_owner=[this,bind](const auto&r,auto object,auto owner,auto&error){
    if(r.kind!=2)return bind?bind(r,object,owner,error):fail(error,"House Canvas concrete source owner is missing");
    const auto it=instances_.find(object);const auto*s=tree_->state(object);
    const auto*d=tree_->descriptor(object);
    if(r.owner!=FieldCanvasOwner::Sparkles||r.owner_id!=r.id||owner!=object||it==instances_.end()||
       it->second.kind!=Kind::Animated||!it->second.bound||!s||!s->alive||!s->bound||
       !d||d->script!=r.owner_script||d->script_sha!=r.owner_sha||
       it->second.animated.source!=&r||it->second.animated.object!=object||
       tree_->source_object(r.id)!=object||!registry_->object_exists(object)||
       registry_->tree_owner(object).get()!=tree_)
      return fail(error,"House Sparkles binding lacks its actual constructed source/native consumer");
    error.clear();return true;
  };
  art_host_.appearance=[this,appearance](const auto&r,auto object,auto owner,auto&out,auto&error){
    if(r.kind!=2)return appearance?appearance(r,object,owner,out,error):fail(error,"House Canvas concrete appearance consumer is missing");
    if(object!=owner||r.owner!=FieldCanvasOwner::Sparkles)return fail(error,"House Sparkles source owner differs");
    std::string animation;uint32_t frame=0;FieldCanvasAppearance native;
    if(!animated_snapshot(object,animation,frame,native,error))return false;
    // The shared command already selected this actual frame's AtlasTexture.
    // Source appearance supplies native offset/flip without replacing that
    // selected physical texture with the AnimatedSprite's null base texture.
    out.offset=native.offset;out.size=native.size;out.centered=native.centered;
    out.flip_h=native.flip_h;out.flip_v=native.flip_v;out.action=native.action;
    return true;
  };
  if(materials_&&!materials_->bind_images(*art_gpu_,e))return false;
  e.clear();return true;
}
const FieldMapView*PodunkSceneNative::draw_map_source()const{return house_map_?house_map_:map_->source();}
bool PodunkSceneNative::tile_pose(uint32_t index,FieldMapDraw&out,std::string&e)const{
  const auto*source=draw_map_source();
  if(!source||index>=source->draw_count())return fail(e,"Native TileMap draw source index rejected");
  if(!house_sources_){out=map_->draw(index);e.clear();return true;}
  out=source->draw(index);const auto layer=source->map(out.map);
  FieldObjectId id=0;FieldTransform world;
  if(!house_live(e)||!source_object(layer.stable_id,id,e)||
     !tree_->world_transform(id,world,e)||!translation(world))
    return fail(e,"House TileMap pose lacks actual same-space translated native owner");
  const auto&n=instances_.at(id);const auto*s=tree_->state(id);
  if(n.kind!=Kind::Map||n.map!=out.map||!n.entered||!n.ready||!s->inside||!s->ready_notified)
    return fail(e,"House TileMap pose before actual native Enter/Ready");
  out.position.x+=world[2].x-layer.position.x;
  out.position.y+=world[2].y-layer.position.y;
  if(!std::isfinite(out.position.x)||!std::isfinite(out.position.y))
    return fail(e,"House TileMap translated pose overflow");
  e.clear();return true;
}
bool PodunkSceneNative::collect_house_tiles(FieldMapRect rect,std::vector<uint32_t>&out,
    std::string&e)const{
  if(!house_live(e))return false;
  std::vector<uint32_t>candidate;
  // Each map has its own actual translated Canvas transform. Culling uses
  // the inverse translation against the immutable native quadrant bounds.
  for(uint32_t i=0;i<house_map_->map_count();++i){
    const auto layer=house_map_->map(i);
    const auto q=source_objects_.find(layer.stable_id);
    if(q==source_objects_.end()){
      if(!house_deleted_.count(layer.stable_id))
        return fail(e,"House TileMap source vanished without actual native deletion");
      continue;
    }
    FieldObjectId id=0;FieldTransform world;
    if(!source_object(layer.stable_id,id,e)||!tree_->world_transform(id,world,e)||!translation(world))return false;
    const auto*s=tree_->state(id);const auto&n=instances_.at(id);
    if(!s||!s->inside||!s->ready_notified||!n.entered||!n.ready||n.map!=i)
      return fail(e,"House TileMap spatial draw before its actual native lifecycle");
    if(!tree_->visible_in_tree(id))continue;
    const Vec2 delta{world[2].x-layer.position.x,world[2].y-layer.position.y};
    auto query=rect;query.minimum.x-=delta.x;query.minimum.y-=delta.y;
    query.maximum.x-=delta.x;query.maximum.y-=delta.y;
    std::vector<uint8_t>mask(house_map_->map_count(),0);mask[i]=1;
    std::vector<uint32_t>indices;
    if(!house_map_->collect_draws_masked(query,mask,house_map_->draw_count(),indices,e))return false;
    candidate.insert(candidate.end(),indices.begin(),indices.end());
  }
  std::sort(candidate.begin(),candidate.end());
  if(std::adjacent_find(candidate.begin(),candidate.end())!=candidate.end()||candidate.size()>house_map_->draw_count())
    return fail(e,"House TileMap actual spatial collection duplicated native draw indices");
  out=std::move(candidate);e.clear();return true;
}
bool PodunkSceneNative::tile_sort_children(FieldObjectId id,
    std::vector<FieldCanvasNativeTileChild>&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  const auto it=instances_.find(id);
  if(!house_live(e)||it==instances_.end()||!actual(id,d,s,e)||
     it->second.kind!=Kind::Map||!it->second.entered||!it->second.ready||
     !s->inside||!s->ready_notified||!(s->flags&128u)||it->second.map>=house_map_->map_count())
    return fail(e,"House native tile children lack actual sorted TileMap owner/Ready");
  const auto layer=house_map_->map(it->second.map);
  if(layer.stable_id!=d->id||!(layer.flags&1u))
    return fail(e,"House native tile children differ from checked native ysort property");
  std::vector<FieldCanvasNativeTileChild>children;
  for(uint32_t i=layer.draw_first;i<layer.draw_first+layer.draw_count;++i){
    const auto draw=house_map_->draw(i);const auto cell=house_map_->cell(draw.cell);
    const auto anchor=house_map_->sort_anchor(i);
    if(draw.map!=it->second.map||draw.cell<layer.cell_first||
       draw.cell>=layer.cell_first+layer.cell_count||cell.quadrant_ordinal<0||
       i<cell.draw_first||i>=cell.draw_first+cell.draw_count)
      return fail(e,"House native tile quadrant/source draw ordinal rejected");
    FieldCanvasNativeTileChild child;
    child.draw_index=i;child.quadrant_order=uint32_t(cell.quadrant_ordinal);
    child.local_position={anchor.x-layer.position.x,anchor.y-layer.position.y};child.z=draw.z;
    if(!std::isfinite(child.local_position.x)||!std::isfinite(child.local_position.y))
      return fail(e,"House native tile quadrant anchor overflow");
    children.push_back(child);
  }
  out=std::move(children);e.clear();return true;
}
bool PodunkSceneNative::bind_replaced_house(const FieldNodeTreeRuntime&old_tree,
    FieldObjectId old_root,FieldObjectId root,std::string&e){
  const auto*s=tree_?tree_->state(root):nullptr;const auto*d=tree_?tree_->descriptor(root):nullptr;
  if(!house_sources_||!finished_||house_replaced_||&old_tree==tree_||!old_root||
     old_tree.root()!=old_root||old_tree.state(old_root)||old_tree.lifecycle_pending()||
     registry_->object_exists(old_root)||old_tree.object_domain()!=tree_->object_domain()||
     geometry_->source()!=&house_sources_->geometry()||map_->house_source()!=&house_sources_->reentry()||
     !s||!s->alive||!s->bound||!d||root!=tree_->root()||
     tree_->source_object(data_->identity().scene_id)!=root||d->id!=data_->identity().scene_id||
     !registry_->object_exists(root)||registry_->tree_owner(root).get()!=tree_)
    return fail(e,"House Canvas bind requires actual old-root deletion and same-space source replacement");
  house_old_tree_=&old_tree;house_old_root_=old_root;house_root_=root;house_replaced_=true;
  e.clear();return true;
}
bool PodunkSceneNative::house_live(std::string&e)const{
  const auto*s=tree_?tree_->state(house_root_):nullptr;
  if(!house_sources_||!house_sources_->valid()||!house_replaced_||!house_old_tree_||
     house_old_tree_->state(house_old_root_)||registry_->object_exists(house_old_root_)||
     house_old_tree_->lifecycle_pending()||geometry_->source()!=&house_sources_->geometry()||
     map_->house_source()!=&house_sources_->reentry()||house_map_!=&house_sources_->map()||
     art_!=&house_sources_->canvas()||tree_->root()!=house_root_||
     tree_->source_object(data_->identity().scene_id)!=house_root_||
     !s||!s->alive||!s->inside||!s->bound||
     !s->ready_notified||!registry_->object_exists(house_root_)||
     registry_->tree_owner(house_root_).get()!=tree_||
     !root_->viewport().world_registered||root_->viewport().current_scene!=house_root_)
    return fail(e,"House Canvas draw lacks actual current same-space House source owners");
  e.clear();return true;
}
bool PodunkSceneNative::owns(const FieldNodeDescriptor &d) const {
  const auto &c = d.native_class;
  if(house_sources_){
    const auto*source=data_->record(d.id);
    if(!source||source->path!=d.path||source->class_index>=data_->classes().size()||
       data_->classes()[source->class_index]!=c)return false;
    // CollisionObject/shape and Control owners are independently concrete.
    return c=="Node"||c=="Node2D"||c=="Position2D"||c=="YSort"||c=="TileMap"||
      c=="Sprite"||c=="TextureRect"||c=="AnimatedSprite"||c=="ColorRect";
  }
  return c == "Node" || c == "Node2D" || c == "Position2D" || c == "YSort" ||
         c == "TileMap" || c == "StaticBody2D" || c == "Area2D" ||
         c == "CollisionShape2D" || c == "CollisionPolygon2D" || c == "Sprite";
}
bool PodunkSceneNative::owns(FieldObjectId id) const {
  return instances_.count(id) != 0;
}
bool PodunkSceneNative::actual(FieldObjectId id, const FieldNodeDescriptor *&d,
                               const FieldNodeState *&s, std::string &e) const {
  FieldIdentity identity;
  auto owner = registry_ ? registry_->tree_owner(id) : nullptr;
  d = tree_ ? tree_->descriptor(id) : nullptr;
  s = tree_ ? tree_->state(id) : nullptr;
  if (!data_ || !owner || owner.get() != tree_ ||
      !registry_->object_exists(id) || !d || !s || !s->alive ||
      !tree_->object_identity(id, identity) ||
      !same(identity, data_->identity()))
    return fail(e, "Scene native actual ObjectDB/tree identity rejected");
  const auto *source = data_->record(d->id);
  if (!source || source->path != d->path ||
      source->class_index != d->class_index ||
      source->script_sha != d->script_sha || source->script != d->script ||
      data_->classes().at(source->class_index) != d->native_class)
    return fail(e, "Scene native source descriptor binding rejected");
  if(house_sources_&&(tree_->source_object(d->id)!=id||
      data_!=&house_sources_->tree()||art_!=&house_sources_->canvas()||
      tree_->object_domain()!=registry_->kernel()))
    return fail(e,"House Canvas actual source lookup/domain changed");
  return true;
}
bool PodunkSceneNative::material(FieldObjectId id, const FieldCanvasRecord &a,
                                 std::string &e) const {
  if (a.shader == FieldCanvasShader::Default)
    return true;
  PodunkSceneMaterialState state;
  if (!materials_ || !materials_->material(id, a, state, e))
    return fail(e, "Scene native material owner unavailable: " + a.node);
  const auto *resource = registry_->source_resource(state.material);
  std::array<uint8_t, 32> sha{};
  if (!resource ||
      std::string_view(resource->resource_class()) != "ShaderMaterial" ||
      !art_->source_hash(a.shader_source.substr(0, a.shader_source.find("::")),
                         sha) ||
      state.shader_source != a.shader_source || state.shader_source_sha != sha)
    return fail(e, "Scene native actual source ShaderMaterial rejected: " +
                       a.node);
  const auto b = resource->binding();
  std::array<uint8_t, 32> material_sha{};
  if (b.object != state.material ||
      !art_->source_hash(b.source.source.substr(0, b.source.source.find("::")),
                         material_sha) ||
      b.source.source_sha != material_sha ||
      b.source.identity.upstream_commit != data_->identity().upstream_commit)
    return fail(e,
                "Scene native ShaderMaterial source proof mismatch: " + a.node);
  return true;
}
bool PodunkSceneNative::construct(FieldObjectId id,
                                  const FieldNodeDescriptor &d,
                                  const FieldIdentity &identity,
                                  std::string &e) {
  const FieldNodeDescriptor *actual_d;
  const FieldNodeState *s;
  if (!same(identity, data_ ? data_->identity() : FieldIdentity{}) ||
      !owns(d) || instances_.count(id) || source_objects_.count(d.id) ||
      !actual(id, actual_d, s, e) || actual_d->id != d.id || s->inside)
    return fail(e, "Scene native constructor cursor/identity rejected");
  Instance n;
  n.source = d.id;
  if (d.native_class == "Node")
    n.kind = Kind::Node;
  else if (d.native_class == "TileMap")
    n.kind = Kind::Map;
  else if (d.native_class == "StaticBody2D")
    n.kind = Kind::Body;
  else if (d.native_class == "Area2D")
    n.kind = Kind::Area;
  else if (d.native_class == "CollisionShape2D" ||
           d.native_class == "CollisionPolygon2D")
    n.kind = Kind::Shape;
  else if (d.native_class == "Sprite")
    n.kind = Kind::Sprite;
  else if(house_sources_&&d.native_class=="TextureRect")n.kind=Kind::TextureRect;
  else if(house_sources_&&d.native_class=="AnimatedSprite")n.kind=Kind::Animated;
  else if(house_sources_&&d.native_class=="ColorRect")n.kind=Kind::ColorRect;
  else
    n.kind = Kind::Canvas;
  auto *g = house_sources_?&house_sources_->geometry():geometry_->source();
  for (uint32_t i = 0; i < g->node_count(); ++i)
    if (g->node(i).stable_id == d.id) {
      n.geometry_node = i;
      break;
    }
  if (n.kind == Kind::Body || n.kind == Kind::Area) {
    for (uint32_t i = 0; i < g->owner_count(); ++i)
      if (g->owner(i).node == n.geometry_node) {
        n.owner = i;
        break;
      }
    if (n.owner == UINT32_MAX)
      return fail(e, "Scene native collision owner missing: " + d.path);
    const auto o = g->owner(n.owner);
    if ((n.kind == Kind::Body &&
         (o.kind != 1 || o.constant_linear_velocity.x ||
          o.constant_linear_velocity.y || o.constant_angular_velocity)) ||
        (n.kind == Kind::Area &&
         (o.kind != 4 || o.space_override || (o.flags & 8))))
      return fail(e, "Scene native collision owner behavior unsupported: " +
                         d.path);
  }
  if (n.kind == Kind::Shape) {
    for (uint32_t i = 0; i < g->shape_count(); ++i)
      if (g->shape(i).node == n.geometry_node) {
        n.shape = i;
        break;
      }
    if (n.shape == UINT32_MAX)
      return fail(e, "Scene native source shape missing: " + d.path);
    const auto shape = g->shape(n.shape);
    if (shape.flags & 2)
      return fail(e, "Scene native one-way collision adapter unavailable: " +
                         d.path);
    n.disabled = bool(shape.flags & 1);
  }
  if (n.kind == Kind::Map) {
    for (uint32_t i = 0; i < draw_map_source()->map_count(); ++i)
      if (draw_map_source()->map(i).stable_id == d.id) {
        n.map = i;
        break;
      }
    if (n.map == UINT32_MAX || !translation(d.world))
      return fail(e,
                  "Scene native TileMap source transform missing: " + d.path);
    if(house_sources_&&!tree_->set_sort_children(id,bool(house_map_->map(n.map).flags&1u),e))
      return false;
  }
  if (n.kind == Kind::Sprite||n.kind==Kind::TextureRect||n.kind==Kind::Animated||n.kind==Kind::ColorRect) {
    const auto *a = art_->record(d.id);
    const auto kind=n.kind==Kind::Sprite?0u:n.kind==Kind::TextureRect?1u:n.kind==Kind::Animated?2u:3u;
    if (!a || a->kind != kind || !material(id, *a, e))
      return fail(e, "Scene native Sprite source/material rejected: " + d.path +
                         ": " + e);
    n.sprite.texture = a->texture;
    n.sprite.hframes = a->hframes;
    n.sprite.vframes = a->vframes;
    n.sprite.frame = a->frame;
    n.sprite.offset = a->offset;
    n.sprite.size = a->size;
    n.sprite.centered = a->centered;
    n.sprite.flip_h = a->flip_h;
    n.sprite.flip_v = a->flip_v;
    if(n.kind==Kind::Animated&&
       !field_canvas_animation_initialize(*art_,*tree_,id,n.animated,e))return false;
    if(n.kind==Kind::ColorRect)n.color=a->color;
  }
  instances_.emplace(id, n);
  source_objects_.emplace(d.id, id);
  e.clear();
  return true;
}
bool PodunkSceneNative::bind(FieldObjectId id, const FieldNodeBinding &b,
                             std::string &e) {
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  auto it = instances_.find(id);
  if (it == instances_.end() || !actual(id, d, s, e) ||
      !same(b.identity, data_->identity()) || b.stable_id != d->id ||
      b.class_index != d->class_index || b.native_class != d->native_class ||
      b.script_sha != d->script_sha || !b.family || !b.capability)
    return fail(e, "Scene native combined typed binding rejected");
  if(house_sources_&&it->second.kind==Kind::Animated&&
     (b.family!=0x454e0040||b.capability!=2||d->script_methods!=1))
    return fail(e,"House Sparkles combined binding must name its actual Canvas source consumer");
  it->second.bound = true;
  e.clear();
  return true;
}
bool PodunkSceneNative::finish_factory(std::string &e) {
  if (!data_ || finished_ || !tree_->root())
    return fail(e, "Scene native factory state rejected");
  for (const auto &row : data_->records()) {
    FieldNodeDescriptor d = row;
    d.native_class = data_->classes().at(row.class_index);
    if (owns(d) && !source_objects_.count(d.id))
      return fail(e, "Scene native factory omitted source node: " + d.path);
  }
  if (!canvas_.initialize(*art_, *data_, *tree_, art_host_, e) ||
      !canvas_.bind_native(*this, e))
    return false;
  if(house_sources_){
    if(!house_controls_||house_controls_->canvas_tree()!=tree_||house_controls_->canvas_data()!=art_||
       !canvas_.bind_control(*house_controls_,e))
      return fail(e,"House Canvas factory requires actual complete Control property owners");
    for(const auto&c:art_->control_boundaries()){
      const auto object=tree_->source_object(c.id),owner=tree_->source_object(c.owner_id);
      const auto*s=tree_->state(object);const auto*d=tree_->descriptor(object);
      if(!s||!d||!s->alive||!s->bound||d->native_class!=c.native_class||
         registry_->tree_owner(object).get()!=tree_||!registry_->object_exists(object)||
         !house_controls_->admit_control(c,object,owner,e))
        return fail(e,"House Canvas Control native factory/property receipt incomplete: "+c.node);
    }
  }
  finished_ = true;
  e.clear();
  return true;
}
bool PodunkSceneNative::bind_animated_leaves(PodunkSceneAnimatedLeaves &owner,
                                             std::string &e) {
  if (!finished_ || animated_leaves_ || owner.tree() != tree_ ||
      owner.registry() != registry_ || owner.source() != data_)
    return fail(
        e, "Scene native animated leaves require same actual source owner");
  animated_leaves_ = &owner;
  return true;
}
bool PodunkSceneNative::bind_canvas_leaf(PodunkSceneCanvasLeaf &owner,
                                       std::string &e) {
  if (!finished_ || owner.canvas_tree()!=tree_ ||
      owner.canvas_registry()!=registry_ ||
      std::find(canvas_leaves_.begin(),canvas_leaves_.end(),&owner)!=canvas_leaves_.end())
    return fail(e,"Canvas leaf must borrow the same actual tree/ObjectDB once");
  canvas_leaves_.push_back(&owner); e.clear(); return true;
}
bool PodunkSceneNative::bind_grass(PodunkSceneGrassFactory &owner, std::string &e) {
  auto *leaf = owner.canvas_leaf();
  if (!finished_ || grass_ || !leaf || owner.registry() != registry_ ||
      leaf->canvas_tree() != tree_ || leaf->canvas_registry() != registry_)
    return fail(e, "Grass foreign compositor requires the same actual tree and GPU owner");
  grass_ = &owner;
  if (foreign_) foreign_->bind_grass(grass_);
  e.clear(); return true;
}
bool PodunkSceneNative::bind_foreign(PodunkPlayerHost &player,
                                     const PlayerInitializationData &initial,
                                     PodunkConcretePlayerEffectOwners &effects,
                                     const PlayerEffectsData &data,
                                     std::string &e) {
  if (!finished_ || foreign_)
    return fail(e, "Scene native foreign owner binding cursor rejected");
  auto owner = std::make_unique<PodunkPlayerCanvasForeign>(
      player, initial, effects, data, *registry_, *tree_);
  owner->bind_grass(grass_);
  if (!owner->sources(e) || !canvas_.bind_foreign(*owner, e))
    return false;
  foreign_ = std::move(owner);
  e.clear();
  return true;
}
bool PodunkSceneNative::synchronize(FieldObjectId id, Instance &n,
                                    std::string &e) {
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  if (!actual(id, d, s, e))
    return false;
  if(house_sources_){
    if(n.geometry_node!=UINT32_MAX&&!house_geometry_->observe_transform(id,e))return false;
    if(n.kind==Kind::Map){FieldTransform world;
      if(!tree_->world_transform(id,world,e)||!translation(world))
        return fail(e,"House TileMap live affine transform is outside checked native drawing capability");
    }
    e.clear();return true;
  }
  if (n.geometry_node != UINT32_MAX && geometry_bound_) {
    FieldGeometryNodeUpdate u;
    u.stable_id = n.source;
    if (!n.synchronized || s->local[0].x != n.space_local[0].x ||
        s->local[0].y != n.space_local[0].y ||
        s->local[1].x != n.space_local[1].x ||
        s->local[1].y != n.space_local[1].y ||
        s->local[2].x != n.space_local[2].x ||
        s->local[2].y != n.space_local[2].y)
      u.fields = 1;
    u.local = {s->local[0], s->local[1], s->local[2]};
    if (n.kind == Kind::Shape &&
        (!n.synchronized || n.space_disabled != (n.disabled || !n.entered))) {
      u.fields |= 4;
      u.disabled = n.disabled || !n.entered;
    }
    if (u.fields && !geometry_->apply_updates({u}, e))
      return false;
    n.space_local = s->local;
    n.space_disabled = n.disabled || !n.entered;
    n.synchronized = true;
  }
  for (uint32_t i = 0; i < map_->source()->canvas_count(); ++i)
    if (map_->source()->canvas(i).stable_id == n.source) {
      if (!map_->set_canvas_visibility(n.source,
                                       n.entered && bool(s->flags & 2), e))
        return false;
      break;
    }
  if (n.kind == Kind::Map) {
    FieldTransform world;
    const auto c = map_->canvas(map_->map(n.map).canvas);
    if (!tree_->world_transform(id, world, e) || !translation(world) ||
        world[2].x != c.position.x || world[2].y != c.position.y)
      return fail(e, "Scene native TileMap live transform not synchronized "
                     "with actual MapSpace: " +
                         d->path);
  }
  e.clear();
  return true;
}
bool PodunkSceneNative::phase(FieldObjectId id, FieldTreePhase p,
                              std::string &e) {
  if(house_sources_&&(p==FieldTreePhase::ReadyScript||p==FieldTreePhase::EnterScript||
                     p==FieldTreePhase::ExitScript))
    return fail(e,"House Canvas native phase cannot grant or discard a source script lifecycle");
  auto it = instances_.find(id);
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  if (it == instances_.end() || !it->second.bound || !actual(id, d, s, e))
    return fail(e, "Scene native phase actual owner unavailable");
  auto &n = it->second;
  if (p == FieldTreePhase::EnterNative) {
    if (n.entered || !s->inside || !root_->viewport().world_registered)
      return fail(e, "Scene native Enter world/parent cursor rejected");
    n.entered = true;
    if (!synchronize(id, n, e))
      return false;
  } else if (p == FieldTreePhase::ReadyNative) {
    if (!n.entered || !s->inside || !s->bound)
      return fail(e, "Scene native Ready without actual Enter/bind");
    if ((n.kind == Kind::Sprite||n.kind==Kind::TextureRect||n.kind==Kind::Animated||n.kind==Kind::ColorRect) && !material(id, *art_->record(n.source), e))
      return false;
    if (!synchronize(id, n, e))
      return false;
    n.ready = true;
    if(house_sources_&&n.kind==Kind::Animated&&n.animated.playing&&
       !tree_->add_group(id,"idle_process_internal",e))return false;
  } else if (p == FieldTreePhase::ExitNative) {
    if (!n.entered)
      return fail(e, "Scene native Exit without Enter");
    if (n.monitored && (!world_ || !world_->static_monitor_exit(id, e)))
      return false;
    n.monitored = false;
    n.entered = false;
    if (!synchronize(id, n, e))
      return false;
    monitors_active_ = false;
  } else if (p == FieldTreePhase::TransformChanged ||
             p == FieldTreePhase::LocalTransformChanged ||
             p == FieldTreePhase::VisibilityChanged ||
             p == FieldTreePhase::Hide) {
    if (!synchronize(id, n, e))
      return false;
  } else if (p == FieldTreePhase::Deleting) {
    if (n.entered || n.monitored)
      return fail(e, "Scene native deletion before physical Exit");
    if (n.geometry_node != UINT32_MAX&&house_sources_) {
      if(!house_geometry_->observe_deleted(id,e))return false;
    }else if(n.geometry_node != UINT32_MAX) {
      FieldGeometryNodeUpdate u;
      u.stable_id = n.source;
      u.fields = 2;
      u.deleted = true;
      if (!geometry_->apply_updates({u}, e))
        return false;
    }
    if(!house_sources_)for (uint32_t i = 0; i < map_->source()->canvas_count(); ++i)
      if (map_->source()->canvas(i).stable_id == n.source &&
          !map_->commit_deleted(n.source, e))
        return false;
    if(house_sources_){
      house_deleted_.insert(n.source);
      if(n.kind==Kind::Animated&&!tree_->remove_group(id,"idle_process_internal",e))return false;
    }
    source_objects_.erase(n.source);
    instances_.erase(it);
  } else if (p == FieldTreePhase::Physics || p == FieldTreePhase::Idle ||
             p == FieldTreePhase::PhysicsInternal ||
             p == FieldTreePhase::IdleInternal || p == FieldTreePhase::Input ||
             p == FieldTreePhase::UnhandledInput ||
             p == FieldTreePhase::UnhandledKeyInput)
    return fail(
        e,
        "Scene native script/internal process must use actual typed owner: " +
            d->path);
  // The remaining base Node/Canvas notifications have no subclass body here;
  // their actual parent/path/order/visibility state is maintained by Tree.
  e.clear();
  return true;
}
bool PodunkSceneNative::sprite_snapshot(FieldObjectId id,
                                        FieldCanvasAppearance &out,
                                        std::string &e) const {
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  const auto i = instances_.find(id);
  if (i == instances_.end() || (i->second.kind != Kind::Sprite&&i->second.kind!=Kind::TextureRect) ||
      !actual(id, d, s, e) || !art_->record(d->id))
    return fail(e, "Scene native Sprite snapshot has no same live body");
  out = i->second.sprite;
  e.clear();
  return true;
}
FieldCanvasAnimationHost PodunkSceneNative::animation_host(){
  FieldCanvasAnimationHost host;
  host.source_call=[this](FieldObjectId id,FieldCanvasNativeCall call,std::string&e){
    const FieldNodeDescriptor*d;const FieldNodeState*s;
    auto it=instances_.find(id);
    if(!house_sources_||!animation_call_active_||animation_call_object_!=id||
       it==instances_.end()||it->second.kind!=Kind::Animated||!actual(id,d,s,e)||
       !it->second.bound||!it->second.entered||!s->inside||!s->bound||
       !s->ready_notified||it->second.animated.source!=art_->record(d->id))
      return fail(e,"House AnimatedSprite call lacks actual source/native invocation scope");
    const auto&r=*it->second.animated.source;
    if(r.owner!=FieldCanvasOwner::Sparkles||r.owner_id!=r.id||
       d->script!=r.owner_script||d->script_sha!=r.owner_sha||d->script_methods!=1||
       s->binding.family!=0x454e0040||s->binding.capability!=2||
       r.ready_method!="_ready")
      return fail(e,"House AnimatedSprite source script/owner receipt differs");
    // Source _ready invokes the native set_frame setter in this same scope.
    if(call!=animation_call_&&!(call==FieldCanvasNativeCall::SetProperty&&
                               animation_call_==FieldCanvasNativeCall::Ready))
      return fail(e,"House AnimatedSprite native call differs from actual dispatch");
    if(call==FieldCanvasNativeCall::IdleInternal&&
       (!it->second.ready||!it->second.source_ready))
      return fail(e,"House AnimatedSprite internal notification before source/native Ready");
    e.clear();return true;
  };
  host.random_range=[this](float from,float to,double&out,std::string&e){
    auto it=instances_.find(animation_call_object_);
    if(!house_random_||!animation_call_active_||animation_call_!=FieldCanvasNativeCall::Ready||
       it==instances_.end()||!it->second.animated.source||
       from!=it->second.animated.source->ready_min||to!=it->second.animated.source->ready_max)
      return fail(e,"House Sparkles RNG is outside actual source Ready bounds");
    out=house_random_->rand_range(from,to);e.clear();return true;
  };
  host.signal=[this](FieldObjectId id,FieldCanvasNativeSignal signal,std::string&e){
    const FieldNodeDescriptor*d;const FieldNodeState*s;
    if(!animation_call_active_||animation_call_object_!=id||!actual(id,d,s,e)||
       !sprite_signals_||sprite_signals_->registry()!=registry_||
       (signal!=FieldCanvasNativeSignal::FrameChanged&&signal!=FieldCanvasNativeSignal::AnimationFinished))
      return fail(e,"House AnimatedSprite signal lacks same actual native call/ObjectDB bus");
    return sprite_signals_->emit(id,signal==FieldCanvasNativeSignal::FrameChanged?
      "frame_changed":"animation_finished",{},e);
  };
  return host;
}
bool PodunkSceneNative::house_sparkles_binding(FieldObjectId id,FieldNodeBinding&out,
    std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  auto it=instances_.find(id);
  if(!house_sources_||it==instances_.end()||it->second.kind!=Kind::Animated||
     !actual(id,d,s,e)||d->script_methods!=1)
    return fail(e,"House Sparkles source binding has no exact constructed source/native instance");
  const auto*r=art_->record(d->id);
  if(!r||r->kind!=2||r->owner!=FieldCanvasOwner::Sparkles||r->owner_id!=d->id||
     r->owner_script!=d->script||r->owner_sha!=d->script_sha||r->ready_method!="_ready"||
     it->second.animated.source!=r||it->second.animated.object!=id)
    return fail(e,"House Sparkles attachment differs from checked source implementation");
  out={data_->identity(),d->id,d->class_index,0x454e0040,2,d->script_sha,d->native_class};
  e.clear();return true;
}
bool PodunkSceneNative::phase_house(FieldObjectId id,FieldTreePhase p,float dt,
    bool paused,bool update_pending,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  auto it=instances_.find(id);
  if(!house_sources_||it==instances_.end()||!actual(id,d,s,e))
    return fail(e,"House Canvas phase has no actual owning instance");
  if(p!=FieldTreePhase::ReadyScript&&p!=FieldTreePhase::IdleInternal){
    if(p==FieldTreePhase::EnterScript||p==FieldTreePhase::ExitScript){
      FieldNodeBinding source;
      if(!house_sparkles_binding(id,source,e)||!it->second.bound||!s->bound||
         !it->second.entered||!s->inside)
        return fail(e,"House source lifecycle belongs to its concrete script consumer");
      // Only this audited source has neither _enter_tree nor _exit_tree.
      e.clear();return true;
    }
    return phase(id,p,e);
  }
  auto&n=it->second;
  if(n.kind!=Kind::Animated||!n.bound||!n.entered||!s->inside||!s->ready_notified||
     !s->bound||animation_call_active_||!finished_||
     !std::isfinite(dt)||dt<0)
    return fail(e,"House Sparkles actual source/native notification cursor rejected");
  if(p==FieldTreePhase::IdleInternal){
    if(!n.source_ready||!n.ready)return fail(e,"House Sparkles process before source/native Ready");
    if(!tree_->can_process(id,paused)){e.clear();return true;}
  }else{
    // An explicit request_ready repeats the original script body; a normal
    // detach/re-enter does not dispatch ReadyScript and preserves this state.
    if(s->ready_first)return fail(e,"House Sparkles ReadyScript before actual Tree Ready cursor");
  }
  animation_call_active_=true;animation_call_object_=id;
  animation_call_=p==FieldTreePhase::ReadyScript?FieldCanvasNativeCall::Ready:FieldCanvasNativeCall::IdleInternal;
  const auto host=animation_host();
  const bool ok=p==FieldTreePhase::ReadyScript?
    field_canvas_animation_ready(n.animated,host,e):
    field_canvas_animation_idle(n.animated,dt,update_pending,host,e);
  animation_call_active_=false;animation_call_object_=0;
  if(ok&&p==FieldTreePhase::ReadyScript)n.source_ready=true;
  return ok;
}
bool PodunkSceneNative::deferred_house(const FieldDeferredMessage&m,std::string&e){
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  auto it=instances_.find(m.object);
  if(!house_sources_||it==instances_.end()||!actual(m.object,d,s,e)||
     it->second.kind!=Kind::Animated||m.kind!=FieldDeferredKind::Set||m.args.size()!=1||
     !it->second.source_ready||!it->second.ready||!it->second.entered||!s->inside||
     !s->ready_notified||animation_call_active_)
    return fail(e,"House AnimatedSprite setter lacks actual checked live receiver");
  const auto&v=m.args.front();auto&n=it->second;
  // Validate the source setter signature before entering its native scope.
  if((m.member=="frame"&&(!std::holds_alternative<int64_t>(v)||
       std::get<int64_t>(v)<INT32_MIN||std::get<int64_t>(v)>INT32_MAX))||
     (m.member=="playing"&&!std::holds_alternative<bool>(v))||
     (m.member=="speed_scale"&&(!std::holds_alternative<double>(v)||
       !std::isfinite(std::get<double>(v))||std::abs(std::get<double>(v))>std::numeric_limits<float>::max()))||
     (m.member=="animation"&&!std::holds_alternative<std::string>(v))||
     (m.member!="frame"&&m.member!="playing"&&m.member!="speed_scale"&&m.member!="animation"))
    return fail(e,"House AnimatedSprite unknown setter/property signature");
  animation_call_active_=true;animation_call_object_=m.object;animation_call_=FieldCanvasNativeCall::SetProperty;
  const auto host=animation_host();bool ok=false;
  if(m.member=="frame")ok=field_canvas_animation_set_frame(n.animated,int32_t(std::get<int64_t>(v)),host,e);
  else if(m.member=="playing")ok=field_canvas_animation_set_playing(n.animated,std::get<bool>(v),host,e);
  else if(m.member=="speed_scale")ok=field_canvas_animation_set_speed(n.animated,float(std::get<double>(v)),host,e);
  else ok=field_canvas_animation_set_animation(n.animated,std::get<std::string>(v),host,e);
  animation_call_active_=false;animation_call_object_=0;
  if(!ok)return false;
  return n.animated.playing?tree_->add_group(m.object,"idle_process_internal",e):
    tree_->remove_group(m.object,"idle_process_internal",e);
}
bool PodunkSceneNative::house_signal_declaration(FieldObjectId id,std::string_view name,
    uint32_t&arity,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  auto it=instances_.find(id);
  if(!house_sources_||it==instances_.end()||!actual(id,d,s,e))return false;
  if((it->second.kind==Kind::Animated&&(name=="frame_changed"||name=="animation_finished"))||
     (it->second.kind==Kind::Sprite&&(name=="frame_changed"||name=="texture_changed"))){
    arity=0;e.clear();return true;
  }
  return fail(e,"House Canvas signal is outside actual native class declarations");
}
bool PodunkSceneNative::animated_snapshot(FieldObjectId id,std::string&animation,uint32_t&frame,
    FieldCanvasAppearance&out,std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  auto it=instances_.find(id);
  if(!house_sources_||it==instances_.end()||it->second.kind!=Kind::Animated||
     !actual(id,d,s,e)||!it->second.source_ready||!it->second.ready||!it->second.entered||
     !s->inside||!s->ready_notified||!s->bound)
    return fail(e,"House AnimatedSprite snapshot before actual source/native Ready");
  const auto&a=it->second.animated;const auto*r=art_->record(d->id);
  if(a.source!=r||!r||a.animation>=r->animations.size()||a.frame>=r->animations[a.animation].frames.size())
    return fail(e,"House AnimatedSprite actual source state/frames rejected");
  animation=r->animations[a.animation].name;frame=a.frame;out=it->second.sprite;
  e.clear();return true;
}
bool PodunkSceneNative::color_rect_snapshot(FieldObjectId id,Vec2&size,FieldColor&color,
    std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;
  auto it=instances_.find(id);
  if(!house_sources_||it==instances_.end()||it->second.kind!=Kind::ColorRect||
     !actual(id,d,s,e)||!it->second.ready||!it->second.entered||!s->inside||!s->bound||
     !s->ready_notified||!d->script.empty())
    return fail(e,"House ColorRect snapshot lacks actual native source owner/Ready");
  size=it->second.sprite.size;color=it->second.color;e.clear();return true;
}
bool PodunkSceneNative::control_state(FieldObjectId id,PodunkSceneControlState&out,
    std::string&e)const{
  const FieldNodeDescriptor*d;const FieldNodeState*s;auto it=instances_.find(id);
  FieldIdentity identity;
  if(!house_sources_||it==instances_.end()||
     (it->second.kind!=Kind::TextureRect&&it->second.kind!=Kind::ColorRect)||
     !actual(id,d,s,e)||!tree_->object_identity(id,identity)||
     art_!=&house_sources_->canvas()||!art_->record(d->id))
    return fail(e,"House native Control state lacks same actual Canvas instance");
  PodunkSceneControlState value;value.tree=tree_;value.registry=registry_;
  value.object=id;value.source_id=d->id;value.identity=identity;value.native_class=d->native_class;
  value.position=s->local[2];value.size=it->second.sprite.size;value.constructed=true;
  value.bound=it->second.bound;value.entered=it->second.entered;value.ready=it->second.ready;
  out=std::move(value);e.clear();return true;
}
bool PodunkSceneNative::sprite_publish(FieldObjectId id,
                                       const FieldCanvasAppearance &value,
                                       std::string &e) {
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  auto i = instances_.find(id);
  if (i == instances_.end() || i->second.kind != Kind::Sprite ||
      !actual(id, d, s, e) || !art_->record(d->id))
    return fail(e, "Scene native Sprite publication has no same live body");
  if (value.action != FieldCanvasAction::Default ||
      (value.texture && !art_->texture(value.texture)) ||
      !value.hframes || !value.vframes || value.hframes > 1024 ||
      value.vframes > 1024 || value.frame >= value.hframes * value.vframes ||
      !std::isfinite(value.offset.x) || !std::isfinite(value.offset.y) ||
      !std::isfinite(value.size.x) || !std::isfinite(value.size.y) ||
      value.size.x < 0 || value.size.y < 0)
    return fail(e, "Scene native Sprite property pose/actual GPU asset rejected");
  // This is the completed native property pose supplied by the source owner.
  // Rendering action, visibility and setter invocation history are separate.
  i->second.sprite = value;
  e.clear();
  return true;
}
bool PodunkSceneNative::bind_sprite_signals(FieldObjectSignals &signals,
                                           std::string &e) {
  if (!data_ || sprite_signals_ || signals.registry() != registry_ ||
      registry_->poisoned())
    return fail(e, "Native Sprite signals require same actual ObjectDB bus");
  sprite_signals_ = &signals;
  e.clear();
  return true;
}
bool PodunkSceneNative::sprite_set_frame(FieldObjectId id, uint32_t frame,
                                        std::string &e) {
  FieldCanvasAppearance state;
  if (!sprite_snapshot(id, state, e)||instances_.at(id).kind!=Kind::Sprite)
    return false;
  if (!sprite_signals_ || sprite_signals_->registry() != registry_ ||
      frame >= state.hframes * state.vframes)
    return fail(e, "Native Sprite frame setter/same signal bus rejected");
  instances_.at(id).sprite.frame = frame;
  // Godot 3.6.2 Sprite::set_frame emits even if the numeric frame is unchanged.
  return sprite_signals_->emit(id, "frame_changed", {}, e);
}
bool PodunkSceneNative::sprite_set_texture(FieldObjectId id, uint32_t texture,
                                          std::string &e) {
  FieldCanvasAppearance state;
  if (!sprite_snapshot(id, state, e)||instances_.at(id).kind!=Kind::Sprite)
    return false;
  if (state.texture == texture)
    return true;
  if (!sprite_signals_ || sprite_signals_->registry() != registry_ ||
      (texture && !art_->texture(texture)))
    return fail(e, "Native Sprite texture setter/actual GPU asset rejected");
  instances_.at(id).sprite.texture = texture;
  return sprite_signals_->emit(id, "texture_changed", {}, e);
}
bool PodunkSceneNative::set_disabled(FieldObjectId id, bool value,
                                     std::string &e) {
  auto it = instances_.find(id);
  if (it == instances_.end() || it->second.kind != Kind::Shape)
    return fail(e, "Scene native disabled target is not actual shape");
  const FieldNodeDescriptor *descriptor;
  const FieldNodeState *state;
  if (!actual(id,descriptor,state,e)) return false;
  if (!geometry_bound_) {
    // NPC _ready mutates its already constructed child's native property.
    // The dormant physics space is synchronized from this exact property at
    // activation; this does not admit an ancestor or run collision queries.
    it->second.disabled=value; e.clear(); return true;
  }
  FieldGeometryNodeUpdate u;
  u.stable_id = it->second.source;
  u.fields = 4;
  u.disabled = value || !it->second.entered;
  if (!geometry_->apply_updates({u}, e))
    return false;
  it->second.disabled = value;
  it->second.space_disabled = u.disabled;
  return true;
}
bool PodunkSceneNative::set_collision(FieldObjectId id, uint32_t layer,
                                      uint32_t mask, std::string &e) {
  auto it = instances_.find(id);
  if (it == instances_.end() || it->second.owner == UINT32_MAX)
    return fail(e, "Scene native masks target is not actual body/Area");
  if (!geometry_bound_)
    return fail(e, "Scene native masks setter before source ancestors Ready");
  FieldGeometryNodeUpdate u;
  u.stable_id = it->second.source;
  u.fields = 8;
  u.layer = layer;
  u.mask = mask;
  return geometry_->apply_updates({u}, e);
}
bool PodunkSceneNative::activate_monitors(FieldSceneHost &scene,
                                          PodunkPlayerPhysicsWorld &world,
                                          std::string &e) {
  if(house_sources_)return fail(e,"House monitors belong to the actual House geometry owner");
  if (!finished_ || !scene.scene_ready() || world.registry() != registry_ ||
      world.tree() != tree_)
    return fail(
        e, "Scene native monitors require full actual source Ready/same world");
  world_ = &world;
  geometry_bound_ = true;
  monitors_active_ = true;
  std::vector<FieldGeometryNodeUpdate> updates;
  for (auto &row : instances_) {
    auto &n = row.second;
    if (n.geometry_node == UINT32_MAX)
      continue;
    const FieldNodeDescriptor *d;
    const FieldNodeState *s;
    if (!actual(row.first, d, s, e))
      return false;
    FieldGeometryNodeUpdate u;
    u.stable_id = n.source;
    u.fields = 1;
    u.local = {s->local[0], s->local[1], s->local[2]};
    if (n.kind == Kind::Shape) {
      u.fields |= 4;
      u.disabled = n.disabled || !n.entered;
    }
    updates.push_back(u);
  }
  if (!geometry_->apply_updates(updates, e)) {
    monitors_active_ = false;
    return false;
  }
  for (auto &row : instances_) {
    auto &n = row.second;
    if (n.geometry_node == UINT32_MAX)
      continue;
    n.space_local = tree_->state(row.first)->local;
    n.space_disabled = n.disabled || !n.entered;
    n.synchronized = true;
  }
  for (auto &row : instances_)
    if (!synchronize(row.first, row.second, e)) {
      monitors_active_ = false;
      return false;
    }
  for (auto &row : instances_) {
    auto &n = row.second;
    if (n.kind == Kind::Area && n.entered && n.ready && !n.monitored) {
      if (!world.admit_static_monitor(row.first, n.owner, e))
        return false;
      n.monitored = true;
    }
  }
  monitors_active_ = true;
  return physics_admitted(e);
}
bool PodunkSceneNative::physics_admitted(std::string &e) const {
  if(house_sources_)return house_live(e)&&house_geometry_->physics_admitted(e);
  if (!finished_ || !monitors_active_ || !root_->viewport().world_registered)
    return fail(e, "Scene native physical activation incomplete");
  for (uint32_t i = 0; i < map_->source()->map_count(); ++i) {
    const auto stable = map_->source()->map(i).stable_id;
    auto q = source_objects_.find(stable);
    if (q == source_objects_.end())
      continue; // Committed source deletion only.
    const auto &n = instances_.at(q->second);
    if (!n.entered || !n.ready)
      return fail(e, "Scene native TileMap has not actually entered/Ready");
  }
  for (const auto &q : instances_) {
    const auto &n = q.second;
    if ((n.kind == Kind::Shape || n.kind == Kind::Body ||
         n.kind == Kind::Area) &&
        (!n.entered || !n.ready || (n.kind == Kind::Area && !n.monitored)))
      return fail(e, "Scene native physical source owner incomplete");
  }
  e.clear();
  return true;
}
bool PodunkSceneNative::begin_draw(uint64_t epoch, float delta,
                                   float shader_time, std::string &e) {
  if (!finished_ || !epoch || epoch <= draw_epoch_ || !std::isfinite(delta) ||
      delta < 0 || !std::isfinite(shader_time) || shader_time < 0 ||
      !root_->viewport().active)
    return fail(e, "Scene native actual draw epoch rejected");
  if(house_sources_&&!house_live(e))return false;
  auto next = animations_;
  for (auto &v : next)
    if (!draw_map_source()->advance_animation(v.second, delta, e))
      return false;
  animations_ = std::move(next);
  draw_epoch_ = epoch;
  draw_started_ = true;
  art_gpu_->begin_frame();
  if (materials_ && !materials_->begin_frame(epoch, shader_time, e)) return false;
  if (animated_leaves_ && !animated_leaves_->begin_draw(epoch, e))
    return false;
  e.clear();
  return true;
}
bool PodunkSceneNative::draw(const FieldMapGateQuery &gates, std::string &e) {
  if (!draw_started_)
    return fail(e, "Scene native draw has no actual GPU frame boundary");
  FieldMapRect rect;
  if (!root_->world_rect(rect, e))
    return false;
  std::vector<FieldCanvasDraw> art;
  if (!canvas_.collect(art, e))
    return false;
  struct Command {
    int64_t z;
    uint64_t native_order;
    uint32_t tile_order;
    bool tile;
    uint32_t index;
    FieldColor color;
    bool foreign = false;
    bool animated = false;
    bool leaf = false;
  };
  std::vector<Command> commands;
  std::map<FieldObjectId, FieldCanvasOrderSlot> slots;
  std::map<uint32_t,FieldCanvasOrderSlot>native_tiles;
  std::vector<FieldCanvasOrderSlot> foreign_slots;
  std::vector<FieldObjectId> animated_slots;
  std::vector<std::pair<PodunkSceneCanvasLeaf *,FieldCanvasOrderSlot>> leaf_slots;
  std::set<FieldObjectId> leaf_objects, material_leaf_objects;
  for (const auto &s : canvas_.canvas_order()) {
    if(s.native_tile){
      const auto*n=tree_->descriptor(s.object);
      if(!house_sources_||!n||n->native_class!="TileMap"||s.foreign||
         s.source!=n->id||s.tile_draw>=house_map_->draw_count()||
         !owns(s.object)||!native_tiles.emplace(s.tile_draw,s).second)
        return fail(e,"Scene native sorted tile slot has no actual House TileMap owner");
      const auto draw=house_map_->draw(s.tile_draw);const auto cell=house_map_->cell(draw.cell);
      if(house_map_->map(draw.map).stable_id!=n->id||cell.quadrant_ordinal<0||
         s.tile_quadrant!=uint32_t(cell.quadrant_ordinal))
        return fail(e,"Scene native sorted tile slot differs from actual source quadrant");
      continue;
    }
    slots.emplace(s.object, s);
    const auto *n = tree_->descriptor(s.object);
    PodunkSceneCanvasLeaf *leaf=nullptr;
    for (auto *owner:canvas_leaves_) if (owner->owns_drawable(s.object)) {
      if (leaf) return fail(e,"Canvas leaf has ambiguous actual source owners");
      leaf=owner;
    }
    if (leaf) {
      if (!n || s.foreign || (n->native_class!="Label" && n->native_class!="TextureRect"))
        return fail(e,"Canvas leaf source/class is outside native Control capability");
      const auto *record = art_->record(n->id);
      if (record && record->shader != FieldCanvasShader::Default) {
        if (n->native_class != "TextureRect" || !materials_)
          return fail(e, "Canvas material leaf has no actual typed renderer");
        material_leaf_objects.insert(s.object);
        continue; // Its collected material draw occupies this same native slot.
      }
      commands.push_back({s.z,s.native_order,0,false,uint32_t(leaf_slots.size()),{},false,false,true});
      leaf_slots.emplace_back(leaf,s); leaf_objects.insert(s.object);
      continue;
    }
    if (!house_sources_ && !s.foreign && n && n->native_class == "AnimatedSprite") {
      if (!animated_leaves_ || !animated_leaves_->owns(s.object) ||
          !animated_leaves_->drawable(s.object))
        return fail(
            e, "Scene native AnimatedSprite has no actual source leaf owner");
      commands.push_back({s.z,
                          s.native_order,
                          0,
                          false,
                          uint32_t(animated_slots.size()),
                          {},
                          false,
                          true});
      animated_slots.push_back(s.object);
    }
    if (s.foreign && s.foreign_drawable) {
      commands.push_back({s.z,
                          s.native_order,
                          0,
                          false,
                          uint32_t(foreign_slots.size()),
                          {},
                          true});
      foreign_slots.push_back(s);
    }
  }
  for (uint32_t i = 0; i < art.size(); ++i) {
    if (leaf_objects.count(art[i].object)) continue;
    if(art[i].primitive==2)
      return fail(e,"House visible native Label has no actual same-tree GPU leaf owner");
    const auto s = slots.find(art[i].object);
    if (s == slots.end() || (!owns(art[i].object) &&
        !material_leaf_objects.count(art[i].object)))
      return fail(e, "Scene native Sprite command has no actual native owner");
    commands.push_back({art[i].z, s->second.native_order, 0, false, i, {}});
  }
  std::vector<uint32_t> tiles;
  std::set<FieldObjectId> synchronized_maps;
  if(house_sources_){if(!collect_house_tiles(rect,tiles,e))return false;}
  else if (!map_->collect_draws(rect, gates, map_->source()->draw_count(), tiles, e))return false;
  for (auto index : tiles) {
    FieldMapDraw pose;if(!tile_pose(index,pose,e))return false;
    const auto layer = house_sources_?house_map_->map(pose.map):map_->map(pose.map);
    FieldObjectId id;
    if (!source_object(layer.stable_id, id, e))
      return false;
    auto n = instances_.find(id);
    auto slot = slots.find(id);
    if (n == instances_.end() || !n->second.entered || !n->second.ready)
      return fail(e, "Scene native TileMap draw before native Ready");
    if (slot == slots.end())
      continue; // Actual canvas ancestor visibility.
    if (synchronized_maps.insert(id).second && !synchronize(id, n->second, e))
      return false;
    if(house_sources_&&(layer.flags&1u)){
      const auto native=native_tiles.find(index);
      if(native==native_tiles.end()||native->second.object!=id)
        return fail(e,"House ysorted tile draw lacks its actual native quadrant slot");
      commands.push_back({native->second.z,native->second.native_order,pose.order,true,index,
                          native->second.color});
    }else commands.push_back({int64_t(slot->second.z) + pose.z,
                             slot->second.native_order, pose.order, true, index,
                             slot->second.color});
  }
  std::stable_sort(commands.begin(), commands.end(),
                   [](const Command &a, const Command &b) {
                     return std::tie(a.z, a.native_order, a.tile_order) <
                            std::tie(b.z, b.native_order, b.tile_order);
                   });
  const auto &viewport = root_->viewport();
  if (!translation(viewport.canvas))
    return fail(e,
                "Scene native compositor needs actual translated 1:1 viewport");
  const Vec2 camera{-viewport.canvas[2].x, -viewport.canvas[2].y};
  FieldCanvasArtRenderer::Delegate delegate;
  if (materials_)
    delegate = [this](const FieldCanvasDraw &a, Vec2 c, float w, float h,
                      std::string &error) {
      return materials_->draw(a, c, w, h, error);
    };
  for (const auto &c : commands) {
    if (c.leaf) {
      auto &leaf=leaf_slots.at(c.index);
      if (!leaf.first->draw_leaf(leaf.second,viewport.canvas,art_->pixel_snap(),e)) return false;
    } else if (c.animated) {
      if (!animated_leaves_ ||
          !animated_leaves_->draw(animated_slots.at(c.index), camera, e))
        return false;
    } else if (c.foreign) {
      if (!foreign_ || !foreign_->draw(foreign_slots.at(c.index),
                                       viewport.canvas, art_->pixel_snap(), e))
        return false;
    } else if (c.tile) {
      FieldMapDraw pose;if(!tile_pose(c.index,pose,e))return false;
      auto texture = pose.texture;
      const auto tex = draw_map_source()->texture(texture);
      if (tex.frames > 1)
        texture = tex.group + animations_.at(tex.group).frame;
      for (size_t i = 0; i < 4; ++i)
        pose.color[i] *= c.color[i];
      if (!map_gpu_->draw(pose, texture, bool(pose.flags & 4), camera.x,
                          camera.y))
        return fail(e, "Scene native actual GPU tile draw rejected");
    } else {
      auto a = art[c.index];
      if (c.z < std::numeric_limits<int32_t>::min() ||
          c.z > std::numeric_limits<int32_t>::max())
        return fail(e, "Scene native canvas z overflow");
      a.z = int32_t(c.z);
      if (!art_gpu_->draw({a}, camera, viewport.size.x, viewport.size.y,
                          delegate, e))
        return false;
    }
  }
  draw_started_ = false;
  e.clear();
  return true;
}
bool PodunkSceneNative::bind_external_material(FieldObjectId id,
    const FieldNodeDescriptor &d, std::string &e) {
  const FieldNodeDescriptor *actual_d;
  const FieldNodeState *state;
  if (!actual(id, actual_d, state, e) || actual_d->id != d.id)
    return fail(e, "Canvas material external native identity differs");
  const auto *record = art_->record(d.id);
  if (record && !material(id, *record, e)) return false;
  e.clear(); return true;
}
bool PodunkSceneNative::canvas_appearance(const FieldCanvasRecord &record,
    FieldObjectId id, FieldCanvasAppearance &out, std::string &e) const {
  const FieldNodeDescriptor *d;
  const FieldNodeState *state;
  if (!actual(id, d, state, e) || d->id != record.id ||
      art_->record(d->id) != &record)
    return fail(e, "Canvas appearance must use its actual source record");
  PodunkSceneCanvasLeaf *leaf = nullptr;
  for (auto *owner : canvas_leaves_) if (owner->owns_drawable(id)) {
    if (leaf) return fail(e, "Canvas appearance has ambiguous native leaf owners");
    leaf = owner;
  }
  if (leaf) {
    if (!leaf->appearance(record, id, out, e)) return false;
  } else if (!sprite_snapshot(id, out, e)) return false;
  if (record.shader != FieldCanvasShader::Default) {
    if (!material(id, record, e)) return false;
    out.action = FieldCanvasAction::Delegate;
  }
  e.clear(); return true;
}
bool PodunkSceneNative::draw_default(const FieldCanvasDraw &draw, Vec2 camera,
    float width, float height, std::string &e) {
  if (!draw_started_ || !art_gpu_ || draw.shader != FieldCanvasShader::Default ||
      draw.action != FieldCanvasAction::Default)
    return fail(e, "Default GPU draw requires actual default source command");
  return art_gpu_->draw({draw}, camera, width, height, {}, e);
}
bool PodunkSceneNative::source_object(uint32_t id, FieldObjectId &out,
                                      std::string &e) const {
  auto q = source_objects_.find(id);
  const FieldNodeDescriptor *d;
  const FieldNodeState *s;
  if (q == source_objects_.end() || !actual(q->second, d, s, e))
    return fail(e, "Scene native actual source object unavailable");
  out = q->second;
  e.clear();
  return true;
}
bool PodunkSceneNative::shutdown(std::string &e) {
  for (const auto &q : instances_)
    if (q.second.entered || q.second.monitored)
      return fail(e,
                  "Scene native shutdown requires actual Exit and GPU fence");
  if (map_gpu_)
    map_gpu_->free();
  art_gpu_.reset();
  map_gpu_.reset();
  canvas_.clear();
  foreign_.reset();
  grass_ = nullptr;
  canvas_leaves_.clear();
  instances_.clear();
  source_objects_.clear();
  animations_.clear();
  art_host_ = {};
  data_ = nullptr;
  tree_ = nullptr;
  registry_ = nullptr;
  root_ = nullptr;
  map_ = nullptr;
  geometry_ = nullptr;
  art_ = nullptr;
  house_sources_=nullptr;house_map_=nullptr;house_geometry_=nullptr;
  house_random_=nullptr;house_controls_=nullptr;house_old_tree_=nullptr;
  house_old_root_=0;house_root_=0;animation_call_object_=0;
  animation_call_active_=false;house_replaced_=false;house_deleted_.clear();
  materials_ = nullptr;
  sprite_signals_ = nullptr;
  animated_leaves_ = nullptr;
  world_ = nullptr;
  finished_ = false;
  draw_started_ = false;
  monitors_active_ = false;
  geometry_bound_ = false;
  draw_epoch_ = 0;
  e.clear();
  return true;
}
} // namespace encore::ctr
