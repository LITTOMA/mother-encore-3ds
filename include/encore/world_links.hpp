#pragma once
#include "encore/movement.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// ENCLNK01: reviewed cross-scene door routes. A route only names checked scene
// identities, destination pose and the original Door/SceneTransition values.
enum class WorldLinkSection:uint32_t {Bytes=1,Scenes,Routes};
constexpr uint32_t kWorldLinkSectionCount=3;
enum class WorldTransitionKind:uint16_t {Fade=0,CircleFocus=1,CirclePop=2};
enum class WorldRouteFlag:uint32_t {FadeMusic=1};
struct WorldScene {uint32_t id=0;std::string_view source;uint32_t room_role=0,map_role=0;};
struct WorldRoute {
    uint32_t id=0,from=0,to=0;std::string_view door;Vec2 destination{},direction{};
    std::string_view sound,end_sound;uint16_t in_kind=0,out_kind=0;float in_speed=0,out_speed=0,music_fade=0;
    std::string_view flag;bool flag_value=false;uint32_t flags=0;
};
class WorldLinksData {
public:
    WorldLinksData()=default;
    WorldLinksData(const WorldLinksData&)=delete;
    WorldLinksData& operator=(const WorldLinksData&)=delete;
    bool load(const uint8_t*,size_t,std::string&);
    bool load_file(const char*,std::string&);
    bool valid()const{return !bytes_.empty();}
    uint32_t scene_count()const;
    uint32_t route_count()const;
    WorldScene scene(uint32_t index)const;
    WorldRoute route(uint32_t index)const;
    // kNotFound when absent. Stable IDs, not indexes, cross resource boundaries.
    static constexpr uint32_t kNotFound=0xffffffffu;
    uint32_t find_scene(uint32_t id)const;
    uint32_t find_route(uint32_t id)const;
    uint32_t find_route_from(uint32_t scene,std::string_view door)const;
private:
    std::vector<uint8_t> bytes_;
};
}
