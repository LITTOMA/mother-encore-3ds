#include "encore/geyser_timeline.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
using namespace encore::upstream;
namespace {unsigned checks=0;void check_at(bool ok,unsigned line){++checks;if(!ok){std::cerr<<"Geyser failed check "<<checks<<" at line "<<line<<'\n';std::exit(1);}}
#define check(condition) check_at((condition),__LINE__)
uint32_t bits(float v){uint32_t out;std::memcpy(&out,&v,4);return out;}
float value(uint32_t v){float out;std::memcpy(&out,&v,4);return out;}
uint32_t get(const std::vector<uint8_t>&v,size_t at){return uint32_t(v[at])|uint32_t(v[at+1])<<8|uint32_t(v[at+2])<<16|uint32_t(v[at+3])<<24;}
void put(std::vector<uint8_t>&v,size_t at,uint32_t n){for(unsigned j=0;j<4;++j)v[at+j]=uint8_t(n>>(8*j));}
void crc(std::vector<uint8_t>&v){put(v,16,encore::crc32(v.data()+32,v.size()-32));}
void observe(GeyserTimeline&actor,std::ifstream&in,std::string&e){GeyserSnapshot s;check(actor.inspect(11,42,s,e));uint32_t time,frame,monitor,disabled,active,paused,gain,queued,finished;in>>time>>frame>>monitor>>disabled>>active>>paused>>gain>>queued>>finished;check(bool(in));check(bits(s.time)==time);check(s.frame==frame);check(s.monitoring==bool(monitor));check(s.disabled==bool(disabled));check(s.active==bool(active));check(s.stream_paused==bool(paused));check(bits(s.gain)==gain);check(s.queued_free==bool(queued));check(!s.playing==bool(finished));unsigned count;in>>count;auto commands=actor.take_commands();check(commands.size()==count);for(const auto&c:commands){uint32_t kind,sound,offset;in>>kind>>sound>>offset;check(uint32_t(c.kind)==kind);check(c.sound==sound);check(bits(c.offset)==offset);}check(bool(in));}
}
int main(int argc,char**argv){check(argc==3);std::string e;std::vector<uint8_t>bytes;check(encore::read_file(argv[1],bytes,65536,e));GeyserTimelineData data;check(data.load(bytes.data(),bytes.size(),e));check(!data.load(bytes.data(),bytes.size(),e));check(data.policy().frames==32&&data.keys().size()==48&&data.cues().size()==3);check(data.sound(data.sounds()[0].id)&&!data.sound(0));
 for(size_t n=0;n<bytes.size();++n){GeyserTimelineData rejected;check(!rejected.load(bytes.data(),n,e));check(!rejected.valid());}
 auto reject=[&](size_t at,uint32_t v){auto b=bytes;put(b,at,v);crc(b);GeyserTimelineData rejected;check(!rejected.load(b.data(),b.size(),e));check(!rejected.valid());};
 for(auto at:{0u,8u,12u,20u,24u,28u,32u,52u,56u,60u})reject(at,0x7fffffff);
 for(unsigned n=0;n<6;++n){reject(64+n*16,99);reject(68+n*16,UINT32_MAX);reject(72+n*16,UINT32_MAX);reject(76+n*16,0);}
 const size_t pool=get(bytes,68),policy=get(bytes,84),clips=get(bytes,100),keys=get(bytes,116),cues=get(bytes,132),sounds=get(bytes,148);
 reject(pool,0x2f2e2e2f);reject(policy,UINT32_MAX);reject(policy+4,0);reject(policy+8,get(bytes,policy));reject(policy+32,0);reject(policy+36,get(bytes,policy+32));reject(policy+40,0);reject(policy+44,256);reject(policy+48,2);reject(policy+60,0x100000);reject(policy+68,0);reject(policy+76,2);reject(policy+80,0x7fc00000);reject(policy+104,0);reject(policy+132,0);reject(policy+148,0x7f800000);reject(policy+172,1);
 for(auto at:{clips,clips+4,clips+8,clips+12,clips+16,clips+20,clips+24,clips+28})reject(at,UINT32_MAX);
 reject(keys,0x7fc00000);reject(keys+4,4);reject(keys+8,999);reject(keys+12,2);reject(cues,0x7f800000);reject(cues+4,3);reject(cues+8,0);reject(cues+12,0x7fc00000);reject(cues+16,0xbf800000);reject(cues+20,32);reject(cues+24,1);reject(cues+52,0);reject(cues+84,3);reject(sounds,0);reject(sounds+12,0x7f800000);reject(sounds+16,4);reject(sounds+20,1);reject(sounds+32,get(bytes,sounds));reject(sounds+48,get(bytes,sounds+16));
 {auto b=bytes;b.push_back(0);GeyserTimelineData d;check(!d.load(b.data(),b.size(),e));}GeyserTimelineData absent;check(!absent.load(nullptr,bytes.size(),e));check(!absent.load_file(nullptr,e));check(!absent.load_file("",e));check(!absent.load_file("build/no-geyser-file",e));check(absent.clips().empty()&&absent.keys().empty()&&absent.cues().empty()&&absent.sounds().empty());
 std::ifstream in(argv[2]);check(bool(in));unsigned cases;in>>cases;check(cases==11);for(unsigned c=0;c<cases;++c){unsigned count,voice_raw,on_screen;in>>count>>voice_raw>>on_screen;GeyserNamedVoice voice=GeyserNamedVoice(voice_raw);GeyserTimeline actor;check(actor.prepare(data,11,42,e));observe(actor,in,e);for(unsigned j=0;j<count;++j){int pause;uint32_t delta,gate;in>>pause>>delta>>gate;if(pause>=0)check(actor.player_signal(11,42,pause!=0,e));check(actor.manual_advance(11,42,value(delta),e));GeyserContext ctx{bool(on_screen),gate==1,gate==2,gate==3};check(actor.process_ui(11,42,ctx,e));observe(actor,in,e);check(actor.flush_deferred(11,42,ctx,voice,e));observe(actor,in,e);}check(actor.retire(11,42,e));check(!actor.valid());}std::string extra;check(!(in>>extra));
 GeyserTimeline actor;check(!actor.prepare(absent,1,1,e));check(!actor.prepare(data,0,1,e));check(!actor.prepare(data,1,0,e));check(actor.prepare(data,7,9,e));check(!actor.prepare(data,8,1,e));GeyserSnapshot before,after;check(actor.inspect(7,9,before,e));
 for(double v:{-1.,-0.,61.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){check(!actor.manual_advance(7,9,v,e));check(!actor.advance_idle(7,9,v,false,e));check(actor.inspect(7,9,after,e));check(bits(after.time)==bits(before.time));}
 check(!actor.manual_advance(6,9,.5,e));check(!actor.manual_advance(7,8,.5,e));check(!actor.player_signal(8,9,true,e));check(!actor.process_ui(7,8,{},e));check(!actor.retire(8,9,e));check(!actor.inspect(7,8,after,e));auto unknown=GeyserNamedVoice(0);check(!actor.flush_deferred(7,9,{},unknown,e));check(uint32_t(unknown)==0);
 check(actor.player_signal(7,9,true,e));check(actor.advance_idle(7,9,1,true,e));check(actor.inspect(7,9,after,e)&&after.time==0&&after.stream_paused&&!after.active);check(actor.process_ui(7,9,{false,true,false,false},e));check(actor.inspect(7,9,after,e)&&after.gain==data.policy().muted_gain);check(actor.player_signal(7,9,false,e));check(actor.advance_idle(7,9,1,false,e));check(actor.inspect(7,9,after,e)&&after.time==0);check(actor.advance_idle(7,9,1.2,true,e));check(actor.inspect(7,9,after,e)&&after.pending_rumbles==1);check(!actor.retire(7,9,e));auto playing=GeyserNamedVoice::Playing;check(actor.flush_deferred(7,9,{true,false,false,false},playing,e));check(playing==GeyserNamedVoice::Playing);check(actor.commands().size()==2);actor.take_commands();check(actor.retire(7,9,e));check(!actor.prepare(data,7,10,e));check(actor.prepare(data,8,10,e));
 check(actor.manual_advance(8,10,3,e));check(actor.inspect(8,10,after,e)&&after.queued_free&&after.pending_rumbles==1);check(!actor.manual_advance(8,10,0,e));auto idle=GeyserNamedVoice::Idle;check(actor.flush_deferred(8,10,{true,false,false,false},idle,e));check(idle==GeyserNamedVoice::Playing);check(actor.commands().size()==3);check(actor.commands().back().kind==GeyserCommandKind::GlobalRumble);actor.take_commands();check(actor.retire(8,10,e));
 std::cout<<checks<<" geyser timeline checks passed;native manual/deferred commands,not full scene/audio/hardware\n";
}
