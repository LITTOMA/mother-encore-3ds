#pragma once
#include "encore/movement.hpp"
#include "encore/room_data.hpp"
#include "encore/field_data.hpp"
#include "encore/source_random.hpp"
#include "encore/story_object_host.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
enum class FieldObjectFlag:uint32_t {ShowAfterIdle=1,RandomFrame=2,Loop=4,Playing=8,MainCentered=16,EffectCentered=32,BehindParent=64,SuppressAtReady=128,PrecomputedReady=256};
struct FieldObjectFrame {uint32_t x=0,y=0,width=0,height=0;};
struct FieldObjectDefinition {
 uint32_t id=0,kind=0,flags=0,main_frame=0,first=0,count=0,initial=0,ready_delay=0;
 std::string path,ready_flag;Vec2 position{},main_offset{},effect_offset{};
 float fps=0,random_min=0,random_max=0;
 uint32_t resolved=0,body=kFieldNone,draw=kFieldNone,notice=kFieldNone;
 Vec2 body_center{},body_extents{};
 uint32_t body_layer=0,body_mask=0;
};
// Independent source-owned visuals and ready metadata. Interaction/collection
// are not inferred from a drawing record and need separately admitted hosts.
class FieldObjectsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool bind(RoomView,std::string&)const;
 bool bind(RoomView,FieldMapView,std::string&)const;
 bool valid()const{return valid_;}uint32_t scene_id()const{return scene_;}
 uint32_t width()const{return width_;}uint32_t height()const{return height_;}
 uint32_t capabilities()const{return capability_;}bool rendering_bound()const{return valid_&&capability_>=2;}
 bool drawing_bound()const{return valid_&&capability_==3;}
 bool texture_matches(const uint8_t*,size_t,std::string&)const;
 uint64_t ready_state()const{return ready_state_;}
 uint32_t player_layer()const{return player_layer_;}uint32_t player_mask()const{return player_mask_;}
 const std::string&texture()const{return texture_;}
 const std::array<uint8_t,32>&texture_sha256()const{return texture_sha256_;}
 const std::vector<FieldObjectDefinition>&objects()const{return objects_;}
 const std::vector<FieldObjectFrame>&frames()const{return frames_;}
 const std::vector<uint32_t>&animation_frames()const{return animation_frames_;}
private:
 bool valid_=false;uint32_t scene_=0,width_=0,height_=0,capability_=0;uint64_t ready_state_=0;std::string texture_;
 uint32_t player_layer_=0,player_mask_=0;
 uint32_t texture_bytes_=0,texture_crc_=0;
 std::array<uint8_t,32>texture_sha256_{};
 std::vector<FieldObjectDefinition>objects_;std::vector<FieldObjectFrame>frames_;
 std::vector<uint32_t>animation_frames_;
};
struct FieldObjectState {
 bool ready=false,alive=true,visible=false,queued=false,show_pending=false;
 uint32_t frame=0;float timeout=0;
};
// Source-owned DroppedItem ready/idle/delete subset. Preparation draws no RNG;
// the caller dispatches ready in the original scene-tree order, interleaved
// with other owners. Collection/tweening is a separate, unadmitted operation.
class FieldObjects final:public StoryObjectHost {
public:
 bool prepare(const FieldObjectsData&,RoomView,std::string&);
 bool enter_ready(uint32_t id,bool ready_flag,SourceRandom&,std::string&);
 // A production scene reuses its offline ready replay sample. It must never
 // consume the same Sparkles draw a second time from the live stream.
 bool enter_ready(uint32_t id,bool ready_flag,std::string&);
 bool idle_frame(std::string&);bool advance_idle(double,std::string&);
 bool validate_story_object(uint32_t,std::string_view,std::string&)const override;
 bool can_queue_free(std::string_view,std::string&)const override;
 bool queue_free(std::string_view,std::string&)override;bool flush_queued(std::string&);
 bool prepared()const{return data_.valid();}
 const FieldObjectsData&data()const{return data_;}
 const std::vector<FieldObjectState>&states()const{return states_;}
 const FieldObjectState*state(uint32_t id)const;
private:
 size_t index(uint32_t)const;
 FieldObjectsData data_;std::vector<FieldObjectState>states_;
};
}
