#pragma once
#include "encore/field_swing.hpp"
#include <string_view>
namespace encore::upstream {
struct SwingIdleDirection {uint32_t clip=0;Vec2 direction{};uint32_t frame=0,flags=0;};
class FieldSwingActionData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool bind(const FieldSwingData&,std::string&)const;
 bool valid()const{return valid_;}const std::array<uint32_t,8>&policy()const{return policy_;}
 const std::array<std::string,4>&strings()const{return strings_;}
 const std::vector<SwingIdleDirection>&directions()const{return directions_;}
 const SwingIdleDirection*direction(uint32_t clip)const;
private:
 bool valid_=false;std::array<uint32_t,8>policy_{};std::array<std::string,4>strings_;
 std::vector<SwingIdleDirection>directions_;
};
struct SwingActionContext {
 Vec2 direction{};bool moving=true,paused=false,entering_door=false,climbing=false,spinning=false;
 bool skill_available=false,incapacitated=false;
};
struct SwingMotionFlags {bool crouch=false,idle=false,tap_run=false,running=false,walking=false;};
// The checked single-skill graph owns logical input, sparse Bat tracks and its
// direct Idle transition. The platform owns physical input, idle scheduling,
// pause, sound/rumble execution and physics monitoring. No motion is integrated.
class FieldSwingAction {
public:
 bool initialize(const FieldSwingActionData&,const FieldSwingData&,Vec2,std::string_view selected,std::string&);
 // accepted is false for a normal gated/irrelevant input. Errors preserve the
 // controller, external motion flags and output. A true acceptance emits one
 // sound/rumble start, obtained from the unchanged action resource.
 bool input(std::string_view action,bool pressed,const SwingActionContext&,SwingMotionFlags&,bool&accepted,std::string&);
 bool advance(double,std::string&);bool flush(SwingMotionFlags&,std::string&);
 const SwingPose&pose()const{return pose_;}uint32_t main_frame()const{return main_frame_;}
 bool attacking()const{return player_.attacking();}bool bat_node()const{return bat_node_;}
 bool bat_visible()const{return data_.valid()&&selected_==data_.strings()[3];}
 float position()const{return bat_node_?player_.position():0;}
 const std::string&selected_skill()const{return selected_;}uint32_t starts()const{return starts_;}
 const std::string&sound()const{return swing_.strings()[2];}
 const FieldSwingData&action_data()const{return swing_;}
private:
 FieldSwingActionData data_;FieldSwingData swing_;FieldSwingPlayer player_;SwingPose pose_;
 uint32_t clip_=0,main_frame_=0,starts_=0;bool travel_=false,bat_node_=false;
 std::string selected_;
};
}
