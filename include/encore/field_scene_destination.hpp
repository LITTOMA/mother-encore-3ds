#pragma once
#include "encore/field_door.hpp"
#include "encore/podunk_bundle.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/room_data.hpp"
namespace encore::upstream {
// The source House Door, rather than a compiled map name, selects the exact
// destination. Keep this immutable owner alive through destination teardown.
// Loading grants no native/script Ready, changes no session and uses no RNG.
class FieldSceneDestinationData {
public:
  bool load(const ResourceCatalog&, const char* romfs_root, RoomView current,
            std::string&);
  bool valid() const { return loaded_; }
  const FieldDoorData& exit() const { return exit_; }
  const PodunkBundleData& bundle() const { return bundle_; }
  bool destination(uint32_t door, const PodunkBundleData*&, std::string&) const;
private:
  FieldDoorData exit_;
  PodunkBundleData bundle_;
  bool loaded_=false, attempted_=false;
};
}
