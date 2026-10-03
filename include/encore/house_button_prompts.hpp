#pragma once
#include "encore/house_data.hpp"
#include "encore/phone_data.hpp"
#include <limits>
namespace encore::upstream {
enum class HousePromptKind:uint32_t {Npc=1,Door=2,Phone=3};
struct HousePromptResource {std::string path;uint32_t width=0,height=0;};
struct HousePromptTarget {HousePromptKind kind=HousePromptKind::Npc;uint32_t index=0,category=0;std::string source_path;Vec2 position{},center{},extents{},offset{};};
struct HousePromptPreview {uint32_t resource=0,category=0;Vec2 position{},offset{};};
class HouseButtonPromptData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool validate_bindings(HouseView,PhoneView,std::string&)const;
 bool valid()const{return valid_;}
 Vec2 ray_origin{};float ray_length=0;uint32_t collision_mask=0,color=0;
 BattleValue prompt_rect{};std::vector<uint32_t>choice_masks;std::vector<HousePromptResource>resources;std::vector<HousePromptTarget>targets;std::vector<HousePromptPreview>previews;
private:bool valid_=false;
};
// The caller supplies current source state, not proximity or a guessed target.
// Invisible/replaced NPCs have visible=false. Unsupported current dialogue,
// unported locked-door actions and disabled prompts have supported/enabled=false.
// Unsupported but collidable candidates still occlude later candidates.
struct HousePromptTargetObservation {Vec2 position{};bool visible=false,enabled=false,supported=false;};
struct HousePromptObservation {
 Vec2 player{},direction{};bool paused=true,crouching=false;
 std::vector<HousePromptTargetObservation>targets;
 // Optional nearest collider outside the supported candidate set. Infinity
 // means no additional hit; this is not a full Godot broadphase implementation.
 float occlusion_distance=std::numeric_limits<float>::infinity();
};
struct HousePromptPose {bool visible=false;uint32_t target=house_no_index;Vec2 position{};};
bool evaluate_house_button_prompt(const HouseButtonPromptData&,uint32_t choice,const HousePromptObservation&,HousePromptPose&,std::string&);
}
