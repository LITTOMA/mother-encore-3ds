#pragma once
#include "encore/field_node_tree.hpp"
#include "encore/field_map.hpp"
namespace encore::upstream {
struct FieldVisibilityTracked { uint32_t id=0,kind=0; };
struct FieldVisibilityRect {float x=0,y=0,width=0,height=0;};
struct FieldVisibilityRecord {
  uint32_t id=0,parent=0,scope=0,kind=0,ready=0,flags=0;
  FieldVisibilityRect rect{};std::string path;
  std::vector<FieldVisibilityTracked> tracked;
};
struct FieldVisibilityConnection {
  uint32_t emitter=0,target=0,adapter=0,signal=0;
  std::string method,script;std::array<uint8_t,32>script_sha{};
};
// Native visibility only, bound to the entire checked source tree. This data
// never admits script Ready, native animated state owners, or dynamic recipes.
class FieldVisibilityData {
public:
  bool load(const uint8_t*,size_t,const FieldNodeTreeData&,std::string&);
  bool load_file(const char*,const FieldNodeTreeData&,std::string&);
  bool valid()const{return valid_;}
  const FieldIdentity&identity()const{return identity_;}
  const std::array<uint8_t,32>&ir_sha256()const{return ir_sha_;}
  const FieldVisibilityRecord*record(uint32_t)const;
  const std::vector<FieldVisibilityRecord>&records()const{return records_;}
  const std::vector<FieldVisibilityConnection>&connections()const{return connections_;}
  int32_t cell_size()const{return cell_;}
  uint32_t scan_cutoff()const{return scan_cutoff_;}
private:
  bool valid_=false;FieldIdentity identity_{};std::array<uint8_t,32>ir_sha_{};
  int32_t cell_=0;uint32_t scan_cutoff_=0;std::vector<FieldVisibilityRecord>records_;
  std::vector<FieldVisibilityConnection>connections_;
  std::map<uint32_t,size_t>index_;
};
}
