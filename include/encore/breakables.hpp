#pragma once
#include "encore/prop_clips.hpp"
#include <array>
#include <map>
#include <set>

namespace encore::upstream {
struct BreakablePoint {float x=0,y=0;};
struct BreakablePolicy {
 double distance_delay=0,maximum_delay=0,weak=0,strong=0,rumble_seconds=0;
 double lift=0,lift_seconds=0,fall=0,fall_seconds=0;
 uint32_t device=0,clear_hit_layer=0,clear_hit_mask=0,clear_body_layer=0,sound=0;
 std::string source,name;
};
struct BreakableFamily {
 uint32_t id=0,columns=0,rows=0,width=0,height=0,break_clip=0,ready_clip=0,reset_clip=0;
 std::string source,texture;BreakablePoint sprite_offset,drop_offset;
 float drop_seconds=0;uint32_t body_layer=0,body_mask=0,hit_layer=0,hit_mask=0;
 bool spatial_audio=false;std::array<std::array<float,4>,2>shapes{};
 struct AudioKey {uint32_t clip=0;float time=0;bool value=false;uint32_t update=0,track=0;};
 std::vector<AudioKey>audio;PropClipsData clips;
};
struct BreakableOwner {
 uint32_t id=0,family=0,content=UINT32_MAX,coins=0;
 std::string path,parent,new_parent,drop_parent,spawner,flag,appear,disappear;
 bool object_flag=false,emit_flag=false,reset_area=false,reset_region=false;
 bool drop_hidden=false,interact_content=false,pause=false,hide_drop=false,tween_drop=false;
 BreakablePoint position;
};
struct BreakableContentBinding {std::string path,type,script,source,parent;};
class BreakablesData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint32_t scene()const{return scene_;}
 const BreakablePolicy&policy()const{return policy_;}
 const std::vector<BreakableFamily>&families()const{return families_;}
 const std::vector<BreakableOwner>&owners()const{return owners_;}
 const std::vector<BreakableContentBinding>&contents()const{return contents_;}
 const BreakableFamily*family(uint32_t)const;const BreakableOwner*owner(uint32_t)const;
private:
 bool valid_=false;uint32_t scene_=0;BreakablePolicy policy_;
 std::vector<BreakableFamily>families_;std::vector<BreakableOwner>owners_;
 std::vector<BreakableContentBinding>contents_;
};

// Prepared content snapshot. The scene owner must provide the actual runtime
// child-shape handles and optional properties, not infer them from node names.
// This holder does not execute the contents' own AI/physics/interact method.
struct BreakableContentState {
 bool present=false,visible=true,physics=false,has_start=false,has_changing=false,has_interact=false;
 bool changing=false;uint32_t interactions=0;
 std::string parent;BreakablePoint position,start;
 std::map<uint64_t,bool>shapes;
};
enum class BreakableRequestKind:uint32_t {Pause=1,Rumble=2,Audio=3,Coins=4,Interact=5,FlagsUpdated=6};
struct BreakableRequest {BreakableRequestKind kind=BreakableRequestKind::Pause;uint64_t epoch=0,frame=0;uint32_t owner=0,identity=0;std::array<double,4>value{};};
struct BreakableServices {
 uint64_t epoch=0;uint32_t scene=0;bool paused=false;
 std::map<std::string,bool>story_flags,object_flags;
 std::set<std::string>parents,factories;std::set<uint32_t>sounds;
 std::map<std::string,BreakableContentState>contents;
 std::vector<BreakableRequest>requests;
};
// Source-specific lifecycle, idle timers, animation, drop and reparent owner.
// All effects stage against supplied prepared services before committing.
// Factory/audio/interact requests still require admitted scene consumers.
class BreakableActor {
public:
 bool prepare(const BreakablesData&,uint32_t,uint64_t,BreakableServices&,std::string&);
 bool hit(uint64_t,BreakablePoint,BreakableServices&,std::string&);
 bool advance_idle(uint64_t,double,BreakableServices&,std::string&);
 bool leave(uint64_t,bool,BreakableServices&,std::string&);
 bool valid()const{return valid_;}bool removed()const{return removed_;}
 bool root_visible()const{return visible_;}const std::string&parent()const{return parent_;}
 BreakablePoint position()const{return owner_.position;}
 const PropClipPlayer&animation()const{return animation_;}
 uint32_t body_layer()const{return body_layer_;}uint32_t hit_layer()const{return hit_layer_;}uint32_t hit_mask()const{return hit_mask_;}
 bool body_disabled()const{return body_disabled_;}bool pending()const{return hit_pending_||animation_.playing();}
 uint64_t frame()const{return frame_;}
private:
 bool context(uint64_t,const BreakableServices&,std::string&)const;
 bool tick(float,BreakableServices&,std::string&);
 bool start_break(BreakableServices&,std::string&);
 bool set_flag(bool,BreakableServices&,std::string&);
 bool request(BreakableServices&,BreakableRequestKind,uint32_t,std::array<double,4>,std::string&);
 BreakableContentState*content(BreakableServices&)const;
 bool valid_=false,removed_=false,visible_=false,body_disabled_=false,hit_pending_=false,hit_started_=false;
 uint64_t epoch_=0,frame_=0;uint32_t scene_=0,body_layer_=0,hit_layer_=0,hit_mask_=0,audio_cursor_=0;
 BreakablePolicy policy_;BreakableFamily family_;BreakableOwner owner_;std::string content_path_,content_parent_,parent_;
 PropClipPlayer animation_;float hit_left_=0;double drop_left_=0,tween_elapsed_=0;
 enum class Drop:uint8_t {None,Timer,Lift,Fall,Done};Drop drop_=Drop::None;
 BreakablePoint tween_start_;std::vector<uint64_t>captured_shapes_;
};
}
