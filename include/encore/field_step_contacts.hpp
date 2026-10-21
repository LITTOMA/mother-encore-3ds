#pragma once
#include "encore/field_area_contacts.hpp"
#include "encore/field_steps.hpp"
namespace encore::upstream {
// Party identifies the source class,including Player subclasses. The current
// player is supplied separately; Other bodies must not have a Shadow binding.
struct FieldStepContactBinding {uint32_t owner=0;FieldStepBody kind=FieldStepBody::Other;FieldStepShadowState*shadow=nullptr;};
struct FieldStepContactBody {uint32_t owner=0,layer=0,mask=0;bool enabled=false;const Vec2*points=nullptr;size_t count=0;};
struct FieldStepContactEvent {uint32_t body=0,area=0;bool entered=false;};
// Borrowed metadata and bound Shadow addresses must outlive this service.
// begin publishes the previous physics sample;end samples after movement.
// Area enabled controls sound only. Body collision/filter admission is separate.
// Multiple area transitions for one body have unaudited source ordering and
// reject atomically;this is not a complete physics server or scene admission.
class FieldStepContacts {
public:
 bool prepare(FieldStepsView,uint64_t,const std::vector<FieldStepContactBinding>&,std::string&);
 bool begin(uint64_t,uint32_t player,const FieldStepContactBody*,size_t,FieldRunLoop&,std::string&);
 bool end(uint64_t,const FieldStepContactBody*,size_t,std::string&);
 bool retire(uint64_t,std::string&);
 bool valid()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}
 size_t count()const{return bindings_.size();}bool sampling()const{return valid()&&pairs_.front().sampling();}
 bool inside(size_t body,size_t area)const{return body<count()&&pairs_[body].inside(area);}
 const std::vector<FieldStepContactEvent>&events()const{return events_;}
private:
 bool validate(uint64_t,const FieldStepContactBody*,size_t,std::string&)const;
 void enabled(const FieldStepContactBody&,std::array<uint8_t,128>&)const;
 FieldStepsView data_;uint64_t epoch_=0,last_epoch_=0;
 std::vector<FieldStepContactBinding>bindings_;std::vector<FieldAreaContacts>pairs_;std::vector<FieldStepContactEvent>events_;
};
}
