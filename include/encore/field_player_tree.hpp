#pragma once
#include "encore/field_swing_entry.hpp"
namespace encore::upstream {
// The immutable resources must outlive this controller. Animation state and
// control ownership are independent: a pause releases ATTACK without stopping
// an already running nonloop animation. Call flush after the animation pass.
class FieldPlayerTree {
public:
 bool initialize(const FieldSwingEntryData&,const FieldSwingActionData&,const FieldSwingData&,uint32_t,Vec2,float,std::string_view,std::string&);
 bool travel(uint32_t,std::string&);
 bool input(std::string_view,bool,const SwingActionContext&,SwingMotionFlags&,bool&,std::string&);
 bool pause(bool stop_running,bool start_idle,bool incapacitated,SwingMotionFlags&,std::string&);
 bool unpause(std::string&);
 bool advance(double,std::string&);bool flush(SwingMotionFlags&,std::string&);
 bool ready()const{return data_!=nullptr;}bool attacking()const{return attack_;}bool paused()const{return paused_;}
 uint32_t state_id()const{return state_;}uint32_t target()const{return target_;}uint32_t paused_state()const{return paused_state_;}
 float position()const;
 uint32_t main_frame()const{return playback_.sample.frame;}const SwingPose&pose()const{return pose_;}
 uint32_t starts()const{return starts_;}const std::string&selected_skill()const{return selected_;}
 const std::string&sound()const{return swing_?swing_->strings()[2]:selected_;}
private:
 const FieldSwingEntryData*data_=nullptr;const FieldSwingActionData*action_=nullptr;const FieldSwingData*swing_=nullptr;
 FieldSwingPlayer player_;SwingPose pose_;AnimationPlayback playback_{0,{0,true,false}};
 uint32_t state_=0,target_=0,paused_state_=0,starts_=0;Vec2 facing_{};
 bool attack_=false,paused_=false,new_node_=false,incap_=false;std::string selected_;
};
}
