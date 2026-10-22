#pragma once
#include "encore/dropped_items.hpp"

namespace encore::upstream {
// Prompt control remains a separately admitted scene consumer. This observation
// is the real prepared widget's state before a script interaction, not a guess
// from distance or a static icon. Its token changes when the widget is replaced.
struct DroppedPromptObservation {
 uint64_t token=0;bool prepared=false,visible=false,can_show=false;
};
enum class DroppedRequestKind:uint32_t {
 ForcePrompt=1,PressPrompt=2,ReparentPrompt=3,FreePrompt=4,Dialogue=5,FreeItem=6,SetPromptEnabled=7
};
struct DroppedRequest {
 DroppedRequestKind kind=DroppedRequestKind::ForcePrompt;
 uint64_t epoch=0,instance=0,frame=0,token=0;
 uint32_t owner=0;std::string target;BreakablePoint position;
 bool value=false;
};
struct DroppedServices {
 uint64_t epoch=0,frame=0,idle_step=0;
 const DroppedItemsData*data=nullptr;
 BreakableServices*world=nullptr;SessionSnapshot*session=nullptr;
 SourceRandom*random=nullptr;std::vector<uint32_t>*uid_ledger=nullptr;
 LoadRngClockProvider clock;BreakablePoint player;
 bool player_paused=false;
 std::map<uint64_t,uint32_t>owners;
 std::set<uint32_t>item_templates;
 std::set<std::string>dialogues,identity_parents;
 std::map<uint64_t,DroppedPromptObservation>prompts;
 // Static body and interaction Area respectively. Breakable's original child
 // traversal controls the interaction shape; the static-body shape is separate.
 std::map<uint64_t,std::array<uint64_t,2>>shapes;
 bool has_global_item=false;ItemHolderGrantResult global_item;
 std::vector<DroppedRequest>requests;
};
struct DroppedItemPose {
 BreakablePoint scale{1,1};uint32_t sparkle=0,animation=0;
 float rotation_degrees=0,animation_time=0,animation_speed=0;
 double timer_left=0;bool sprite_visible=true,timer_paused=false,timer_running=false;
 bool queued=false,removed=false;
};
// Scope: the original item script and its native child clocks. The scene owner
// supplies the actual breakable content slot, prompt/UID/dialogue services and
// handles renderer/shape updates. No automatic interaction or whole-room claim.
// ready_signal models the inherited _ready idle-frame continuation. Internal
// processing and deferred-free flush are distinct source clock phases.
class DroppedItemActor {
public:
 bool prepare(std::shared_ptr<const DroppedItemsData>,uint32_t owner,uint64_t epoch,
              uint64_t instance,DroppedServices&,std::string&);
 bool ready_signal(uint64_t,DroppedServices&,std::string&);
 bool interact(uint64_t,DroppedServices&,std::string&);
 // Consume the real Breakable drop's remove/add lifecycle exactly once. Parent
 // names alone cannot detect a reentry whose old and new parent are identical.
 bool content_reentered(uint64_t,const BreakableRequest&,const BreakablesData&,DroppedServices&,std::string&);
 bool disappear(uint64_t,DroppedServices&,std::string&);
 bool player_pause(uint64_t,bool,DroppedServices&,std::string&);
 bool advance_idle(uint64_t,double,DroppedServices&,std::string&);
 bool flush_deferred(uint64_t,DroppedServices&,std::string&);
 bool leave(uint64_t,DroppedServices&,std::string&);
 bool valid()const{return valid_;}
 bool waiting_ready()const{return ready_pending_;}
 const DroppedItemPose&pose()const{return pose_;}
 uint64_t instance()const{return instance_;}
private:
 bool context(uint64_t,const DroppedServices&,std::string&,bool retired=false)const;
 BreakableContentState*content(DroppedServices&)const;
 bool set_flag(bool,DroppedServices&,std::string&);
 bool queue_free(DroppedServices&,std::string&);
 bool request(DroppedServices&,DroppedRequestKind,std::string,BreakablePoint,std::string&);
 bool tick(float,DroppedServices&,BreakablePoint&,bool&,std::string&);
 bool sample_blink(float,std::string&);
 std::shared_ptr<const DroppedItemsData>data_;const DroppedOwner*owner_=nullptr;
 DroppedItemPose pose_;bool valid_=false,ready_pending_=false,idle_seen_=false,collecting_=false;
 bool collect_started_=false,pause_connected_=false;
 bool legacy_stopped_=false,reentered_=false;
 bool collect_position_done_=false,collect_scale_done_=false;
 uint64_t epoch_=0,instance_=0,generation_=0,last_idle_=0,prompt_token_=0;
 BreakableServices*world_=nullptr;SessionSnapshot*session_=nullptr;
 SourceRandom*random_=nullptr;std::vector<uint32_t>*ledger_=nullptr;
 BreakableContentState*content_=nullptr;
 float sparkle_left_=0,collect_time_=0,blink_time_=0;
 BreakablePoint collect_start_,collect_target_;
 std::vector<float>reverts_;
 uint32_t disappear_stage_=0;
 std::array<uint64_t,2>shapes_{};
};
}
