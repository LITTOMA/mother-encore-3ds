#include "encore/geyser_timeline.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;void check(bool ok){++checks;if(!ok){std::cerr<<"Geyser idle mismatch check "<<checks<<'\n';std::exit(1);}}
float number(uint32_t raw){float f;std::memcpy(&f,&raw,4);return f;}
uint32_t bits(float f){uint32_t n;std::memcpy(&n,&f,4);return n;}
void state(GeyserTimeline&a,std::ifstream&in,std::string&e){GeyserSnapshot s;check(a.inspect(5,12,s,e));uint32_t t,frame,monitor,disabled,active,paused,gain,queued,finished,count;in>>t>>frame>>monitor>>disabled>>active>>paused>>gain>>queued>>finished>>count;check(bool(in));check(bits(s.time)==t);check(s.frame==frame);check(s.monitoring==bool(monitor));check(s.disabled==bool(disabled));check(s.active==bool(active));check(s.stream_paused==bool(paused));check(bits(s.gain)==gain);check(s.queued_free==bool(queued));check(!s.playing==bool(finished));const auto commands=a.take_commands();check(commands.size()==count);for(const auto&c:commands){uint32_t kind,sound,offset;in>>kind>>sound>>offset;check(uint32_t(c.kind)==kind);check(c.sound==sound);check(bits(c.offset)==offset);}check(bool(in));}
}
int main(int argc,char**argv){check(argc==3);std::string e;GeyserTimelineData d;check(d.load_file(argv[1],e));GeyserTimeline a;check(a.prepare(d,5,12,e));std::ifstream in(argv[2]);unsigned frames;in>>frames;check(frames==224);state(a,in,e);auto voice=GeyserNamedVoice::Absent;bool tree=false;GeyserContext previous{true,false,false,false};
 for(unsigned i=0;i<frames;++i){uint32_t delta,paused,on_screen,battle,cutscene,gameover,after_screen,after_battle,after_cutscene,after_gameover,count;in>>delta>>paused>>on_screen>>battle>>cutscene>>gameover>>after_screen>>after_battle>>after_cutscene>>after_gameover>>count;check(bool(in));check(tree==bool(paused));check(previous.battle==bool(battle)&&previous.cutscene==bool(cutscene)&&previous.game_over==bool(gameover));check(a.advance_idle(5,12,number(delta),!tree,e));if(!tree)check(a.process_ui(5,12,{bool(on_screen),bool(battle),bool(cutscene),bool(gameover)},e));state(a,in,e);
  for(unsigned j=0;j<count;++j){uint32_t op,value;in>>op>>value;check(bool(in));if(op==1)check(a.player_signal(5,12,value,e));else if(op==2){tree=value;check(a.tree_notification(5,12,tree,false,e));}else if(op==3)check(value<=1);else if(op==4){previous.battle=value==1;previous.cutscene=value==2;previous.game_over=value==3;}else check(false);}
  GeyserContext ctx{bool(after_screen),bool(after_battle),bool(after_cutscene),bool(after_gameover)};check(a.flush_deferred(5,12,ctx,voice,e));state(a,in,e);
 }
 std::string extra;check(!(in>>extra));check(a.retire(5,12,e));GeyserTimeline notifications;check(notifications.prepare(d,6,13,e));GeyserSnapshot pose;check(notifications.tree_notification(6,13,true,true,e));check(notifications.inspect(6,13,pose,e)&&!pose.stream_paused);check(notifications.player_signal(6,13,true,e));check(notifications.tree_notification(6,13,true,true,e));check(notifications.inspect(6,13,pose,e)&&pose.stream_paused&&!pose.active);check(notifications.tree_notification(6,13,false,true,e));check(notifications.inspect(6,13,pose,e)&&!pose.stream_paused&&!pose.active);check(!notifications.tree_notification(5,13,true,false,e));check(!notifications.tree_notification(6,12,true,false,e));check(notifications.tree_notification(6,13,true,false,e));check(notifications.inspect(6,13,pose,e)&&pose.stream_paused);std::cout<<checks<<" geyser actual idle checks passed;224 original frames,not spatial audio/GPU/hardware\n";
}
