#include "encore/music_bank_routes.hpp"
#include "encore/music_scene_graph.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music bank routes %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
std::vector<uint8_t>read(const char*path){std::ifstream f(path,std::ios::binary);CHECK(bool(f));return {std::istreambuf_iterator<char>(f),{}};}
void crc(std::vector<uint8_t>&bytes){std::fill(bytes.begin()+16,bytes.begin()+20,0);auto value=audio_crc32(bytes.data(),bytes.size());for(unsigned i=0;i<4;++i)bytes[16+i]=uint8_t(value>>(8*i));}
void word(std::vector<uint8_t>&bytes,size_t offset,uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=uint8_t(value>>(8*i));crc(bytes);}
std::string digest(const MusicBankRoutes&r){std::ostringstream out;out<<r.ready()<<std::hexfloat;for(const auto&p:r.providers())out<<' '<<p.identity<<' '<<p.bank.count()<<' '<<p.bank.master_db()<<' '<<p.bank.silence_db();for(const auto&v:r.routes())out<<' '<<v.player<<' '<<v.generation<<' '<<v.provider<<' '<<v.track_id<<' '<<v.slot<<' '<<v.gain_db<<' '<<v.stream_paused;return out.str();}
std::vector<MusicPlayerStart>starts(const MusicPlayers&p,const MusicBankRoutes&r,uint64_t provider){std::vector<MusicPlayerStart>out;for(const auto&node:p.nodes())if(node.playing){bool old=false;for(const auto&route:r.routes())if(route.player==node.identity&&route.generation==node.generation)old=true;if(!old)out.push_back({node.identity,node.generation,provider});}return out;}
void close(MusicSceneGraph&g){const auto*binding=&g.data();for(auto it=g.data().regions().rbegin();it!=g.data().regions().rend();++it)CHECK(g.tree_exit(g.scene_epoch(),it->source_path,error));CHECK(g.finish_scene(g.scene_epoch(),error));CHECK(binding==&g.data());}
void finish(MusicSceneGraph&g){double seconds=0;for(const auto&n:g.players().nodes())if(n.tween.active)seconds=std::max(seconds,n.tween.duration-n.tween.elapsed);while(seconds>0){auto delta=std::min(1.,seconds);CHECK(g.advance(delta,false,error));seconds-=delta;}CHECK(g.flush_deletions(error));}
MusicRoutingPolicy policy(const AudioBank&b,uint32_t providers=8,uint32_t voices=16){return {providers,voices,b.master_db(),b.silence_db()};}
void scenes(const MusicRegionData&a,const AudioBank&ab,const MusicRegionData&b,const AudioBank&bb){
 MusicBankRoutes router;CHECK(router.initialize(policy(ab),error));{auto data=a;auto bank=ab;CHECK(router.add_provider(1,data,bank,error));}{auto data=b;auto bank=bb;CHECK(router.add_provider(2,data,bank,error));}
 CHECK(router.provider(1)->bank.asset(0).source_path==ab.asset(0).source_path);CHECK(router.provider(2)->regions.tracks()[0].source_path==b.tracks()[0].source_path);
 MusicSceneGraph graph;CHECK(graph.initialize({a.silence_db(),0,a.fade_to_seconds(),16},error));CHECK(graph.attach_scene(1,a,error));MusicRegionContext context;context.flag=[](std::string_view){return false;};
 CHECK(graph.enter(1,a.regions()[0].source_path,context,error));CHECK(router.reconcile(graph.players(),starts(graph.players(),router,1),error));CHECK(router.routes().size()==1&&router.routes()[0].slot==0&&router.routes()[0].provider==1);auto first=router.routes()[0];
 auto unchanged=digest(router);CHECK(!router.remove_provider(1,error)&&digest(router)==unchanged);
 close(graph);CHECK(graph.attach_scene(2,b,error));CHECK(graph.enter(2,b.regions()[0].source_path,context,error));CHECK(router.reconcile(graph.players(),starts(graph.players(),router,2),error));CHECK(router.routes().size()==2&&router.routes()[0].player==first.player&&router.routes()[0].generation==first.generation&&router.routes()[0].slot==0&&router.routes()[1].slot==1&&router.routes()[1].provider==2);
 close(graph);CHECK(graph.attach_scene(3,a,error));CHECK(graph.enter(3,a.regions()[0].source_path,context,error));CHECK(router.reconcile(graph.players(),starts(graph.players(),router,1),error));CHECK(router.routes().size()==3&&router.routes()[2].slot==2&&router.routes()[2].provider==1);auto current=router.routes()[2];
 finish(graph);CHECK(router.reconcile(graph.players(),{},error));CHECK(router.routes().size()==1&&router.routes()[0].slot==current.slot&&router.routes()[0].player==current.player&&router.routes()[0].generation==current.generation);
 CHECK(router.remove_provider(2,error)&&!router.provider(2));CHECK(router.add_provider(3,b,bb,error));
 // Removing the first array entry does not renumber a surviving DSP slot.
 close(graph);CHECK(graph.attach_scene(4,b,error));CHECK(graph.enter(4,b.regions()[0].source_path,context,error));CHECK(router.reconcile(graph.players(),starts(graph.players(),router,3),error));CHECK(router.routes().size()==2&&router.routes()[0].slot==2&&router.routes()[1].slot==0);
 auto staged_graph=graph;auto staged_router=router;CHECK(staged_graph.players().stop(staged_router.routes()[0].player,error));CHECK(staged_router.reconcile(staged_graph.players(),{},error));CHECK(staged_router.routes().size()==1&&router.routes().size()==2);CHECK(graph.players().owner()==staged_graph.players().owner());
 // The source's stopped latest node can be rebound to a different bank while
 // keeping its node and slot identity; the playback generation must change.
 MusicSceneGraph reuse;MusicBankRoutes rr;CHECK(reuse.initialize({a.silence_db(),0,a.fade_to_seconds(),16},error));CHECK(rr.initialize(policy(ab),error));CHECK(rr.add_provider(1,a,ab,error)&&rr.add_provider(2,b,bb,error));CHECK(reuse.attach_scene(1,a,error));CHECK(reuse.enter(1,a.regions()[0].source_path,context,error));CHECK(rr.reconcile(reuse.players(),starts(reuse.players(),rr,1),error));auto old=rr.routes()[0];CHECK(reuse.stop_area(1,a.regions()[0].source_path,0,error));close(reuse);CHECK(reuse.attach_scene(2,b,error));CHECK(reuse.enter(2,b.regions()[0].source_path,context,error));CHECK(rr.reconcile(reuse.players(),starts(reuse.players(),rr,2),error));CHECK(rr.routes().size()==1&&rr.routes()[0].player==old.player&&rr.routes()[0].slot==old.slot&&rr.routes()[0].generation==old.generation+1&&rr.routes()[0].provider==2);CHECK(rr.remove_provider(1,error));
}
void negative(const MusicRegionData&data,const AudioBank&bank,const std::vector<uint8_t>&music_bytes,const std::vector<uint8_t>&audio_bytes){
 MusicBankRoutes r;MusicPlayers players;uint64_t id=0;MusicRegionData invalid;AudioBank absent;
 auto before=digest(r);auto reject=[&](bool result){CHECK(!result&&digest(r)==before);};
 reject(r.add_provider(1,data,bank,error));reject(r.remove_provider(1,error));reject(r.reconcile(players,{},error));
 for(auto p:{MusicRoutingPolicy{},MusicRoutingPolicy{65,16,-6,-80},MusicRoutingPolicy{8,257,-6,-80},MusicRoutingPolicy{8,16,1,-80},MusicRoutingPolicy{8,16,-6,-19},MusicRoutingPolicy{8,16,NAN,-80}})reject(r.initialize(p,error));
 CHECK(r.initialize(policy(bank,4,2),error));before=digest(r);reject(r.initialize(policy(bank),error));reject(r.add_provider(0,data,bank,error));reject(r.add_provider(UINT64_MAX,data,bank,error));reject(r.add_provider(1,invalid,bank,error));reject(r.add_provider(1,data,absent,error));
 CHECK(r.add_provider(1,data,bank,error));before=digest(r);reject(r.add_provider(1,data,bank,error));reject(r.add_provider(0,data,bank,error));
 // Deliberately corrupt in-memory negative inputs; no generated pack is edited.
 for(unsigned kind=0;kind<4;++kind){auto mb=music_bytes,ab=audio_bytes;if(kind==0){mb[68]^=1;ab[68]^=1;}if(kind==1)mb[100]^=1;if(kind==2)ab[120]^=1;if(kind==3)word(ab,140,0xbf800000u);crc(mb);crc(ab);MusicRegionData changed;AudioBank altered;CHECK(changed.load(mb.data(),mb.size(),error)&&altered.load(ab.data(),ab.size(),error));CHECK(changed.matches(altered,error));reject(r.add_provider(2,changed,altered,error));}
 auto bytes=audio_bytes;word(bytes,36,0xbf800000u);AudioBank wrong;CHECK(wrong.load(bytes.data(),bytes.size(),error));reject(r.add_provider(2,data,wrong,error));
 CHECK(r.add_provider(2,data,bank,error));CHECK(r.add_provider(3,data,bank,error));CHECK(r.add_provider(4,data,bank,error));before=digest(r);reject(r.add_provider(5,data,bank,error));CHECK(r.remove_provider(4,error));before=digest(r);reject(r.add_provider(4,data,bank,error));reject(r.remove_provider(99,error));
 CHECK(players.initialize({data.silence_db(),0,data.fade_to_seconds(),16},error));CHECK(players.play_loop(0,data.tracks()[0].source_path,error));before=digest(r);reject(r.reconcile(players,{},error));reject(r.reconcile(players,{{0,1,99}},error));reject(r.reconcile(players,{{0,2,1}},error));reject(r.reconcile(players,{{9,1,1}},error));reject(r.reconcile(players,{{0,1,1},{0,1,1}},error));
 CHECK(r.reconcile(players,{{0,1,1}},error));before=digest(r);reject(r.reconcile(players,{{0,1,1}},error));reject(r.remove_provider(1,error));
 CHECK(players.play_loop(0,"res://fixture/unknown",error));reject(r.reconcile(players,{{0,2,1}},error));CHECK(players.play_loop(0,data.tracks()[0].source_path,error));reject(r.reconcile(players,{},error));CHECK(r.reconcile(players,{{0,3,2}},error));CHECK(r.routes()[0].provider==2&&r.routes()[0].slot==0);CHECK(r.remove_provider(1,error));
 // Independent new-game player owners may reuse numeric IDs/generations.
 MusicPlayers other;CHECK(other.initialize({data.silence_db(),0,data.fade_to_seconds(),16},error));for(unsigned i=0;i<3;++i)CHECK(other.play_loop(0,data.tracks()[0].source_path,error));before=digest(r);reject(r.reconcile(other,{},error));
 auto same_owner=players;CHECK(r.reconcile(same_owner,{},error));CHECK(players.owner()==same_owner.owner()&&players.owner()!=other.owner());
 CHECK(players.add_player(id,error));CHECK(players.play_loop(id,data.tracks()[0].source_path,error));CHECK(r.reconcile(players,{{id,1,2}},error));CHECK(players.add_player(id,error));CHECK(players.play_loop(id,data.tracks()[0].source_path,error));before=digest(r);reject(r.reconcile(players,{{id,1,2}},error));
 CHECK(players.stop(id,error));reject(r.reconcile(players,{{id,1,2}},error));CHECK(r.reconcile(players,{},error));CHECK(players.stop(0,error));CHECK(r.reconcile(players,{},error));CHECK(r.routes().size()==1&&r.routes()[0].slot==1);CHECK(players.stop(1,error));CHECK(r.reconcile(players,{},error)&&r.routes().empty());CHECK(r.remove_provider(2,error)&&r.remove_provider(3,error));
}
}
int main(int argc,char**argv){CHECK(argc==5);MusicRegionData a,b;AudioBank ab,bb;CHECK(a.load_file(argv[1],error)&&ab.load_file(argv[2],error)&&b.load_file(argv[3],error)&&bb.load_file(argv[4],error));scenes(a,ab,b,bb);negative(a,ab,read(argv[1]),read(argv[2]));std::printf("Music bank routes: %u checks; owned checked metadata, stable node/slot/generation routes, cross-bank tails/reuse and source/import/PCM/owner/capacity rejection; no PCM/DSP submission claim\n",checks);}
