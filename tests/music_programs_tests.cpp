#include "encore/music_programs.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music programs %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
const std::string title="res://Audio/Music/Mother Earth.mp3",piano="res://Audio/Music/Mother_Earth_Piano.mp3",house="res://Audio/Music/House.mp3",foe="res://Audio/Music/Battle Themes/Flippant Foe.mp3",intro="res://Audio/Music/Battle Encounter/Encounter Enemy.mp3",level="res://Audio/Music/You Win/LVLUP.mp3",personal="res://Audio/Music/You Win/LVLUP_ninten.mp3";
MusicArgument text(std::string s){MusicArgument a;a.text=std::move(s);return a;}MusicArgument number(double n){MusicArgument a;a.kind=MusicArgument::Kind::Number;a.number=n;return a;}MusicArgument boolean(bool b){MusicArgument a;a.kind=MusicArgument::Kind::Boolean;a.boolean=b;return a;}
std::string digest(const MusicSceneGraph&g){std::ostringstream o;o<<std::hexfloat<<g.active()<<g.scene_epoch()<<g.players().cleanup_waiting()<<g.players().deferred_finished_count();for(const auto&n:g.players().nodes())o<<n.identity<<n.generation<<n.stream<<n.finished_loop<<n.finished_index<<n.playing<<n.stream_paused<<n.pending_delete<<n.remove_on_finished<<n.gain_db<<n.start_seconds<<n.tween.active<<n.tween.from<<n.tween.to<<n.tween.duration<<n.tween.elapsed;return o.str();}
MusicSceneGraph fresh(){MusicSceneGraph g;CHECK(g.initialize({-80,0,1,32},error));return g;}
MusicProgramResult invoke(const MusicPrograms&book,uint32_t id,const std::vector<MusicArgument>&args,MusicSceneGraph&g,uint32_t elapsed=0,MusicPositionQuery q={}){MusicProgramResult out;CHECK(book.evaluate(id,args,g,q,out,error,elapsed)==MusicProgramStep::Ready);CHECK(out.program==id&&out.executed>0);g=out.graph;return out;}
void calls(const MusicPrograms&book,const char*scene_path){
 auto g=fresh();auto result=invoke(book,1,{},g);CHECK(result.changes==3&&g.players().nodes().size()==2&&g.players().nodes().back().stream==title);const auto old=digest(g);result=invoke(book,1,{},g);CHECK(!result.changes&&digest(g)==old);
 invoke(book,2,{},g);invoke(book,3,{text(piano),boolean(false)},g);CHECK(g.players().nodes().size()==3&&g.players().nodes().back().stream==piano&&g.players().nodes().back().gain_db==8&&!g.players().nodes().back().stream_paused);
 CHECK(g.advance(.5,false,error)&&g.flush_deletions(error)&&g.players().nodes().size()==1&&g.players().nodes()[0].stream==piano);const auto piano_id=g.players().nodes()[0].identity;
 invoke(book,5,{text(""),text(house)},g);CHECK(g.players().nodes().back().stream==house&&g.players().nodes().back().finished_loop==house);
 invoke(book,6,{text(intro)},g);CHECK(g.players().nodes().back().remove_on_finished&&g.players().nodes().back().finished_loop.empty());
 invoke(book,7,{},g);CHECK(g.advance(1,false,error)&&std::abs(g.players().find(piano_id)->gain_db-2.5)<.00001&&g.players().nodes().back().gain_db==0);
 MusicProgramResult pending;pending.program=999;auto before=digest(g);CHECK(book.evaluate(8,{number(-6)},g,{},pending,error)==MusicProgramStep::Pending&&pending.program==999&&digest(g)==before);
 invoke(book,8,{number(-6)},g,1);CHECK(g.advance(.5,false,error)&&std::abs(g.players().find(piano_id)->gain_db+1.75)<.00001);
 invoke(book,4,{number(.2)},g);CHECK(g.advance(.2,false,error)&&g.flush_deletions(error)&&g.players().nodes().size()==1&&!g.players().nodes()[0].playing);
 auto shifted=fresh();CHECK(shifted.players().play_loop(0,house,error));uint64_t later;CHECK(shifted.players().add_player(later,error)&&shifted.players().play_loop(later,title,error));
 MusicProgramResult deferred;deferred.program=999;CHECK(book.evaluate(8,{number(-6)},shifted,{},deferred,error)==MusicProgramStep::Pending&&deferred.program==999);CHECK(shifted.players().queue_remove(0,error)&&shifted.flush_deletions(error));
 invoke(book,8,{number(-6)},shifted,1);CHECK(shifted.advance(.5,false,error)&&shifted.players().nodes().size()==1&&shifted.players().nodes()[0].identity==later&&shifted.players().nodes()[0].gain_db==-3);
 auto embedded=fresh();CHECK(embedded.players().play_loop(0,house,error));invoke(book,3,{text(piano),boolean(true)},embedded);CHECK(embedded.players().find(0)->stream_paused&&!embedded.players().nodes().back().stream_paused);
 g=fresh();CHECK(g.players().play_loop(0,house,error));invoke(book,9,{text(intro),boolean(true)},g);CHECK(g.players().find(0)->stream_paused&&!g.players().nodes().back().stream_paused&&g.players().nodes().back().remove_on_finished);
 const auto encounter=g.players().nodes().back().identity;CHECK(g.players().signal_finished(encounter,1,error)&&g.players().dispatch_finished(error)&&g.flush_deletions(error));
 invoke(book,10,{text(""),text(foe),boolean(true)},g);CHECK(g.players().nodes().back().stream==foe);const auto enemy_music=g.players().nodes().back().identity;
 invoke(book,11,{boolean(true),boolean(false)},g);CHECK(g.players().find(enemy_music)->stream_paused&&g.players().nodes().back().stream.find("/YOUWON.mp3")!=std::string::npos);
 const auto victory=g.players().nodes().back().identity;CHECK(g.players().signal_finished(victory,1,error)&&g.players().dispatch_finished(error)&&g.players().find(victory)->stream.find("/Victory.mp3")!=std::string::npos);
 invoke(book,12,{boolean(true),text(personal)},g);CHECK(g.players().nodes().back().stream==personal&&g.players().nodes().back().start_seconds==0);const auto count=g.players().nodes().size();unsigned reads=0;
 result=invoke(book,12,{boolean(true),text(personal)},g,0,[&](uint64_t,uint64_t,double&position,std::string&){++reads;position=1.375;return MusicProgramStep::Ready;});CHECK(reads==1&&g.players().nodes().size()==count&&!result.changes);
 invoke(book,13,{boolean(true),text(foe),text(""),boolean(false)},g);CHECK(!g.players().find(0)->stream_paused&&g.players().find(0)->tween.active&&g.players().find(0)->tween.duration==3);CHECK(g.flush_deletions(error)&&g.players().nodes().size()==1&&g.players().find(0));
 invoke(book,14,{text(house)},g);CHECK(!g.players().find(0)->playing);
 g=fresh();MusicRegionData scene;CHECK(scene.load_file(scene_path,error)&&g.attach_scene(1,scene,error));MusicRegionContext context;context.flag=[](std::string_view){return false;};CHECK(g.enter(1,scene.regions()[0].source_path,context,error)&&g.players().pause_all(error));invoke(book,13,{boolean(true),text(""),text(""),boolean(true)},g);CHECK(g.registered().empty()&&!g.players().nodes()[0].playing&&!g.players().nodes()[0].stream_paused);
 for(uint32_t id:{10u,11u,12u,13u}){g=fresh();CHECK(g.players().play_loop(0,house,error));const auto unchanged=digest(g);auto args=id==10?std::vector<MusicArgument>{text(""),text(foe),boolean(false)}:id==11?std::vector<MusicArgument>{boolean(false),boolean(true)}:id==12?std::vector<MusicArgument>{boolean(false),text(personal)}:std::vector<MusicArgument>{boolean(false),text(""),text(""),boolean(false)};invoke(book,id,args,g);CHECK(digest(g)==unchanged);}
 CHECK(book.encounter(1)&&book.encounter(2)&&book.encounter(3)&&book.encounter(4)&&book.encounter(1)->music.empty()&&book.encounter(4)->music==foe&&!book.encounter(999));
}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void store(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void crc(std::vector<uint8_t>&b){store(b,12,uint32_t(b.size()));store(b,16,0);store(b,16,encore::crc32(b.data(),b.size()));}
void negatives(const char*path,const char*mechanisms){
 std::ifstream f(path,std::ios::binary);CHECK(bool(f));std::vector<uint8_t>raw(std::istreambuf_iterator<char>(f),{});MusicPrograms book;CHECK(book.load(raw.data(),raw.size(),error));
 auto reject=[&](const std::vector<uint8_t>&b){CHECK(!book.load(b.data(),b.size(),error)&&book.programs().size()==14);};for(size_t size=0;size<raw.size();++size)reject({raw.begin(),raw.begin()+size});CHECK(!book.load(nullptr,raw.size(),error)&&!book.load_file(nullptr,error));
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{8,0},{8,2},{20,0},{20,2},{24,257},{28,0},{28,65},{32,65},{36,1}}){auto b=raw;store(b,change.first,change.second);crc(b);reject(b);}
 size_t offset=40;for(unsigned i=0;i<word(raw.data()+24);++i)offset+=4+word(raw.data()+offset);const auto first=offset;const auto instruction=first+12;
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{first,0},{first+4,2},{first+8,0},{first+8,257},{instruction,0},{instruction,20},{instruction+4,999},{instruction+8,5},{instruction+12,4},{instruction+16,7},{instruction+20,1}}){auto b=raw;store(b,change.first,change.second);crc(b);reject(b);}
 auto b=raw;b.push_back(0);crc(b);reject(b);b=raw;b[0]^=1;crc(b);reject(b);b=raw;b[60]^=1;reject(b);b=raw;b[44]=255;crc(b);reject(b);
 auto g=fresh();MusicProgramResult output;output.program=777;const auto unchanged=digest(g);auto failed=[&](uint32_t id,std::vector<MusicArgument>args,MusicPositionQuery q={}){CHECK(book.evaluate(id,args,g,q,output,error)==MusicProgramStep::Failed&&output.program==777&&digest(g)==unchanged);};failed(999,{});failed(1,std::vector<MusicArgument>(9));failed(3,{number(1),boolean(false)});failed(4,{number(-1)});failed(4,{number(INFINITY)});failed(9,{text("bad-uri"),boolean(true)});failed(5,{text(""),text("")});
 CHECK(g.players().play_loop(0,level,error));const auto position_before=digest(g);auto query_failure=[&](MusicPositionQuery q,MusicProgramStep expected){CHECK(book.evaluate(12,{boolean(true),text(personal)},g,q,output,error)==expected&&output.program==777&&digest(g)==position_before);};query_failure({},MusicProgramStep::Failed);query_failure([](uint64_t,uint64_t,double&,std::string&){return MusicProgramStep::Pending;},MusicProgramStep::Pending);query_failure([](uint64_t,uint64_t,double&,std::string&){return MusicProgramStep(255);},MusicProgramStep::Failed);query_failure([](uint64_t,uint64_t,double&v,std::string&){v=NAN;return MusicProgramStep::Ready;},MusicProgramStep::Failed);
 MusicPrograms extra;CHECK(extra.load_file(mechanisms,error));g=fresh();CHECK(g.players().play_loop(0,title,error));invoke(extra,100,{},g);CHECK(g.players().find(0)->gain_db==1);const auto before=digest(g);CHECK(extra.evaluate(101,{},g,{},output,error)==MusicProgramStep::Failed&&digest(g)==before);CHECK(extra.evaluate(102,{},g,{},output,error)==MusicProgramStep::Failed&&digest(g)==before);invoke(extra,103,{},g);CHECK(digest(g)==before);
}
}
int main(int argc,char**argv){CHECK(argc==4);MusicPrograms book;CHECK(book.load_file(argv[1],error)&&book.valid()&&book.programs().size()==14&&book.encounters().size()==4);calls(book,argv[2]);negatives(argv[1],argv[3]);std::printf("Music programs: %u checks; 14 source-mapped call programs/four encounter bindings, branches, selectors, deferred volume, position staging and atomic parse/query/value failures; no main/emulator/audio pass claim\n",checks);}
