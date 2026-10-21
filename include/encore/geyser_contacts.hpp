#pragma once
#include "encore/geyser_lifecycle.hpp"
#include "encore/poison_contacts.hpp"
namespace encore::upstream {
struct GeyserPlayerContact {uint64_t epoch=0,geyser=0,body=0;bool entered=false;};
// One checked Ready factory instance and one Player/Other body. Geometry comes
// from the independent source resources. The owner schedules physics sampling
// and calls retire at its committed removal boundary, before unregistering the
// instance. Cross-instance broadphase ordering and full scenes are not implied.
class GeyserPlayerContacts {
public:
 bool prepare(const PoisonContactsData&,const GeyserLifecycle&,uint64_t epoch,uint64_t ticket,uint64_t body,FieldStepBody,std::string&);
 bool begin(const GeyserLifecycle&,uint64_t,const PoisonBodySample&,bool paused,WorldDamageActor&,SessionCharacter&,FieldRunLoop&,FieldStepShadowState*,std::vector<WorldDamageEvent>&,std::string&);
 bool end(const GeyserLifecycle&,uint64_t,const PoisonBodySample&,std::string&);
 // An inside pair emits the original exit callback even when the player is
 // paused. Pending, unpublished transitions must first be resolved explicitly.
 bool retire(const GeyserLifecycle&,uint64_t,bool paused,WorldDamageActor&,SessionCharacter&,FieldRunLoop&,FieldStepShadowState*,std::vector<WorldDamageEvent>&,std::string&);
 bool shape(const GeyserLifecycle&,uint64_t,std::array<Vec2,4>&,std::string&)const;
 bool valid()const{return data_.valid()&&registry_&&epoch_&&ticket_&&body_;}
 bool sampling()const{return valid()&&sampling_;}bool inside()const{return valid()&&inside_;}
 const std::vector<GeyserPlayerContact>&events()const{return events_;}
private:
 bool owner(const GeyserLifecycle&,uint64_t,GeyserInstanceSnapshot&,std::string&)const;
 bool borrowed(const GeyserLifecycle&,uint64_t,WorldDamageActor&,const SessionCharacter&,FieldRunLoop&,const std::vector<WorldDamageEvent>&,GeyserInstanceSnapshot&,std::string&)const;
 bool enabled(const GeyserInstanceSnapshot&,const PoisonBodySample&)const;
 PoisonContactsData data_;const GeyserLifecycle*registry_=nullptr;GeyserLifecycleData family_;
 uint64_t epoch_=0,last_epoch_=0,ticket_=0,body_=0;FieldStepBody kind_=FieldStepBody::Other;
 bool sampling_=false,inside_=false,pending_=false;std::vector<GeyserPlayerContact>events_;
};
}
