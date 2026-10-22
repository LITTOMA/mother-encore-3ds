#include "encore/storage_menu.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <tuple>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Catalog storage failed at %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static std::vector<uint32_t>uids(const std::vector<ItemInstance>&items){std::vector<uint32_t>result;for(const auto&item:items)result.push_back(item.id);return result;}
int main(int argc,char**argv){
 std::string error;CHECK(argc==2||argc==3);const std::string root=argv[1];
 ItemData items,old_items;StorageData data,old;
 CHECK(items.load_file((root+"/candidate.encitems").c_str(),error));CHECK(old_items.load_file((root+"/legacy.encitems").c_str(),error));
 CHECK(data.load_file((root+"/candidate.encstorage").c_str(),error));CHECK(old.load_file((root+"/legacy.encstorage").c_str(),error));
 const auto view=data.view();CHECK(view.bind_items(items.view(),error));CHECK(old.view().bind_items(old_items.view(),error));
 CHECK(!view.bind_items(old_items.view(),error));CHECK(view.count(StorageSection::Policies)==7);CHECK(old.view().count(StorageSection::Policies)==4);
 auto policy=[&](const std::string&source,StoragePolicy&result){for(uint32_t i=0;i<view.count(StorageSection::Policies);++i){const auto row=view.policy(i);if(view.string(row.source)==source){result=row;return true;}}return false;};
 std::ifstream input(argc==3?argv[2]:root+"/original.txt");std::string magic;uint32_t version=0,rows=0,pairs=0;
 CHECK(bool(input>>magic>>version>>rows>>pairs));CHECK(magic=="ENCSORT1"&&version==1&&rows==584&&pairs==98);
 std::set<std::tuple<std::string,std::string,std::string>>seen_pairs;
 for(uint32_t i=0;i<pairs;++i){
  std::string locale,a,b;uint32_t less=2;CHECK(bool(input>>locale>>a>>b>>less));CHECK((locale=="en"||locale=="zh_Hans_CN")&&less<=1);
  CHECK(seen_pairs.insert({locale,a,b}).second);StoragePolicy pa,pb;CHECK(policy(a,pa));CHECK(policy(b,pb));
  CHECK(bool(less)==((locale=="en"?pa.rank_en:pa.rank_zh)>(locale=="en"?pb.rank_en:pb.rank_zh)));
 }
 uint32_t en_index=0,zh_index=0;
 for(uint32_t row=0;row<rows;++row){
  std::string locale;uint32_t index=UINT32_MAX,count=0;CHECK(bool(input>>locale>>index>>count));
  CHECK(locale=="en"||locale=="zh_Hans_CN");CHECK(index==(locale=="en"?en_index++:zh_index++));CHECK(count>0&&count<=16);
  std::vector<ItemInstance>before;std::set<uint32_t>identities;
  for(uint32_t i=0;i<count;++i){
   std::string name;ItemInstance item;StoragePolicy p;CHECK(bool(input>>name>>item.id>>item.doses));CHECK(policy(name,p));
   item.definition=p.definition;item.equipped=0;CHECK(identities.insert(item.id).second);CHECK(item.doses>0&&item.doses<=p.doses);before.push_back(item);
  }
  std::vector<uint32_t>once(count),twice(count);for(auto&id:once)CHECK(bool(input>>id));for(auto&id:twice)CHECK(bool(input>>id));
  for(const auto&state:{once,twice})CHECK(std::set<uint32_t>(state.begin(),state.end())==identities);
  StorageState stored;InventoryState on_hand;CHECK(stored.restore(items.view(),uint32_t(view.parameter(StorageParameter::StorageCapacity)),before,error));
  CHECK(on_hand.restore(items.view(),{},error));
  // Valid stat service double: no equipment is enabled during these transfers.
  BattleSessionStats stats;stats.maxhp=stats.hp=1;StorageMenu menu;
  const StorageMenu::DoseAdmission doses=[](const ItemInstance&item,uint32_t limit){return item.doses>0&&item.doses<=limit;};
  CHECK(menu.initialize(view,on_hand,stored,stats,locale=="zh_Hans_CN",doses));CHECK(menu.open());CHECK(uids(stored.instances())==once);
  CHECK(stored.sort(view,locale=="zh_Hans_CN",error));CHECK(uids(stored.instances())==twice);
  for(const auto&item:stored.instances()){
   auto original=std::find_if(before.begin(),before.end(),[&](const ItemInstance&value){return value.id==item.id;});
   CHECK(original!=before.end());CHECK(original->definition==item.definition&&original->doses==item.doses&&!item.equipped);
  }
  while(stored.size()){
   CHECK(menu.input(1,0,false,false));CHECK(menu.input(0,0,true,false));
   if(menu.phase()==StorageMenuPhase::AskEquip)CHECK(menu.input(0,0,false,true));
  }
  CHECK(uids(on_hand.instances())==once);
  while(on_hand.size()){CHECK(menu.input(-1,0,false,false));CHECK(menu.input(0,0,true,false));}
  CHECK(uids(stored.instances())==once);CHECK(stats.maxhp==1&&stats.hp==1);
  for(uint32_t i=0;i<view.count(StorageSection::Policies);++i){
   const auto p=view.policy(i);if(p.max_count<=1||uint32_t(std::count_if(before.begin(),before.end(),[&](const ItemInstance&item){return item.definition==p.definition;}))!=p.max_count)continue;
   ItemInstance extra;extra.id=50000+i;extra.definition=p.definition;extra.equipped=0;extra.doses=p.doses;
   InventoryState too_many;CHECK(too_many.restore(items.view(),{extra},error));StorageMenu rejected;
   const auto retained=uids(stored.instances());CHECK(!rejected.initialize(view,too_many,stored,stats,false,doses));
   CHECK(uids(stored.instances())==retained);CHECK(too_many.size()==1&&too_many.instance(0).id==extra.id);
  }
 }
 CHECK(en_index==292&&zh_index==292);std::string trailing;CHECK(!(input>>trailing)&&input.eof());
 std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/candidate.encstorage").c_str(),bytes,8*1024*1024,error));
 const auto retained=data.view();for(size_t size=0;size<bytes.size();++size){CHECK(!data.load(bytes.data(),size,error));CHECK(data.view().same_content(retained));}
 std::ifstream negative_file(root+"/negative-count.txt");uint32_t negative_count=0;CHECK(bool(negative_file>>negative_count)&&negative_count>=8);
 for(uint32_t i=0;i<negative_count;++i){std::vector<uint8_t>bad;CHECK(encore::read_file((root+"/bad-"+std::to_string(i)+".encstorage").c_str(),bad,8*1024*1024,error));CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().same_content(retained));}
 CHECK(data.load(bytes.data(),bytes.size(),error));
 std::printf("Catalog storage: %u checks; 98 original comparisons, 584 complete inventories, two sorts and all deposit/withdraw UID/dose preservation; %u negative variants\n",checks,negative_count);
}
