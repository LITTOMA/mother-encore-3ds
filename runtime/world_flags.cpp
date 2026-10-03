#include "encore/world_flags.hpp"
#include <algorithm>

namespace encore::upstream {
namespace {
std::string object_key(const FlaggableObjectKey& key) {
    std::string result(key.scene_name);result+='/';result+=key.node_name;return result;
}
}
bool WorldFlags::initialize(const RoomView& content) {
    if(!content.valid())return false;
    content_=content;story_.assign(content.flag_count(),false);objects_.clear();
    for(uint32_t i=0;i<content.flag_count();++i)story_[i]=content.flag(i).default_value;
    return true;
}
std::size_t WorldFlags::flag_index(std::string_view name) const {
    for(uint32_t i=0;i<content_.flag_count();++i)if(content_.string(content_.flag(i).name_string)==name)return i;
    return story_.size();
}
std::string_view WorldFlags::story_flag_name(std::size_t index) const {
    return index<story_.size()?content_.string(content_.flag(uint32_t(index)).name_string):std::string_view{};
}
bool WorldFlags::has_story_flag(std::string_view name) const {return flag_index(name)<story_.size();}
bool WorldFlags::story_flag(std::string_view name) const {const auto index=flag_index(name);return index<story_.size()?story_[index]:false;}
bool WorldFlags::story_flag(uint32_t index) const {return index<story_.size()?story_[index]:false;}
FlagWriteResult WorldFlags::set_story_flag(std::string_view name,bool value,bool emit) {
    const auto index=flag_index(name);if(index>=story_.size())return {};
    story_[index]=value;return {true,emit};
}
FlagWriteResult WorldFlags::set_story_flag(uint32_t index,bool value,bool emit) {
    if(index>=story_.size())return {};
    story_[index]=value;return {true,emit};
}
bool WorldFlags::object_flag(std::string_view name) const {
    const auto found=objects_.find(name);return found==objects_.end()?false:found->second;
}
FlagWriteResult WorldFlags::set_object_flag(std::string_view name,bool value,bool emit) {
    objects_[std::string(name)]=value;return {true,emit};
}
bool check_appear_disappear_flags(const WorldFlags& flags,std::string_view appear,std::string_view disappear) {
    return (appear.empty()||flags.story_flag(appear))&&(disappear.empty()||!flags.story_flag(disappear));
}
bool flaggable_object_status(const WorldFlags& flags,const FlaggableObjectKey& key) {
    if(key.flag.empty())return flags.object_flag(object_key(key));
    return key.is_object_flag?flags.object_flag(key.flag):flags.story_flag(key.flag);
}
FlagWriteResult set_flaggable_object_status(WorldFlags& flags,const FlaggableObjectKey& key,bool value,bool emit) {
    if(key.flag.empty())return flags.set_object_flag(object_key(key),value,emit);
    return key.is_object_flag?flags.set_object_flag(key.flag,value,emit):flags.set_story_flag(key.flag,value,emit);
}
bool reset_flag_on_area_leave(bool area,bool region,bool region_changed) {
    return area||(region_changed&&region);
}
void update_flag_landmark(FlagLandmarkState& state,const WorldFlags& flags,
    std::string_view appear,std::string_view disappear,bool delete_if_hidden) {
    if(!state.in_tree)return;
    const bool shown=check_appear_disappear_flags(flags,appear,disappear);
    if(delete_if_hidden&&!shown)state.queued_for_deletion=true;
    else state.visible=shown;
}
void flush_flag_landmark_deletion(FlagLandmarkState& state) {
    if(state.queued_for_deletion)state.in_tree=false;
}
NpcVisibilityState npc_visibility_state(bool visible_in_tree,bool has_dialog) {
    return {!visible_in_tree,!visible_in_tree||!has_dialog,visible_in_tree};
}
OpenableDoorFlagState openable_door_flag_state(const WorldFlags& flags,const OpenableDoorFlagRule& rule) {
    const bool locked=!rule.key.empty()||rule.blocked||rule.one_way||rule.locked;
    OpenableDoorFlagState state{!locked,!locked,locked};
    if(!rule.flag.empty()&&flags.has_story_flag(rule.flag)) {
        state.unlocked=flags.story_flag(rule.flag);state.prompt_enabled=!state.unlocked;
    }
    return state;
}

bool body_active(uint32_t body_id,const WorldFlags& flags,bool& enabled,const std::vector<bool>*reviewed_mutations) {
    const auto& content=flags.content();if(!content.valid()||flags.object_flag_count()!=0)return false;
    if(reviewed_mutations&&reviewed_mutations->size()!=content.flag_count())return false;
    for(uint32_t i=0;i<content.flag_count();++i){const auto flag=content.flag(i);if(!(flag.flags&1)&&!(reviewed_mutations&&(*reviewed_mutations)[i])&&flags.story_flag(i)!=flag.default_value)return false;}
    const auto scene=content.scene();
    for(uint32_t i=0;i<scene.body_rule_count;++i){const auto rule=content.body_rule(scene.body_rule_first+i);if(rule.body_id==body_id){enabled=rule.initially_enabled;return true;}}
    return false;
}
}
