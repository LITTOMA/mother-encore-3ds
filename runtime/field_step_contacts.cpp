#include "encore/field_step_contacts.hpp"
#include <utility>
namespace encore::upstream {namespace {bool fail(std::string&e,const char*s){e=s;return false;}}
bool FieldStepContacts::prepare(FieldStepsView data,uint64_t epoch,const std::vector<FieldStepContactBinding>&bindings,std::string&e){
 if(valid()||!epoch||epoch<=last_epoch_||!data.valid()||data.capability()!=2||bindings.empty()||bindings.size()>128)return fail(e,"Stepping contacts fresh source/epoch/capacity rejected");
 FieldStepContacts next;next.data_=data;next.epoch_=next.last_epoch_=epoch;std::vector<uint32_t>areas;bool party=false;
 for(uint32_t i=0;i<data.area_count();++i)areas.push_back(data.area_id(i));
 for(size_t i=0;i<bindings.size();++i){const auto&b=bindings[i];
  if(!b.owner||b.owner==UINT32_MAX||(b.kind!=FieldStepBody::Party&&b.kind!=FieldStepBody::Other)||(b.kind==FieldStepBody::Party?(!b.shadow||!b.shadow->prepared()||b.shadow->content().bytes_identity()!=data.bytes_identity()):b.shadow!=nullptr))return fail(e,"Stepping contacts body/class/Shadow owner rejected");
  for(size_t j=0;j<i;++j)if(bindings[j].owner==b.owner||(b.shadow&&bindings[j].shadow==b.shadow))return fail(e,"Stepping contacts duplicate body/Shadow rejected");
  FieldAreaContacts pairs;if(!pairs.initialize(areas,e))return false;next.pairs_.push_back(std::move(pairs));next.bindings_.push_back(b);party|=b.kind==FieldStepBody::Party;
 }
 if(!party)return fail(e,"Stepping contacts require a registered PartyObject player");*this=std::move(next);e.clear();return true;
}
bool FieldStepContacts::validate(uint64_t epoch,const FieldStepContactBody*bodies,size_t n,std::string&e)const{
 if(!valid()||epoch!=epoch_||!bodies||n!=count())return fail(e,"Stepping contact scene/body batch rejected");
 for(size_t i=0;i<n;++i){const auto&b=bindings_[i];if(bodies[i].owner!=b.owner||bodies[i].layer>0xfffff||bodies[i].mask>0xfffff||(b.shadow&&(!b.shadow->prepared()||b.shadow->content().bytes_identity()!=data_.bytes_identity())))return fail(e,"Stepping contact body/filter/Shadow binding changed");}
 e.clear();return true;
}
void FieldStepContacts::enabled(const FieldStepContactBody&body,std::array<uint8_t,128>&bits)const{
 for(uint32_t i=0;i<data_.area_count();++i){const auto area=data_.area(i);bits[i]=uint8_t(body.enabled&&((area.mask&body.layer)||(area.layer&body.mask)));}
}
bool FieldStepContacts::begin(uint64_t epoch,uint32_t player,const FieldStepContactBody*bodies,size_t n,FieldRunLoop&loop,std::string&e){
 if(!validate(epoch,bodies,n,e))return false;if(!loop.prepared()||loop.content().bytes_identity()!=data_.bytes_identity())return fail(e,"Stepping contacts run-loop owner rejected");
 bool admitted=false;for(const auto&b:bindings_)if(b.owner==player&&b.kind==FieldStepBody::Party)admitted=true;if(!admitted)return fail(e,"Stepping contacts current player identity rejected");
 auto next=*this;auto candidate=loop;next.events_.clear();std::vector<FieldStepShadowState>shadows(n);
 for(size_t i=0;i<n;++i){if(bindings_[i].shadow)shadows[i]=*bindings_[i].shadow;std::array<uint8_t,128>bits{};enabled(bodies[i],bits);FieldAreaEvents events;
  if(!next.pairs_[i].begin(bits.data(),data_.area_count(),events,e))return false;
  if(events.count>1)return fail(e,"Stepping multi-area callback order is not admitted");
  for(size_t j=0;j<events.count;++j){const auto&event=events.values[j];const auto&binding=bindings_[i];const auto kind=binding.owner==player?FieldStepBody::Player:binding.kind;
   if(!apply_step_callback(candidate,binding.shadow?&shadows[i]:nullptr,event.index,event.entered,kind,e))return false;next.events_.push_back({binding.owner,event.owner,event.entered});
  }
 }
 for(size_t i=0;i<n;++i)if(bindings_[i].shadow)*bindings_[i].shadow=shadows[i];loop=std::move(candidate);*this=std::move(next);e.clear();return true;
}
bool FieldStepContacts::end(uint64_t epoch,const FieldStepContactBody*bodies,size_t n,std::string&e){
 if(!validate(epoch,bodies,n,e))return false;auto next=*this;
 for(size_t i=0;i<n;++i){std::array<uint8_t,128>bits{},overlap{};enabled(bodies[i],bits);
  for(uint32_t j=0;j<data_.area_count();++j){bool hit=false;if(!step_polygon_overlap(data_,j,bodies[i].points,bodies[i].count,hit,e))return false;overlap[j]=uint8_t(hit);}
  if(!next.pairs_[i].end(bits.data(),overlap.data(),data_.area_count(),e))return false;
 }*this=std::move(next);e.clear();return true;
}
bool FieldStepContacts::retire(uint64_t epoch,std::string&e){if(!valid()||epoch!=epoch_)return fail(e,"Stepping contact retirement epoch rejected");data_={};bindings_.clear();pairs_.clear();events_.clear();epoch_=0;e.clear();return true;}
}
