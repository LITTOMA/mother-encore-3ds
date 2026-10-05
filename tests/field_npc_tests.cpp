// Manual only. Real pinned NPC content; mocks verify event boundaries only.
#include "encore/field_npc.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cassert>
#include <map>
#include <set>
using namespace encore::upstream;
static void put(std::vector<uint8_t>&b,size_t x,uint32_t v){for(size_t i=0;i<4;++i)b[x+i]=uint8_t(v>>(i*8));}
static void fix(std::vector<uint8_t>&b){put(b,16,0);put(b,16,encore::crc32(b.data(),b.size()));}
int main(int argc,char**argv){
 std::string error;std::vector<uint8_t>bytes;assert(encore::read_file(argc>1?argv[1]:"romfs/data/podunk-npcs.encnpc",bytes,8*1024*1024,error));FieldNpcData data;assert(data.load(bytes.data(),bytes.size(),error)&&data.npcs().size()==67);const auto kept=data.npcs().front().id;
 for(const size_t at:{size_t(0),size_t(7),size_t(32),size_t(63),size_t(64),size_t(164),bytes.size()-1})assert(!data.load(bytes.data(),at,error)&&data.valid()&&data.npcs().front().id==kept);
 for(const size_t at:{size_t(8),size_t(20),size_t(24),size_t(28),size_t(56),size_t(64),size_t(68),size_t(164),size_t(172)}){auto bad=bytes;put(bad,at,UINT32_MAX);fix(bad);assert(!data.load(bad.data(),bad.size(),error));}
 const auto* mick=&data.npcs().front();for(const auto&n:data.npcs())if(n.node=="Objects/NPCS/npc21")mick=&n;assert(mick->node=="Objects/NPCS/npc21");
 SourceRandom random(79);FieldNpcRuntime runtime;FieldNpcContext ctx;ctx.player={-80,8};std::map<std::string,bool>flags;std::set<std::string>seen;std::string opened;unsigned talkers=0,closed=0,effects=0;uint64_t timer_receipt=0;
 auto identity=[](uint32_t id,const FieldNpcDialogue&d){return std::to_string(id)+":"+d.flag+":"+std::to_string(d.ordinal)+":"+d.program;};
 FieldNpcHost host;host.context=[&](uint32_t,FieldNpcContext&out,std::string&){out=ctx;return true;};host.flag=[&](const std::string&name,bool&out,std::string&){out=flags[name];return true;};host.seen=[&](uint32_t id,const FieldNpcDialogue&d,bool&out,std::string&){out=seen.count(identity(id,d))!=0;return true;};host.mark_seen=[&](uint32_t id,const FieldNpcDialogue&d,std::string&){seen.insert(identity(id,d));return true;};
 host.admit_program=[](const std::string&name,std::string&){return name=="Podunk/woof"||name=="Podunk/woof_secret"||name=="Podunk/woof_deal"||name=="Podunk/woof_food"||name=="Podunk/woof_key";};host.open_program=[&](uint32_t,const std::string&name,bool,const FieldNpcDescriptor&,std::string&){opened=name;return true;};host.telepathy_effect=[&](uint32_t,bool enabled,std::string&){assert(enabled);++effects;return true;};host.begin_talker=[&](uint32_t,std::string&){++talkers;return true;};host.close_commands=[&](std::string&){++closed;return true;};
 host.cached_raycast=[](uint32_t,Vec2,bool&hit,std::string&){hit=false;return true;};host.move_and_slide=[](uint32_t,Vec2 velocity,float dt,Vec2&position,Vec2&out,std::string&){position.x+=velocity.x*dt;position.y+=velocity.y*dt;out=velocity;return true;};host.present=[](uint32_t,FieldNpcPresentation,const FieldNpcDescriptor&,const FieldNpcInstance&,std::string&){return true;};host.timer=[&](uint32_t,FieldNpcTimer,double,uint64_t receipt,std::string&){timer_receipt=receipt;return true;};
 assert(!runtime.initialize(&data,&random,{},error));assert(runtime.initialize(&data,&random,host,error));assert(!runtime.interact(mick->id));assert(runtime.ready(mick->id));assert(!runtime.ready(mick->id));bool has=false;assert(runtime.has_dialog(mick->id,false,has)&&has);assert(runtime.interact(mick->id)&&opened=="Podunk/woof"&&closed==1&&talkers==1);assert(runtime.stop_interaction(mick->id));
 flags["mick_scratch"]=true;assert(runtime.telepathy(mick->id)&&opened=="Podunk/woof_key"&&effects==1&&talkers==1);flags["mick_telepathy"]=true;assert(runtime.telepathy(mick->id)&&opened=="Podunk/woof_food"&&effects==2);assert(runtime.interact(mick->id)&&opened=="Podunk/woof_deal");
 flags["got_dog_treats"]=true;const auto before=seen.size();const auto before_talkers=talkers;assert(!runtime.interact(mick->id));assert(seen.size()==before&&talkers==before_talkers);flags["got_dog_treats"]=false;
 auto draws=random.raw_draw_count();ctx.cutscene=true;assert(runtime.screen_entered(mick->id)&&random.raw_draw_count()==draws);ctx.cutscene=false;assert(runtime.screen_entered(mick->id)&&random.raw_draw_count()>draws);ctx.player_paused=true;draws=random.raw_draw_count();assert(runtime.wander_timeout(mick->id)&&random.raw_draw_count()>draws);assert(runtime.view_entered(mick->id,true));assert(runtime.near_entered(mick->id,true));assert(runtime.physics_step(mick->id,1.f/60));assert(runtime.idle_animations(mick->id,1.0/60));assert(!runtime.return_direction_timeout(mick->id,999));assert(runtime.tree_exiting(mick->id,false)&&runtime.destroy(mick->id));assert(!runtime.interact(mick->id));(void)timer_receipt;
}
