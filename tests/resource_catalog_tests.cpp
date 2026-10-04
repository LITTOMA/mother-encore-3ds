#include "encore/resource_catalog.hpp"
#include "encore/battle_round.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace encore::upstream;
namespace fs=std::filesystem;
namespace {
unsigned checks=0;
void check(bool value,const std::string& why){++checks;if(!value){std::cerr<<why<<'\n';std::exit(1);}}
uint32_t get(const std::vector<uint8_t>& b,size_t o){return uint32_t(b.at(o))|(uint32_t(b.at(o+1))<<8)|(uint32_t(b.at(o+2))<<16)|(uint32_t(b.at(o+3))<<24);}
void put(std::vector<uint8_t>& b,size_t o,uint32_t v){for(unsigned i=0;i<4;++i)b.at(o+i)=uint8_t(v>>(i*8));}
uint32_t crc(const uint8_t* p,size_t size){uint32_t c=~0u;for(size_t n=0;n<size;++n){c^=p[n];for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
void fix(std::vector<uint8_t>& b){put(b,12,uint32_t(b.size()));put(b,16,crc(b.data()+32,b.size()-32));}
size_t row(const std::vector<uint8_t>& b,ResourceRole role){size_t o=60;for(uint32_t i=0;i<get(b,52);++i){if(get(b,o+4)==uint32_t(role))return o;o+=20+get(b,o+16);}check(false,"resource role missing from test input");return 0;}
size_t pairs(const std::vector<uint8_t>& b){size_t o=60;for(uint32_t i=0;i<get(b,52);++i)o+=20+get(b,o+16);return o;}
std::string row_path(const std::vector<uint8_t>& b,size_t o){return std::string(b.begin()+o+20,b.begin()+o+20+get(b,o+16));}
void path(std::vector<uint8_t>& b,ResourceRole role,const std::string& replacement){const auto o=row(b,role);const auto n=get(b,o+16);b.erase(b.begin()+o+20,b.begin()+o+20+n);b.insert(b.begin()+o+20,replacement.begin(),replacement.end());put(b,o+16,uint32_t(replacement.size()));fix(b);}
std::vector<uint8_t> read(const fs::path& p){std::vector<uint8_t>b;std::string error;check(encore::read_file(p.string().c_str(),b,16*1024*1024,error),error);return b;}
void write(const fs::path& p,const std::vector<uint8_t>& b){fs::create_directories(p.parent_path());std::ofstream stream(p,std::ios::binary|std::ios::trunc);stream.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));check(bool(stream),"test fixture write failed");}
std::string prefix(const fs::path& root){return root.generic_string()+"/";}

// Presentation is explicitly limited to immediate HP and ready signals, as in
// the retained mechanics oracle. The production BattleRound consumes every
// catalog-selected binding, skill and rule; these are not M0 fixture values.
struct Host final:BattleRoundHost {
 RoundView content;int32_t maximum_hp=0;int32_t hp[2]{};std::vector<BattleRoundCue> cues;
 bool emit(const BattleRoundCue& c,SourceRandom& random)override{
  cues.push_back(c);if(c.kind==BattleRoundCueKind::Hit){
   const auto motion=content.parameter(RoundParameter::FlyingNumberRandom);
   (void)random.rand_range(motion.x,motion.y);(void)random.randi();hp[c.target]=c.hp_after;
   if(c.target==content.binding().player_participant&&c.amount>maximum_hp/content.parameter(RoundParameter::PartyHit).x)(void)random.rand_range(1,content.parameter(RoundParameter::PartyHit).w);
  }return true;
 }
 bool ready(BattleRoundGate,uint32_t)const override{return true;}
 int32_t current_hp(uint32_t actor)const override{return hp[actor];}
};
std::string play(const ResourceCatalog& catalog,const fs::path& root,bool guard){
 std::string error;BattleData entry;BattleRoundData data;
 check(entry.load_file((root/catalog.path(ResourceRole::Battle)).string().c_str(),error),error);
 check(data.load_file((root/catalog.companion_path(catalog.path(ResourceRole::Battle))).string().c_str(),error),error);
 const auto view=data.view();const auto binding=view.binding();Host host;host.content=view;
 host.maximum_hp=entry.view().participant(binding.player_participant).maxhp;
 for(unsigned i=0;i<2;++i)host.hp[i]=entry.view().participant(i).hp;
 SourceRandom random(823);BattleRound round;check(round.begin(view,entry.view(),random,host),round.error());
 check(round.request_menu(binding.basic_menu),round.error());check(round.phase()==BattleRoundPhase::Targeting,"catalog-selected basic command must target");
 check(round.target_input(0,false,true),round.error());check(round.take_menu_return()&&round.phase()==BattleRoundPhase::Commands,"target cancel returns to commands");
 check(random.raw_draw_count()==0,"target navigation cannot consume source randomness");
 if(guard){check(round.request_menu(binding.guard_menu),round.error());check(round.battler(binding.player_participant).defending,"catalog-selected guard command must defend");}
 else{check(round.request_menu(binding.basic_menu)&&round.target_input(0,true),round.error());}
 unsigned frames=0;while(round.phase()==BattleRoundPhase::Running&&frames++<2000)check(round.idle_frame(double(float(1.0/60))),round.error());
 check(frames<2000,"actual round must reach a supported result boundary");
 check(round.phase()==BattleRoundPhase::Commands||round.phase()==BattleRoundPhase::VictoryPending,"actual round result is supported");
 check(std::any_of(host.cues.begin(),host.cues.end(),[&](const BattleRoundCue& c){return c.kind==BattleRoundCueKind::TurnStart&&c.actor==binding.player_participant;}),"actual player action reaches source turn execution");
 if(guard)check(std::any_of(host.cues.begin(),host.cues.end(),[&](const BattleRoundCue& c){return c.kind==BattleRoundCueKind::TurnStart&&c.actor==binding.enemy_participant;}),"guard turn executes actual enemy response, including non-damage choices");
 else check(!round.decisions().empty(),"actual basic action reaches checked damage decision");
 std::ostringstream result;result<<uint32_t(round.phase())<<':'<<round.number()<<':'<<frames<<':'<<random.raw_draw_count()<<':'<<random.randi();
 for(unsigned i=0;i<2;++i)result<<':'<<round.battler(i).target_hp;
 for(const auto& d:round.decisions())result<<'|'<<d.round<<','<<d.actor<<','<<d.target<<','<<d.skill<<','<<d.damage<<','<<d.miss<<','<<d.smash<<','<<d.random_state<<','<<d.raw_draw_count;
 for(const auto& c:host.cues)result<<'/'<<uint32_t(c.kind)<<','<<c.actor<<','<<c.target<<','<<c.skill<<','<<c.text<<','<<c.amount<<','<<c.hp_after;
 return result.str();
}
}
int main(int argc,char** argv){
 check(argc==4,"catalog path, RomFS root and build fixture directory required");
 const fs::path source_root=argv[2],fixture_root=argv[3];
 // Caller supplies a dedicated directory under build; never touch user saves.
 check(std::find(fixture_root.begin(),fixture_root.end(),fs::path("build"))!=fixture_root.end(),"fixtures must remain under build");
 auto blob=read(argv[1]);std::string error;ResourceCatalog catalog;
 check(!catalog.valid(),"new catalog is invalid");check(!catalog.verify_files(prefix(source_root).c_str(),error),"invalid catalog cannot verify files");
 check(!catalog.load_file((fixture_root/"missing.enccatalog").string().c_str(),error),"missing catalog rejected");
 check(catalog.load(blob.data(),blob.size(),error),error);check(catalog.valid(),"loaded catalog valid");
 check(catalog.verify_files(prefix(source_root).c_str(),error),error);
 check(catalog.path(ResourceRole(0)).empty()&&catalog.path(ResourceRole(25)).empty(),"unknown resource roles have no fallback");
 check(catalog.companion_path("unknown.encbattle").empty(),"unknown encounters have no fallback");
 check(catalog.companion_path(catalog.path(ResourceRole::Battle))==catalog.path(ResourceRole::Round),"reviewed root encounter companion is externally bound");
 const auto original_round=catalog.path(ResourceRole::Round);
 auto reject=[&](const std::vector<uint8_t>& bad,const char* why){check(!catalog.load(bad.data(),bad.size(),error),why);check(!error.empty(),"failed loads explain rejection");check(catalog.valid()&&catalog.path(ResourceRole::Round)==original_round,"failed catalog load preserves prior binding");};
 for(size_t n=0;n<blob.size();++n){check(!catalog.load(blob.data(),n,error),"every truncated catalog rejected");check(catalog.path(ResourceRole::Round)==original_round,"truncation cannot replace valid catalog");}
 check(!catalog.load(nullptr,blob.size(),error),"null catalog rejected");
 auto bad=blob;bad.back()^=1;reject(bad,"payload corruption rejected");
 bad=blob;bad[0]^=1;reject(bad,"unknown magic rejected");
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{8,0},{8,2},{12,uint32_t(blob.size()+1)},{20,0},{20,2},{24,1},{28,1},{52,0},{52,129},{56,0},{56,33}}){bad=blob;put(bad,edit.first,edit.second);if(edit.first!=12)fix(bad);reject(bad,"unknown header or record count rejected");}
 bad=blob;bad[32]^=1;fix(bad);reject(bad,"unreviewed upstream pin rejected");
 const auto round_row=row(blob,ResourceRole::Round),battle_row=row(blob,ResourceRole::Battle);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{round_row,0},{round_row,uint32_t(ResourceRole::Battle)},{round_row+4,0},{round_row+4,25},{round_row+4,uint32_t(ResourceRole::Battle)},{round_row+8,0},{round_row+8,16*1024*1024+1},{round_row+16,0},{round_row+16,UINT32_MAX}}){bad=blob;put(bad,edit.first,edit.second);fix(bad);reject(bad,"unknown or duplicate id/role, unsupported size or path span rejected");}
 for(const auto& unsafe:std::vector<std::string>{"../data/round.encround","data/../round.encround","/data/round.encround","C:/round.encround","data\\round.encround","data//round.encround","data/./round.encround","data/round.encbattle","data/round.encround/","data/round.encround?x","data/\xc3\xa9.encround"}){bad=blob;path(bad,ResourceRole::Round,unsafe);reject(bad,"noncanonical or wrong typed resource path rejected");}
 bad=blob;path(bad,ResourceRole::EncounterBattle,catalog.path(ResourceRole::Battle));reject(bad,"duplicate catalog path rejected");
 const auto extra=row(blob,ResourceRole::EncounterBattle);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{extra,255},{extra,get(blob,battle_row)},{extra+4,uint32_t(ResourceRole::Battle)}}){bad=blob;put(bad,edit.first,edit.second);fix(bad);reject(bad,"extra identities cannot collide with reserved root ids or roles");}
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{pairs(blob),0},{pairs(blob),uint32_t(ResourceRole::Round)},{pairs(blob)+4,255},{pairs(blob)+4,uint32_t(ResourceRole::Battle)},{pairs(blob)+8,uint32_t(ResourceRole::Battle)},{pairs(blob)+12,uint32_t(ResourceRole::Round)}}){bad=blob;put(bad,edit.first,edit.second);fix(bad);reject(bad,"unknown or duplicate encounter binding rejected");}
 bad=blob;bad.erase(bad.end()-8,bad.end());put(bad,56,get(bad,56)-1);fix(bad);reject(bad,"unpaired checked encounter resources rejected");
 bad=blob;bad.push_back(0);fix(bad);reject(bad,"trailing unsupported record rejected");
 bad.resize(16385);fix(bad);reject(bad,"oversized catalog rejected");
 auto owned=blob;check(catalog.load(owned.data(),owned.size(),error),error);std::fill(owned.begin(),owned.end(),0);check(catalog.path(ResourceRole::Round)==original_round,"catalog owns decoded bindings");
 size_t o=60;for(uint32_t i=0;i<get(blob,52);++i){const auto name=row_path(blob,o);write(fixture_root/name,read(source_root/name));o+=20+get(blob,o+16);}
 check(catalog.verify_files(prefix(fixture_root).c_str(),error),error);
 const auto basic_a=play(catalog,fixture_root,false),guard_a=play(catalog,fixture_root,true);
 const fs::path original=fixture_root/original_round;const auto bytes=read(original);
 fs::remove(original);check(!catalog.verify_files(prefix(fixture_root).c_str(),error),"missing bound resource rejected");
 auto truncated=bytes;truncated.pop_back();write(original,truncated);check(!catalog.verify_files(prefix(fixture_root).c_str(),error),"truncated bound resource rejected");
 auto corrupted=bytes;corrupted.back()^=1;write(original,corrupted);check(!catalog.verify_files(prefix(fixture_root).c_str(),error),"same-size wrong fingerprint rejected");
 const std::string relocated="data/alternate/checked-round.encround";write(fixture_root/relocated,bytes);fs::remove(original);
 auto alternate=blob;path(alternate,ResourceRole::Round,relocated);const auto relocated_row=row(alternate,ResourceRole::Round);
 put(alternate,relocated_row+8,uint32_t(bytes.size()));put(alternate,relocated_row+12,crc(bytes.data(),bytes.size()));fix(alternate);
 ResourceCatalog catalog_b;check(catalog_b.load(alternate.data(),alternate.size(),error),error);check(catalog_b.verify_files(prefix(fixture_root).c_str(),error),error);
 check(catalog_b.path(ResourceRole::Round)!=catalog.path(ResourceRole::Round),"catalog B changes the selected resource without executable changes");
 check(play(catalog_b,fixture_root,false)==basic_a,"unchanged consumer reproduces actual basic turn with relocated Round resource");
 check(play(catalog_b,fixture_root,true)==guard_a,"unchanged consumer reproduces actual guard turn with relocated Round resource");
 check(get(alternate,row(alternate,ResourceRole::Battle))==get(blob,battle_row),"relocation preserves paired Battle identity");
 std::cout<<"ResourceCatalog: "<<checks<<" checks; fail-closed bindings, file fingerprints, rollback and original BattleRound A/B relocation\n";
}
