#pragma once
#include "encore/field_data.hpp"
namespace encore::upstream {
// ENCDSW01 format1/capability1: detached source switch graph metadata.
// No scene, source script, timeline, camera or audio is activated by loading.
enum class DungeonSwitchKind:uint32_t {Lever=1,Plate=2};
enum class DungeonControlledKind:uint32_t {Gate=1,Wall=2};
struct DungeonSwitchPolicy {FieldText region;float ready_delay=0;uint32_t main_switch=0,wall=0;float post_delay=0,music_fade=0;FieldText denied_audio,denied_channel;};
struct DungeonSwitchNode {
 uint32_t id=0;FieldText path,flag;uint32_t bits=0;float camera_delay=0,x=0,y=0;
 uint32_t first_edge=0,edge_count=0;FieldText audio;DungeonSwitchKind kind=DungeonSwitchKind::Lever;
};
struct DungeonControlledNode {
 uint32_t id=0;FieldText path;DungeonControlledKind kind=DungeonControlledKind::Gate;
 int32_t order=0;uint32_t bits=0;float x=0,y=0,offset_x=0,offset_y=0;
};
struct DungeonSwitchEdge {uint32_t source=0,target=0;};
class DungeonSwitchesView {
public:
 bool valid()const{return p_!=nullptr;}
 uint32_t scene_id()const;uint32_t switch_count()const;uint32_t object_count()const;uint32_t edge_count()const;
 DungeonSwitchPolicy policy()const;DungeonSwitchNode switch_node(uint32_t)const;DungeonControlledNode object(uint32_t)const;DungeonSwitchEdge edge(uint32_t)const;
 std::string_view text(FieldText)const;const void*bytes_identity()const{return p_;}
private:
 friend class DungeonSwitchesData;
 uint32_t count(uint32_t)const;const uint8_t*record(uint32_t,uint32_t)const;const uint8_t*p_=nullptr;
};
class DungeonSwitchesData {
public:
 DungeonSwitchesData()=default;DungeonSwitchesData(const DungeonSwitchesData&)=delete;DungeonSwitchesData&operator=(const DungeonSwitchesData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 DungeonSwitchesView view()const{DungeonSwitchesView v;if(!bytes_.empty())v.p_=bytes_.data();return v;}
private:std::vector<uint8_t>bytes_;
};
}
