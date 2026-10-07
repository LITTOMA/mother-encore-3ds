#pragma once
#include "encore/restore_data.hpp"
#include "encore/house_reentry.hpp"
#include "encore/field_node_tree.hpp"

namespace encore::upstream {
// Supplied from the same validated resource load, before its Reentry byte
// buffer is released. IR hashes come from actual admitted bundle entries.
struct HouseInspectionRestoreBindings {
  RoomView room{};
  HouseView house{};
  const FieldDoorData *doors = nullptr;
  const HouseReentryData *reentry = nullptr;
  const FieldNodeTreeData *tree = nullptr;
  const uint8_t *reentry_bytes = nullptr;
  size_t reentry_size = 0;
  std::array<uint8_t,32> room_ir_sha{},tree_ir_sha{};
};
// Independent immutable Restore owner for the complete inspection Room.
// Admission creates no VM, Node, lifecycle callback or random allocation.
class HouseInspectionRestoreData final {
public:
  bool load(const uint8_t*,size_t,const HouseInspectionRestoreBindings&,
            std::string&);
  bool load_file(const char*,const HouseInspectionRestoreBindings&,
                 std::string&);
  bool matches(const HouseInspectionRestoreBindings&,std::string&)const;
  bool valid()const{return valid_;}
  bool scene_admitted()const{return false;}
  const RestoreData&restore()const{return restore_;}
  const FieldIdentity&identity()const{return identity_;}
  const auto&ir_sha256()const{return ir_;}
  const auto&source_semantics_sha256()const{return semantics_;}
  const auto&original_restore_ir_sha256()const{return original_ir_;}
  bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
  bool cross_bind(const HouseInspectionRestoreBindings&,std::string&)const;
  bool valid_=false;
  FieldIdentity identity_{};
  uint32_t room_scene_=0,reentry_scene_=0,door_=0;
  std::string scene_;
  std::array<uint8_t,32>ir_{},tree_ir_{},original_ir_{},semantics_{};
  struct Fingerprint {
    uint32_t bytes=0;
    std::array<uint8_t,32>pack{},ir{};
  } room_,house_,reentry_;
  std::map<std::string,std::array<uint8_t,32>>sources_;
  RestoreData restore_;
};
} // namespace encore::upstream
