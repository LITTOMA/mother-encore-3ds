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
} // namespace

const FieldNodeDescriptor *HouseReturnSources::tilemap_node(uint32_t id) const {
  if (valid_)
    for (const auto &certificate : reentry_.tilemaps())
      if (certificate.id == id) return tree_.record(id);
  return nullptr;
}
bool HouseReturnSources::load(const PodunkBundleData &bundle,
                              const std::string &romfs_root,
                              const FieldDoorData &doors, RoomView room,
                              HouseView house, std::string &error) {
  if (!bundle.valid() || bundle.packs().size() != uint32_t(PodunkPackRole::HouseSprites) ||
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
  if (!reentry || !geometry || !tree || !npcs || !npc_world || !timers || !visibility || !sprites ||
      reentry->identity.upstream_commit != bundle.identity().upstream_commit ||
      !target(reentry->identity, geometry->identity) ||
      !target(reentry->identity, tree->identity) ||
      !same(npcs->identity, tree->identity) || !same(npc_world->identity, tree->identity) ||
      !same(timers->identity, tree->identity) || !same(visibility->identity, tree->identity) ||
      !same(sprites->identity, tree->identity))
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
      !field_sprite_npc_binding(candidate.sprites_, candidate.npcs_, error))
    return false;
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
