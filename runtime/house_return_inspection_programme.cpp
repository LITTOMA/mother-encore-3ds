#include "encore/house_return_inspection_programme.hpp"
#include "encore/dialogue.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace encore::upstream {
namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool reject(std::string&e,const char*t){e=t;return false;}
RoomCommand command(DialogueActionKind kind,uint32_t phrase){RoomCommand c;c.opcode=uint16_t(kind);c.phrase=phrase;return c;}
bool same(const RoomCommand&a,const RoomCommand&b){
  return a.opcode==b.opcode&&a.actor_index==b.actor_index&&a.phrase==b.phrase&&
    a.target_index==b.target_index&&a.auxiliary_index==b.auxiliary_index&&a.flags==b.flags&&
    a.vector.x==b.vector.x&&a.vector.y==b.vector.y&&a.value==b.value&&a.duration==b.duration;
}
void end(std::vector<RoomCommand>&out,uint32_t phrase,double duration){
  auto c=command(DialogueActionKind::StopInteraction,phrase);c.flags=1;out.push_back(c);
  out.push_back(command(DialogueActionKind::SetTalker,phrase));
  out.push_back(command(DialogueActionKind::CutsceneEnded,phrase));
  c=command(DialogueActionKind::DialogueDone,phrase);c.duration=duration;out.push_back(c);
}
bool text_id(HouseView text,std::string_view path,uint32_t id){
  for(uint32_t i=0;i<text.count(HouseSection::Dialogues);++i){const auto d=text.dialogue(i);
    if(d.id==id&&text.string(d.source_path)==path)return true;}
  return false;
}
uint32_t flag(RoomView room,std::string_view name){
  for(uint32_t i=0;i<room.flag_count();++i)if(room.string(room.flag(i).name_string)==name)return i;
  return kRoomNoIndex;
}
uint32_t audio(RoomView room,std::string_view path){
  uint32_t found=kRoomNoIndex;
  for(uint32_t i=0;i<room.resource_count();++i){const auto r=room.resource(i);
    if(r.kind==uint16_t(RoomResourceKind::AudioRequestOnly)&&room.string(r.path_string)==path){
      if(found!=kRoomNoIndex)return kRoomNoIndex;found=i;}}
  return found;
}
bool drawer_commands(RoomView room,HouseView text,DrawerProgramView drawer,
    std::vector<RoomCommand>&out,uint32_t&phrases,std::string&e){
  const auto path=drawer.string(drawer.binding().source_path);
  const auto n=drawer.count(DrawerSection::Commands);std::vector<uint32_t>pc(n+1),phrase(n);
  uint32_t current=0,seen=0;
  for(uint32_t i=0;i<n;++i){const auto c=drawer.command(i);
    if(c.opcode==uint32_t(DrawerOpcode::ShowText)){current=seen++;
      if(!text_id(text,path,c.a))return reject(e,"Drawer command lost its actual same-House text source");}
    // Original item grant precedes the upcoming text phrase, not the prior one.
    phrase[i]=c.opcode==uint32_t(DrawerOpcode::GrantItem)?seen:current;
  }
  phrases=seen;if(!seen)return reject(e,"Drawer has no actual text phrases");
  out.push_back(command(DialogueActionKind::BeginCutscene,0));
  struct Fix {size_t output;uint32_t input;bool auxiliary;};std::vector<Fix>fixes;
  for(uint32_t i=0;i<n;++i){pc[i]=uint32_t(out.size());const auto d=drawer.command(i);
    RoomCommand c;
    switch(DrawerOpcode(d.opcode)){
    case DrawerOpcode::ShowText:c=command(DialogueActionKind::ShowDialogue,phrase[i]);c.target_index=d.a;c.flags=1;break;
    case DrawerOpcode::AwaitText:c=command(DialogueActionKind::AwaitDialogue,phrase[i]);break;
    case DrawerOpcode::BranchFlag:
      c=command(DialogueActionKind::BranchFlag,phrase[i]);c.target_index=flag(room,drawer.string(d.a));c.value=d.b;
      if(c.target_index==kRoomNoIndex)return reject(e,"Drawer source conditional flag is absent from the same Room");
      fixes.push_back({out.size(),d.c,true});break;
    case DrawerOpcode::BranchSpace:c=command(DialogueActionKind::BranchInventorySpace,phrase[i]);c.value=d.a;
      fixes.push_back({out.size(),d.b,true});break;
    case DrawerOpcode::GrantItem:c=command(DialogueActionKind::GrantInventoryItem,phrase[i]);c.target_index=d.a;
      if(d.a>=drawer.count(DrawerSection::Templates))return reject(e,"Drawer grant has no actual checked template");break;
    case DrawerOpcode::PlaySound:c=command(DialogueActionKind::PlaySound,phrase[i]);
      c.target_index=audio(room,std::string("res://")+std::string(drawer.string(d.a)));
      if(c.target_index==kRoomNoIndex)return reject(e,"Drawer source sound is absent/ambiguous in the same Room");break;
    case DrawerOpcode::SetFlag:c=command(DialogueActionKind::SetFlag,phrase[i]);c.target_index=flag(room,drawer.string(d.a));c.value=d.b;
      if(c.target_index==kRoomNoIndex)return reject(e,"Drawer source flag effect is absent from the same Room");break;
    case DrawerOpcode::Jump:c=command(DialogueActionKind::Jump,phrase[i]);fixes.push_back({out.size(),d.a,false});break;
    case DrawerOpcode::End:end(out,phrase[i],room.rule(RoomRuleKey::CameraReturnSeconds).f64);continue;
    default:return reject(e,"Unknown actual Drawer source instruction");
    }
    out.push_back(c);
  }
  pc[n]=uint32_t(out.size());
  for(const auto&f:fixes){if(f.input>=n)return reject(e,"Drawer conditional source target is outside its actual programme");
    if(f.auxiliary)out[f.output].auxiliary_index=pc[f.input];else out[f.output].target_index=pc[f.input];}
  return true;
}
}
bool HouseReturnInspectionProgrammes::admit(RoomView original,RoomView next,HouseView text,
    DrawerProgramView drawer,const FieldInteractData&interact,std::string&e){
  if(!original.valid()||!next.valid()||!text.valid()||!drawer.valid()||!interact.valid()||
      u32(original.bytes()+32)!=8||u32(original.bytes()+36)!=9||
      u32(next.bytes()+32)!=8||u32(next.bytes()+36)!=10||
      std::memcmp(original.bytes()+56,next.bytes()+56,20)||
      std::memcmp(next.bytes()+56,text.bytes()+32,20)||
      std::memcmp(next.bytes()+56,drawer.reviewed_commit(),20)||
      std::memcmp(next.bytes()+56,interact.source_pin().data(),20)||
      next.string(next.scene().source_scene_string)!=std::string("res://")+std::string(interact.scene()))
    return reject(e,"House inspection Room lost its actual original capability/source/House/Drawer identity");
  if(next.string_count()<original.string_count())return reject(e,"House inspection Room deleted original strings");
  for(uint32_t i=0;i<original.string_count();++i)if(original.string(i)!=next.string(i))
    return reject(e,"House inspection Room changed original stable string identity");
  for(uint16_t i=3;i<=kRoomSectionCount;++i){const auto section=RoomSection(i);
    const auto old=original.count(section),count=next.count(section);
    const bool append=section==RoomSection::Program||section==RoomSection::Command;
    if((append?count<old:count!=old)||
        (old&&std::memcmp(original.bytes()+original.section_offset(section),next.bytes()+next.section_offset(section),size_t(old)*kRoomStrides[i-1])))
      return reject(e,"House inspection Room changed an original source table/prefix");
  }
  std::set<std::string>paths;
  for(const auto&r:interact.records()){
    if(!r.dialogue.empty())paths.insert(r.dialogue);
    for(const auto&c:r.choices)if(!c.programme.empty())paths.insert(c.programme);
  }
  if(next.program_count()!=original.program_count()+paths.size())
    return reject(e,"House inspection Room omitted/added an unselected actual Interact programme");
  const auto duration=original.rule(RoomRuleKey::CameraReturnSeconds).f64;
  if(!(duration>0))return reject(e,"House inspection dialogue has no actual source camera return timing");
  uint32_t oldmax=0;for(uint32_t i=0;i<original.program_count();++i)oldmax=std::max(oldmax,original.program(i).stable_id);
  uint32_t index=original.program_count(),first=original.command_count();
  for(const auto&identity:paths){const auto p=next.program(index++);
    const auto source=std::string("Data/Dialogue/")+identity+".yaml";std::array<uint8_t,32>sha;
    if(!interact.source_hash(source,sha)||next.string(p.source_path_string)!=identity||p.first_command!=first||p.stable_id!=++oldmax)
      return reject(e,"House inspection programme index/identity does not come from the complete source Interact closure");
    for(uint32_t i=0;i<original.program_count();++i)if(original.string(original.program(i).source_path_string)==identity)
      return reject(e,"House inspection programme replaced an original Room identity");
    std::vector<RoomCommand>expected;uint32_t phrases=1;
    if(source==drawer.string(drawer.binding().source_path)){
      if(!drawer_commands(next,text,drawer,expected,phrases,e))return false;
    }else{
      uint32_t id=0,matches=0;
      for(uint32_t t=0;t<text.count(HouseSection::Dialogues);++t){const auto d=text.dialogue(t);
        if(text.string(d.source_path)==source){id=d.id;++matches;}}
      if(matches!=1)return reject(e,"House inspection literal source text is absent/ambiguous");
      expected.push_back(command(DialogueActionKind::BeginCutscene,0));
      auto c=command(DialogueActionKind::ShowDialogue,0);c.target_index=id;c.flags=1;expected.push_back(c);
      expected.push_back(command(DialogueActionKind::AwaitDialogue,0));end(expected,0,duration);
    }
    if(p.phrase_count!=phrases||p.command_count!=expected.size())
      return reject(e,"House inspection programme changed original source phrase/command coverage");
    for(uint32_t pc=0;pc<p.command_count;++pc)if(!same(next.command(p.first_command+pc),expected[pc]))
      return reject(e,"House inspection Room changed source item/text/sound/flag/wait/branch/end ordering");
    first+=p.command_count;
  }
  if(first!=next.command_count())return reject(e,"House inspection Room has commands outside the actual source closure");
  e.clear();return true;
}
bool HouseReturnInspectionProgrammes::load(const uint8_t*bytes,size_t n,RoomView original,
    HouseView text,DrawerProgramView drawer,const FieldInteractData&interact,std::string&e){
  RoomData candidate;if(!candidate.load(bytes,n,e)||!admit(original,candidate.view(),text,drawer,interact,e))return false;
  room_=std::move(candidate);e.clear();return true;
}
bool HouseReturnInspectionProgrammes::resolve(std::string_view path,uint32_t&out,std::string&e)const{
  const auto room=view();uint32_t found=kRoomNoIndex;
  if(!room.valid())return reject(e,"House inspection Room has not been admitted");
  for(uint32_t i=0;i<room.program_count();++i)if(room.string(room.program(i).source_path_string)==path){
    if(found!=kRoomNoIndex)return reject(e,"House inspection source programme is ambiguous");found=i;}
  if(found==kRoomNoIndex)return reject(e,"House inspection source programme is absent");
  out=found;e.clear();return true;
}
} // namespace encore::upstream
