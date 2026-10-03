#pragma once
#include "encore/room_data.hpp"
#include <vector>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace encore::upstream {
// Pinned globalData._init_flags data only; no story or whole-game approval.

struct FlagWriteResult { bool applied=false, flags_updated=false; };
class WorldFlags {
    RoomView content_;
    std::vector<bool> story_;
    std::size_t flag_index(std::string_view name) const;
    std::map<std::string,bool,std::less<>> objects_;
public:
    // Upstream initializes every registered story flag false and object flags empty.
    bool initialize(const RoomView& content);
    std::size_t story_flag_count() const { return story_.size(); }
    const RoomView& content() const { return content_; }
    std::string_view story_flag_name(std::size_t index) const;
    bool story_flag(uint32_t index) const;
    FlagWriteResult set_story_flag(uint32_t index,bool value,bool emit=true);
    bool has_story_flag(std::string_view name) const;
    bool story_flag(std::string_view name) const;
    // Unknown story writes do nothing, including no flags_updated signal.
    // Known writes emit when requested even when the value did not change.
    FlagWriteResult set_story_flag(std::string_view name,bool value,bool emit=true);
    bool object_flag(std::string_view name) const;
    FlagWriteResult set_object_flag(std::string_view name,bool value,bool emit=true);
    std::size_t object_flag_count() const { return objects_.size(); }
};

// Empty appear/disappear names impose no condition. Unknown reads are false,
// exactly as Dictionary.get(name,false), not an invented registration error.
bool check_appear_disappear_flags(const WorldFlags& flags,
    std::string_view appear_flag,std::string_view disappear_flag);

// FlaggableObject's fallback identity uses scene name + '/' + leaf node name,
// NOT a full NodePath. is_object_flag applies only with a nonempty explicit flag.
struct FlaggableObjectKey {
    std::string_view flag,scene_name,node_name;
    bool is_object_flag=false;
};
bool flaggable_object_status(const WorldFlags& flags,const FlaggableObjectKey& key);
FlagWriteResult set_flaggable_object_status(WorldFlags& flags,
    const FlaggableObjectKey& key,bool value=true,bool emit=false);
bool reset_flag_on_area_leave(bool reset_when_leaving_area,
    bool reset_when_leaving_region,bool region_changed);

// FlagLandmark queue_free is deferred and permanent for this scene instance.
// A flags update cannot resurrect queued/freed nodes. Hiding alone does not
// disable arbitrary descendant static bodies. The world owns the actual tree.
struct FlagLandmarkState { bool visible=true,queued_for_deletion=false,in_tree=true; };
void update_flag_landmark(FlagLandmarkState& state,const WorldFlags& flags,
    std::string_view appear_flag,std::string_view disappear_flag,bool delete_if_hidden=true);
void flush_flag_landmark_deletion(FlagLandmarkState& state);

// Direct mapping of npc.update_visibility_changed. Caller supplies effective
// tree visibility after flags, parent visibility and native notifier events.
// This does not implement notifier screen rectangles or the entire npc script.
struct NpcVisibilityState {
    bool collision_disabled=false,interaction_disabled=false,physics_processing=true;
};
NpcVisibilityState npc_visibility_state(bool visible_in_tree,bool has_dialog);

// Direct mapping of Openable Door._update_door_state/lock/unlock only.
// The disabled change is DEFERRED. A flag overrides _unlocked/prompt but does
// not override the collision write. Door animation/entry/timer logic is separate.
struct OpenableDoorFlagRule {
    std::string_view key,flag;
    bool blocked=false,locked=false,one_way=false;
};
struct OpenableDoorFlagState {
    bool unlocked=true,deferred_player_shape_disabled=true,prompt_enabled=false;
};
OpenableDoorFlagState openable_door_flag_state(const WorldFlags& flags,
    const OpenableDoorFlagRule& rule);

// Audited fresh-opening body shape enablement AFTER ready/deferred door writes,
// before NPC screen-exit, scripted movement, door entry or story events.
// Exact root-relative physics body paths only. Rejects unknown paths, any
// non-opening story value (visited_podunk may be true), or nonempty object flags.
// Layer/mask filtering remains the collision backend's separate responsibility.
// A false return leaves enabled unchanged; this is not a saved-world evaluator.
bool body_active(uint32_t body_id,const WorldFlags& flags,bool& enabled,const std::vector<bool>*reviewed_mutations=nullptr);
}
