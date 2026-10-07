#include "encore/house_return_sources.hpp"
#include <algorithm>
#include <map>
#include <utility>

namespace encore::upstream {
namespace {
bool fail(std::string &error, const char *why) {
  error = why;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool same(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
bool same(const FieldGeometryTransform &a, const FieldTransform &b) {
  return same(a.x, b[0]) && same(a.y, b[1]) && same(a.origin, b[2]);
}
bool target(const FieldIdentity &a, const FieldIdentity &b) {
  // Each resource keeps its own schema identity; all refer to one source scene.
  return a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
std::string_view native_class(const FieldNodeTreeData &tree,
                              const FieldNodeDescriptor &node) {
  return node.class_index < tree.classes().size()
             ? std::string_view(tree.classes()[node.class_index]) : std::string_view{};
}
bool read(const PodunkBundleData &bundle, PodunkPackRole role,
          const std::string &root, std::vector<uint8_t> &bytes, std::string &error) {
  const auto *entry = bundle.entry(role);
  if (!entry || !bundle.read(role, root, bytes, error)) return false;
  if (entry->kind == 1 && (bytes.size() < 124 ||
      !std::equal(entry->ir_sha256.begin(), entry->ir_sha256.end(), bytes.begin() + 92)))
    return fail(error, "House typed resource authoring fingerprint differs from bundle");
  return true;
}
bool cross_bind(const HouseReentryData &reentry,
                const FieldGeometryView &geometry,
                const FieldNodeTreeData &tree, std::string &error) {
  if (!target(reentry.identity(), geometry.identity()) ||
      !target(reentry.identity(), tree.identity()) ||
      geometry.source_scene() != reentry.target_scene() ||
      tree.source_scene() != reentry.target_scene())
    return fail(error, "House destination resource source identities differ");
  std::array<uint8_t, 32> proof{};
  if (!tree.source_hash(reentry.target_scene(), proof) ||
      proof != reentry.identity().source_sha256)
    return fail(error, "House complete tree target source proof differs");
  for (const auto &native : reentry.native_nodes()) {
    const auto *node = tree.record(native.id);
    if (!node || node->path != native.node || node->name != native.name ||
        native_class(tree, *node) != native.native_class ||
        node->parent != native.parent || node->script != native.script ||
        node->script_sha != native.script_sha || !same(native.local, node->local))
      return fail(error, "House reentry native descriptor differs from complete tree");
  }
  if (reentry.native_nodes().empty() ||
      reentry.native_nodes().front().id != tree.identity().scene_id)
    return fail(error, "House reentry root differs from complete tree identity");
  for (const auto &certificate : reentry.tilemaps()) {
    const auto *node = tree.record(certificate.id);
    if (!node || node->path != certificate.node || native_class(tree, *node) != "TileMap" ||
        !(node->flags & 1u))
      return fail(error, "House TileMap certificate differs from complete tree");
    if (!tree.source_hash(certificate.resource_source, proof) ||
        proof != certificate.resource_sha ||
        !reentry.source_hash(certificate.resource_source, proof) ||
        proof != certificate.resource_sha)
      return fail(error, "House TileSet source proof differs from complete tree");
  }
  for (uint32_t i = 0; i < geometry.node_count(); ++i) {
    const auto source = geometry.node(i);
    const auto *node = tree.record(source.stable_id);
    const uint32_t parent = source.parent == UINT32_MAX
                                ? 0 : geometry.node(source.parent).stable_id;
    if (!node || node->path != geometry.string(source.path) ||
        native_class(tree, *node) != geometry.string(source.class_name) ||
        node->parent != parent || node->script != geometry.string(source.script) ||
        node->script_sha != source.script_sha256 ||
        !same(source.local, node->local) || !same(source.world, node->world))
      return fail(error, "House geometry source node differs from complete tree");
  }
  return true;
}
bool cross_bind_map(const HouseReentryData &reentry, const FieldMapView &map,
                    const FieldNodeTreeData &tree, std::string &error) {
  if (!same(map.identity(), tree.identity()) ||
      map.source_scene() != tree.source_scene() ||
      map.map_count() != reentry.tilemaps().size())
    return fail(error, "House map source/tree coverage differs");
  for (uint32_t i = 0; i < map.map_count(); ++i) {
    const auto layer = map.map(i);
    const auto *node = tree.record(layer.stable_id);
    const auto certificate = std::find_if(reentry.tilemaps().begin(),
        reentry.tilemaps().end(), [&](const auto &c) { return c.id == layer.stable_id; });
    if (!node || certificate == reentry.tilemaps().end() ||
        native_class(tree, *node) != "TileMap" ||
        node->path != map.string(layer.node) || node->path != certificate->node ||
        layer.cell_count != certificate->cell_count ||
        layer.layer != certificate->layer || layer.mask != certificate->mask ||
        !same(layer.position, node->world[2]) ||
        !same(node->world[0], {1, 0}) || !same(node->world[1], {0, 1}) ||
        layer.polygon_count)
      return fail(error, "House map TileMap source/collision certificate differs");
  }
  for (uint32_t i = 0; i < map.canvas_count(); ++i) {
    const auto canvas = map.canvas(i);
    const auto *node = tree.record(canvas.stable_id);
    if (!node || node->path != map.string(canvas.node) ||
        !(node->flags & 1u) || !same(canvas.position, node->world[2]))
      return fail(error, "House map Canvas source transform differs");
  }
  for (uint32_t i = 0; i < map.texture_count(); ++i) {
    const auto texture = map.texture(i);
    std::array<uint8_t, 32> proof{};
    if (!tree.source_hash(map.string(texture.source), proof) ||
        proof != texture.source_sha256)
      return fail(error, "House map texture original source differs");
  }
  return true;
}
bool cross_bind_tint(const FieldTintData &tint, const FieldNodeTreeData &tree,
                     std::string &error) {
  if (tint.scene_id() != tree.identity().scene_id ||
      tint.source_pin() != tree.identity().upstream_commit)
    return fail(error, "House Tint source identity differs from complete tree");
  std::array<uint8_t, 32> proof{};
  if (!tint.source_hash(tree.source_scene(), proof) ||
      proof != tree.identity().source_sha256)
    return fail(error, "House Tint original scene proof differs");
  std::map<std::string, std::array<uint8_t, 32>> scripts;
  for (const auto &record : tint.records()) {
    const auto *node = tree.record(record.id);
    if (record.kind != FieldTintKind::Scene ||
        record.scene_id != tree.identity().scene_id ||
        record.scene != tree.source_scene() || !node ||
        node->path != record.node || native_class(tree, *node) != "Node" ||
        node->script.empty() || node->ready != record.ready_ordinal ||
        !tint.source_hash(node->script, proof) || proof != node->script_sha)
      return fail(error, "House Tint source script/Ready attachment differs");
    scripts.emplace(node->script, node->script_sha);
    for (const auto &target : record.targets) {
      const auto *native = tree.record(target.source_id);
      if (target.exists != bool(native))
        return fail(error, "House Tint nullable native target differs");
      if (native && (native->path != target.node ||
          native_class(tree, *native) != target.kind ||
          !(native->flags & 1u) || native->self_modulate != target.initial_self_modulate))
        return fail(error, "House Tint actual Canvas target/color differs");
    }
  }
  size_t count = 0;
  for (const auto &node : tree.records()) {
    const auto script = scripts.find(node.script);
    if (script == scripts.end()) continue;
    if (script->second != node.script_sha || !tint.record(node.id))
      return fail(error, "House Tint source instance coverage differs");
    ++count;
  }
  if (scripts.empty() || count != tint.records().size())
    return fail(error, "House Tint lacks its complete source instance closure");
  return true;
}
bool cross_bind_canvas(const FieldCanvasArtData &canvas,
                       const FieldNodeTreeData &tree, std::string &error) {
  const char *classes[] = {"Sprite", "TextureRect", "AnimatedSprite", "ColorRect"};
  for (const auto &record : canvas.records()) {
    const auto *node = tree.record(record.id);
    if (record.kind >= sizeof(classes) / sizeof(*classes) || !node ||
        node->path != record.node || node->flags != record.flags ||
        native_class(tree, *node) != classes[record.kind])
      return fail(error, "House Canvas source drawable attachment differs");
    if (record.owner != FieldCanvasOwner::Native) {
      const auto *owner = tree.record(record.owner_id);
      if (!owner || owner->script != record.owner_script ||
          owner->script_sha != record.owner_sha)
        return fail(error, "House Canvas source appearance owner differs");
    }
  }
  size_t count = 0;
  for (const auto &node : tree.records()) {
    const auto name = native_class(tree, node);
    if (std::find(std::begin(classes), std::end(classes), name) != std::end(classes)) {
      if (!canvas.record(node.id))
        return fail(error, "House Canvas native drawable source coverage differs");
      ++count;
    }
    if (!node.script.empty()) {
      std::array<uint8_t, 32> proof{}, original{};
      const auto file = node.script.substr(0, node.script.find("::"));
      if (!tree.source_hash(file, original) ||
          !canvas.source_hash(file, proof) || proof != original)
        return fail(error, "House Canvas script source closure differs");
    }
  }
  if (count != canvas.records().size())
    return fail(error, "House Canvas contains an unowned native drawable");
  for (const auto &control : canvas.control_boundaries()) {
    const auto *node = tree.record(control.id);
    const auto *owner = tree.record(control.owner_id);
    if (!node || node->path != control.node || node->flags != control.flags ||
        native_class(tree, *node) != control.native_class || !owner ||
        owner->script != control.owner_script || owner->script_sha != control.owner_sha)
      return fail(error, "House Canvas actual native Control boundary differs");
  }
  return true;
}
} // namespace

bool HouseReturnSources::admit_interact(const FieldInteractData&d,
    const FieldNodeTreeData&t,std::string&e){
 std::array<uint8_t,32>sha;
 if(!t.valid()||!d.valid()||d.scene()!=t.source_scene()||
    d.scene_id()!=t.identity().scene_id||d.source_pin()!=t.identity().upstream_commit||
    !d.source_hash(d.scene(),sha)||sha!=t.identity().source_sha256)
  return fail(e,"House Interact source identity differs from complete tree");
 size_t count=0;
 for(const auto&n:t.records())if(n.script==d.script()){
  ++count;const auto*r=d.record(n.id);const auto*p=r?t.record(r->prompt):nullptr;
  if(!r||r->node!=n.path||r->ready!=n.ready||n.native_class!="Area2D"||
     n.script_methods!=1||!(n.flags&1)||bool(n.flags&2)!=bool(r->flags&8)||
     !d.source_hash(n.script,sha)||sha!=n.script_sha||!p||p->parent!=n.id||
     p->path!=n.path+"/ButtonPrompt"||p->native_class!="Node2D"||p->ready>=n.ready||
     !d.source_hash(p->script,sha)||sha!=p->script_sha)
   return fail(e,"House Interact actual Area/Ready/Prompt source attachment differs");
 }
 if(!count||count!=d.records().size())return fail(e,"House Interact full source instance coverage differs");
 for(const auto&n:t.records())if(!n.script.empty()){
  const auto path=n.script.substr(0,n.script.find("::"));std::array<uint8_t,32>original;
  if(!t.source_hash(path,original)||!d.source_hash(path,sha)||original!=sha)
   return fail(e,"House Interact original script source closure differs");
 }
 e.clear();return true;
}
const FieldNodeDescriptor *HouseReturnSources::tilemap_node(uint32_t id) const {
  if (valid_)
    for (const auto &certificate : reentry_.tilemaps())
      if (certificate.id == id) return tree_.record(id);
  return nullptr;
}
bool HouseReturnSources::load(const PodunkBundleData &bundle,
                              const std::string &romfs_root,
                              const FieldDoorData &doors, RoomView room,
                              HouseView house, DrawerProgramView drawer, std::string &error) {
  if (!bundle.valid() || bundle.packs().size() != uint32_t(PodunkPackRole::HouseInspectionReentry) ||
      !doors.valid() || !same(bundle.identity(), doors.identity()) ||
      bundle.source_scene() != doors.source_scene())
    return fail(error, "House return requires the actual complete outdoor bundle and Door");
  const auto *reentry = bundle.entry(PodunkPackRole::HouseReentry);
  const auto *geometry = bundle.entry(PodunkPackRole::HouseGeometry);
  const auto *tree = bundle.entry(PodunkPackRole::HouseNodeTree);
  const auto *npcs = bundle.entry(PodunkPackRole::HouseNpc);
  const auto *npc_world = bundle.entry(PodunkPackRole::HouseNpcWorld);
  const auto *timers = bundle.entry(PodunkPackRole::HouseNativeTimers);
  const auto *visibility = bundle.entry(PodunkPackRole::HouseVisibility);
  const auto *sprites = bundle.entry(PodunkPackRole::HouseSprites);
  const auto *map = bundle.entry(PodunkPackRole::HouseMap);
  const auto *canvas = bundle.entry(PodunkPackRole::HouseCanvas);
  const auto *tint = bundle.entry(PodunkPackRole::HouseTint);
  if (!reentry || !geometry || !tree || !npcs || !npc_world || !timers || !visibility || !sprites ||
      !map || !canvas || !tint ||
      reentry->identity.upstream_commit != bundle.identity().upstream_commit ||
      !target(reentry->identity, geometry->identity) ||
      !target(reentry->identity, tree->identity) ||
      !same(npcs->identity, tree->identity) || !same(npc_world->identity, tree->identity) ||
      !same(timers->identity, tree->identity) || !same(visibility->identity, tree->identity) ||
      !same(sprites->identity, tree->identity) || !same(map->identity, tree->identity) ||
      !same(canvas->identity, tree->identity) || !same(tint->identity, tree->identity))
    return fail(error, "House return typed bundle source bindings differ");
  HouseReturnSources candidate;
  std::vector<uint8_t> bytes;
  if (!read(bundle, PodunkPackRole::HouseReentry, romfs_root, bytes, error) ||
      !candidate.reentry_.load(bytes.data(), bytes.size(), doors, room, house, error))
    return false;
  if (!same(candidate.reentry_.identity(), reentry->identity))
    return fail(error, "House reentry bundle identity differs");
  if (!read(bundle, PodunkPackRole::HouseGeometry, romfs_root, bytes, error) ||
      !candidate.geometry_.load(bytes.data(), bytes.size(), geometry->identity, error) ||
      !read(bundle, PodunkPackRole::HouseNodeTree, romfs_root, bytes, error) ||
      !candidate.tree_.load(bytes.data(), bytes.size(), tree->identity, error) ||
      !cross_bind(candidate.reentry_, candidate.geometry_, candidate.tree_, error) ||
      !read(bundle, PodunkPackRole::HouseMap, romfs_root, bytes, error) ||
      !candidate.map_.load(bytes.data(), bytes.size(), map->identity, error) ||
      !cross_bind_map(candidate.reentry_, candidate.map_, candidate.tree_, error) ||
      !read(bundle, PodunkPackRole::HouseCanvas, romfs_root, bytes, error) ||
      !candidate.canvas_.load(bytes.data(), bytes.size(), canvas->identity, error) ||
      !read(bundle, PodunkPackRole::HouseNpc, romfs_root, bytes, error) ||
      !candidate.npcs_.load(bytes.data(), bytes.size(), error) ||
      !read(bundle, PodunkPackRole::HouseNpcWorld, romfs_root, bytes, error) ||
      !candidate.npc_world_.load(bytes.data(), bytes.size(), candidate.tree_,
                                 candidate.npcs_, candidate.geometry_, error) ||
      !read(bundle, PodunkPackRole::HouseNativeTimers, romfs_root, bytes, error) ||
      !candidate.timers_.load(bytes.data(), bytes.size(), error) ||
      !read(bundle, PodunkPackRole::HouseVisibility, romfs_root, bytes, error) ||
      !candidate.visibility_.load(bytes.data(), bytes.size(), candidate.tree_, error) ||
      !read(bundle, PodunkPackRole::HouseSprites, romfs_root, bytes, error) ||
      !candidate.sprites_.load(bytes.data(), bytes.size(), error) ||
      !field_sprite_npc_binding(candidate.sprites_, candidate.npcs_, error) ||
      !read(bundle, PodunkPackRole::HouseTint, romfs_root, bytes, error) ||
      !candidate.tint_.load(bytes.data(), bytes.size(), error) ||
      !cross_bind_tint(candidate.tint_, candidate.tree_, error))
    return false;
  const auto*prompt_entry=bundle.entry(PodunkPackRole::HouseButtonPrompt);
  const auto*control_entry=bundle.entry(PodunkPackRole::HouseControls);
  const auto*interact_entry=bundle.entry(PodunkPackRole::HouseInteract);
  const auto*return_entry=bundle.entry(PodunkPackRole::HouseInspectionReentry);
  if(!prompt_entry||!control_entry||!interact_entry||!return_entry||
      !same(prompt_entry->identity,tree->identity)||!same(control_entry->identity,tree->identity)||
      !same(interact_entry->identity,tree->identity)||!same(return_entry->identity,reentry->identity))
    return fail(error,"House Prompt/Control/Interact/return Room source identity differs");
  if(!read(bundle,PodunkPackRole::HouseButtonPrompt,romfs_root,bytes,error)||
      !candidate.button_prompts_.load(bytes.data(),bytes.size(),candidate.tree_,tree->ir_sha256,error)||
      candidate.button_prompts_.ir_sha256()!=prompt_entry->ir_sha256||
      !read(bundle,PodunkPackRole::HouseControls,romfs_root,bytes,error)||
      !candidate.controls_.load(bytes.data(),bytes.size(),tree->identity,candidate.tree_,candidate.canvas_,error)||
      !read(bundle,PodunkPackRole::HouseInteract,romfs_root,bytes,error)||
      !candidate.interact_.load(bytes.data(),bytes.size(),error)||
      !admit_interact(candidate.interact_,candidate.tree_,error)||
      !read(bundle,PodunkPackRole::HouseInspectionRoom,romfs_root,bytes,error)||
      !candidate.inspections_.load(bytes.data(),bytes.size(),room,house,drawer,candidate.interact_,error))
    return false;
  // Keep the opening Room/certificate unchanged. The complete return Room has
  // its own freshly generated Door certificate and exact file dependencies.
  HouseReentryData return_reentry;
  if(!read(bundle,PodunkPackRole::HouseInspectionReentry,romfs_root,bytes,error)||
      !return_reentry.load(bytes.data(),bytes.size(),doors,candidate.inspections_.view(),house,error)||
      !cross_bind(return_reentry,candidate.geometry_,candidate.tree_,error))return false;
  candidate.reentry_=std::move(return_reentry);
  if (candidate.canvas_.source_scene() != candidate.tree_.source_scene() ||
      candidate.canvas_.tree_ir_sha() != tree->ir_sha256)
    return fail(error, "House Canvas complete tree authoring/source binding differs");
  if (!cross_bind_canvas(candidate.canvas_, candidate.tree_, error)) return false;
  if (candidate.sprites_.scene_id() != candidate.tree_.identity().scene_id ||
      candidate.sprites_.source_pin() != candidate.tree_.identity().upstream_commit)
    return fail(error, "House Sprite source identity differs from complete tree");
  // The loaded resource supplies script attachments. Do not duplicate game
  // paths in executable code to identify Character/Fetcher source receivers.
  struct SpriteScript {
    FieldSpriteKind kind;
    std::array<uint8_t, 32> sha;
  };
  std::map<std::string, SpriteScript> sprite_scripts;
  bool has_character = false, has_fetcher = false;
  for (const auto &record : candidate.sprites_.records()) {
    const auto *node = candidate.tree_.record(record.id);
    const auto expected_class = record.kind == FieldSpriteKind::Character ? "Sprite" : "Node";
    if (!node || node->script.empty() || native_class(candidate.tree_, *node) != expected_class)
      return fail(error, "House Sprite source native/script receiver differs");
    const auto inserted = sprite_scripts.emplace(node->script, SpriteScript{record.kind, node->script_sha});
    if (!inserted.second && (inserted.first->second.kind != record.kind ||
                            inserted.first->second.sha != node->script_sha))
      return fail(error, "House Sprite source script has conflicting typed receivers");
    has_character |= record.kind == FieldSpriteKind::Character;
    has_fetcher |= record.kind == FieldSpriteKind::Fetcher;
  }
  if (!has_character || !has_fetcher)
    return fail(error, "House Sprite resource lacks its typed Character/Fetcher closure");
  size_t sprite_count = 0;
  for (const auto &node : candidate.tree_.records()) {
    const auto script = sprite_scripts.find(node.script);
    if (script == sprite_scripts.end()) continue;
    const auto *record = candidate.sprites_.record(node.id);
    if (!record || node.script_sha != script->second.sha || record->kind != script->second.kind || record->node != node.path ||
        record->parent_id != node.parent || record->ready_ordinal != node.ready)
      return fail(error, "House Sprite source attachment differs from complete tree");
    if (record->kind == FieldSpriteKind::Fetcher) {
      const auto *target_node = candidate.tree_.record(record->target_id);
      if (!target_node || (native_class(candidate.tree_, *target_node) != "Sprite" &&
                          native_class(candidate.tree_, *target_node) != "AnimatedSprite"))
        return fail(error, "House Fetcher actual native Sprite target differs");
    }
    ++sprite_count;
  }
  if (sprite_count != candidate.sprites_.records().size())
    return fail(error, "House Sprite coverage differs from complete tree");
  size_t timer_count = 0;
  for (const auto &node : candidate.tree_.records()) {
    if (native_class(candidate.tree_, node) != "Timer") continue;
    const auto *record = candidate.timers_.record(candidate.tree_.identity(), node.id);
    if (!record || record->script_sha != node.script_sha)
      return fail(error, "House Timer source tree attachment differs");
    ++timer_count;
  }
  if (timer_count != candidate.timers_.records().size())
    return fail(error, "House native Timer coverage differs from complete tree");
  candidate.valid_ = true;
  *this = std::move(candidate);
  error.clear();
  return true;
}
} // namespace encore::upstream
