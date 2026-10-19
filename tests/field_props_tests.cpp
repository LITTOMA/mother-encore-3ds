#include "encore/field_props.hpp"
#include "encore/field_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Field props %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));}
void number(std::vector<uint8_t>&b,size_t at,float value){uint32_t bits;std::memcpy(&bits,&value,4);put(b,at,bits);}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
struct Host:FieldPropHost {
 FieldProps*owner=nullptr;unsigned calls=0;bool admit=true,all_queued_alive=false;
 bool can_call(uint32_t,std::string&e)override{if(!admit){e="Explicit unsupported callback";return false;}return true;}
 bool call(uint32_t,uint32_t index,std::string&)override{CHECK(owner&&owner->states()[index].alive&&owner->states()[index].queued);if(all_queued_alive)for(const auto&s:owner->states())if(s.queued)CHECK(s.alive);++calls;return true;}
};
}
int main(int argc,char**argv){
 CHECK(argc==4);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,128*1024,error));FieldPropsData data;CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.props().size()==13&&data.flags().size()==3&&data.policy().width==48&&data.policy().height==48&&data.clips().frame_count()==21);
 const auto policy=get(bytes,84),records=get(bytes,116),flags_offset=get(bytes,132);uint32_t bat=0,cem=0,shovel=0;
 for(uint32_t i=0;i<data.flags().size();++i){if(data.flags()[i]=="bat")bat=i;else if(data.flags()[i]=="cem_roots_grown")cem=i;else if(data.flags()[i]=="got_shovel")shovel=i;else CHECK(false);}
 const auto callback=data.props()[11].callback;CHECK(callback!=kFieldNone);
 std::ifstream trace(argv[2]);CHECK(bool(trace));std::string line;CHECK(bool(std::getline(trace,line)));unsigned rows=0,cases=0;FieldProps runtime;std::vector<bool>flags;Host host;
 while(std::getline(trace,line)){
  std::istringstream in(line);std::string type,extra;CHECK(bool(in>>type));
  if(type=="start"){
   std::string name;unsigned appear,finish,called,bv,cv,sv;CHECK(bool(in>>name>>appear>>finish>>called>>bv>>cv>>sv)&&!(in>>extra));auto candidate=bytes;
   // An undefined finish flag never reaches calledObject in the source. The
   // compiled closed policy deliberately has no unreachable callback binding.
   put(candidate,records+16,appear?cem:kFieldNone);put(candidate,records+20,finish?shovel:kFieldNone);put(candidate,records+24,called&&finish?callback:kFieldNone);
   number(candidate,records+32,32);number(candidate,records+36,64);number(candidate,records+40,32);number(candidate,records+44,48);sign(candidate);
   FieldPropsData scoped;CHECK(scoped.load(candidate.data(),candidate.size(),error));flags.assign(data.flags().size(),false);flags[bat]=bv;flags[cem]=cv;flags[shovel]=sv;CHECK(runtime.initialize(scoped,flags,error));host=Host{};host.owner=&runtime;++cases;continue;
  }
  CHECK(type=="step");std::string op;double a;unsigned b;CHECK(bool(in>>op>>a>>b));
  unsigned alive,frame,visible,body,hitbox,interaction,node,prompt,queued,playing,current,clones,clone_mask,rumble,dialogue,called;
  CHECK(bool(in>>alive>>frame>>visible>>body>>hitbox>>interaction>>node>>prompt>>queued>>playing>>current>>clones>>clone_mask>>rumble>>dialogue>>called)&&!(in>>extra));
  host.calls=0;
  if(op=="ready"){}
  else if(op=="advance")CHECK(runtime.advance(a,flags,error));
  else if(op=="grow")CHECK(runtime.grow(0,error));
  else if(op=="hit")CHECK(runtime.hit(0,error));
  else if(op=="enter"||op=="exit")CHECK(runtime.set_visible(0,op=="enter",error));
  else if(op=="interact")CHECK(runtime.interact(0,flags,error));
  else if(op=="flag"){CHECK(a==0||a==1||a==2);flags[a==0?bat:a==1?cem:shovel]=b;}
  else if(op=="flush")CHECK(runtime.flush_deferred(host,error));else CHECK(false);
  const auto&s=runtime.states()[0];const auto pose=s.animation.pose();CHECK(s.alive==bool(alive));
  if(alive){if(!(pose.frame==frame&&pose.visible==bool(visible)&&pose.body==bool(body)&&pose.hitbox==bool(hitbox)&&pose.interaction==bool(interaction)))std::fprintf(stderr,"Native lifecycle row%u %s; actual frame%u v%u b%u h%u i%u clip%u t%.17g\n",rows,line.c_str(),pose.frame,pose.visible,pose.body,pose.hitbox,pose.interaction,s.animation.assigned_clip(),s.animation.position());CHECK(pose.frame==frame&&pose.visible==bool(visible)&&pose.body==bool(body)&&pose.hitbox==bool(hitbox)&&pose.interaction==bool(interaction));CHECK(s.visible==bool(node)&&s.prompt==bool(prompt)&&s.queued==bool(queued)&&s.animation.playing()==bool(playing)&&(playing?s.animation.assigned_clip():0)==current);}
  CHECK(runtime.clones().size()==clones);for(unsigned i=0;i<clones;++i){const auto&c=runtime.clones()[i];CHECK(c.frame==data.policy().clone_frame&&c.center.x==32&&c.center.y==48&&c.visible==bool(clone_mask&(1u<<i)));}
  unsigned got_rumble=0,got_dialogue=0,got_sound=0;for(auto e:runtime.take_events()){CHECK(e.prop==0);switch(e.kind){case FieldPropEventKind::Rumble:++got_rumble;break;case FieldPropEventKind::Sound:++got_sound;break;case FieldPropEventKind::HardDialogue:CHECK(!got_dialogue);got_dialogue=1;break;case FieldPropEventKind::EasyDialogue:CHECK(!got_dialogue);got_dialogue=2;break;}}
  CHECK(got_rumble==rumble&&got_dialogue==dialogue&&host.calls==called);CHECK(got_sound==unsigned(op=="hit"));++rows;
 }
 CHECK(cases==14&&rows==86);
 // A declined callback preflight preserves all queued live owners and clones.
 flags.assign(3,false);CHECK(runtime.initialize(data,flags,error));host.owner=&runtime;host.admit=false;CHECK(runtime.hit(11,error)&&runtime.advance(1,flags,error));CHECK(runtime.states()[11].queued&&runtime.states()[11].alive);CHECK(!runtime.flush_deferred(host,error)&&runtime.states()[11].alive&&runtime.clones().size()==1);host.admit=true;CHECK(runtime.flush_deferred(host,error)&&!runtime.states()[11].alive);CHECK(!runtime.hit(11,error)&&!runtime.grow(11,error)&&!runtime.set_visible(11,true,error));CHECK(runtime.flush_deferred(host,error));
 for(double bad:{-1.,2.,double(INFINITY),double(NAN)})CHECK(!runtime.advance(bad,flags,error));CHECK(!runtime.advance(0,{},error)&&!runtime.initialize(data,{},error)&&runtime.ready());CHECK(!runtime.grow(1000,error)&&!runtime.interact(1000,flags,error));
 FieldPropsData invalid;CHECK(!runtime.initialize(invalid,flags,error));CHECK(runtime.initialize(data,flags,error));for(unsigned i=0;i<64;++i)CHECK(runtime.grow(0,error));CHECK(!runtime.grow(0,error)&&runtime.states()[0].grow_waiters==64);CHECK(runtime.advance(1,flags,error)&&runtime.states()[0].grow_waiters==0);
 {auto v=bytes;put(v,records+20,shovel);put(v,records+24,callback);sign(v);FieldPropsData two;CHECK(two.load(v.data(),v.size(),error));CHECK(runtime.initialize(two,flags,error));CHECK(runtime.hit(0,error)&&runtime.hit(11,error)&&runtime.advance(1,flags,error));host=Host{};host.owner=&runtime;host.all_queued_alive=true;CHECK(runtime.flush_deferred(host,error)&&host.calls==2&&!runtime.states()[0].alive&&!runtime.states()[11].alive);}
 CHECK(runtime.initialize(data,flags,error));for(unsigned i=0;i<512;++i)CHECK(runtime.hit(0,error));CHECK(!runtime.hit(0,error)&&runtime.clones().size()==512);
 auto reject=[&](std::vector<uint8_t>bad){sign(bad);CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.valid()&&data.props().size()==13&&data.policy().scene==2);};
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{24,1},{28,1},{32,0},{52,4},{56,1},{60,1},{64,2},{68,143},{72,0},{76,2},{84,UINT32_MAX},{92,92},{policy,0},{policy+4,99},{policy+8,1},{policy+20,21},{policy+24,3},{policy+28,0},{policy+32,1025},{policy+36,UINT32_MAX},{policy+40,0},{policy+84,UINT32_MAX},{policy+88,0},{policy+92,1},{records,0},{records+104,1},{records+108,get(bytes,records+4)},{records+112,get(bytes,records+8)},{records+16,3},{records+20,4},{records+24,1},{records+28,1},{flags_offset,UINT32_MAX},{flags_offset+4,0}}){auto bad=bytes;put(bad,change.first,change.second);reject(bad);}
 for(float bad:{-1.f,INFINITY,NAN}){auto v=bytes;number(v,policy+68,bad);reject(v);}
 for(float bad:{0.f,-1.f,INFINITY,NAN}){auto v=bytes;number(v,records+56,bad);reject(v);}
 for(float bad:{1000001.f,INFINITY,NAN}){auto v=bytes;number(v,records+32,bad);reject(v);}
 {auto v=bytes;put(v,flags_offset+8,get(v,flags_offset));put(v,flags_offset+12,get(v,flags_offset+4));reject(v);}
 {auto v=bytes;v[get(v,68)]=0;reject(v);}
 {auto v=bytes;v.push_back(0);put(v,12,uint32_t(v.size()));reject(v);}
 for(size_t i=0;i<bytes.size();++i){CHECK(!data.load(bytes.data(),i,error)&&data.valid());auto v=bytes;v[i]^=0x80;CHECK(!data.load(v.data(),v.size(),error)&&data.valid());}CHECK(!data.load(nullptr,bytes.size(),error));
 // Actual compiled source scene binding and dynamic bodies; not a release or
 // input-driven quest test. NPC/renderer/doors are outside this focused slice.
 const std::string root=argv[3];RoomData room;FieldMapData map;WorldLinksData links;
 CHECK(room.load_file((root+"/world/podunk/podunk.encroom").c_str(),error));CHECK(map.load_file((root+"/world/podunk/podunk.encmap").c_str(),error));CHECK(links.load_file((root+"/world/world.enclinks").c_str(),error));CHECK(data.bind(room.view(),map.view(),error));CHECK(!data.bind({},map.view(),error)&&!data.bind(room.view(),{},error));
 for(auto mutation:std::vector<std::pair<size_t,uint32_t>>{{policy,999},{records+4,UINT32_MAX},{records+8,UINT32_MAX},{records+12,UINT32_MAX},{records+12,0},{records+104*11+24,0}}){auto v=bytes;put(v,mutation.first,mutation.second);sign(v);FieldPropsData wrong;CHECK(wrong.load(v.data(),v.size(),error)&&!wrong.bind(room.view(),map.view(),error));}
 {auto v=bytes;const auto at=get(v,68)+get(v,flags_offset);v[at]='z';v[at+1]='z';v[at+2]='z';sign(v);FieldPropsData wrong;CHECK(wrong.load(v.data(),v.size(),error)&&!wrong.bind(room.view(),map.view(),error));}
 FieldScene scene;CHECK(!scene.bind_props(data,error));std::vector<bool>story(room.view().flag_count(),false);const auto&gated=data.props()[2];CHECK(scene.prepare(room.view(),map.view(),links,story,{gated.position.x-40,gated.position.y},{1,0},{400,240},error));CHECK(scene.bind_props(data,error));CHECK(!scene.bind_props(data,error));CHECK(scene.idle_frame(0,false));PropPose pose;CHECK(scene.prop_sprite(gated.sprite,pose)&&!pose.visible&&!pose.body&&pose.hitbox&&!pose.interaction);CHECK(!scene.sprite_visible(gated.sprite));
 CHECK(scene.world.set_story_flag(data.flags()[cem],true,true)&&scene.refresh_conditions(error));CHECK(scene.prop_sprite(gated.sprite,pose)&&!pose.visible);CHECK(scene.grow_prop(gated.id,error)&&scene.idle_frame(0,false));CHECK(scene.prop_sprite(gated.sprite,pose)&&pose.visible&&pose.body&&pose.hitbox&&pose.interaction&&scene.sprite_visible(gated.sprite));CHECK(scene.idle_frame(1,false)&&scene.idle_frame(0,false));CHECK(scene.prop_sprite(gated.sprite,pose)&&pose.frame==data.clips().initial().frame);CHECK(!scene.grow_prop(UINT32_MAX,error));
 CHECK(scene.hit_prop(2,error)&&scene.idle_frame(0,false));CHECK(scene.prop_sprite(gated.sprite,pose)&&!pose.body&&!pose.hitbox&&!pose.interaction);CHECK(scene.props().clones().size()==1);CHECK(scene.refresh_conditions(error)&&scene.prop_sprite(gated.sprite,pose)&&!pose.body);CHECK(!scene.prop_sprite(UINT32_MAX,pose));
 CHECK(!scene.prepare(room.view(),map.view(),links,story,{0,0},{1,0},{400,240},error)&&scene.props().ready()&&scene.props().clones().size()==1);
 FieldCollision collision;std::vector<bool>layers(map.view().count(FieldMapSection::Layers),true),bodies(map.view().count(FieldMapSection::Bodies),true);CHECK(collision.bind(map.view(),layers,bodies,error));std::vector<MotionObstacle>before,after;CHECK(collision.collect({gated.position.x-9,gated.position.y-9},{gated.position.x+9,gated.position.y+9},before));CHECK(collision.set_static_body_enabled(gated.body,false));CHECK(collision.collect({gated.position.x-9,gated.position.y-9},{gated.position.x+9,gated.position.y+9},after));CHECK(before.size()==after.size()+1);CHECK(!collision.set_static_body_enabled(UINT32_MAX,true));CHECK(collision.set_static_body_enabled(gated.body,true));after.clear();CHECK(collision.collect({gated.position.x-9,gated.position.y-9},{gated.position.x+9,gated.position.y+9},after)&&before.size()==after.size());for(unsigned i=0;i<before.size();++i)CHECK(before[i].points==after[i].points&&before[i].count==after[i].count);
 std::printf("Field props: %u checks, %u original-engine lifecycle observations / %u cases, source scene binding, targeted collision and atomic malformed loads; candidate only, no release/quest/render/audio/hardware pass\n",checks,rows,cases);
}
