#include "encore/dropped_items.hpp"
#include <algorithm>
#include <set>

namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool inventory(const std::vector<SessionItem>&items,std::set<uint32_t>&uids){
 if(items.size()>session_save_max_entries)return false;
 for(const auto&i:items)if(i.item_id.empty()||i.item_id.size()>session_save_max_string_bytes||i.doses<=0||i.doses>65535||!uids.insert(i.uid).second)return false;
 return true;
}
bool same_items(const std::vector<SessionItem>&a,const std::vector<SessionItem>&b){
 if(a.size()!=b.size())return false;
 for(size_t i=0;i<a.size();++i)if(a[i].item_id!=b[i].item_id||a[i].equipped!=b[i].equipped||a[i].doses!=b[i].doses||a[i].uid!=b[i].uid)return false;
 return true;
}
}
bool same_item_holder_inventory_domain(const SessionSnapshot&a,const SessionSnapshot&b){
 if(a.party!=b.party||a.characters.size()!=b.characters.size()||!same_items(a.key_items,b.key_items)||!same_items(a.storage,b.storage))return false;
 for(size_t i=0;i<a.characters.size();++i)if(a.characters[i].character_id!=b.characters[i].character_id||!same_items(a.characters[i].inventory,b.characters[i].inventory))return false;
 return true;
}
bool grant_item_holder_item(const DroppedItemsData&data,uint32_t template_id,
 SessionSnapshot&session,SourceRandom&random,std::vector<uint32_t>&ledger,
 const LoadRngClockProvider&clock,ItemHolderGrantResult&output,std::string&e){
 if(!data.valid())return fail(e,"ItemHolder grant data not prepared");
 const auto*definition=data.item(template_id);
 if(!definition)return fail(e,"ItemHolder unknown source template");
 if(session.characters.size()>session_save_max_characters||session.party.size()>session_save_max_characters)return fail(e,"ItemHolder session holder bounds");
 std::set<uint32_t>owned_uids;std::set<std::string>characters,party;
 for(const auto&c:session.characters)if(c.character_id.empty()||!characters.insert(c.character_id).second||!inventory(c.inventory,owned_uids))return fail(e,"ItemHolder character/inventory identity");
 if(!inventory(session.key_items,owned_uids)||!inventory(session.storage,owned_uids))return fail(e,"ItemHolder key/storage identity");
 uint32_t selected=UINT32_MAX;
 // The original walks the live party order, not the character dictionary or
 // saved roster order. Validate every participating holder before sampling UID.
 for(const auto&id:session.party){
  if(!party.insert(id).second)return fail(e,"ItemHolder duplicate party holder");
  size_t at=0;while(at<session.characters.size()&&session.characters[at].character_id!=id)++at;
  if(at==session.characters.size()||session.characters[at].inventory.size()>data.info().capacity)return fail(e,"ItemHolder missing/overfull party inventory");
  if(selected==UINT32_MAX&&session.characters[at].inventory.size()<data.info().capacity)selected=uint32_t(at);
 }
 const bool acquired=definition->key_item||selected!=UINT32_MAX;
 if(definition->key_item&&session.key_items.size()==session_save_max_entries)return fail(e,"ItemHolder key inventory structural limit");
 const auto generation=data.generation();const auto before=session;const auto old_state=random.state(),old_draws=random.raw_draw_count();const auto old_ledger_size=ledger.size();auto staged_random=random;auto staged_ledger=ledger;
 std::vector<LoadUidAllocation>allocations;
 if(!apply_load_uid_allocations(staged_random,staged_ledger,{{definition->id,1}},clock,e,&allocations))return false;
 if(data.generation()!=generation||allocations.size()!=1)return fail(e,"ItemHolder constructor context changed");
 if(!same_item_holder_inventory_domain(before,session)||random.state()!=old_state||random.raw_draw_count()!=old_draws||ledger.size()!=old_ledger_size||!std::equal(ledger.begin(),ledger.end(),staged_ledger.begin()))return fail(e,"ItemHolder inventory/RNG domain changed during constructor");
 ItemHolderGrantResult result;
 result.item={definition->item,false,int64_t(definition->doses),allocations[0].generated_uid};
 result.kind=acquired?ItemHolderGrantKind::Acquired:ItemHolderGrantKind::Full;
 if(acquired){
  // The original generated ledger does not include saved UIDs. A collision
  // with an independently restored identity cannot enter the native save
  // domain; fail closed without retrying or inventing a replacement UID.
  if(owned_uids.count(result.item.uid))return fail(e,"ItemHolder generated UID collides with restored item");
  if(definition->key_item){result.target=ItemHolderGrantTarget::Keys;session.key_items.push_back(result.item);}
  else{result.target=ItemHolderGrantTarget::Party;result.character_index=selected;session.characters[selected].inventory.push_back(result.item);}
 }
 random=staged_random;ledger.swap(staged_ledger);output=std::move(result);e.clear();return true;
}
}
