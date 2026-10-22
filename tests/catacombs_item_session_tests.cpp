#include "encore/native_session.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

using namespace encore::upstream;
static unsigned checks=0;
static std::string error;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"Catalog session check %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);} } while(0)
static std::vector<uint8_t>read(const std::string&p){std::ifstream f(p,std::ios::binary);CHECK(bool(f));return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}

int main(int argc,char**argv){
 CHECK(argc==3);const std::string root=argv[1],fixture=argv[2];
 NativeSessionData data,legacy,production;RoomData room;HouseData house;BattleRoundData round;ItemData items,old_items,production_items;
 CHECK(data.load_file((fixture+"/candidate.encsession").c_str(),error));
 CHECK(legacy.load_file((fixture+"/legacy11.encsession").c_str(),error));
 CHECK(production.load_file((root+"/session/opening.encsession").c_str(),error));
 CHECK(room.load_file((root+"/world/house/opening.encroom").c_str(),error));
 CHECK(house.load_file((root+"/world/house/opening.enchouse").c_str(),error));
 CHECK(round.load_file((root+"/battle/doll/doll-entry.encround").c_str(),error));
 CHECK(items.load_file((fixture+"/candidate.encitems").c_str(),error));
 CHECK(old_items.load_file((fixture+"/legacy.encitems").c_str(),error));
 CHECK(production_items.load_file((root+"/ui/opening.encitems").c_str(),error));
 CHECK(read(root+"/session/opening.encsession")==read(fixture+"/candidate.encsession"));
 CHECK(read(root+"/ui/opening.encitems")==read(fixture+"/candidate.encitems"));
 auto valid=[&](const SessionSnapshot&s){
  const bool candidate=validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),s,error);
  CHECK(validate_native_session_snapshot(production,room.view(),house.view(),round.view(),production_items.view(),s,error)==candidate);
  return candidate;
 };
 CHECK(valid(data.defaults()));
 std::vector<NativeSessionAcquisition>grants;
 for(const auto&a:data.acquisitions())if(a.flag_policy==2&&a.flag_id.rfind("Catacombs/",0)==0)grants.push_back(a);
 CHECK(grants.size()==7);
 unsigned roundtrips=0;
 for(unsigned mask=0;mask<(1u<<grants.size());++mask){
  auto base=data.defaults();std::vector<std::string>identities;
  for(size_t i=0;i<grants.size();++i){base.object_flags.push_back({grants[i].flag_id,bool(mask&(1u<<i))});if(std::find(identities.begin(),identities.end(),grants[i].item_id)==identities.end())identities.push_back(grants[i].item_id);}
  CHECK(valid(base));
  for(const auto&id:identities){
   const auto available=std::count_if(grants.begin(),grants.end(),[&](const NativeSessionAcquisition&a){auto at=size_t(&a-grants.data());return a.item_id==id&&(mask&(1u<<at));});
   for(unsigned count=1;count<=unsigned(available)+1;++count)for(unsigned split=0;split<=count;++split){
    auto state=base;
    for(unsigned n=0;n<count;++n){SessionItem item{id,false,1,uint32_t(1000+n)};if(n<split)state.characters[0].inventory.push_back(item);else state.storage.push_back(item);}
    CHECK(valid(state)==(count<=unsigned(available)));
    if(count==unsigned(available)&&split==0){
     std::vector<uint8_t>bytes;SessionSnapshot loaded;
     CHECK(encode_session_save(state,data.compatibility(),bytes,error));CHECK(bytes.size()>12&&bytes[8]==1&&bytes[9]==0);
     CHECK(decode_session_save(bytes.data(),bytes.size(),data.compatibility(),loaded,error));CHECK(valid(loaded));
     CHECK(loaded.storage.size()==state.storage.size());for(size_t k=0;k<loaded.storage.size();++k)CHECK(loaded.storage[k].uid==state.storage[k].uid);
     ++roundtrips;
    }
   }
   if(available){
    auto state=base;state.characters[0].inventory.push_back({id,false,2,2000});CHECK(!valid(state));
    state.characters[0].inventory.back().doses=1;state.characters[0].inventory.back().equipped=true;CHECK(!valid(state));
   }
  }
 }
 CHECK(roundtrips>0);
 // Removing/consuming all picked items preserves the seven one-time flags.
 auto consumed=data.defaults();for(const auto&a:grants)consumed.object_flags.push_back({a.flag_id,true});CHECK(valid(consumed));
 auto unknown=consumed;unknown.object_flags.push_back({"Catacombs/Unreviewed",true});CHECK(!valid(unknown));
 auto duplicate=consumed;duplicate.object_flags.push_back(duplicate.object_flags.front());CHECK(!valid(duplicate));
 // The legacy payload is byte-identical. Only the independent resource header
 // changes for a v12 resource without grouped additions.
 auto v11=read(fixture+"/legacy11.encsession"),v12=read(fixture+"/legacy12.encsession");CHECK(v11.size()==v12.size());CHECK(std::equal(v11.begin()+24,v11.end(),v12.begin()+24));
 NativeSessionData same;CHECK(same.load(v12.data(),v12.size(),error));CHECK(validate_native_session_snapshot(same,room.view(),house.view(),round.view(),old_items.view(),same.defaults(),error));
 auto source=read(fixture+"/candidate.encsession");
 for(size_t n=0;n<source.size();++n){CHECK(!data.load(source.data(),n,error));CHECK(data.acquisitions().size()==legacy.acquisitions().size()+grants.size());}
 unsigned negative_count=0;{std::ifstream f(fixture+"/negative-count.txt");CHECK(bool(f>>negative_count));}CHECK(negative_count>=14);
 for(unsigned n=0;n<negative_count;++n){auto bad=read(fixture+"/bad-"+std::to_string(n)+".encsession");CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.valid());CHECK(valid(consumed));}
 std::printf("Catalog grouped acquisitions: %u checks, %u legacy-save roundtrips; no scene/emulator claim\n",checks,roundtrips);
}
