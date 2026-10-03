#include "encore/house_presentation.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}}
std::string hex(const WorldDialoguePose&p){const char digits[]="0123456789abcdef";std::string out;for(const auto&l:p.lines)for(unsigned char c:l.text){out+=digits[c>>4];out+=digits[c&15];}return out.empty()?"-":out;}
struct Case {uint32_t first=0,count=0,fast=0,next=0,lines=0;};
struct Completion {HousePresentation*p=nullptr;unsigned calls=0;uint32_t counter=0;bool saw_finished=false;};
bool completed(void*state){auto&c=*static_cast<Completion*>(state);++c.calls;c.counter=c.p->visible_characters();c.saw_finished=c.p->dialogue_finished();c.p->set_choice_rows(1);return true;}
}
int main(int argc,char**argv){
 check(argc==4,"fixture directory, native reports and font provided");std::map<std::string,Case>cases;std::string name;
 std::ifstream metadata(std::string(argv[2])+"/cases.tsv"),finals(std::string(argv[2])+"/finals.tsv"),frames(std::string(argv[2])+"/frames.tsv");check(bool(metadata)&&bool(finals)&&bool(frames),"native source oracle files available");
 Case c;while(metadata>>name>>c.first>>c.count>>c.fast)cases[name]=c;
 while(finals>>name>>c.next>>c.lines){check(cases.count(name)==1,"native final case known");cases[name].next=c.next;cases[name].lines=c.lines;}
 BattleData font;HouseData data;std::string error,last;HousePresentation p;SourceRandom random(123);uint32_t tick,visible,stopped,finished,voices,actual_voices=0,frame_count=0;std::string shown;
 check(font.load_file(argv[3],error),error.c_str());
 while(frames>>name>>tick>>visible>>stopped>>finished>>voices>>shown){
  check(cases.count(name)==1,"native frame case known");const auto test=cases[name];
  if(last!=name){random.seed(123);check(data.load_file((std::string(argv[1])+"/"+name+".enchouse").c_str(),error),error.c_str());check(p.begin(data.view(),font.view(),random),p.error());check(p.begin_dialogue(test.first,test.count,"Ninten"),p.error());check(p.idle_frame(.31),p.error());p.take_audio_events();actual_voices=0;last=name;}
  if(p.dialogue_stopped())p.input(true,false);
  if(tick>=test.fast)p.input(false,true);
  check(p.physics_frame(1.0/60,{}),p.error());
  for(const auto&e:p.take_audio_events())if(e.kind==HouseAudioKind::VoiceStart)++actual_voices;
  const auto pose=p.dialogue_pose();
  if(p.visible_characters()!=visible||p.dialogue_stopped()!=bool(stopped)||p.dialogue_finished()!=bool(finished)||actual_voices!=voices||hex(pose)!=shown){std::cerr<<name<<" tick "<<tick<<" native visible="<<visible<<" stopped="<<stopped<<" finished="<<finished<<" voices="<<voices<<" text="<<shown<<" actual visible="<<p.visible_characters()<<" stopped="<<p.dialogue_stopped()<<" finished="<<p.dialogue_finished()<<" voices="<<actual_voices<<" text="<<hex(pose)<<'\n';}
  check(p.visible_characters()==visible,"native printable/control counter");check(p.dialogue_stopped()==bool(stopped),"native WAIT timing");check(p.dialogue_finished()==bool(finished),"native final overrun timing");check(actual_voices==voices,"native delay-suppressed voice calls");check(random.raw_draw_count()==uint64_t(voices)*3,"native shared RNG consumption");check(hex(pose)==shown,"native visible glyphs omit delay and newline cells");
  for(const auto&line:pose.lines)check(line.text.size()==line.colors.size(),"color indices align after invisible cells");
  if(name=="colored_delay")for(const auto&line:pose.lines)for(size_t i=0;i<line.text.size();++i)check(line.colors[i]==(line.text[i]=='B'?0xff2c8beau:0xffffffffu),"inline color survives invisible-cell filtering");
  if(finished){check(random.randi()==test.next,"native next random integer");check(pose.lines.size()==test.lines,"native source forced/wrapped line count");}
  ++frame_count;
 }
 check(cases.size()==11&&frame_count>100,"complete source fixture exercised");
 check(data.load_file((std::string(argv[1])+"/excessive-delay.enchouse").c_str(),error),error.c_str());check(p.begin(data.view(),font.view(),random),p.error());c=cases.at("only_delay");check(!p.begin_dialogue(c.first,c.count,"Ninten"),"excessive dynamic delay expansion fails closed before allocation");
 // Source choices reserve one trailing blank row. Completion callback runs
 // inside the overrun iteration, before its voice RNG, without another tick.
 random.seed(123);check(data.load_file((std::string(argv[1])+"/forced_newline.enchouse").c_str(),error),error.c_str());check(p.begin(data.view(),font.view(),random),p.error());
 Completion callback{&p};p.set_text_completion_callback(completed,&callback);c=cases.at("forced_newline");check(p.begin_dialogue(c.first,c.count,"Ninten"),p.error());p.idle_frame(.31);p.take_audio_events();
 for(unsigned guard=0;guard<60&&!p.dialogue_finished();++guard)check(p.physics_frame(1.0/60,{}),p.error());
 check(callback.calls==1&&callback.saw_finished&&callback.counter==3,"completion callback is synchronous final overrun");
 auto pose=p.dialogue_pose();check(p.dialogue_active()&&!p.dialogue_closing()&&!pose.cursor_visible,"choice panel suppresses ordinary down arrow");const auto oldbox=pose.box;const auto oldname=pose.name;
 p.input(true,false,true);check(!p.take_dialogue_advance()&&!p.dialogue_closing(),"choice input cannot advance ordinary dialogue");
 p.set_choice_rows(0);check(p.dialogue_pose().cursor_visible,"normal cursor resumes after choices reset");
 // Use four completed lines to require scrolling and measure the actual row.
 p.set_text_completion_callback(nullptr,nullptr);p.clear_story_text();check(p.present_story_dialogue(c.first,c.count,"Ninten"),p.error());p.input(false,true);check(p.physics_frame(1,{}),p.error());check(p.present_story_dialogue(c.first,c.count,"Ninten"),p.error());p.input(false,true);check(p.physics_frame(1,{}),p.error());
 const auto no_choices=p.dialogue_pose();p.set_choice_rows(1);const auto choices=p.dialogue_pose();const auto metrics=data.view().parameter(HouseParameter::FontMetrics);
 check(no_choices.lines.size()==4&&choices.lines.size()==4,"reserved blank row does not invent printable text");check(std::abs(no_choices.lines[0].y-choices.lines[0].y-metrics.x-metrics.y)<.00001,"source options append exactly one line of scroll");
 const auto rng_before=random.raw_draw_count();check(p.physics_frame(1,{}),p.error());check(random.raw_draw_count()==rng_before,"choices reserve rows without voice or delay time");
 p.take_audio_events();p.clear_story_text();pose=p.dialogue_pose();check(pose.visible&&pose.text_visible&&pose.lines.empty()&&!pose.cursor_visible,"clear preserves visible box and clears text/choices");check(p.visible_characters()==0&&!p.dialogue_stopped()&&!p.talking(),"clear resets counters and talking");check(pose.box.y==oldbox.y&&pose.name.y==oldname.y,"clear preserves box/name animation");check(p.physics_frame(.1,{}),"cleared live box can wait safely");
 check(p.present_story_dialogue(c.first,c.count,"Ninten"),p.error());pose=p.dialogue_pose();check(pose.lines.size()==1&&pose.lines[0].text.empty()&&pose.box.y==oldbox.y,"branch starts new text in existing open box");check(p.take_audio_events().empty(),"clear/present does not reopen or confirm the box");
 p.input(false,true);check(p.physics_frame(1,{}),p.error());check(p.dialogue_pose().cursor_visible,"new phrase resets reserved choice rows");p.close_story_dialogue();check(p.dialogue_closing(),"new phrase can close normally");
 std::cout<<"House Record presentation: "<<checks<<" checks passed; "<<frame_count<<" official Godot frames\n";
}
