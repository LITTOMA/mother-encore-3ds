#pragma once
#include "encore/dungeon_key_clocks.hpp"
#include "encore/audio_data.hpp"
namespace encore::upstream {
struct KeySprite {uint32_t id=0,texture_bytes=0,texture_crc=0,width=0,height=0,cell_width=0,cell_height=0,columns=0,rows=0,frames=0,bits=0;int32_t z_index=0;Vec2 offset{};std::string texture,source;std::array<uint8_t,32>sha{};};
struct KeyPlacement {uint32_t owner=0,profile=0,bits=0,order=0,parent_order=0;int32_t z_base=0;Vec2 position{},child_position{};std::string path,parent;};
enum class KeyShapeRole:uint32_t {Pickup=1,Body=2,Interaction=3};
struct KeyShape {uint32_t owner=0,layer=0,mask=0,bits=0;KeyShapeRole role=KeyShapeRole::Pickup;Vec2 center{},extents{};float one_way_margin=0;std::string path;};
struct KeySound {uint32_t owner=0,profile=0,asset=0,bus=0;bool finished_callback=false;float gain=0,pitch=0;std::string path;};
struct KeyDraw {uint32_t owner=0,profile=0,x=0,y=0,width=0,height=0,bits=0,order=0,parent_order=0;int32_t z_index=0;Vec2 top_left{};float sort_y=0;bool parent_y_sort=false;};
struct KeyRectangle {uint32_t owner=0,layer=0,mask=0,bits=0;KeyShapeRole role=KeyShapeRole::Pickup;Vec2 center{},extents{},minimum{},maximum{};bool enabled=false;};
// ENCKMP01 format1/capability1: prepared object geometry and media identity.
// Clocks and presentation share the same immutable resource owner at binding.
// Terrain, UI subtree, physics delivery, media IO and save admission are separate.
class DungeonKeyMediaData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}uint32_t scene()const;
 const DungeonKeyClocksData&clocks()const;const AudioBank&audio_bank()const;
 const std::vector<KeySprite>&sprites()const;const std::vector<KeyPlacement>&owners()const;const std::vector<KeyShape>&shapes()const;const std::vector<KeySound>&sounds()const;
 const KeySprite*sprite(uint32_t)const;const KeyPlacement*owner(uint32_t)const;
 bool matches(const DungeonKeyClocks&,std::string&)const;
 bool texture_matches(uint32_t,const uint8_t*,size_t,std::string&)const;
 bool draw(const DungeonKeyClocks&,std::vector<KeyDraw>&,std::string&)const;
 bool rectangles(const DungeonKeyClocks&,std::vector<KeyRectangle>&,std::string&)const;
private:struct State;std::shared_ptr<const State>state_;
};
struct KeySoundCommand {uint64_t epoch=0;uint32_t slot=0,owner=0,profile=0;bool playing=false;};
// Only Clock.Audio writes become hardware requests. Method audio events retain
// observer snapshots and must not produce a duplicate start. Every other event
// is validated by its known producer interface before being left to its owner.
class DungeonKeyAudioRouter {
public:
 bool prepare(const DungeonKeyMediaData&,const DungeonKeyClocks&,std::string&);
 bool plan(const std::vector<DungeonKeyClockEvent>&,std::vector<KeySoundCommand>&,std::string&)const;
 bool finished(uint64_t,uint32_t,DungeonKeyClocks&,std::string&)const;
 bool retire(std::vector<KeySoundCommand>&,std::string&);
 uint64_t epoch()const{return epoch_;}const DungeonKeyMediaData&data()const{return data_;}
private:DungeonKeyMediaData data_;uint64_t epoch_=0,last_epoch_=0;
};
}
