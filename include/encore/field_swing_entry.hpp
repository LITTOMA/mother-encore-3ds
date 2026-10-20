#pragma once
#include "encore/field_swing_action.hpp"
#include "encore/animation.hpp"
namespace encore::upstream {
struct SwingEntryState {uint32_t id=0,next=0,mode=0,first=0,count=0;std::string name;uint32_t automatic=0;bool pausable=false;};
struct SwingTreeRoute {uint32_t from=0,to=0,next=0,mode=0;};
struct SwingEntryClip {uint32_t state=0,ordinal=0;Vec2 direction{};FrameClip clip{};};
class FieldSwingEntryData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool bind(const FieldSwingActionData&,const FieldSwingData&,std::string&)const;
 bool valid()const{return valid_;}uint16_t frames()const{return frames_;}
 uint32_t capability()const{return capability_;}uint32_t incapacitated_idle()const{return incap_idle_;}
 const SwingTreeRoute*route(uint32_t from,uint32_t to)const;
 uint32_t idle()const{return idle_;}uint32_t attack()const{return attack_;}
 const std::array<uint32_t,4>&motion_states()const{return motion_;}
 const std::vector<SwingEntryState>&states()const{return states_;}
 const std::vector<SwingEntryClip>&clips()const{return clips_;}
 const SwingEntryState*state(uint32_t)const;const SwingEntryClip*select(uint32_t,Vec2)const;
private:
 bool valid_=false;uint16_t frames_=0;uint32_t idle_=0,attack_=0,capability_=0,incap_idle_=0;
 std::vector<SwingTreeRoute>routes_;
 std::array<uint32_t,4>motion_{};std::vector<SwingEntryState>states_;std::vector<SwingEntryClip>clips_;
};
// Rebase from the actual persistent motion snapshot before a new action. While
// active this owner advances only the requested entry path, then delegates Bat
// and deferred completion to the separately checked action executor.
class FieldSwingEntry {
public:
 bool initialize(const FieldSwingEntryData&,const FieldSwingActionData&,const FieldSwingData&,uint32_t state,Vec2 facing,float position,std::string_view selected,std::string&);
 bool rebase(uint32_t state,Vec2 facing,float position,std::string&);
 bool input(std::string_view,bool,const SwingActionContext&,SwingMotionFlags&,bool&,std::string&);
 bool advance(double,std::string&);bool flush(SwingMotionFlags&,std::string&);
 bool attacking()const{return action_.attacking();}bool ready()const{return data_.valid();}
 uint32_t state_id()const{return handed_?(action_.bat_node()?data_.attack():data_.idle()):state_;}
 float position()const{return handed_?action_.position():playback_.position;}
 uint32_t main_frame()const{return handed_?action_.main_frame():playback_.sample.frame;}
 SwingPose pose()const{auto p=action_.pose();if(!handed_){p.main=playback_.sample.main_visible;p.special=playback_.sample.special_visible;}return p;}
 const FieldSwingAction&action()const{return action_;}
private:
 FieldSwingEntryData data_;FieldSwingAction action_;uint32_t state_=0,clip_=0;
 Vec2 facing_{};AnimationPlayback playback_{0,{0,true,false}};
 bool requested_=false,pending_=false,handed_=false;
};
}
