#pragma once
#include "encore/house_reentry.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/podunk_bundle.hpp"

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
  // The complete tree owns the TileMaps' authoritative local/world matrices.
  // Only IDs already bound to a loaded reentry certificate can resolve here.
  const FieldNodeDescriptor *tilemap_node(uint32_t source_id) const;

private:
  bool valid_ = false;
  HouseReentryData reentry_;
  FieldGeometryView geometry_;
  FieldNodeTreeData tree_;
};
} // namespace encore::upstream
