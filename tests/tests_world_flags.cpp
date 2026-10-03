#include "room_fixture.hpp"
#include "encore/world_flags.hpp"
#include "fixtures/world_flags_v0410.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(e) do{++checks;if(!(e)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);std::exit(1);}}while(0)
static bool opening_body_active(std::string_view path,const WorldFlags& flags,bool& enabled) {
 const auto room=encore_test::room();for(uint32_t i=0;i<room.body_rule_count();++i){const auto rule=room.body_rule(i);if(room.string(rule.source_path_string)==path)return body_active(rule.body_id,flags,enabled);}return false;
}
int main() {
    WorldFlags flags;CHECK(flags.initialize(encore_test::room()));
    const auto upstream_story_flag_count=encore_test::room().flag_count();CHECK(upstream_story_flag_count==sizeof(world_flags_reference::names)/sizeof(const char*));
    for(std::size_t i=0;i<upstream_story_flag_count;++i) {
        const auto name=flags.story_flag_name(i);
        CHECK(name==world_flags_reference::names[i]);CHECK(flags.has_story_flag(name));CHECK(!flags.story_flag(name));
        CHECK(flags.set_story_flag(name,true).flags_updated);CHECK(flags.story_flag(name));
        CHECK(flags.set_story_flag(name,false,false).applied);CHECK(!flags.story_flag(name));
    }
    CHECK(flags.story_flag_name(upstream_story_flag_count).empty());
    CHECK(flags.story_flag_name(static_cast<std::size_t>(-1)).empty());
    CHECK(flags.object_flag_count()==0);
    for(const auto& sample:world_flags_reference::appearance) {
        flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));
        flags.set_story_flag("poltergeist",(sample.bits&1)!=0);
        flags.set_story_flag("doll_melody",(sample.bits&2)!=0);
        CHECK(check_appear_disappear_flags(flags,sample.appear,sample.disappear)==sample.expected);
    }
    for(const auto& sample:world_flags_reference::writes) {
        flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));
        const auto result=sample.object?flags.set_object_flag(sample.name,sample.value,sample.emit):flags.set_story_flag(sample.name,sample.value,sample.emit);
        CHECK(result.applied==sample.present);CHECK(result.flags_updated==(sample.signals!=0));
        CHECK((sample.object?flags.object_flag(sample.name):flags.story_flag(sample.name))==sample.result);
        if(!sample.object)CHECK(flags.has_story_flag(sample.name)==sample.present);
    }
    for(const auto& sample:world_flags_reference::doors) {
        flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));flags.set_story_flag("poltergeist",sample.flag_value);
        OpenableDoorFlagRule rule;
        rule.key=(sample.blocker&1)?"key":"";rule.blocked=(sample.blocker&2)!=0;
        rule.one_way=(sample.blocker&4)!=0;rule.locked=(sample.blocker&8)!=0;rule.flag=sample.flag;
        const auto result=openable_door_flag_state(flags,rule);
        CHECK(result.unlocked==sample.unlocked);CHECK(result.deferred_player_shape_disabled==sample.disabled);CHECK(result.prompt_enabled==sample.prompt);
    }
    for(const auto& sample:world_flags_reference::npcs) {
        const auto result=npc_visibility_state(sample.parent_shown&&sample.shown,sample.dialogue);
        CHECK(result.collision_disabled==sample.collision_disabled);CHECK(result.interaction_disabled==sample.interaction_disabled);CHECK(result.physics_processing==sample.processing);
    }
    for(const auto& sample:world_flags_reference::landmarks) {
        flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));FlagLandmarkState state;
        update_flag_landmark(state,flags,"","poltergeist",sample.delete_hidden);
        flags.set_story_flag("poltergeist",true);
        update_flag_landmark(state,flags,"","poltergeist",sample.delete_hidden);
        CHECK(state.visible==sample.hidden_visible);CHECK(state.queued_for_deletion==sample.hidden_queued);CHECK(state.in_tree);
        flags.set_story_flag("poltergeist",false);
        update_flag_landmark(state,flags,"","poltergeist",sample.delete_hidden);
        CHECK(state.visible==sample.restored_visible);CHECK(state.queued_for_deletion==sample.restored_queued);
        flush_flag_landmark_deletion(state);CHECK(state.in_tree==sample.remains);
        update_flag_landmark(state,flags,"","",sample.delete_hidden);CHECK(state.in_tree==sample.remains);
    }
    for(const auto& sample:world_flags_reference::objects) {
        flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));
        const FlaggableObjectKey key{sample.flag,"Ninten's House","Present2",sample.object};
        CHECK(!flaggable_object_status(flags,key));
        const auto result=set_flaggable_object_status(flags,key,true,sample.emit);
        CHECK(flaggable_object_status(flags,key)==sample.status);CHECK(result.flags_updated==(sample.signals!=0));
        if(std::string(sample.object_key).empty())CHECK(flags.object_flag_count()==0);
        else {CHECK(flags.object_flag_count()==1);CHECK(flags.object_flag(sample.object_key));}
    }
    for(unsigned bits=0;bits<8;++bits)CHECK(reset_flag_on_area_leave(bits&1,bits&2,bits&4)==bool((bits&1)||((bits&2)&&(bits&4))));
    flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));CHECK(flags.set_story_flag("visited_podunk",true).applied);
    bool enabled=false;
    for(const auto* path:{"Collisions","Objects/npc","Objects/npc2","Objects/npc3","Objects/pillow","Objects/lamp","Objects/npcdoll","Objects/Phone/StaticBody2D","Objects/Present1/StaticBody2D","Objects/Present2/StaticBody2D","Objects/Present3/StaticBody2D","Objects/Present4/StaticBody2D","DoorBlock/Entrance"}) {
        CHECK(opening_body_active(path,flags,enabled));CHECK(enabled);
    }
    for(unsigned door=1;door<=4;++door) {
        const std::string base="Below/Openable Door"+(door==1?std::string{}:std::to_string(door));
        CHECK(opening_body_active(base+"/StaticBody2D",flags,enabled));CHECK(enabled==(door==2||door==3));
        CHECK(opening_body_active(base+"/NonPlayerStaticBody2D",flags,enabled));CHECK(enabled);
    }
    for(const auto* path:{"","Objects/npc4","Objects/lamp/CollisionShape2D","./Collisions","Ninten's House/Collisions","Objects/Present5/StaticBody2D","Below/Openable Door5/StaticBody2D"}) {
        enabled=true;CHECK(!opening_body_active(path,flags,enabled));CHECK(enabled);
    }
    for(std::size_t i=0;i<upstream_story_flag_count;++i) {
        flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));const auto name=flags.story_flag_name(i);flags.set_story_flag(name,true);
        enabled=false;CHECK(opening_body_active("Collisions",flags,enabled)==(name=="visited_podunk"));CHECK(enabled==(name=="visited_podunk"));
    }
    flags=WorldFlags{};CHECK(flags.initialize(encore_test::room()));flags.set_object_flag("Ninten's House/Present2",false);
    enabled=true;CHECK(!opening_body_active("Collisions",flags,enabled));CHECK(enabled);
    // Empty/unknown names preserve original dictionary semantics, not M0 indexes.
    CHECK(!flags.set_story_flag("missing",true).applied);CHECK(!flags.story_flag("missing"));
    CHECK(flags.set_object_flag("",true).applied);CHECK(flags.object_flag(""));
    CHECK((4353U&1596U)==0);CHECK((4353U&1597U)==1);
    std::printf("Upstream world flags: %u checks; Godot3.6.2 registry178, appearance64, writes24, doors96, NPC8, landmarks2, objects12\n",checks);
}
