#include "encore/session_migration.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool v,const char*x,int line,const std::string&e){++checks;if(!v){std::fprintf(stderr,"Migration line %d: %s; %s\n",line,x,e.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x,__LINE__,error)
std::vector<uint8_t>read(const std::string&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void put(std::vector<uint8_t>&b,size_t offset,uint32_t v){for(unsigned i=0;i<4;++i)b[offset+i]=uint8_t(v>>(8*i));}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
void fix(std::vector<uint8_t>&b){put(b,16,crc(b.data()+24,b.size()-24));}
struct ReadOnlyFiles:SessionSaveFileOps {
 std::map<std::string,std::vector<uint8_t>>files;unsigned reads=0,writes=0;
 SessionFileReadResult read(const std::string&p,size_t,std::vector<uint8_t>&out,std::string&e)override{++reads;auto i=files.find(p);if(i==files.end()){e="fixture missing";return SessionFileReadResult::Missing;}out=i->second;e.clear();return SessionFileReadResult::Ok;}
 bool write_new(const std::string&,const std::vector<uint8_t>&,std::string&)override{++writes;return false;}
 bool replace(const std::string&,const std::string&,std::string&)override{++writes;return false;}
 bool remove(const std::string&,std::string&)override{++writes;return false;}
};
struct MemoryFiles:ReadOnlyFiles {
 bool write_new(const std::string&p,const std::vector<uint8_t>&b,std::string&e)override{++writes;if(files.count(p)){e="fixture exists";return false;}files[p]=b;return true;}
 bool replace(const std::string&a,const std::string&b,std::string&e)override{++writes;auto i=files.find(a);if(i==files.end()){e="fixture missing";return false;}files[b]=i->second;files.erase(a);return true;}
 bool remove(const std::string&p,std::string&)override{++writes;files.erase(p);return true;}
};
}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs/data",legacy_root=argc>2?argv[2]:"content/legacy-rules6";std::string error;
 NativeSessionData data,old;RoomData room;HouseData house;BattleRoundData round;ItemData items;BattleData font;SessionMigrationData migration;
 CHECK(data.load_file((root+"/opening.encsession").c_str(),error));CHECK(old.load_file((legacy_root+"/opening.encsession").c_str(),error));
 CHECK(room.load_file((root+"/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/opening.enchouse").c_str(),error));
 CHECK(round.load_file((root+"/opening.encround").c_str(),error));CHECK(items.load_file((root+"/opening.encitems").c_str(),error));CHECK(font.load_file((root+"/opening.encbattle").c_str(),error));
 const auto policy=read(root+"/rules6-to7.encmigration");CHECK(migration.load(policy.data(),policy.size(),error));
 auto bytes=[&](const SessionSnapshot&s,SessionSaveCompatibility c){std::vector<uint8_t>b;CHECK(encode_session_save(s,c,b,error));return b;};
 auto prepare=[&](const std::vector<uint8_t>&b,PreparedSessionRestore&out,bool&changed){return prepare_compatible_session_restore(b.data(),b.size(),migration,data,room.view(),house.view(),round.view(),items.view(),font.view(),out,error,&changed);};
 PreparedSessionRestore prepared;bool migrated=false;
 auto saved=old.defaults();auto&c=saved.characters.front();const auto&level=old.levels().back();c.level=level.level;c.experience=level.minimum_exp+1;c.hp=19;c.pp=3;c.learned_skills=level.skills;c.nickname="Ana";c.inventory.front().uid=0;
 saved.key_items.front().uid=UINT32_MAX;saved.player_name="Player";saved.favorite_food="Bread";saved.position_x=148.25;saved.position_y=704.5;saved.direction_x=-std::sqrt(.5);saved.direction_y=std::sqrt(.5);saved.bank=15;saved.cash=UINT32_MAX;saved.earned_cash=7;saved.playtime_seconds=1234.75;saved.saved_at="2026-10-02T08:00:00Z";
 for(auto&f:saved.flags)if(std::find(old.mutable_flags().begin(),old.mutable_flags().end(),f.id)!=old.mutable_flags().end())f.value=true;
 std::reverse(saved.flags.begin(),saved.flags.end());const auto original=bytes(saved,old.compatibility());
 SessionSnapshot strict;CHECK(!decode_session_save(original.data(),original.size(),data.compatibility(),strict,error));CHECK(prepare(original,prepared,migrated));CHECK(migrated&&prepared.valid());CHECK(bytes(prepared.state,old.compatibility())==original);CHECK(bytes(saved,old.compatibility())==original);
 CHECK(prepared.inventory.instance(0).id==0);CHECK(prepared.state.key_items.front().uid==UINT32_MAX);CHECK(prepared.stats.cash==UINT32_MAX&&prepared.stats.bank==15);CHECK(prepared.state.position_x==148.25&&prepared.state.direction_x==saved.direction_x);
 for(const auto&f:prepared.state.flags)if(f.id=="minnie_leave"||f.id=="minnie_door")CHECK(!f.value);
 // Every originally permitted camera remains accepted, and neither old flags
 // nor explicit false encounter/seen entries are filled, removed, or reordered.
 RoomData old_room;HouseData old_house;CHECK(old_room.load_file((legacy_root+"/opening.encroom").c_str(),error));CHECK(old_house.load_file((legacy_root+"/opening.enchouse").c_str(),error));
 for(uint32_t i=0;i<old_room.view().battle_count();++i)saved.encountered.push_back({std::string(old_room.view().string(old_room.view().battle(i).enemy_string)),i%2==0});
 for(uint32_t i=0;i<old_house.view().count(HouseSection::Npcs);++i){auto id=old_house.view().string(old_house.view().npc(i).seen_key);if(!id.empty())saved.seen_dialogue_flags.push_back({std::string(id),false});}
 for(auto id:old.camera_area_ids())for(uint32_t i=0;i<old_room.view().camera_area_count();++i){auto area=old_room.view().camera_area(i);if(area.stable_id!=id)continue;auto s=saved;s.position_x=area.center.x;s.position_y=area.center.y;auto b=bytes(s,old.compatibility());CHECK(prepare(b,prepared,migrated));CHECK(bytes(prepared.state,old.compatibility())==b);}
 CHECK(prepare(original,prepared,migrated));
 auto unchanged=[&]{CHECK(prepared.valid()&&bytes(prepared.state,old.compatibility())==original);CHECK(migrated);};
 const std::vector<std::function<void(SessionSnapshot&)>>bad={
  [](auto&s){s.scene_id="unknown";},[](auto&s){s.source_version="unknown";},[](auto&s){s.flags.pop_back();},
  [](auto&s){s.flags.front().id="unknown";},[](auto&s){s.characters.front().status.push_back({"unknown",0});},
  [](auto&s){s.characters.front().inventory.front().item_id="unknown";},[](auto&s){s.characters.front().hp=99999;},
  [](auto&s){s.characters.front().learned_skills.push_back("unknown");},[](auto&s){s.settings.text_speed=0.125;},
  [](auto&s){s.position_x=-100000;s.position_y=-100000;},[](auto&s){s.direction_x=0.25;s.direction_y=0.75;},
  [](auto&s){s.seen_dialogue_flags.push_back({"unknown",false});},[](auto&s){s.encountered.push_back({"Pillow",false});},
  [](auto&s){for(auto&f:s.flags)if(f.id=="minnie_leave"||f.id=="minnie_door")f.value=true;}
 };
 for(const auto&edit:bad){auto s=old.defaults();edit(s);auto b=bytes(s,old.compatibility());CHECK(!prepare(b,prepared,migrated));unchanged();}
 // The newly added Mom room is valid under7 but must not be back-labeled6.
 auto modern=data.defaults();bool new_camera=false;for(uint32_t i=0;i<room.view().camera_area_count();++i){auto a=room.view().camera_area(i);if(std::find(old.camera_area_ids().begin(),old.camera_area_ids().end(),a.stable_id)==old.camera_area_ids().end()&&std::find(data.camera_area_ids().begin(),data.camera_area_ids().end(),a.stable_id)!=data.camera_area_ids().end()){modern.position_x=a.center.x;modern.position_y=a.center.y;new_camera=true;break;}}
 CHECK(new_camera);CHECK(!prepare(bytes(modern,old.compatibility()),prepared,migrated));unchanged();CHECK(prepare(bytes(modern,data.compatibility()),prepared,migrated));CHECK(!migrated);
 for(auto&f:modern.flags)if(f.id=="minnie_leave"||f.id=="minnie_door")f.value=true;
 CHECK(prepare(bytes(modern,data.compatibility()),prepared,migrated));CHECK(!migrated);
 // New encounters and seen dialogue keys are not accepted merely because
 // the new content knows them; their historical absence is checked first.
 auto expanded=data.defaults();bool new_encounter=false,new_seen=false;
 for(uint32_t i=0;i<room.view().battle_count();++i){const std::string id(room.view().string(room.view().battle(i).enemy_string));bool old_known=false;for(uint32_t j=0;j<old_room.view().battle_count();++j)old_known|=old_room.view().string(old_room.view().battle(j).enemy_string)==id;if(!old_known){expanded.encountered.push_back({id,false});new_encounter=true;}}
 for(uint32_t i=0;i<house.view().count(HouseSection::Overrides);++i){const std::string id(house.view().string(house.view().override_dialogue(i).seen_key));if(id.empty())continue;bool old_known=false;for(uint32_t j=0;j<old_house.view().count(HouseSection::Overrides);++j)old_known|=old_house.view().string(old_house.view().override_dialogue(j).seen_key)==id;for(uint32_t j=0;j<old_house.view().count(HouseSection::Npcs);++j)old_known|=old_house.view().string(old_house.view().npc(j).seen_key)==id;if(!old_known){expanded.seen_dialogue_flags.push_back({id,false});new_seen=true;}}
 CHECK(new_encounter);CHECK(prepare(bytes(expanded,data.compatibility()),prepared,migrated));CHECK(!migrated);
 CHECK(!prepare(bytes(expanded,old.compatibility()),prepared,migrated));
 (void)new_seen; // A later content revision may bind the added keys via Npcs.
 CHECK(prepare(original,prepared,migrated));
 for(auto identity:std::vector<SessionSaveCompatibility>{{old.compatibility().content_family+1,1,6},{old.compatibility().content_family,2,6},{old.compatibility().content_family,1,5},{old.compatibility().content_family,1,8}}){CHECK(!prepare(bytes(old.defaults(),identity),prepared,migrated));unchanged();}
 for(size_t cut:std::vector<size_t>{0,1,8,27,original.size()-1}){std::vector<uint8_t>b(original.begin(),original.begin()+cut);CHECK(!prepare(b,prepared,migrated));unchanged();}
 auto corrupt=original;corrupt.back()^=1;CHECK(!prepare(corrupt,prepared,migrated));unchanged();auto schema=original;put(schema,8,2);put(schema,schema.size()-4,crc(schema.data(),schema.size()-4));CHECK(!prepare(schema,prepared,migrated));unchanged();
 for(size_t at:std::vector<size_t>{0,8,12,16,20,24,32,44,48,52}){auto b=policy;b[at]^=1;if(at>=24)fix(b);CHECK(!migration.load(b.data(),b.size(),error));CHECK(migration.valid());}
 CHECK(prepare(original,prepared,migrated));
 // One read, no writer calls, unchanged primary/backup and transactional result.
 ReadOnlyFiles fs;fs.files["generated.encsave"]=original;fs.files["generated.encsave.bak"]={1,2,3};auto before=fs.files;SessionSnapshot snapshot=modern;
 CHECK(read_compatible_session_restore("generated.encsave",(root+"/rules6-to7.encmigration").c_str(),data,room.view(),house.view(),round.view(),items.view(),font.view(),snapshot,prepared,error,&fs,&migrated));CHECK(migrated&&fs.reads==1&&fs.writes==0&&fs.files==before);CHECK(bytes(snapshot,old.compatibility())==original);
 fs.files["generated.encsave"]=corrupt;CHECK(!read_compatible_session_restore("generated.encsave",(root+"/rules6-to7.encmigration").c_str(),data,room.view(),house.view(),round.view(),items.view(),font.view(),snapshot,prepared,error,&fs,&migrated));unchanged();CHECK(bytes(snapshot,old.compatibility())==original&&fs.writes==0);
 // Rules7 reads do not depend on a present historical bundle.
 fs.files["generated.encsave"]=bytes(modern,data.compatibility());CHECK(read_compatible_session_restore("generated.encsave","nonexistent-policy",data,room.view(),house.view(),round.view(),items.view(),font.view(),snapshot,prepared,error,&fs,&migrated));CHECK(!migrated&&fs.writes==0);
 // Explicit Record is still guarded: an old occupied slot cannot be
 // overwritten, while an empty new slot gets a rules7 save and reloads.
 MemoryFiles record;record.files["old.encsave"]=original;record.files["old.encsave.bak"]={4,5,6};const auto old_files=record.files;
 CHECK(!write_session_save("old.encsave",modern,data.compatibility(),error,&record));CHECK(record.writes==0&&record.files==old_files);CHECK(error.find("Refusing to replace invalid/incompatible primary session save:")==0);
 CHECK(write_session_save("new.encsave",modern,data.compatibility(),error,&record));CHECK(record.files["old.encsave"]==original&&record.files["old.encsave.bak"]==old_files.at("old.encsave.bak"));
 CHECK(read_compatible_session_restore("new.encsave","nonexistent-policy",data,room.view(),house.view(),round.view(),items.view(),font.view(),snapshot,prepared,error,&record,&migrated));CHECK(!migrated&&bytes(snapshot,data.compatibility())==record.files.at("new.encsave"));
 std::printf("Session migration: %u focused checks passed; generated fixtures only, no save writes\n",checks);
}
