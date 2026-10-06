#include "encore/field_scene_destination.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
uint32_t integer(const uint8_t* p) {
  return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
}
bool FieldSceneDestinationData::load(const ResourceCatalog& catalog,
                                    const char* root, RoomView current,
                                    std::string& e) {
  if(attempted_||!catalog.valid()||!current.valid()||current.byte_size()<76) {
    e="Destination sources require fresh ownership and an actual current Room";
    return false;
  }
  attempted_=true;
  if(!std::equal(catalog.source_pin().begin(),catalog.source_pin().end(),current.bytes()+56)) {
    e="Destination catalog and actual current Room pin differ";return false;
  }
  std::vector<uint8_t> bytes;
  if(!catalog.read_file(ResourceRole::HouseSceneDoor,root,bytes,e))return false;
  if(bytes.size()<128) { e="Catalog-owned House Door header truncated";return false; }
  // These source identities are metadata of the already checked catalog
  // binding. The Door loader still validates schema, CRC and source closure.
  FieldIdentity expected;
  expected.scene_id=integer(bytes.data()+36);
  expected.upstream_commit=catalog.source_pin();
  std::copy_n(bytes.data()+60,32,expected.source_sha256.begin());
  if(!exit_.load(bytes.data(),bytes.size(),expected,e))return false;
  auto scene=current.string(current.scene().source_scene_string);
  if(scene.substr(0,6)=="res://")scene.remove_prefix(6);
  if(exit_.source_scene()!=scene||exit_.door_count()!=1) {
    e="Source House Door does not own the actual current Room";return false;
  }
  const auto door=exit_.door(0);
  const auto target=exit_.string(door.target_path);
  if(target.empty()||!exit_.source_hash(target,expected.source_sha256)) {
    e="House Door destination source proof absent";return false;
  }
  if(!catalog.read_file(ResourceRole::FieldSceneBundle,root,bytes,e)||
     !bundle_.load(bytes.data(),bytes.size(),expected,e))return false;
  if(bundle_.source_scene()!=target) {
    e="Destination bundle differs from the source House Door target";return false;
  }
  const auto* embedded=bundle_.entry(PodunkPackRole::HouseExitDoor);
  if(!embedded||embedded->path!=catalog.path(ResourceRole::HouseSceneDoor)||
     embedded->identity.scene_id!=exit_.identity().scene_id||
     embedded->identity.upstream_commit!=exit_.identity().upstream_commit||
     embedded->identity.source_sha256!=exit_.identity().source_sha256) {
    e="Destination bundle has a different owning House Door";return false;
  }
  loaded_=true;e.clear();return true;
}
bool FieldSceneDestinationData::destination(uint32_t id,
    const PodunkBundleData*& out,std::string& e)const {
  FieldDoorDescriptor door;
  std::array<uint8_t,32> source{};
  if(!loaded_||!exit_.find(id,door)||
     exit_.string(door.target_path)!=bundle_.source_scene()||
     !exit_.source_hash(exit_.string(door.target_path),source)||
     source!=bundle_.identity().source_sha256) {
    e="Source Door has no owned destination bundle";return false;
  }
  out=&bundle_;e.clear();return true;
}
}
