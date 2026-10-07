#pragma once
#include "encore/animation.hpp"
#include "encore/movement.hpp"
#include "encore/collision.hpp"
#include "encore/world_flags.hpp"
#include "encore/dialogue.hpp"
#include "encore/actor_actions.hpp"
#include "encore/persistent_player.hpp"
#include "encore/room_data.hpp"
#include "encore/source_random.hpp"
#include "encore/scene_motion.hpp"
#include "encore/basement_progression.hpp"
#include "encore/basement_actor_assets.hpp"
#include <vector>
namespace encore::upstream {
enum class OpeningStage : uint8_t { Walking, ScriptRunning, BattleRequested, Error };
enum class AudioRequestKind : uint8_t { FadeMusic, PlayMusic, PlayEffect,PlayDialogueMusic,StopMusicResource,FadeInMusic };
enum class DialogueTalkerKind : uint8_t {None,Actor,OriginalNpc};
struct DialogueTalker {DialogueTalkerKind kind=DialogueTalkerKind::None;uint32_t index=kRoomNoIndex;};
struct OpeningEffectRequest {bool appear=false;uint32_t resource_index=kRoomNoIndex;Vec2 center{};uint64_t actor_mask=0;uint32_t npc_index=kRoomNoIndex;};
struct OpeningAudioRequest { AudioRequestKind kind; uint32_t resource_index; double duration; uint32_t phrase;float gain_db=0; };
struct OpeningBattleRequest {
    bool queued=false,requested=false,can_run=false,overworld_music=false;
    int32_t advantage=0;
    uint32_t record_index=kRoomNoIndex,actor_index=kRoomNoIndex,win_flag_index=kRoomNoIndex;
    uint32_t battle_resource_index=kRoomNoIndex,win_cutscene_string=kRoomNoIndex;
    bool keep_actor_after_battle=false;
    const char* enemy="";const char* win_flag="";
};
struct OpeningMusicRegionCall {bool play=false;uint32_t node_string=kRoomNoIndex;double fadeout_seconds=0;};
using OpeningMusicRegionValidator=std::function<bool(std::string_view,std::string_view,bool,double,std::string&)>;
struct OpeningSceneCall {uint32_t object=0;};
struct OpeningTraceEvent { DialogueAction action; uint64_t physics_tick,idle_frame; };
// Borrowed Room source plus copies of the last committed collision state.
// Pending flag deletion is excluded until end_scene_frame commits it.
struct OpeningCollisionSnapshot {
    RoomView source;
    std::vector<uint32_t> active_polygons,erased_bodies;
    std::vector<Vec2> polygon_offsets;
};
class OpeningWorld;
// A fixed-address external source owner, borrowed rather than owned by World.
// Concrete targets inspect the actual Room/native UI/source callback owners.
// Keep it alive until the exact-owner checked unbind succeeds (or World dies).
class OpeningHouseProgrammeOwner {
public:
    virtual ~OpeningHouseProgrammeOwner()=default;
    virtual const OpeningWorld* world()const=0;
    // Admission must inspect closed source sessions/callbacks/waits. A function
    // object's presence or a scheduling wrapper is not a source-frame proof.
    virtual bool source_frame_closed(const OpeningWorld&,std::string&)const=0;
    virtual bool source_started(OpeningWorld&,uint32_t generation,std::string&)=0;
    virtual bool before_action(OpeningWorld&,const DialogueAction&,std::string&)=0;
};
class OpeningWorld final : private DialogueSink {
public:
    OpeningWorld();
    // Explicit empty owner injection is also checked by initialize; it is never
    // replaced with an implicit player. New Game / LOAD use the default path.
    explicit OpeningWorld(PersistentPlayerOwner owner);
    OpeningWorld(RetainPersistentPlayer,const OpeningWorld& active);
    OpeningWorld(const OpeningWorld&)=delete;
    OpeningWorld& operator=(const OpeningWorld&)=delete;
    OpeningWorld(OpeningWorld&&)=delete;
    OpeningWorld& operator=(OpeningWorld&&)=delete;
    const PersistentPlayerState* persistent_player()const{return persistent_player_.get();}
    PersistentPlayerOwner retain_player()const{return persistent_player_;}
    bool retains_existing_player()const{return player_read_only_;}
    bool initialize(const RoomView& content,Vec2 viewport={400,240});
    // Bind before initialization. The SceneHost owns all referenced data and
    // backend lifetimes. Initialization cross-checks source pin and scene.
    bool bind_basement(const BasementProgressionData&,const BasementActorData&,BasementProgressionHost,std::string&);
    bool bind_music_region_validator(OpeningMusicRegionValidator,std::string&);
    std::vector<OpeningMusicRegionCall> take_music_region_calls(){auto out=std::move(music_region_calls_);music_region_calls_.clear();return out;}
    bool bind_scene_motion(const SceneMotionBackend&,std::string&);
    const BasementActorResource*special_actor_resource(uint32_t index)const;
    uint32_t special_actor_frame(uint32_t index)const;
    BattleValue basement_white_fade()const{return white_fade_active_&&basement_.bound()?basement_.data()->white_fade(white_fade_elapsed_):BattleValue{};}
    // Construct on a fresh owner; the caller commits that owner only after all initialization succeeds.
    bool initialize_restored(const RoomView&,const std::vector<bool>& story_flags,const std::vector<bool>&reviewed_mutations,Vec2 position,Vec2 direction,Vec2 viewport={400,240});
    bool accept_battle_entry();
    bool begin_house_program(uint32_t program_index,uint32_t original_npc=kRoomNoIndex);
    // Optional explicit integration with the SAME existing programme VM.
    // Binding/checked unbinding do not initialize, grant Ready or tick anything.
    bool bind_house_programme_owner(OpeningHouseProgrammeOwner&,std::string&);
    bool unbind_house_programme_owner(OpeningHouseProgrammeOwner&,std::string&);
    const OpeningHouseProgrammeOwner* house_programme_owner()const{return house_programme_owner_;}
    // The real World counter, including initialized-but-unstarted VM state.
    uint32_t source_generation()const{return generation_;}
    bool begin_battle_continuation(std::string_view source_path);
    bool story_completed()const{return dialogue_.status()==DialogueStatus::Completed;}
    DialogueStatus story_status()const{return dialogue_.status();}
    bool finish_story_dialogue(bool automatic=false);
    bool set_party_leader(std::string_view identity){if(!persistent_player_||player_read_only_)return false;persistent_player_.state_->party_leader_=std::string(identity);return true;}
    uint32_t story_program_index()const{return program_index_;}
    uint32_t story_next_command_index()const{return dialogue_.next_command_index();}
    uint32_t story_generation()const{return dialogue_.generation();}
    uint32_t pending_choice_group()const{return choice_group_;}
    bool story_choices_waiting()const{return dialogue_.status()==DialogueStatus::AwaitChoices;}
    bool story_submenu_waiting()const{return dialogue_.status()==DialogueStatus::AwaitSubmenu;}
    bool take_storage_request(){const bool value=storage_requested_;storage_requested_=false;return value;}
    bool take_save_request(){const bool value=save_requested_;save_requested_=false;return value;}
    bool choose_story_option(uint32_t pc,uint32_t generation);
    bool close_story_submenu(uint32_t generation);
    bool set_story_talking(bool talking);
    bool request_sound(uint32_t resource_index);
    bool refresh_scene_rules();
    bool end_scene_frame();
    std::vector<OpeningSceneCall> take_scene_calls(){auto out=std::move(scene_calls_);scene_calls_.clear();return out;}
    uint32_t pending_dialogue_id()const{return pending_dialogue_id_;}
    uint32_t dialogue_talker()const{return talker_;}
    DialogueTalker story_talker()const{return story_talker_;}
    bool story_has_next_phrase()const{return dialogue_.has_next_phrase();}
    bool story_input_allowed()const{return dialogue_.input_allowed();}
    bool story_auto_advance_ready()const{return dialogue_.auto_advance_ready();}
    std::vector<bool> take_story_hides(){auto result=std::move(story_hides_);story_hides_.clear();return result;}
    const std::vector<OpeningEffectRequest>& effect_requests()const{return effects_;}
    bool pause_for_house();
    bool unpause_from_house();
    bool set_house_direction(Vec2 direction);
    bool warp_same_scene(Vec2 position,Vec2 direction);
    bool house_paused()const{return persistent_player_&&persistent_player_.state_->house_paused_;}
    void attach_random(SourceRandom& random){random_=&random;}
    bool erase_battle_actor(uint32_t body_id);
    bool set_body_enabled(uint32_t body_id,bool enabled);
    bool body_enabled(uint32_t body_id)const;
    bool body_visible(uint32_t body_id)const;
    bool set_body_offset(uint32_t body_id,Vec2 delta);
    bool committed_collision_snapshot(const RoomView& expected,OpeningCollisionSnapshot&,std::string&)const;
    bool set_initial_actor_pose(uint32_t index,Vec2 position,Vec2 direction);
    OpenableDoorFlagState door_flag_state(const OpenableDoorFlagRule& rule)const{return openable_door_flag_state(flags_,rule);}
    bool shake_house_camera(double magnitude,double duration,Vec2 direction){return healthy_&&persistent_player_.state_->camera_.shake(magnitude,duration,direction);}
    bool set_battle_story_flag();
    bool set_story_flag(std::string_view name,bool value,bool emit);
    bool story_flag(std::string_view name)const{return flags_.story_flag(name);}
    bool resume_battle_camera();
    bool stop_area_music();
    bool start_area_music(uint32_t resource,double gain_db,double fadein_seconds);
    uint32_t area_music_resource()const{return area_music_resource_;}
    bool start_battle_return_camera(Vec2 local_target,double duration);
    bool land_battle_player(Vec2 direction,uint16_t frame);
    bool rotate_battle_player(double interval);
    bool finish_battle_return();
    bool battle_player_visible()const{return persistent_player_&&persistent_player_.state_->return_player_visible_;}
    bool advance(WalkInput input);
    bool idle_frame(double delta);
    const RoomView& content() const { return content_; }
    const WalkState& player() const { static const WalkState unavailable{};return persistent_player_?persistent_player_.state_->player_:unavailable; }
    AnimationSample animation() const { return persistent_player_?persistent_player_.state_->playback_.sample:AnimationSample{0,true,false}; }
    bool healthy() const { return healthy_; }
    const char* error() const { return error_; }
    OpeningStage stage() const { return stage_; }
    bool cutscene_active() const { return stage_==OpeningStage::ScriptRunning; }
    bool has_cutscene_actors() const;
    uint32_t actor_count() const { return uint32_t(actors_.size()); }
    const ActorActionState& actor(uint32_t index) const { return actors_[index]; }
    bool actor_restore_requested(uint32_t index)const{return index<actor_restore_.size()&&actor_restore_[index];}
    bool complete_actor_restore(uint32_t index);
    bool actor_bound(uint32_t index) const { return index<actor_bound_.size()&&actor_bound_[index]; }
    bool instance_visible(uint32_t index) const { return initialized_&&index<actor_visible_.size()&&actor_visible_[index]; }
    // Camera observation requires persistent_player()!=nullptr (normally healthy()).
    const CutsceneCamera& cutscene_camera() const { return persistent_player_.state_->camera_; }
    bool actor_cleanup_pending() const { return restore_pending_; }
    uint32_t phrase() const { return dialogue_.phrase(); }
    const OpeningBattleRequest& battle_request() const { return battle_; }
    size_t audio_request_count() const { return audio_.size(); }
    const std::vector<OpeningAudioRequest>& audio_requests() const { return audio_; }
    const std::vector<OpeningTraceEvent>& action_trace() const { return trace_; }
private:
    OpeningHouseProgrammeOwner*house_programme_owner_=nullptr;
    bool house_programme_callback_=false,house_programme_failed_=false;
    std::string house_programme_error_;
    bool house_programme_failure(const char*);
    PersistentPlayerOwner persistent_player_;
    bool player_read_only_=false;
    uint32_t area_music_resource_=kRoomNoIndex;
    DialogueTalker story_talker_;
    std::vector<OpeningEffectRequest> effects_;
    std::vector<bool> story_hides_;
    std::vector<OpeningSceneCall>scene_calls_;
    OpeningMusicRegionValidator music_region_validator_;
    std::vector<OpeningMusicRegionCall>music_region_calls_;
    struct FlaggedBody {uint32_t binding=0;FlagLandmarkState state;};
    std::vector<FlaggedBody>flagged_bodies_;
    bool initialize_state(const RoomView&,Vec2 viewport,const std::vector<bool>*story_flags,const std::vector<bool>*reviewed_mutations,Vec2 position,Vec2 direction);
    bool write_story_flag(uint32_t index,bool value,bool emit=true);
    std::vector<Vec2> polygon_offsets_;
    std::vector<uint32_t> erased_bodies_;
    bool apply(const DialogueAction& action) override;
    bool branch_condition(const DialogueAction&,bool&) override;
    uint32_t choice_group_=kRoomNoIndex;bool save_requested_=false,storage_requested_=false;
    bool fail(const char* message);
    bool flush_deferred();
    bool process_room_shakers(double delta);
    bool vibrate_room(uint32_t binding);
    uint32_t walk_clip(MotionAnimation animation,Vec2 direction) const;
    bool trigger_conditions(uint32_t index) const;
    RoomView content_;
    Vec2 viewport_{400,240};double last_idle_delta_=0;bool battle_accepted_=false;
    StaticMotionSolver solver_;
    const SceneMotionBackend*scene_motion_=nullptr;std::string motion_scene_,motion_commit_,host_error_;
    BasementProgressionConsumer basement_;const BasementActorData*basement_actors_=nullptr;
    std::vector<BasementActorPlayback>special_actors_;
    bool white_fade_active_=false;double white_fade_elapsed_=0;
    std::vector<uint32_t> active_polygons_;
    WorldFlags flags_;
    DialoguePlayer dialogue_;
    std::vector<ActorActionState> actors_;
    OpeningBattleRequest battle_;
    std::vector<OpeningAudioRequest> audio_;
    std::vector<OpeningTraceEvent> trace_;
    std::vector<DialogueAction> deferred_;
    std::vector<uint8_t> actor_bound_,actor_persistent_,actor_visible_,actor_restore_,inside_trigger_,trigger_pending_;
    Vec2 battle_original_direction_{};
    struct Boundary { uint32_t binding;double remaining; };
    std::vector<Boundary> boundaries_;
    struct RoomShaker {uint32_t binding=0;double remaining=0,wait_time=0;bool delayed=true;};
    std::vector<RoomShaker> room_shakers_;SourceRandom*random_=nullptr;
    DialogueActor pending_actor_=kRoomNoActor,talker_=kRoomNoActor;
    OpeningStage stage_=OpeningStage::Walking;
    uint64_t physics_tick_=0,idle_frame_=0;
    uint32_t generation_=0,program_index_=kRoomNoIndex,follow_actor_=kRoomNoIndex;
    bool healthy_=false,initialized_=false,cutscene_done_=false,restore_pending_=false,overworld_music_=false;
    uint32_t pending_dialogue_id_=kRoomNoIndex;
    const char* error_="World not initialized";
};
}
