#include "encore/geyser_mixer.hpp"
#include "encore/geyser_graphics.hpp"
#include <algorithm>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
}
bool GeyserMixer::sync_lifecycle(uint64_t epoch,GeyserLifecycle&life,GeyserListener listener,std::string&e){
 if(!owner(epoch,e))return false;
 if(!life.valid()||life.epoch()!=epoch||!data_.timeline().same_content(life.content().geyser())||(lifecycle_owner_.valid()&&!lifecycle_owner_.same_owner(life.content())))return bad(e,"Geyser mixer registry source/epoch owner differs");
 if(!lifecycle_owner_.valid()&&(last_instance_||std::any_of(voices_.begin(),voices_.end(),[](const Voice&v){return v.instance!=0;})))return bad(e,"Geyser mixer registry cannot replace manual actor ownership");
 GeyserStereoGain checked;if(!geyser_spatial_gain(data_.timeline().policy(),{},listener,0,checked,e))return false;
 std::vector<GeyserInstanceSnapshot>ready;
 for(auto id:life.instances()){GeyserInstanceSnapshot snapshot;if(!life.inspect(epoch,id,snapshot,e))return false;if(snapshot.pose)ready.push_back(std::move(snapshot));}
 if(ready.size()>voices_.size()||life.commands().size()>32768)return bad(e,"Geyser mixer registry capacity rejected");
 auto next=*this;next.lifecycle_owner_=life.content();
 auto current=[&](uint64_t id){return std::find_if(ready.begin(),ready.end(),[&](const auto&v){return v.source.ticket==id;});};
 for(auto&voice:next.voices_)if(voice.instance&&current(voice.instance)==ready.end())voice={};
 for(const auto&actor:ready){if(next.find(actor.source.ticket)==SIZE_MAX&&!next.bind_source(actor.source.ticket,e))return false;if(!next.apply_source(actor.source.ticket,*actor.pose,actor.source.position,listener,nullptr,0,e))return false;}
 // Retain actual cross-actor phase order;grouping commands by actor here can
 // move a deferred named play before another actor's earlier local audio.
 for(const auto&command:life.commands()){
  const auto actor=current(command.instance);if(command.epoch!=epoch||actor==ready.end())return bad(e,"Geyser mixer registry command lifetime rejected");
  if(!next.apply_source(command.instance,*actor->pose,actor->source.position,listener,&command.command,1,e))return false;
 }
 std::vector<GeyserInstanceCommand>delivered;if(!life.take_commands(epoch,delivered,e))return false;*this=std::move(next);e.clear();return true;
}
bool GeyserGraphicsData::compose_lifecycle(const GeyserLifecycle&life,uint64_t epoch,uint64_t instance,Vec2 camera,bool visible,GeyserSpriteDraw&out,std::string&e)const{
 GeyserInstanceSnapshot snapshot;if(!valid()||!matches(life.content().geyser())||!life.inspect(epoch,instance,snapshot,e)||!snapshot.pose)return bad(e,"Geyser graphics registry source/Ready owner rejected");return compose_pose(*snapshot.pose,snapshot.source.position,camera,visible,out,e);
}
}
