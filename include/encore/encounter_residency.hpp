#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace encore {
// Compiler/scene-factory contract: enumerate every reachable checked encounter
// composition, including all branches of random tables, without sampling them.
// Content identifiers and file paths are data. This class owns no game clocks,
// actors, random source, renderer, filesystem handle or platform allocator.
struct EncounterDependency {
    std::string key,battle_pack,round_pack;
};
struct SceneEncounterDependencies {
    uint64_t scene_epoch=0;
    std::vector<EncounterDependency> encounters;
};
struct EncounterResidencyBytes {
    size_t cpu=0,linear=0;
};
struct EncounterResidencyTicket {uint64_t scene_epoch=0;size_t index=SIZE_MAX;};
class EncounterResidency {
public:
    enum class State {Missing,Preparing,Ready,Failed};
    bool declare(const SceneEncounterDependencies& declaration,EncounterResidencyBytes budget,EncounterResidencyBytes other_live,std::string& error){
        if(!declaration.scene_epoch||!budget.cpu||!budget.linear||other_live.cpu>budget.cpu||other_live.linear>budget.linear||declaration.encounters.size()>1024){error="Invalid encounter scene admission bounds";return false;}
        const auto valid_path=[](const std::string& path,const char* suffix){const std::string tail=suffix;return !path.empty()&&path[0]!='/'&&path.find("..") == std::string::npos&&path.find('\\')==std::string::npos&&path.size()>tail.size()&&path.compare(path.size()-tail.size(),tail.size(),tail)==0;};
        for(size_t i=0;i<declaration.encounters.size();++i){const auto& e=declaration.encounters[i];
            if(e.key.empty()||!valid_path(e.battle_pack,".encbattle")||!valid_path(e.round_pack,".encround")){error="Encounter dependency lacks a checked compiled pack pair";return false;}
            for(size_t j=0;j<i;++j)if(declaration.encounters[j].key==e.key||declaration.encounters[j].battle_pack==e.battle_pack){error="Duplicate encounter key or pack must be unioned before admission";return false;}
        }
        declaration_=declaration;budget_=budget;other_live_=other_live;states_.assign(declaration.encounters.size(),State::Missing);reserved_.assign(states_.size(),{});return true;
    }
    bool reserve(size_t index,EncounterResidencyBytes peak,std::string& error){
        if(index>=states_.size()||states_[index]!=State::Missing){error="Encounter preparation ticket is stale or already started";return false;}
        auto used=other_live_;for(const auto& r:reserved_){if(r.cpu>budget_.cpu-used.cpu||r.linear>budget_.linear-used.linear){error="Encounter working set accounting overflow";return false;}used.cpu+=r.cpu;used.linear+=r.linear;}
        if(peak.cpu>budget_.cpu-used.cpu||peak.linear>budget_.linear-used.linear){error="Scene encounter working set exceeds admission budget";return false;}
        reserved_[index]=peak;states_[index]=State::Preparing;return true;
    }
    bool revise(EncounterResidencyTicket ticket,EncounterResidencyBytes peak,std::string& error){
        if(ticket.scene_epoch!=declaration_.scene_epoch||ticket.index>=states_.size()||states_[ticket.index]!=State::Preparing){error="Stale encounter reservation cannot change";return false;}
        auto used=other_live_;for(size_t i=0;i<reserved_.size();++i)if(i!=ticket.index){const auto r=reserved_[i];if(r.cpu>budget_.cpu-used.cpu||r.linear>budget_.linear-used.linear){error="Encounter reservation accounting overflow";return false;}used.cpu+=r.cpu;used.linear+=r.linear;}
        if(peak.cpu>budget_.cpu-used.cpu||peak.linear>budget_.linear-used.linear){error="Scene encounter working set exceeds admission budget";return false;}
        reserved_[ticket.index]=peak;return true;
    }
    bool retire_other_live(EncounterResidencyBytes released){
        if(released.cpu>other_live_.cpu||released.linear>other_live_.linear)return false;
        other_live_.cpu-=released.cpu;other_live_.linear-=released.linear;return true;
    }
    bool publish(EncounterResidencyTicket ticket,EncounterResidencyBytes retained,std::string& error){
        if(ticket.scene_epoch!=declaration_.scene_epoch||ticket.index>=states_.size()||states_[ticket.index]!=State::Preparing){error="Stale encounter preparation cannot publish";return false;}
        auto& limit=reserved_[ticket.index];if(retained.cpu>limit.cpu||retained.linear>limit.linear){states_[ticket.index]=State::Failed;error="Encounter preparation exceeded its reserved peak";return false;}
        limit=retained;states_[ticket.index]=State::Ready;return true;
    }
    bool ready(EncounterResidencyTicket ticket)const{return ticket.scene_epoch==declaration_.scene_epoch&&ticket.index<states_.size()&&states_[ticket.index]==State::Ready;}
    bool scene_ready()const{return declaration_.scene_epoch&&std::all_of(states_.begin(),states_.end(),[](State s){return s==State::Ready;});}
    void retire(){declaration_={};states_.clear();reserved_.clear();}
    uint64_t epoch()const{return declaration_.scene_epoch;}
    const SceneEncounterDependencies& declaration()const{return declaration_;}
private:
    SceneEncounterDependencies declaration_;EncounterResidencyBytes budget_,other_live_;
    std::vector<State> states_;std::vector<EncounterResidencyBytes> reserved_;
};
}
