#pragma once
#include "encore/poison_hazards.hpp"
#include "encore/geyser_timeline.hpp"
#include <optional>
namespace encore::upstream {
// ENCGLF01 format1/capability1 binds the original source scene, factory and
// deferred parent to two unchanged independently versioned metadata resources.
// It contains no textures, PCM, save changes or full-scene admission.
class GeyserLifecycleData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}
 const PoisonHazardsData&poison()const;
 const GeyserTimelineData&geyser()const;
 const std::string&parent_path()const;
 bool same_owner(const GeyserLifecycleData&other)const{return valid()&&state_==other.state_;}
private:struct State;std::shared_ptr<const State>state_;
};
struct GeyserInstanceSnapshot {
 PoisonSpawnRequest source;
 // No AnimationPlayer Ready has executed for a pending deferred addition.
 std::optional<GeyserSnapshot>pose;
};
struct GeyserInstanceCommand {uint64_t epoch=0,instance=0;GeyserCommand command;};
struct GeyserDeferredContext {uint64_t instance=0;GeyserContext context;};
// Selected source lifetime phases, not a physics sampler or complete SceneTree.
// Drive Timers and existing actors' internal animation in audited tree order,
// then script UI; flush additions then methods, drain commands, and explicitly
// retire queued actors. New additions miss earlier notifications by design.
class GeyserLifecycle {
public:
 bool prepare(const GeyserLifecycleData&,uint64_t epoch,std::string&);
 bool receive_spawns(uint64_t,PoisonHoleTimers&,std::string&);
 bool player_signal(uint64_t,PoisonHoleTimers&,bool paused,std::string&);
 bool tree_notification(uint64_t,bool paused,bool audio_can_process,std::string&);
 // Native internal-animation and script-process groups are distinct. A Timer
 // callback can pause the tree between them. The caller follows audited tree
 // order; never reuse a later pause value for an already executed group.
 bool advance_animations_idle(uint64_t,double,bool process_enabled,std::string&);
 bool process_ui(uint64_t,bool process_enabled,GeyserContext,std::string&);
 bool advance_idle(uint64_t,double,bool process_enabled,GeyserContext,std::string&);
 bool flush_additions(uint64_t,std::string&);
 bool flush_methods(uint64_t,const std::vector<GeyserDeferredContext>&,GeyserNamedVoice&,std::string&);
 bool take_commands(uint64_t,std::vector<GeyserInstanceCommand>&,std::string&);
 bool retire_queued(uint64_t,std::vector<PoisonSpawnRequest>&,std::string&);
 bool retire_scene(uint64_t,std::string&);
 bool inspect(uint64_t,uint64_t instance,GeyserInstanceSnapshot&,std::string&)const;
 bool valid()const{return data_.valid()&&epoch_!=0;}
 uint64_t epoch()const{return epoch_;}
 const GeyserLifecycleData&content()const{return data_;}
 const std::vector<GeyserInstanceCommand>&commands()const{return commands_;}
 std::vector<uint64_t>instances()const;
private:
 struct Instance {PoisonSpawnRequest source;GeyserTimeline actor;};
 bool owner(uint64_t,std::string&)const;
 bool drain(Instance&,std::string&);
 GeyserLifecycleData data_;uint64_t epoch_=0,last_epoch_=0,last_ticket_=0;
 std::vector<Instance>instances_;std::vector<GeyserInstanceCommand>commands_;
};
}
