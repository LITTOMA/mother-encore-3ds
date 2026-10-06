#pragma once
#include "encore/field_data.hpp"
#include <utility>

namespace encore::upstream {
enum class FieldSceneRole:uint32_t {Pending,Grass,Npc,EnemySpawner,Tint,CharacterSprite,SpriteFetcher,FlagLandmark,FlaggableDerived,AreaRoom,DebugStart,Emotes,DandelionSpawner,Door,ButtonPrompt,DeadBush,OpenableDoor=16,Sparkles=17,InteractDialog=18,Payphone=19,Present=20,DroppedItem=21,Butterfly=22,CutsceneArea=23,Birds=24,CameraArea=25,MusicChanger=26,MapArrows=27,GameCamera=28,Reparenter=29,EventActivator=30,SteppingSounds=31,JumpArea=32,Stairs=33,DoorNpc=34,MelodyBackground=35};
struct FieldSceneReady {uint32_t id=0,node=0,name=0,ordinal=0,profile=0,script=0;FieldSceneRole role{};std::array<uint8_t,32>sha{};};
struct FieldSceneLandmark {uint32_t id=0,appear=0,disappear=0;bool delete_if_hidden=false,initial_visible=true;};
struct FieldSceneFlaggable {uint32_t id=0,key=0,flags=0,leaf_script=0;};
struct FieldSceneArea {uint32_t id=0,name=0,region=0,magicant=0,flying_flag=0;bool sub_area=false;Vec2 map_offset{};std::array<float,4>background{};};
struct FieldSceneDebug {uint32_t id=0;Vec2 position{};};
class FieldSceneData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return !bytes_.empty();}bool scene_admitted()const{return false;}
 FieldIdentity identity()const;std::string_view source_scene()const;std::string_view string(uint32_t)const;
 uint32_t ready_count()const;FieldSceneReady ready(uint32_t)const;
 uint32_t landmark_count()const;FieldSceneLandmark landmark(uint32_t)const;
 uint32_t flaggable_count()const;FieldSceneFlaggable flaggable(uint32_t)const;
 FieldSceneArea area()const;FieldSceneDebug debug(uint32_t)const;
 uint32_t override_count()const;std::pair<std::string_view,std::string_view>map_override(uint32_t)const;
 uint32_t visit_count()const;std::pair<std::string_view,std::string_view>visit(uint32_t)const;
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
 bool normal_flag_exists(std::string_view)const;
private:
 bool admit(std::vector<uint8_t>&&,const FieldIdentity&,std::string&);
 uint32_t count(uint32_t)const;const uint8_t*record(uint32_t,uint32_t)const;
 std::vector<uint8_t>bytes_;
};
}
