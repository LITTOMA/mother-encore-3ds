#include "encore/dungeon_mechanism_audio.hpp"
namespace encore::upstream {namespace {bool bad(std::string&e,const char*s){e=s;return false;}}
bool DungeonMechanismAudioRouter::prepare(const DungeonMechanismPresentationData&data,uint64_t epoch,std::string&e){
 if(data_.valid()||!data.media_bound()||!epoch)return bad(e,"Mechanism sound routing needs a fresh prepared media owner and scene epoch");data_=data;epoch_=epoch;e.clear();return true;
}
bool DungeonMechanismAudioRouter::command(uint32_t owner,bool wall,bool playing,uint32_t request_owner,DungeonMechanismSoundCommand&out,std::string&e)const{
 const auto&sounds=data_.sounds();for(size_t i=0;i<sounds.size();++i)if(sounds[i].owner==owner){const auto&s=sounds[i];
  if(s.kind==DungeonMechanismSoundKind::Node){
   const DungeonMechanismPlacement*placement=nullptr;for(const auto&o:data_.owners())if(o.owner==owner)placement=&o;
   if(!placement)return bad(e,"Mechanism sound source placement missing");const auto*profile=data_.bindings().profile(placement->profile);
   if((profile->family==DungeonMechanismFamily::Wall)!=wall)return bad(e,"Mechanism sound request/profile family differs");
  }else if(owner!=0||wall||!playing)return bad(e,"Mechanism manager channel request rejected");
  out={epoch_,uint32_t(i),owner,request_owner,s.kind,playing};return true;
 }return bad(e,"Mechanism sound source owner missing");
}
bool DungeonMechanismAudioRouter::plan(const std::vector<DungeonMechanismEvent>&events,std::vector<DungeonMechanismSoundCommand>&out,std::string&e)const{
 if(!valid()||events.size()>4096)return bad(e,"Mechanism sound routing owner/batch rejected");std::vector<DungeonMechanismSoundCommand>next;next.reserve(events.size());
 for(const auto&event:events){
  if(event.epoch!=epoch_)return bad(e,"Mechanism sound scene epoch differs");DungeonMechanismSoundCommand sound;
  switch(event.kind){
   case DungeonMechanismEventKind::WallAudio:
    if(!event.owner)return bad(e,"Mechanism wall sound owner missing");if(!command(event.owner,true,event.playing,event.owner,sound,e))return false;next.push_back(sound);break;
   case DungeonMechanismEventKind::AnimationFinished:case DungeonMechanismEventKind::WallCamera:break; // Other typed scene consumers own these.
   case DungeonMechanismEventKind::Request:{
    const auto&r=event.request;
    switch(r.kind){
     case DungeonSwitchEventKind::SwitchAudio:case DungeonSwitchEventKind::DeniedAudio:{
      if(!r.target.id||event.owner!=r.target.id||!r.target.generation)return bad(e,"Mechanism switch sound request identity rejected");bool found=false;const auto g=data_.bindings().graph();for(uint32_t i=0;i<g.switch_count();++i)if(g.switch_node(i).id==r.target.id)found=true;if(!found)return bad(e,"Mechanism switch sound source unknown");
      if(!command(r.kind==DungeonSwitchEventKind::DeniedAudio?0:r.target.id,false,true,r.target.id,sound,e))return false;next.push_back(sound);break;
     }
     case DungeonSwitchEventKind::ReadyTimer:case DungeonSwitchEventKind::SwitchAnimation:case DungeonSwitchEventKind::ObjectAnimation:
     case DungeonSwitchEventKind::SwitchHit:case DungeonSwitchEventKind::FlagChanged:case DungeonSwitchEventKind::FlagsUpdated:
     case DungeonSwitchEventKind::Pause:case DungeonSwitchEventKind::Unpause:case DungeonSwitchEventKind::Cutscene:
     case DungeonSwitchEventKind::MusicFade:case DungeonSwitchEventKind::MusicResume:case DungeonSwitchEventKind::CameraMove:
     case DungeonSwitchEventKind::CameraReturn:case DungeonSwitchEventKind::ObjectStateChanged:case DungeonSwitchEventKind::WallTimer:break;
     default:return bad(e,"Unknown mechanism request in sound routing");
    }break;
   }
   default:return bad(e,"Unknown mechanism event in sound routing");
  }
 }
 out=std::move(next);e.clear();return true;
}
bool DungeonMechanismAudioRouter::retire(std::vector<DungeonMechanismSoundCommand>&out,std::string&e){
 if(!valid())return bad(e,"Mechanism sound scene already retired");std::vector<DungeonMechanismSoundCommand>next;
 for(size_t i=0;i<data_.sounds().size();++i){const auto&s=data_.sounds()[i];if(s.kind==DungeonMechanismSoundKind::Node)next.push_back({epoch_,uint32_t(i),s.owner,s.owner,s.kind,false});}
 out=std::move(next);epoch_=0;e.clear();return true;
}
}
