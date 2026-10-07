// Manual source-resource driver only. Not executed by this producer slice.
#include "encore/field_map.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/crc32.hpp"
#include "manual_require.hpp"

using namespace encore::upstream;
namespace {
uint32_t house_map_u32(const std::vector<uint8_t>&b,size_t at) {
 return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;
}
void house_map_word(std::vector<uint8_t>&b,size_t at,uint32_t value) {
 for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(value>>(i*8));
}
void house_map_crc(std::vector<uint8_t>&b) {
 house_map_word(b,24,encore::crc32(b.data()+128,b.size()-128));
}
}
void house_return_map_resource_manual(const std::vector<uint8_t>&bytes,
                                      const FieldNodeTreeData&tree) {
 std::string error;FieldMapView map;
 MANUAL_REQUIRE(map.load(bytes.data(),bytes.size(),tree.identity(),error));
 MANUAL_REQUIRE(!map.scene_admitted()&&map.map_count()==3&&map.cell_count()==291);
 MANUAL_REQUIRE(map.draw_count()==290&&map.polygon_count()==0&&map.texture_count()==2);
 MANUAL_REQUIRE(map.source_scene()==tree.source_scene());
 for(uint32_t i=0;i<map.map_count();++i){
  const auto layer=map.map(i);const auto*node=tree.record(layer.stable_id);
  MANUAL_REQUIRE(node&&node->path==map.string(layer.node)&&tree.classes().at(node->class_index)=="TileMap");
 }
 const auto below=map.map(0);MANUAL_REQUIRE(map.string(below.node)=="Below"&&below.cell_count==1);
 const auto missing=map.cell(below.cell_first);
 MANUAL_REQUIRE(missing.tile==30&&missing.draw_count==0&&missing.polygon_count==0);
 auto rejected=[&](const std::vector<uint8_t>&bad){
  FieldMapView empty;MANUAL_REQUIRE(!empty.load(bad.data(),bad.size(),tree.identity(),error)&&!empty.valid());
  MANUAL_REQUIRE(!map.load(bad.data(),bad.size(),tree.identity(),error));
  MANUAL_REQUIRE(map.valid()&&map.map_count()==3&&map.draw_count()==290);
 };
 for(size_t at:{size_t(8),size_t(12),size_t(16),size_t(20),size_t(28),size_t(32),size_t(36)}){
  auto bad=bytes;house_map_word(bad,at,UINT32_MAX);rejected(bad);
 }
 for(size_t at:{size_t(40),size_t(60)}){
  auto bad=bytes;bad.at(at)^=1;
  rejected(bad);
 }
 auto bad=bytes;house_map_word(bad,128+2*24+12,1);house_map_crc(bad);rejected(bad);
 const auto textures=house_map_u32(bytes,128+7*24+4);
 bad=bytes;house_map_word(bad,textures+12,2048);house_map_crc(bad);rejected(bad);
 for(size_t size:{size_t(559),bytes.size()-1}){bad=bytes;bad.resize(size);rejected(bad);}
}
