#pragma once
#include "encore/field_data.hpp"
#include "encore/source_random.hpp"
#include <functional>
#include <map>
#include <set>
namespace encore::upstream {
class FieldPresentData;class FieldDroppedData;class FieldPresentRuntime;class FieldDroppedRuntime;
enum class FieldSparklesOwner:uint32_t {Self=0,Present=1,Dropped=2};
struct FieldSparklesFrame {uint32_t x=0,y=0,width=0,height=0;};
struct FieldSparklesAnimation {std::string name;float speed=0;bool loop=false;std::vector<FieldSparklesFrame>frames;};
struct FieldSparklesDescriptor {
 uint32_t id=0,parent_id=0,ready=0,profile=0,frame=0,flags=0,pause_mode=0;FieldSparklesOwner owner{};int32_t z_index=0,process_priority=0;
 float speed_scale=0;Vec2 position{},offset{};std::array<Vec2,3>world{};std::string node,parent;
};
class FieldSparklesData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return valid_;}bool scene_admitted()const{return false;}FieldIdentity identity()const{return identity_;}
 const std::string&source_scene()const{return scene_;}const std::string&script()const{return script_;}const std::array<uint8_t,32>&script_sha()const{return script_sha_;}
 const std::string&texture_path()const{return texture_;}const std::string&texture_source()const{return image_;}const std::array<uint8_t,32>&texture_sha()const{return output_sha_;}
 uint32_t width()const{return width_;}uint32_t height()const{return height_;}float random_low()const{return low_;}float random_high()const{return high_;}
 const std::vector<FieldSparklesDescriptor>&records()const{return records_;}const FieldSparklesDescriptor*record(uint32_t)const;const FieldSparklesAnimation*animation(uint32_t)const;
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;FieldIdentity identity_{};std::string scene_,script_,image_,texture_;std::array<uint8_t,32>script_sha_{},output_sha_{};uint32_t width_=0,height_=0;float low_=0,high_=0;
 std::vector<FieldSparklesDescriptor>records_;std::vector<FieldSparklesAnimation>animations_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
enum class FieldSparklesSignal:uint32_t {FrameChanged=1,AnimationFinished};
struct FieldSparklesInstance {uint32_t id=0,profile=0,frame=0;float timeout=0,speed_scale=0;bool ready=false,visible=false,playing=false,backwards=false,is_over=false;};
struct FieldSparklesObservation {
 // Actual SceneTree ancestor approval, queue_free/deferred lifetime and native
 // can_process/OS update state. Hiding is NOT permission to stop the clock.
 bool alive=false,ancestors_admitted=false,can_process=false,update_pending=false,visible_in_tree=false,listeners_admitted=false;
 std::array<Vec2,3>world{};std::array<float,4>canvas_color{};bool material_admitted=false;
};
struct FieldSparklesDraw {uint32_t id=0;FieldSparklesFrame frame{};Vec2 local_origin{},world_origin{},world_scale{};std::array<float,4>color{};bool visible=false,pixel_snap=false;};
struct FieldSparklesHost {
 std::function<bool(const FieldSparklesData&,std::string&)>bind;
 std::function<bool(uint32_t,FieldSparklesObservation&,std::string&)>observe;
 std::function<bool(uint32_t,const FieldSparklesInstance&,std::string&)>publish;
 // Signal dispatch is synchronous; source listeners may change playing/frame
 // before INTERNAL_PROCESS consumes the remainder of this same interval.
 std::function<bool(uint32_t,FieldSparklesSignal,std::string&)>emit;
};
bool validate_sparkles_owner_bridge(const FieldSparklesData&,const FieldPresentData&,const FieldDroppedData&,std::string&);
class FieldSparklesRuntime {
public:
 bool initialize(const FieldSparklesData&,SourceRandom&,FieldSparklesHost,FieldPresentRuntime&,FieldDroppedRuntime&,std::string&);bool create(uint32_t);bool ready(uint32_t);
 bool set_frame(uint32_t,int32_t);bool speed_scale(uint32_t,float);bool play(uint32_t,bool backwards=false);bool stop(uint32_t);bool visibility(uint32_t,bool);bool idle_frame(uint32_t,float);bool destroy(uint32_t);
 // Borrowed children expose only authoritative parent snapshots. idle_frame
 // and generic setters reject those IDs: their one owner advances/changes them.
 bool snapshot(uint32_t,FieldSparklesInstance&);bool draw(uint32_t,FieldSparklesDraw&);const FieldSparklesInstance*instance(uint32_t)const;const std::string&error()const{return error_;}const FieldSparklesData*data()const{return data_;}
private:
 const FieldSparklesData*data_=nullptr;SourceRandom*random_=nullptr;FieldSparklesHost host_;std::map<uint32_t,FieldSparklesInstance>instances_;std::string error_;bool poisoned_=false,had_ready_=false;uint32_t last_ready_=0;
 std::set<uint32_t>dispatching_;
 std::set<uint32_t>created_,borrowed_ready_;FieldPresentRuntime*present_=nullptr;FieldDroppedRuntime*dropped_=nullptr;
 bool fail(const char*);FieldSparklesInstance*get(uint32_t);bool observe(uint32_t,FieldSparklesObservation&);float duration(const FieldSparklesInstance&)const;void reset_timeout(FieldSparklesInstance&);bool publish(FieldSparklesInstance&);bool emit(FieldSparklesInstance&,FieldSparklesSignal);
};
}
