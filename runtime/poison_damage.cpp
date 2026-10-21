#include "encore/poison_damage.hpp"
namespace encore::upstream {
bool deliver_poison_player_damage(const PoisonHazardsData&data,PoisonContactKind kind,
 uint32_t area,bool entered,FieldStepBody body,bool paused,uint64_t epoch,uint64_t instance,
 WorldDamageActor&actor,SessionCharacter&character,FieldRunLoop&run,FieldStepShadowState*shadow,
 std::vector<WorldDamageEvent>&events,std::string&error){
 if(!actor.validates(epoch,instance,character,error))return false;
 auto reject=[&](const char*message){error=message;return false;};
 if(!data.valid()||!events.empty()||body==FieldStepBody::Party)return reject("Poison damage requires checked Player profile and fresh event batch");
 const auto&policy=data.policy();const auto*status=actor.content().status(policy.status);
 if(!status||(status->flags&12)||policy.hit.x!=0||policy.hit.y!=0)return reject("Poison damage status/direction binding incompatible with checked world profile");
 // The original module proves these branches perform no body/Run/Shadow
 // mutation after source ownership validation. Preserve the caller's objects.
 if(body==FieldStepBody::Other||(entered&&paused)){PoisonDamageState ignored;return apply_poison_callback(data,kind,area,entered,body,paused,ignored,run,shadow,error);}
 auto next_actor=actor;auto next_character=character;auto next_run=run;FieldStepShadowState next_shadow;if(shadow)next_shadow=*shadow;
 PoisonDamageState request;request.continuous=actor.continuous();std::vector<WorldDamageEvent>batch;
 if(!apply_poison_callback(data,kind,area,entered,body,paused,request,next_run,shadow?&next_shadow:nullptr,error))return false;
 if(!request.status_requests.empty()){
  if(request.status_requests.size()!=1||!request.continuous||body!=FieldStepBody::Player)return reject("Poison damage source request coverage rejected");
  WorldDamageRequest damage;damage.damage=request.damage;damage.variance=request.variance;damage.hit={policy.hit.x,policy.hit.y};
  if(!next_actor.begin_continuous(epoch,instance,next_character,damage,request.status_requests.front(),batch,error))return false;
 }else if(!entered&&body==FieldStepBody::Player){if(!next_actor.stop_continuous(epoch,instance,error))return false;}
 for(const auto&event:batch)if(event.kind!=WorldDamageEventKind::StatusChanged)return reject("Poison damage unsupported immediate Character side effect");
 actor=std::move(next_actor);
 // Entry can change only Character status. Preserve inventory/skill storage and
 // all unrelated session fields rather than replacing the entire character.
 if(!batch.empty())character.status=std::move(next_character.status);
 run=std::move(next_run);if(shadow)*shadow=next_shadow;events=std::move(batch);error.clear();return true;
}
}
