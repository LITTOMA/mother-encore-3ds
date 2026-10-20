#pragma once
#include "encore/dungeon_mechanism_presentation.hpp"
#include "encore/dungeon_mechanism_state.hpp"
namespace encore::upstream {
struct DungeonMechanismSoundCommand {
 uint64_t epoch=0;uint32_t slot=0,owner=0,request_owner=0;
 DungeonMechanismSoundKind kind=DungeonMechanismSoundKind::Node;bool playing=false;
};
// Translate only this producer's checked sound requests. Repeated true writes
// remain requests; actual audio-thread mixing is not inferred from them.
// Source-local retirement deliberately excludes the global manager channel.
class DungeonMechanismAudioRouter {
public:
 bool prepare(const DungeonMechanismPresentationData&,uint64_t,std::string&);
 bool valid()const{return data_.media_bound()&&epoch_!=0;}
 uint64_t epoch()const{return epoch_;}
 const DungeonMechanismPresentationData&data()const{return data_;}
 bool plan(const std::vector<DungeonMechanismEvent>&,std::vector<DungeonMechanismSoundCommand>&,std::string&)const;
 bool retire(std::vector<DungeonMechanismSoundCommand>&,std::string&);
private:
 bool command(uint32_t,bool,bool,uint32_t,DungeonMechanismSoundCommand&,std::string&)const;
 DungeonMechanismPresentationData data_;uint64_t epoch_=0;
};
}
