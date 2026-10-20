#include "encore/dungeon_mechanism_contacts.hpp"
namespace encore::upstream {namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
}
bool DungeonMechanismContacts::prepare(const DungeonMechanismPresentationData&data,uint64_t epoch,std::string&e){
 if(valid()||!data.valid()||!epoch)return fail(e,"Mechanism contacts require fresh checked data and epoch");
 DungeonMechanismContacts next;next.data_=data;next.epoch_=epoch;std::vector<uint32_t>owners;
 const auto graph=data.bindings().graph();
 for(size_t i=0;i<data.shapes().size();++i){const auto&s=data.shapes()[i];if(s.role==DungeonMechanismShapeRole::Body)continue;
  bool found=false;for(uint32_t j=0;j<graph.switch_count();++j){const auto node=graph.switch_node(j);if(node.id!=s.owner)continue;
   found=(node.kind==DungeonSwitchKind::Lever&&s.role==DungeonMechanismShapeRole::Interaction)||
         (node.kind==DungeonSwitchKind::Plate&&s.role==DungeonMechanismShapeRole::Pressure);break;
  }
  if(!found)return fail(e,"Mechanism contact method/shape ownership rejected");
  next.indices_.push_back(i);owners.push_back(s.owner);
 }
 if(owners.size()!=graph.switch_count()||!next.pairs_.initialize(owners,e))return fail(e,"Mechanism contact coverage rejected");
 *this=std::move(next);e.clear();return true;
}
bool DungeonMechanismContacts::enabled(uint64_t epoch,const DungeonMechanismContactSource&body,
 const DungeonMechanismContactSource&attack,std::array<uint8_t,128>&bits,std::string&e)const{
 if(!valid()||epoch!=epoch_||body.layer>0xfffff||body.mask>0xfffff||attack.layer>0xfffff||attack.mask>0xfffff)
  return fail(e,"Mechanism contact epoch/filter rejected");
 for(size_t i=0;i<count();++i){const auto&s=data_.shapes()[indices_[i]];
  const bool area=s.role==DungeonMechanismShapeRole::Interaction;const auto&source=area?attack:body;
  const bool filters=(s.mask&source.layer)||(s.layer&source.mask);
  bits[i]=uint8_t(!(s.bits&1)&&(s.bits&2)&&source.enabled&&(!area||source.monitorable)&&filters);
 }
 e.clear();return true;
}
bool DungeonMechanismContacts::begin(uint64_t epoch,const DungeonMechanismContactSource&body,
 const DungeonMechanismContactSource&attack,DungeonMechanismState&state,bool paused,bool can_switch,std::string&e){
 std::array<uint8_t,128>bits{};if(!enabled(epoch,body,attack,bits,e))return false;
 if(!state.valid()||state.epoch()!=epoch_||state.data().graph().bytes_identity()!=data_.bindings().graph().bytes_identity())
  return fail(e,"Mechanism contact state owner rejected");
 auto next=*this;auto result=state;FieldAreaEvents events;
 if(!next.pairs_.begin(bits.data(),count(),events,e))return false;
 for(size_t i=0;i<events.count;++i){const auto&event=events.values[i];if(!event.entered)continue;
  const auto role=data_.shapes()[indices_[event.index]].role;
  const auto method=role==DungeonMechanismShapeRole::Interaction?DungeonSwitchMethod::Hit:DungeonSwitchMethod::PlayerEntered;
  if(!result.operate(event.owner,method,true,paused,can_switch,e))return false;
 }
 *this=std::move(next);state=std::move(result);e.clear();return true;
}
bool DungeonMechanismContacts::end(uint64_t epoch,const DungeonMechanismContactSource&body,
 const DungeonMechanismContactSource&attack,const uint8_t*overlap,size_t size,std::string&e){
 std::array<uint8_t,128>bits{};if(!enabled(epoch,body,attack,bits,e))return false;
 return pairs_.end(bits.data(),overlap,size,e);
}
}
