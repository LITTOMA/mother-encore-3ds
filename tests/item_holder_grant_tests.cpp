#include "encore/dropped_items.hpp"
#include <cstdlib>
#include <iostream>

using namespace encore::upstream;
namespace {
size_t checks=0;
void check(bool ok,int line){++checks;if(!ok){std::cerr<<"ItemHolder grant failure at "<<line<<"\n";std::exit(1);}}
#define CHECK(value) check(bool(value),__LINE__)
bool equal(const SessionItem&a,const SessionItem&b){return a.item_id==b.item_id&&a.equipped==b.equipped&&a.doses==b.doses&&a.uid==b.uid;}
bool equal(const std::vector<SessionItem>&a,const std::vector<SessionItem>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(!equal(a[i],b[i]))return false;return true;}
bool equal(const SessionSnapshot&a,const SessionSnapshot&b){
 if(a.party!=b.party||a.characters.size()!=b.characters.size()||!equal(a.key_items,b.key_items)||!equal(a.storage,b.storage)||a.cash!=b.cash||a.bank!=b.bank||a.earned_cash!=b.earned_cash)return false;
 for(size_t i=0;i<a.characters.size();++i)if(a.characters[i].character_id!=b.characters[i].character_id||!equal(a.characters[i].inventory,b.characters[i].inventory))return false;
 return true;
}
SessionSnapshot session(const DroppedItemsData&d,size_t first=0,size_t second=0){
 SessionSnapshot s;s.cash=7;s.bank=13;s.earned_cash=2;s.party={"holder-a","holder-b"};
 for(size_t count:{first,second}){SessionCharacter c;c.character_id=s.party[s.characters.size()];for(size_t i=0;i<count;++i)c.inventory.push_back({d.templates()[0].item,false,1,uint32_t(1000+100*s.characters.size()+i)});s.characters.push_back(std::move(c));}
 return s;
}
}
int main(int argc,char**argv){
 CHECK(argc==3);DroppedItemsData d,key_data;std::string error;
 CHECK(d.load_file(argv[1],error));CHECK(key_data.load_file(argv[2],error));
 const auto id=d.templates()[0].id;const DroppedTemplate*key=nullptr;for(const auto&t:key_data.templates())if(t.key_item)key=&t;CHECK(key);
 size_t reads=0;LoadRngClockProvider clock=[&](LoadRngClockSample&s,std::string&e){++reads;s={100,200};e.clear();return true;};
 SourceRandom random(1);std::vector<uint32_t>ledger;ItemHolderGrantResult result;
 auto s=session(d);auto untouched=s.characters[1].inventory.data();
 CHECK(grant_item_holder_item(d,id,s,random,ledger,clock,result,error));
 CHECK(reads==1&&ledger.size()==1&&result.kind==ItemHolderGrantKind::Acquired&&result.target==ItemHolderGrantTarget::Party&&result.character_index==0);
 CHECK(s.characters[0].inventory.size()==1&&equal(s.characters[0].inventory[0],result.item)&&result.item.uid==ledger[0]);
 CHECK(result.item.item_id==d.templates()[0].item&&result.item.doses==d.templates()[0].doses&&!result.item.equipped);
 CHECK(s.characters[1].inventory.data()==untouched&&s.cash==7&&s.bank==13&&s.earned_cash==2);
 s=session(d,d.info().capacity,0);CHECK(grant_item_holder_item(d,id,s,random,ledger,clock,result,error));
 CHECK(result.character_index==1&&s.characters[0].inventory.size()==d.info().capacity&&s.characters[1].inventory.size()==1&&ledger.size()==2);
 s=session(d);s.party={"holder-b","holder-a"};CHECK(grant_item_holder_item(d,id,s,random,ledger,clock,result,error));
 CHECK(result.character_index==1&&s.characters[0].inventory.empty()&&s.characters[1].inventory.size()==1);
 s=session(d,d.info().capacity,d.info().capacity);auto before=s;const auto old_count=ledger.size();
 CHECK(grant_item_holder_item(d,id,s,random,ledger,clock,result,error));
 CHECK(result.kind==ItemHolderGrantKind::Full&&result.target==ItemHolderGrantTarget::Preview&&result.character_index==UINT32_MAX);
 CHECK(equal(s,before)&&ledger.size()==old_count+1&&result.item.uid==ledger.back());
 const auto preview=result.item.uid;CHECK(grant_item_holder_item(d,id,s,random,ledger,clock,result,error));
 CHECK(equal(s,before)&&ledger.size()==old_count+2&&result.item.uid!=preview);
 s=session(d,d.info().capacity,d.info().capacity);CHECK(grant_item_holder_item(key_data,key->id,s,random,ledger,clock,result,error));
 CHECK(result.kind==ItemHolderGrantKind::Acquired&&result.target==ItemHolderGrantTarget::Keys&&s.key_items.size()==1);
 CHECK(s.characters[0].inventory.size()==d.info().capacity&&s.characters[1].inventory.size()==d.info().capacity&&equal(s.key_items[0],result.item));
 // The source's key inventory has no normal-inventory capacity restriction.
 for(uint32_t i=0;i<d.info().capacity+1;++i)s.key_items.push_back({key->item,false,1,5000+i});
 const auto keys=s.key_items.size();CHECK(grant_item_holder_item(key_data,key->id,s,random,ledger,clock,result,error));CHECK(s.key_items.size()==keys+1);
 const auto reject=[&](DroppedItemsData&data,uint32_t definition,SessionSnapshot&state,const LoadRngClockProvider&provider){
  const auto saved=state;const auto saved_ledger=ledger;const auto saved_random=random;const auto saved_result=result;
  CHECK(!grant_item_holder_item(data,definition,state,random,ledger,provider,result,error));CHECK(!error.empty());
  CHECK(equal(state,saved)&&ledger==saved_ledger&&random.state()==saved_random.state()&&random.raw_draw_count()==saved_random.raw_draw_count());
  CHECK(result.kind==saved_result.kind&&result.target==saved_result.target&&result.character_index==saved_result.character_index&&equal(result.item,saved_result.item));
 };
 const auto old_reads=reads;DroppedItemsData missing;s=session(d);reject(missing,id,s,clock);reject(d,0,s,clock);CHECK(reads==old_reads);
 s=session(d);s.party[1]=s.party[0];reject(d,id,s,clock);
 s=session(d);s.party[0]="missing-holder";reject(d,id,s,clock);
 s=session(d);s.characters[1].character_id=s.characters[0].character_id;reject(d,id,s,clock);
 s=session(d,d.info().capacity+1,0);reject(d,id,s,clock);
 s=session(d,1,1);s.characters[1].inventory[0].uid=s.characters[0].inventory[0].uid;reject(d,id,s,clock);
 s=session(d,1,0);s.characters[0].inventory[0].doses=0;reject(d,id,s,clock);
 s=session(d);reject(d,id,s,LoadRngClockProvider{});
 LoadRngClockProvider failed=[](LoadRngClockSample&,std::string&e){e="source clock unavailable";return false;};
 s=session(d);reject(d,id,s,failed);s=session(d,d.info().capacity,d.info().capacity);reject(d,id,s,failed);
 // Even a clock failure on the Full preview must leave the generated ledger.
 CHECK(error=="source clock unavailable");
 SourceRandom prediction=random;auto predicted_ledger=ledger;std::vector<LoadUidAllocation>trace;
 CHECK(apply_load_uid_allocations(prediction,predicted_ledger,{{id,1}},clock,error,&trace)&&trace.size()==1);
 s=session(d,1,0);s.characters[0].inventory[0].uid=trace[0].generated_uid;reject(d,id,s,clock);
 // The preview is not an owned identity, so this same saved-UID coincidence is
 // legal on the source Full branch and must not add another inventory item.
 s=session(d,d.info().capacity,d.info().capacity);s.characters[0].inventory[0].uid=trace[0].generated_uid;
 before=s;CHECK(grant_item_holder_item(d,id,s,random,ledger,clock,result,error));CHECK(equal(s,before)&&result.kind==ItemHolderGrantKind::Full);
 // A failed/reloaded resource context cannot be used after the clock callback.
 s=session(d);DroppedItemsData mutable_data;CHECK(mutable_data.load_file(argv[1],error));
 LoadRngClockProvider reload=[&](LoadRngClockSample&sample,std::string&e){sample={100,200};return mutable_data.load_file(argv[1],e);};
 reject(mutable_data,id,s,reload);
 CHECK(mutable_data.generation()==2);
 // A provider can invalidate the selected live holder without changing the
 // resource generation. Reject before touching its retired inventory storage.
 s=session(d);const auto prior_random=random;const auto prior_ledger=ledger;const auto prior_output=result;
 LoadRngClockProvider retire=[&](LoadRngClockSample&sample,std::string&e){sample={100,200};e.clear();std::vector<SessionCharacter>().swap(s.characters);return true;};
 CHECK(!grant_item_holder_item(d,id,s,random,ledger,retire,result,error));CHECK(s.characters.empty()&&random.state()==prior_random.state()&&random.raw_draw_count()==prior_random.raw_draw_count()&&ledger==prior_ledger&&equal(result.item,prior_output.item));
 s=session(d);LoadRngClockProvider move_rng=[&](LoadRngClockSample&sample,std::string&e){sample={100,200};e.clear();random.randi();return true;};
 CHECK(!grant_item_holder_item(d,id,s,random,ledger,move_rng,result,error));CHECK(s.characters[0].inventory.empty()&&ledger==prior_ledger&&random.raw_draw_count()==prior_random.raw_draw_count()+1);
 s=session(d);LoadRngClockProvider change_ledger=[&](LoadRngClockSample&sample,std::string&e){sample={100,200};e.clear();ledger.push_back(123);return true;};const auto before_ledger=ledger;
 CHECK(!grant_item_holder_item(d,id,s,random,ledger,change_ledger,result,error));CHECK(s.characters[0].inventory.empty()&&ledger.size()==before_ledger.size()+1&&ledger.back()==123);
 std::cout<<checks<<" ItemHolder grant checks passed\n";
}
