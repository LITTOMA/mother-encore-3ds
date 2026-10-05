#include "encore/storage_menu.hpp"
#include "encore/native_session.hpp"
#include "encore/session_restore.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Storage check failed at %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static uint32_t get(const std::vector<uint8_t>&b,size_t n){return b[n]|uint32_t(b[n+1])<<8|uint32_t(b[n+2])<<16|uint32_t(b[n+3])<<24;}
static void put(std::vector<uint8_t>&b,size_t n,uint32_t v){for(unsigned i=0;i<4;++i)b[n+i]=uint8_t(v>>(i*8));}
static void fix(std::vector<uint8_t>&b){put(b,16,0);put(b,16,encore::crc32(b.data(),b.size()));}
// Manual compatibility fixture conversion, never a release pack producer.
static std::vector<uint8_t>legacy_exact(const std::vector<uint8_t>&source){
 std::vector<uint8_t>out(source.begin(),source.begin()+176);put(out,20,1);
 for(uint32_t i=0;i<7;++i){while(out.size()%4)out.push_back(0);const size_t directory=64+i*16;const auto start=get(source,directory+4),count=get(source,directory+8),stride=get(source,directory+12);put(out,directory+4,uint32_t(out.size()));put(out,directory+12,i==1?24:stride);for(uint32_t j=0;j<count;++j)out.insert(out.end(),source.begin()+start+j*stride,source.begin()+start+j*stride+(i==1?24:stride));}
 put(out,12,uint32_t(out.size()));fix(out);return out;
}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs/data";std::string error;
 ItemData items;StorageData pack;NativeSessionData data;RoomData room;HouseData house;BattleRoundData round;BattleData font;
 CHECK(items.load_file((root+"/opening.encitems").c_str(),error));CHECK(pack.load_file((root+"/opening.encstorage").c_str(),error));CHECK(data.load_file((root+"/opening.encsession").c_str(),error));CHECK(room.load_file((root+"/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));CHECK(round.load_file((root+"/doll-entry.encround").c_str(),error));CHECK(font.load_file((root+"/opening.encbattle").c_str(),error));
 const auto v=pack.view();CHECK(v.bind_items(items.view(),error));CHECK(v.parameter(StorageParameter::StorageCapacity)==64);CHECK(v.parameter(StorageParameter::InventoryCapacity)==16);CHECK(data.storage_capacity()==64);
 InventoryState inv;StorageState storage;CHECK(inv.initialize(items.view()));CHECK(storage.initialize(items.view(),data.storage_capacity()));
 const auto cap=inv.instance(0);const auto initial=data.defaults();const auto row=data.levels().front();BattleSessionStats stats;stats.level=row.level;stats.hp=initial.characters[0].hp;stats.pp=initial.characters[0].pp;stats.maxhp=row.stats[0];stats.maxpp=row.stats[1];stats.offense=row.stats[2];stats.defense=row.stats[3];stats.speed=row.stats[4];stats.iq=row.stats[5];stats.guts=row.stats[6];
 StorageMenu menu;CHECK(menu.initialize(v,inv,storage,stats));CHECK(menu.open());CHECK(menu.input(0,0,true,false));CHECK(menu.phase()==StorageMenuPhase::AskUnequip);CHECK(inv.size()==1&&storage.size()==0&&stats.defense==row.stats[3]);
 // Declining unequip keeps both containers, equipment, UID and derived stats.
 CHECK(menu.input(0,1,false,false));CHECK(!menu.question_yes());CHECK(menu.input(0,0,true,false));CHECK(inv.instance(0).equipped&&inv.instance(0).id==cap.id&&storage.size()==0);
 CHECK(menu.input(0,0,true,false));CHECK(menu.input(0,0,true,false));CHECK(inv.size()==0&&storage.size()==1&&storage.instance(0).id==cap.id&&!storage.instance(0).equipped);
 auto equip=v.equipment(0);CHECK(stats.defense==row.stats[3]-equip.boosts[3]);
 // Withdrawal moves the actual UID before asking equip; cancel keeps item.
 CHECK(menu.input(1,0,false,false));CHECK(menu.storage_panel());CHECK(menu.input(0,0,true,false));CHECK(menu.phase()==StorageMenuPhase::AskEquip);CHECK(inv.size()==1&&storage.size()==0&&inv.instance(0).id==cap.id&&!inv.instance(0).equipped);
 CHECK(menu.input(0,0,false,true));CHECK(!inv.instance(0).equipped&&stats.defense==row.stats[3]-equip.boosts[3]);
 CHECK(menu.input(-1,0,false,false));CHECK(menu.input(0,0,true,false));CHECK(storage.size()==1&&inv.size()==0);CHECK(menu.input(1,0,false,false));CHECK(menu.input(0,0,true,false));CHECK(menu.input(0,0,true,false));CHECK(inv.instance(0).equipped&&stats.defense==row.stats[3]);
 // Source score sorting places asthma spray ahead of equipment; no UID draw.
 const auto&p=data.acquisitions().front();uint32_t spray=item_no_index;for(uint32_t i=0;i<items.view().count(ItemSection::Definitions);++i)if(items.view().string(items.view().definition(i).source)==p.item_id)spray=i;
 CHECK(spray!=item_no_index);CHECK(inv.append(spray,p.doses,0,error));CHECK(menu.input(-1,0,false,false));CHECK(menu.input(0,1,false,false));CHECK(menu.input(0,0,true,false));CHECK(storage.size()==1&&storage.instance(0).id==0);CHECK(menu.input(0,-1,false,false));CHECK(menu.input(0,0,true,false));CHECK(menu.input(0,0,true,false));CHECK(storage.size()==2&&storage.instance(0).definition==spray&&storage.instance(1).id==cap.id);
 auto snapshot=initial;for(auto&f:snapshot.flags)if(f.id==p.flag_id)f.value=true;
 NativeSnapshotInput input{snapshot,&stats,&inv,&storage};SessionSnapshot captured;CHECK(build_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),input,captured,error));CHECK(captured.characters[0].inventory.empty()&&captured.storage.size()==2&&captured.storage[0].uid==0&&captured.storage[1].uid==cap.id);
 std::vector<uint8_t>bytes;CHECK(encode_session_save(captured,data.compatibility(),bytes,error));SessionSnapshot decoded;CHECK(decode_session_save(bytes.data(),bytes.size(),data.compatibility(),decoded,error));PreparedSessionRestore prepared;CHECK(prepare_session_restore(data,room.view(),house.view(),round.view(),items.view(),font.view(),decoded,prepared,error));CHECK(prepared.storage.size()==2&&prepared.inventory.size()==0&&prepared.stats.defense==stats.defense);
 auto validate=[&](const SessionSnapshot&s){return validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),s,error);};
 auto bad=captured;bad.storage.pop_back();CHECK(!validate(bad));bad=captured;bad.storage.back().equipped=true;CHECK(!validate(bad));bad=captured;bad.storage.front().doses++;CHECK(!validate(bad));bad=captured;bad.storage.push_back(bad.storage.front());bad.storage.back().uid=42;CHECK(!validate(bad));bad=captured;for(auto&f:bad.flags)if(f.id==p.flag_id)f.value=false;CHECK(!validate(bad));bad=captured;bad.storage.front().item_id="unknown";CHECK(!validate(bad));
 const auto oldstorage=storage.instances();auto duplicate=oldstorage;duplicate.push_back(duplicate.front());CHECK(!storage.restore(items.view(),64,duplicate,error));CHECK(storage.size()==oldstorage.size());CHECK(!storage.append({42,spray,1,p.doses},error));CHECK(!storage.append({42,spray,0,0},error));
 // Detached binary owner rejects malformed versions/UTF-8/bindings/capacities.
 CHECK(encore::read_file((root+"/opening.encstorage").c_str(),bytes,8*1024*1024,error));const auto retained=pack.view();
 for(size_t n=0;n<bytes.size();++n)CHECK(!pack.load(bytes.data(),n,error));CHECK(pack.view().same_content(retained));
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,3},{20,1},{24,2},{28,8},{52,1},{64,8},{76,2}}){auto b=bytes;put(b,entry.first,entry.second);fix(b);CHECK(!pack.load(b.data(),b.size(),error));CHECK(pack.view().same_content(retained));}
 auto b=bytes;b[get(b,68)+1]=0xff;fix(b);CHECK(!pack.load(b.data(),b.size(),error));
 const auto policy=get(bytes,64+16+4);b=bytes;put(b,policy+8,0);fix(b);CHECK(!pack.load(b.data(),b.size(),error));
 b=bytes;put(b,policy+24,0);fix(b);CHECK(!pack.load(b.data(),b.size(),error));b=bytes;put(b,policy+24,get(b,policy+8)+1);fix(b);CHECK(!pack.load(b.data(),b.size(),error));
 const auto equipment=get(bytes,64+6*16+4);b=bytes;put(b,equipment,99);fix(b);CHECK(!pack.load(b.data(),b.size(),error));
 StorageData legacy;const auto legacy_bytes=legacy_exact(bytes);CHECK(legacy.load(legacy_bytes.data(),legacy_bytes.size(),error));for(uint32_t i=0;i<legacy.view().count(StorageSection::Policies);++i)CHECK(legacy.view().policy(i).min_doses==legacy.view().policy(i).doses);InventoryState legacy_inv;StorageState legacy_store;CHECK(legacy_inv.restore(items.view(),{{0,spray,0,p.doses}},error));CHECK(legacy_store.initialize(items.view(),64));auto legacy_stats=stats;StorageMenu legacy_menu;CHECK(legacy_menu.initialize(legacy.view(),legacy_inv,legacy_store,legacy_stats));CHECK(legacy_inv.restore(items.view(),{{0,spray,0,1}},error));CHECK(!legacy_menu.initialize(legacy.view(),legacy_inv,legacy_store,legacy_stats));
 StorageData foreign;b=bytes;b[32]^=1;fix(b);CHECK(foreign.load(b.data(),b.size(),error));CHECK(!foreign.view().bind_items(items.view(),error));
 auto wrongstats=stats;wrongstats.defense++;input.stats=&wrongstats;CHECK(!build_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),input,decoded,error));CHECK(decoded.storage.size()==captured.storage.size());
 // A consumed spray retains its exact UID/doses through normal deposit,
 // withdrawal and save/restore. The equipment policy remains exact one dose.
 InventoryState partial;StorageState partial_storage;CHECK(partial.restore(items.view(),{{0,spray,0,1}},error));CHECK(partial_storage.initialize(items.view(),64));auto partial_stats=stats;StorageMenu partial_menu;CHECK(partial_menu.initialize(v,partial,partial_storage,partial_stats));CHECK(partial_menu.open());CHECK(partial_menu.input(0,0,true,false));CHECK(partial.size()==0&&partial_storage.size()==1&&partial_storage.instance(0).id==0&&partial_storage.instance(0).doses==1);CHECK(partial_menu.input(1,0,false,false));CHECK(partial_menu.input(0,0,true,false));CHECK(partial.size()==1&&partial.instance(0).id==0&&partial.instance(0).doses==1&&partial_storage.size()==0);
 auto partial_save=captured;partial_save.storage[0].doses=1;CHECK(validate(partial_save));CHECK(encode_session_save(partial_save,data.compatibility(),bytes,error));CHECK(decode_session_save(bytes.data(),bytes.size(),data.compatibility(),decoded,error));CHECK(prepare_session_restore(data,room.view(),house.view(),round.view(),items.view(),font.view(),decoded,prepared,error));CHECK(prepared.storage.instance(0).id==0&&prepared.storage.instance(0).doses==1);
 auto partial_bad=partial_save;partial_bad.storage[0].doses=0;CHECK(!validate(partial_bad));partial_bad=partial_save;partial_bad.storage[0].doses=p.doses+1;CHECK(!validate(partial_bad));CHECK(partial.restore(items.view(),{{cap.id,cap.definition,0,2}},error));CHECK(!partial_menu.initialize(v,partial,partial_storage,partial_stats));
 std::printf("Storage original transfer/save checks: %u\n",checks);
}
