#pragma once
#include "encore/house_reentry.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/podunk_bundle.hpp"
#include "encore/field_npc_world.hpp"
#include "encore/field_native_timer.hpp"
#include "encore/field_visibility.hpp"
#include "encore/field_sprite_bridge.hpp"

namespace encore::upstream {
// One immutable, cross-bound destination owner. Admission does not allocate
// nodes, execute source lifecycle, alter flags or consume random state.
class HouseReturnSources {
public:
  bool load(const PodunkBundleData &, const std::string &romfs_root,
            const FieldDoorData &, RoomView, HouseView, std::string &);
  bool valid() const { return valid_; }
  const HouseReentryData &reentry() const { return reentry_; }
  const FieldGeometryView &geometry() const { return geometry_; }
  const FieldNodeTreeData &tree() const { return tree_; }
  const FieldNpcData &npcs() const { return npcs_; }
  const FieldNpcWorldData &npc_world() const { return npc_world_; }
  const FieldNativeTimerData &timers() const { return timers_; }
  const FieldVisibilityData &visibility() const { return visibility_; }
  const FieldSpriteData &sprites() const { return sprites_; }
  // The complete tree owns the TileMaps' authoritative local/world matrices.
  // Only IDs already bound to a loaded reentry certificate can resolve here.
  const FieldNodeDescriptor *tilemap_node(uint32_t source_id) const;

private:
  bool valid_ = false;
  HouseReentryData reentry_;
  FieldGeometryView geometry_;
  FieldNodeTreeData tree_;
  FieldNpcData npcs_;
  FieldNpcWorldData npc_world_;
  FieldNativeTimerData timers_;
  FieldVisibilityData visibility_;
  FieldSpriteData sprites_;
};
} // namespace encore::upstream
