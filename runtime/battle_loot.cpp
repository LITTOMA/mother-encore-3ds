#include "encore/battle_loot.hpp"
namespace encore::upstream {
bool BattleLoot::prepare(RoundView content,SourceRandom& random,BattleLootHost& host,std::string& error){
 if(ready_||!content.valid()||content.version()!=6||content.count(RoundSection::Loot)!=1||content.victory().reward_item_count!=1){error="Ordinary loot preparation contract rejected";return false;}
 const auto policy=content.loot();const auto item=content.string(policy.item);
 if(!host.validate_sound(content.string(policy.sound),error)||!host.can_receive(item,policy.doses,error))return false;
 auto candidate=random;
 // BattleItemPool selects candidates[randi() % candidates.size()] even when
 // this admitted single-enemy, non-rare schema has exactly one candidate.
 static_cast<void>(candidate.randi());
 const bool selected=candidate.rand_range(0.0,100.0)<=policy.chance;
 uint32_t uid=0;
 if(selected&&!host.reserve(item,policy.doses,candidate,uid,error))return false;
 content_=content;host_=&host;uid_=uid;selected_=selected;ready_=true;
 random=candidate;error.clear();return true;
}
bool BattleLoot::matches(RoundView view)const{return ready_&&content_.bytes_identity()==view.bytes_identity();}
bool BattleLoot::grant(std::string& error){
 if(!ready_||!host_){error="Loot grant has no prepared pool";return false;}
 if(granted_){error="Loot grant was already committed";return false;}
 if(selected_){const auto policy=content_.loot();if(!host_->grant(content_.string(policy.item),policy.doses,uid_,error))return false;}
 granted_=true;error.clear();return true;
}
bool BattleLoot::play_sound(std::string& error){
 if(!ready_||!host_||!selected_||!granted_||sound_played_){error="Loot sound outside the checked reward boundary";return false;}
 if(!host_->play_sound(content_.string(content_.loot().sound),error))return false;
 sound_played_=true;error.clear();return true;
}
}
