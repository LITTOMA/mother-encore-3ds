#pragma once
#include "encore/field_data.hpp"
#include <array>
#include <memory>
namespace encore::upstream {
// ENCTERR1f1/cap1: detached static geometry plus original read-only cell and
// navigation metadata. No scene/actor/navigation-solver/save admission.
struct TerrainInfo {uint32_t scene=0,inactive_script=0,player_layer=0,player_mask=0;std::string source,root_name;};
struct TerrainLayer {uint32_t id=0,graphics_layer=0,layer=0,mask=0,flags=0,origin=0,first=0,count=0,order=0;std::string name;Vec2 position{},cell_size{};std::array<int32_t,4>used{};};
struct TerrainCell {uint32_t layer=0,tile=0,flags=0,graphics_tile=kFieldNone,source_order=0;int16_t x=0,y=0;uint16_t auto_x=0,auto_y=0;};
struct TerrainDefinition {uint32_t id=0,mode=0,shape_count=0,nav_first=0,nav_count=0;int32_t z=0;std::string name,texture;std::array<float,4>region{};Vec2 offset{};};
struct TerrainNavigation {uint32_t tile=0,point_first=0,point_count=0,index_first=0,index_count=0;Vec2 offset{};};
class DungeonTerrainData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}FieldMapView graphics()const;const TerrainInfo&info()const;
 const std::vector<TerrainLayer>&layers()const;const std::vector<TerrainCell>&cells()const;const std::vector<TerrainDefinition>&definitions()const;
 const std::vector<TerrainNavigation>&navigation()const;const std::vector<Vec2>&navigation_points()const;const std::vector<uint32_t>&navigation_indices()const;
 const TerrainDefinition*definition(uint32_t)const;
 // Valid absence differs from an unsupported layer/coordinate. Failure
 // preserves outputs. An unresolved original tile still has a logical cell.
 bool cell(uint32_t,int32_t,int32_t,bool&,TerrainCell&,std::string&)const;
private:struct State;std::shared_ptr<const State>state_;
};
}
