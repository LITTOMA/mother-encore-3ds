#include "encore/dungeon_mechanism_presentation.hpp"
#include "encore/dungeon_mechanism_audio.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Mechanism media %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(v) check((v),#v)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==4);std::vector<uint8_t>b;CHECK(encore::read_file(argv[1],b,512*1024,error));DungeonMechanismPresentationData data,geometry;
 CHECK(!data.media_bound()&&data.capability()==0&&data.encoded_bytes()==0&&data.encoded_crc()==0&&data.sounds().empty()&&data.audio_bank().count()==0&&!data.sound(0));
 CHECK(geometry.load_file(argv[2],error));CHECK(geometry.capability()==1&&!geometry.media_bound()&&geometry.audio_bank().count()==0&&geometry.sounds().empty());
 CHECK(data.load(b.data(),b.size(),error));CHECK(data.capability()==2&&data.media_bound()&&data.encoded_bytes()==b.size()&&data.encoded_crc()==get(b,16)&&data.sounds().size()==7&&data.audio_bank().count()==4);
 const auto g=data.bindings().graph();std::set<uint32_t>used;unsigned node_count=0,denied_count=0;
 for(const auto&s:data.sounds()){
  CHECK(data.sound(s.owner)==&s);AudioAsset asset,joined;CHECK(data.audio_bank().find(s.stable_id,asset));const std::string uri="res://"+s.source;CHECK(data.audio_bank().find_source(uri,s.source_sha256,joined));CHECK(asset.stable_id==joined.stable_id&&asset.source_sha256==s.source_sha256&&asset.source_path==uri&&!asset.loops());CHECK(s.gain_db==0&&s.pitch_scale==1&&s.bus==1&&s.pause_mode==0);used.insert(asset.stable_id);
  if(s.kind==DungeonMechanismSoundKind::ManagerChannel){CHECK(s.owner==0&&s.channel==g.text(g.policy().denied_channel));++denied_count;}
  else {CHECK(s.owner&&!s.channel.size());++node_count;bool found=false;for(const auto&o:data.owners())if(o.owner==s.owner){CHECK(s.path==o.path+"/AudioStreamPlayer");found=true;}CHECK(found);}
  const std::string path=std::string(argv[3])+"/"+std::string(asset.pcm_path);AudioPcmStream first,second;CHECK(first.open(asset,path.c_str(),error)&&second.open(asset,path.c_str(),error));
  std::vector<int16_t>one(2048*asset.channels),two(2048*asset.channels);uint32_t n=0,m=0;CHECK(first.read(one.data(),2048,n,error)&&second.read(two.data(),2048,m,error));CHECK(n==m&&n>0&&std::equal(one.begin(),one.begin()+n*asset.channels,two.begin()));
  CHECK(first.read(one.data(),2048,n,error));CHECK(first.rewind()&&first.read(one.data(),2048,n,error));CHECK(std::equal(one.begin(),one.begin()+n*asset.channels,two.begin()));
  uint32_t total=n;while(total<asset.frames){CHECK(first.read(one.data(),2048,n,error)&&n>0);total+=n;}CHECK(total==asset.frames&&first.read(one.data(),2048,n,error)&&n==0);CHECK(second.rewind()&&second.read(two.data(),2048,m,error)&&m>0);
 }
 CHECK(node_count==6&&denied_count==1&&used.size()==4);auto copy=data;CHECK(copy.audio_bank().count()==4&&copy.sound(0)->source==data.sound(0)->source);CHECK(!data.load(b.data(),b.size(),error)&&data.sounds().size()==7);
 for(const auto&owner:data.owners()){
  const auto*profile=data.bindings().profile(owner.profile);if(profile->family==DungeonMechanismFamily::Wall)CHECK(profile->wall.matches(g,data.audio_bank(),error));
  DungeonMechanismDraw a,c;CHECK(data.draw(owner.owner,0,a,error)&&geometry.draw(owner.owner,0,c,error));CHECK(a.owner==c.owner&&a.top_left.x==c.top_left.x&&a.top_left.y==c.top_left.y&&a.width==c.width&&a.height==c.height&&a.bits==c.bits&&a.order==c.order);
 }
 auto reject=[&](const std::vector<uint8_t>&bytes){DungeonMechanismPresentationData trial;CHECK(!trial.load(bytes.data(),bytes.size(),error));CHECK(!trial.valid()&&!trial.media_bound()&&trial.sounds().empty()&&trial.audio_bank().count()==0);};
 for(size_t size=0;size<b.size();++size){DungeonMechanismPresentationData trial;CHECK(!trial.load(b.data(),size,error));CHECK(!trial.valid()&&!trial.media_bound()&&trial.sounds().empty());}
 const size_t sounds=get(b,148),bank=get(b,164);
 for(const auto&change:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,3},{20,1},{52,5},{144,7},{148,0},{152,0},{152,258},{156,84},{160,6},{164,0},{168,0},{sounds,0},{sounds+4,0},{sounds+4,3},{sounds+24,0},{sounds+56,0},{sounds+60,0x7fc00000},{sounds+64,0},{sounds+64,0x40800001},{sounds+68,2},{sounds+72,1},{sounds+76,1},{sounds+84,1},{bank+8,2},{bank+20,5}}){auto v=b;put(v,change.first,change.second);sign(v);reject(v);}
 {auto v=b;put(v,sounds+88,get(b,sounds));sign(v);reject(v);}
 {auto v=b;put(v,sounds+88+8,get(b,sounds+8));put(v,sounds+88+12,get(b,sounds+12));sign(v);reject(v);}
 {auto v=b;v[sounds+24]^=1;sign(v);reject(v);}
 for(size_t i=0;i<data.sounds().size();++i){const auto&s=data.sounds()[i];const auto*placement=static_cast<const DungeonMechanismPlacement*>(nullptr);for(const auto&o:data.owners())if(o.owner==s.owner)placement=&o;
  if(placement&&data.bindings().profile(placement->profile)->family==DungeonMechanismFamily::Wall){auto v=b;put(v,sounds+i*88+60,0x3f800000);sign(v);reject(v);}
  if(s.kind==DungeonMechanismSoundKind::ManagerChannel){auto v=b;const auto at=sounds+i*88;put(v,at+4,1);sign(v);reject(v);v=b;put(v,at+80,0);sign(v);reject(v);}
 }
 DungeonMechanismAudioRouter router,empty;std::vector<DungeonMechanismSoundCommand>commands{{0,0,17,0,DungeonMechanismSoundKind::Node,false}};
 CHECK(!empty.plan({},commands,error)&&commands[0].owner==17);CHECK(!router.prepare(geometry,42,error)&&!router.prepare(data,0,error));CHECK(router.prepare(data,42,error)&&router.valid()&&router.epoch()==42);CHECK(!router.prepare(data,43,error));
 uint32_t wall=0,lever=0;for(const auto&o:data.owners()){const auto family=data.bindings().profile(o.profile)->family;if(family==DungeonMechanismFamily::Wall)wall=o.owner;else if(family==DungeonMechanismFamily::Lever)lever=o.owner;}CHECK(wall&&lever);
 auto wall_event=[&](bool playing){DungeonMechanismEvent e;e.kind=DungeonMechanismEventKind::WallAudio;e.epoch=42;e.owner=wall;e.playing=playing;return e;};
 auto switch_event=[&](bool denied){DungeonMechanismEvent e;e.kind=DungeonMechanismEventKind::Request;e.epoch=42;e.owner=lever;e.request.kind=denied?DungeonSwitchEventKind::DeniedAudio:DungeonSwitchEventKind::SwitchAudio;e.request.target={lever,1};return e;};
 auto start=wall_event(true),stop=wall_event(false);CHECK(router.plan({start,start,stop,switch_event(false),switch_event(true)},commands,error)&&commands.size()==5);
 CHECK(commands[0].owner==wall&&commands[0].playing&&commands[1].owner==wall&&commands[1].playing&&!commands[2].playing);CHECK(commands[3].owner==lever&&commands[3].kind==DungeonMechanismSoundKind::Node);CHECK(commands[4].owner==0&&commands[4].request_owner==lever&&commands[4].kind==DungeonMechanismSoundKind::ManagerChannel);
 for(const auto&command:commands)CHECK(command.epoch==42&&command.slot<data.sounds().size()&&data.sounds()[command.slot].owner==command.owner);
 auto reject_events=[&](const std::vector<DungeonMechanismEvent>&events){const auto before=commands;CHECK(!router.plan(events,commands,error));CHECK(commands.size()==before.size());for(size_t i=0;i<before.size();++i)CHECK(commands[i].epoch==before[i].epoch&&commands[i].slot==before[i].slot&&commands[i].owner==before[i].owner&&commands[i].playing==before[i].playing);};
 {auto bad=start;bad.epoch=43;reject_events({start,bad});bad=start;bad.owner=0;reject_events({start,bad});bad=start;bad.owner=lever;reject_events({bad});bad=start;bad.kind=static_cast<DungeonMechanismEventKind>(99);reject_events({start,bad});}
 {auto bad=switch_event(false);bad.owner=wall;reject_events({bad});bad=switch_event(false);bad.request.target.generation=0;reject_events({bad});bad=switch_event(false);bad.request.kind=static_cast<DungeonSwitchEventKind>(99);reject_events({start,bad});bad=switch_event(false);bad.owner=bad.request.target.id=wall;reject_events({bad});}
 reject_events(std::vector<DungeonMechanismEvent>(4097,start));
 CHECK(router.retire(commands,error)&&!router.valid()&&router.epoch()==0&&commands.size()==6);
 for(const auto&command:commands)CHECK(command.owner!=0&&!command.playing&&command.kind==DungeonMechanismSoundKind::Node&&command.epoch==42);
 CHECK(!router.retire(commands,error)&&commands.size()==6);CHECK(!router.plan({start},commands,error)&&commands.size()==6);CHECK(!router.prepare(data,43,error));
 std::printf("Mechanism media: %u checks; capability1 compatibility,7 source sound owners,4 real PCM sources,independent streams/restart/end,bank/clock joins,all truncations and atomic negatives; no live platform audio\n",checks);
}
