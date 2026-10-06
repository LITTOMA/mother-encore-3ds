// Manual source consumer checks. Compile/execution are separate; no CI registration.
#include "encore/field_item_definitions.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
static uint32_t crc(const std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=i>=16&&i<20?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return~c;}
static void patch(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned j=0;j<4;++j)b[p+j]=uint8_t(v>>(j*8));const auto h=crc(b);for(unsigned j=0;j<4;++j)b[16+j]=uint8_t(h>>(j*8));}
int main(int argc,char**argv){assert(argc==2);FieldItemDefinitions d;std::string e;assert(d.load_file(argv[1],e));// The reviewed source startup inventories add CashCard and FavFood to the 17 map-derived definitions.
assert(d.definitions().size()==19&&d.bindings().size()==36);const auto*spray=d.definition("AsthmaSpray");const auto*card=d.definition("PhoneCard");const auto*cap=d.legacy(1,1);assert(spray&&card&&cap&&spray->doses==3&&card->doses==10&&cap->item_name=="BaseballCap");for(const auto&item:d.definitions())for(const auto&a:item.actions)assert(a.pending);
 FieldItemSnapshot state{{{1,0,{}},{2,0,{}},{3,1,{}},{4,2,{}}},{1,2}};uint64_t samples=0;bool reject=false;FieldItemResult committed;FieldItemDefinitionsHost h;h.bind=[](const auto&data,std::string&){return data.valid();};h.read=[&](auto&out,std::string&){out=state;return true;};h.commit=[&](const auto&,const auto&after,const auto&result,std::string&){if(reject)return false;state=after;committed=result;return true;};SourceRandom random(11);std::vector<uint32_t>ledger;auto clock=[&](LoadRngClockSample&v,std::string&){v={100,++samples};return true;};FieldItemDefinitionsRuntime r;assert(r.initialize(d,random,ledger,clock,h,e));const FieldItemBinding*ordinary=nullptr;const FieldItemBinding*phone=nullptr;for(const auto&b:d.bindings()){if(b.kind==FieldItemBindingKind::Present&&!d.definition(b.definition)->keyitem())ordinary=&b;if(b.kind==FieldItemBindingKind::Payphone)phone=&b;}assert(ordinary&&phone);FieldItemResult result;assert(r.select_holder(ordinary->kind,ordinary->scene,ordinary->object_id,true,result,e)&&result.owner==1&&ledger.size()==1&&samples==1);const auto uid=result.item.uid;assert(r.transfer(uid,2,result,e)&&result.item.uid==uid&&result.owner==2&&ledger.size()==1);assert(r.drop(uid,result,e)&&result.kind==FieldItemResultKind::Dropped&&ledger.size()==1);
 // UID zero is valid saved identity; drop/reduce preserve identity and source doses.
 state.inventories[0].items={{card->id,0,card->doses,false}};assert(r.validate_snapshot(state,e));bool found=false;assert(r.query(phone->kind,phone->scene,phone->object_id,found,result,e)&&found&&result.item.uid==0);assert(r.reduce_or_drop(0,result,e)&&result.item.doses==card->doses-d.dose_step()&&result.item.uid==0);state.inventories[0].items[0].doses=1;assert(r.reduce_or_drop(0,result,e)&&state.inventories[0].items.empty());
 // All active party inventories full: holder allocates transient/global.item, no owner.
 for(size_t n=0;n<2;++n)for(uint32_t i=0;i<d.capacity(0);++i)state.inventories[n].items.push_back({cap->id,uint32_t(1000+n*100+i),cap->doses,false});const auto count=ledger.size();assert(r.select_holder(ordinary->kind,ordinary->scene,ordinary->object_id,false,result,e)&&result.kind==FieldItemResultKind::Transient&&result.owner==0&&ledger.size()==count+1);auto before=random.state();const auto size=ledger.size();assert(!r.select_holder(ordinary->kind,ordinary->scene,ordinary->object_id,true,result,e)&&random.state()==before&&ledger.size()==size);
 // Original typed phrase grants a key even with full member inventories. Flag is caller-owned.
 const auto*p=d.programme("Podunk/woof_key","4");assert(p);assert(r.grant_programme(p->program,p->label,result,e)&&state.inventories[2].items.back().definition==p->definition);assert(!r.grant_programme(p->program,"5",result,e));
 state.inventories[0].items.clear();reject=true;before=random.state();const auto oldledger=ledger;assert(!r.select_holder(ordinary->kind,ordinary->scene,ordinary->object_id,true,result,e)&&random.state()==before&&ledger==oldledger&&state.inventories[0].items.empty());reject=false;
 auto bad=state;bad.inventories[0].items={{0,123,1,false}};assert(!r.validate_snapshot(bad,e));bad=state;bad.party_order={3};assert(!r.validate_snapshot(bad,e));bad=state;bad.inventories[0].items={{card->id,42,card->doses+1,false}};assert(!r.validate_snapshot(bad,e));FieldItemDefinitionsRuntime missing;auto incomplete=h;incomplete.commit={};assert(!missing.initialize(d,random,ledger,clock,incomplete,e));
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>raw((std::istreambuf_iterator<char>(f)),{});for(auto offset:{size_t(8),size_t(20),size_t(24),size_t(28),size_t(52),size_t(60)}){auto b=raw;patch(b,offset,999);assert(!d.load(b.data(),b.size(),e)&&d.valid());}auto b=raw;b.push_back(0);assert(!d.load(b.data(),b.size(),e)&&d.valid());b=raw;patch(b,96,999);assert(!d.load(b.data(),b.size(),e)&&d.valid());
 // Older schema/capability cannot reinterpret the six source-owned coverage
 // counts as definition data. Failed admission retains the previous pack.
 for (const auto offset : {size_t(8), size_t(20)}) {
  auto legacy=raw;patch(legacy,offset,1);assert(!d.load(legacy.data(),legacy.size(),e)&&d.valid());
 }
 for (size_t offset=100;offset<124;offset+=4) {
  auto coverage=raw;patch(coverage,offset,0);assert(!d.load(coverage.data(),coverage.size(),e)&&d.valid());
 }
 auto mismatch=raw;patch(mismatch,100,18);assert(!d.load(mismatch.data(),mismatch.size(),e)&&d.valid());
}
