#pragma once
#include "encore/movement.hpp"
#include "encore/source_random.hpp"
#include <array>
#include <string>
#include <vector>
namespace encore::upstream {
struct PresentSparklesFrame {uint32_t x=0,y=0,width=0,height=0;};
class PresentSparklesData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const std::string&reviewed_commit()const{return commit_;}
 uint32_t stable_id()const{return id_;}uint32_t parent_id()const{return parent_id_;}
 const std::string&node()const{return node_;}const std::string&parent_node()const{return parent_;}
 const std::string&flag()const{return flag_;}const std::string&animation()const{return animation_;}
 const std::string&texture_path()const{return texture_;}const std::string&source()const{return source_;}
 uint32_t width()const{return width_;}uint32_t height()const{return height_;}uint32_t serialized_frame()const{return serialized_frame_;}
 float speed()const{return speed_;}float speed_scale()const{return speed_scale_;}
 float random_low()const{return low_;}float random_high()const{return high_;}
 bool pixel_snap()const{return flags_&8;}bool centered()const{return flags_&4;}bool playing()const{return flags_&2;}bool looping()const{return flags_&1;}
 Vec2 parent_position()const{return parent_position_;}Vec2 position()const{return position_;}Vec2 offset()const{return offset_;}
 const std::vector<PresentSparklesFrame>&frames()const{return frames_;}
private:
 bool valid_=false;uint32_t id_=0,parent_id_=0,width_=0,height_=0,serialized_frame_=0,flags_=0;
 float low_=0,high_=0,speed_=0,speed_scale_=0;Vec2 parent_position_{},position_{},offset_{};
 std::string commit_,node_,parent_,flag_,animation_,texture_,source_;
 std::vector<PresentSparklesFrame>frames_;
};
// Initialize exactly once at the child's source Ready position in the shared
// RNG ledger, even if parent Present Ready immediately stops it for a saved flag.
// This clock follows internal idle AnimatedSprite processing, not player pause.
class PresentSparklesRuntime {
public:
 bool ready(const PresentSparklesData&,SourceRandom&,std::string&);
 bool set_opened(bool,std::string&);bool idle_frame(double,bool update_pending,std::string&);
 bool initialized()const{return data_!=nullptr;}bool visible()const{return visible_;}bool playing()const{return playing_;}
 uint32_t frame_index()const{return frame_;}float timeout()const{return timeout_;}
 uint64_t frame_changes()const{return frame_changes_;}uint64_t loops()const{return loops_;}
 const PresentSparklesData*data()const{return data_;}const PresentSparklesFrame*frame()const;
private:const PresentSparklesData*data_=nullptr;uint32_t frame_=0;float timeout_=0;bool visible_=false,playing_=false;uint64_t frame_changes_=0,loops_=0;
};
}
