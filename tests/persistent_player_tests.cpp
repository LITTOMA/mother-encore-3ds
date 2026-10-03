#include "encore/fresh_house.hpp"
#include "room_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <type_traits>

using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"Persistent player: %s\n",label);std::exit(1);}}
bool same(Vec2 a,Vec2 b){return std::memcmp(&a,&b,sizeof a)==0;}
}
static_assert(!std::is_move_constructible_v<FreshHouseState>);
static_assert(!std::is_copy_constructible_v<FreshHouseState>);
static_assert(!std::is_move_constructible_v<OpeningWorld>);
static_assert(!std::is_move_constructible_v<PersistentPlayerState>);

int main(){
    const auto room=encore_test::room();
    // Exercise the actual default FreshHouseState route used by CTR New Game
    // and prepare_fresh_house, rather than a separate unused owner fixture.
    auto active=std::make_unique<FreshHouseState>();
    check(active->world.initialize(room),active->world.error());
    auto& world=active->world;
    check(world.set_party_leader("ninten"),"active player identity write");
    const auto* identity=world.persistent_player();
    const auto* walk=&world.player();const auto* camera=&world.cutscene_camera();
    check(identity&&walk==&identity->player()&&camera==&identity->camera(),"actual world borrows owner fields");
    check(world.warp_same_scene({520,397},{1,0}),"existing house warp");
    for(unsigned i=0;i<7;++i)check(world.advance({1,0}),"existing house movement");
    check(world.shake_house_camera(2,4,{1,0}),"camera keeps source shaker state");
    check(world.pause_for_house(),"source house pause");
    const auto pose=world.player();const auto sample=world.animation();
    const auto center=world.cutscene_camera().center();

    auto staged=std::make_unique<FreshHouseState>(RetainPersistentPlayer{},world);
    check(staged->world.persistent_player()==identity,"staged scene retains exact owner");
    check(&staged->world.player()==walk&&&staged->world.cutscene_camera()==camera,"staged player and camera identity");
    check(staged->world.retains_existing_player()&&!staged->world.healthy(),"staged state is not an active scene");
    check(!staged->world.initialize(room),"staged New Game initialization rejected before writes");
    std::vector<bool> flags(room.flag_count(),false),mutations(room.flag_count(),false);
    check(!staged->world.initialize_restored(room,flags,mutations,{136,809},{0,-1}),"staged LOAD initialization rejected before writes");
    check(!staged->world.set_party_leader("replacement"),"staged identity mutation rejected");
    check(!staged->world.advance({-1,0})&&!staged->world.idle_frame(1.0/60.0),"staged callbacks cannot advance shared state");
    check(!staged->world.pause_for_house()&&!staged->world.unpause_from_house(),"staged pause mutation rejected");
    check(!staged->world.warp_same_scene({8,-39},{0,1}),"staged pose mutation rejected");
    check(!staged->world.shake_house_camera(1,1,{1,0}),"staged camera mutation rejected");
    check(same(world.player().position,pose.position)&&same(world.player().velocity,pose.velocity)&&same(world.player().direction,pose.direction),"failed prepare preserves motion exactly");
    check(world.animation().frame==sample.frame&&same(world.cutscene_camera().center(),center)&&world.cutscene_camera().is_shaking(),"failed prepare preserves animation and camera");
    check(world.house_paused()&&identity->party_leader()=="ninten", "failed prepare preserves pause and identity");
    staged.reset();
    check(world.persistent_player()==identity&&world.cutscene_camera().is_shaking(),"candidate destruction leaves live player intact");
    SourceRandom random(123);std::string error;const auto* active_scene=active.get();
    check(!prepare_fresh_house(PreparedSessionRestore{},RestoreData{},room,HouseView{},BattleView{},PhoneView{},random,{400,240},active,error),"checked fresh scene preparation rejects unavailable content");
    check(active.get()==active_scene&&active->world.persistent_player()==identity&&&active->world.player()==walk&&&active->world.cutscene_camera()==camera,"prepare failure preserves exact scene/player/camera owners");

    // unique_ptr swapping moves scene ownership, never the pointer-bearing state.
    auto next=std::make_unique<FreshHouseState>(RetainPersistentPlayer{},world);
    auto retained=world.retain_player();
    active.swap(next);
    check(active->world.persistent_player()==identity&&next->world.persistent_player()==identity,"owner swap keeps both bindings stable");
    next.reset();
    check(active->world.persistent_player()==identity&&&active->world.player()==walk&&&active->world.cutscene_camera()==camera,"old scene destruction preserves retained player/camera");
    check(same(active->world.player().position,pose.position)&&active->world.cutscene_camera().is_shaking(),"retained state remains readable after old scene destruction");
    active.reset();
    check(retained.get()==identity&&same(retained.get()->player().position,pose.position),"external owner outlives both scenes");

    // A sole remaining strong reference still cannot turn a retained player
    // into a New Game initialization target by moving the handle.
    OpeningWorld rebound(std::move(retained));
    check(rebound.retains_existing_player()&&!rebound.initialize(room),"retained identity cannot be reset after old scene deletion");
    check(rebound.persistent_player()==identity&&same(rebound.player().position,pose.position),"rejected reset leaves retained player untouched");
    auto fresh=std::make_unique<FreshHouseState>();
    check(fresh->world.initialize(room),fresh->world.error());
    check(fresh->world.persistent_player()!=identity&&&fresh->world.cutscene_camera()!=camera,"New Game builds an independent owner");
    check(!fresh->world.house_paused()&&!fresh->world.cutscene_camera().is_shaking(),"fresh session clears prior scene transients");

    // The no-allocation failure result follows the same checked initialization
    // path as a failed nothrow allocation, and never fabricates a fallback owner.
    OpeningWorld unavailable(PersistentPlayerOwner{});
    check(!unavailable.initialize(room)&&!unavailable.persistent_player(),"missing allocation rejected");
    check(std::strstr(unavailable.error(),"allocation failed")!=nullptr,"allocation error can be reported");
    check(!unavailable.set_party_leader("ninten")&&!unavailable.advance({}),"missing owner cannot mutate or step");
    check(!unavailable.house_paused()&&!unavailable.battle_player_visible(),"allocation failure status observers are safe");
    std::printf("Persistent player owner: %u focused checks passed; current-house path and staged lifetime, no cross-scene activation claim.\n",checks);
}
