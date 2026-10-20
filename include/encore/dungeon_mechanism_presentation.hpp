#pragma once
#include "encore/dungeon_mechanism_bindings.hpp"
#include "encore/audio_data.hpp"
#include <array>
#include <memory>

namespace encore::upstream {
struct DungeonMechanismSprite {
 uint32_t id=0,texture_bytes=0,texture_crc=0,width=0,height=0;
 uint32_t cell_width=0,cell_height=0,columns=0,rows=0,frames=0,bits=0;
 int32_t z_index=0;Vec2 position{},offset{};
 std::string texture,source;std::array<uint8_t,32>texture_sha256{};
};
struct DungeonMechanismPlacement {
 uint32_t owner=0,profile=0,bits=0,order=0;Vec2 position{};
 std::string path,parent;
};
enum class DungeonMechanismShapeRole:uint32_t {Body=1,Interaction=2,Pressure=3};
struct DungeonMechanismShape {
 uint32_t owner=0,layer=0,mask=0,bits=0;DungeonMechanismShapeRole role=DungeonMechanismShapeRole::Body;
 Vec2 body_position{},shape_position{},extents{};float safe_margin=0,one_way_margin=0;std::string path;
};
struct DungeonMechanismDraw {
 uint32_t owner=0,profile=0,x=0,y=0,width=0,height=0,bits=0,order=0;
 int32_t z_index=0;Vec2 top_left{};float sort_y=0;bool parent_y_sort=false;
};
struct DungeonMechanismRectangle {Vec2 center{},extents{},minimum{},maximum{};};
enum class DungeonMechanismSoundKind:uint32_t {Node=1,ManagerChannel=2};
struct DungeonMechanismSound {
 uint32_t owner=0,stable_id=0,bus=0,pause_mode=0;DungeonMechanismSoundKind kind=DungeonMechanismSoundKind::Node;
 std::string path,source,channel;std::array<uint8_t,32>source_sha256{};float gain_db=0,pitch_scale=0;
};
// ENCDMP01 format1, capabilities1/2. Prepared geometry/drawing and optional
// source-node sound ownership plus checked PCM metadata only;
// this owner creates no Room, texture, physics world, sound, callback or save.
class DungeonMechanismPresentationData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}
 uint32_t capability()const;bool media_bound()const{return capability()==2;}
 uint32_t encoded_bytes()const;uint32_t encoded_crc()const;
 const DungeonMechanismBindingsData&bindings()const;
 const std::vector<DungeonMechanismSprite>&sprites()const;
 const std::vector<DungeonMechanismPlacement>&owners()const;
 const std::vector<DungeonMechanismShape>&shapes()const;
 const std::vector<DungeonMechanismSound>&sounds()const;
 const AudioBank&audio_bank()const;
 const DungeonMechanismSound*sound(uint32_t owner)const;
 const DungeonMechanismSprite*sprite(uint32_t)const;
 bool texture_matches(uint32_t,const uint8_t*,size_t,std::string&)const;
 bool draw(uint32_t owner,uint32_t frame,DungeonMechanismDraw&,std::string&)const;
 bool rectangle(size_t index,DungeonMechanismRectangle&,std::string&)const;
private:
 struct State;std::shared_ptr<const State>state_;
};
}
