#include "encore/house_presentation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <limits>
using namespace encore::upstream;
namespace {unsigned checks=0;void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}}void near(double a,double b,double e,const char*m){check(std::isfinite(a)&&std::abs(a-b)<=e,m);}}
int main(int argc,char**argv){
 check(argc==5,"house, font and both native animation references provided");HouseData data;BattleData font;std::string error;
 check(data.load_file(argv[1],error),error.c_str());check(font.load_file(argv[2],error),error.c_str());const auto h=data.view();SourceRandom rng(13);HousePresentation p;
 check(p.begin(h,font.view(),rng),p.error());check(p.dialogue_done()&&!p.dialogue_active(),"idle initially");
 const auto npc=h.npc(0);auto pose=p.npc_pose(0);near(pose.position.x,192,0,"source Carol position x");near(pose.position.y,704,0,"source Carol position y");check(pose.frame==1,"source idle Down frame");near(pose.sprite_offset.y,-7,0,"source auto-offset plus child offset");
 check(!p.set_npc_looking(100,true),"unknown NPC view rejected");check(!p.physics_frame(-1,npc.position),"negative physics delta rejected");check(!p.idle_frame(std::numeric_limits<double>::infinity()),"nonfinite idle delta rejected");
 // The source world UI is a bottom box and its independent name tag, with
 // the original cubic interpolation/ease coefficients, unlike battle text.
 auto s=p.sample(HouseClipRole::DialogueOpen,0);near(s.position.y,180,0,"source opening starts below canvas");s=p.sample(HouseClipRole::DialogueOpen,.31);near(s.position.y,120,0,"source world box final origin");s=p.sample(HouseClipRole::NameOpen,.21);near(s.position.y,-15,0,"source name rises above box");
 check(!p.begin_dialogue(0,"\xff",npc.position),"unsupported glyph fails closed");
 check(p.begin_dialogue(0,"Ninten",{192,725}),p.error());check(!p.begin_dialogue(0,"Ninten",{192,725}),"active dialogue cannot be replaced");
 auto events=p.take_audio_events();check(events.size()==1&&events[0].kind==HouseAudioKind::MenuOpen,"source open audio request");
 auto d=p.dialogue_pose();check(d.visible&&d.name_visible&&d.text_visible&&!d.cursor_visible&&d.speaker=="Carol","speaker shown and cursor hidden");check(d.lines.size()==1&&d.lines[0].text.empty()&&d.lines[0].bullet,"initial newline/bullet layout");
 const double period=h.interaction().text_seconds;
 p.input(false,true);check(p.physics_frame(period,{192,725}),p.error());check(p.visible_characters()==0,"opening ignores B and typewriter uses strict greater");
 check(p.physics_frame(period,{192,725}),p.error());check(p.visible_characters()==1,"one native text character");events=p.take_audio_events();check(events.size()==1&&events[0].kind==HouseAudioKind::VoiceStart,"source voice first character");check(rng.raw_draw_count()==3,"source global rand_range consumes three raw draws");
 check(p.idle_frame(.31),p.error());p.input(false,true);check(p.physics_frame(1.0/60,{192,725}),p.error());check(p.dialogue_stopped()&&!p.dialogue_finished(),"B reveals to first WAIT and never dismisses");
 d=p.dialogue_pose();check(d.cursor_visible&&d.lines.back().text=="Are you okay, Ninten?","first original question retained");
 unsigned stops=1;for(unsigned guard=0;guard<200&&!p.dialogue_finished();++guard){if(p.dialogue_stopped()){p.input(true,false);++stops;}p.input(false,true);check(p.physics_frame(1.0/60,{192,725}),p.error());check(p.idle_frame(1.0/60),p.error());}
 check(stops==4&&p.dialogue_finished()&&!p.dialogue_stopped(),"three WAIT markers and final phrase completion");
 d=p.dialogue_pose();check(d.cursor_visible&&d.lines.back().text=="If only your father were here now...","source final text retained");
 // Count source printing iterations independently: wrapped text glyphs plus
 // three invisible WAIT characters plus the final completion overrun.
 uint32_t chars=0;for(const auto&line:d.lines)chars+=uint32_t(line.text.size());check(p.visible_characters()==chars+4,"control characters and final overrun consume time");check(rng.raw_draw_count()==uint64_t(p.visible_characters())*3,"voice RNG includes spaces, WAITs and final overrun");
 const auto before=rng.state();p.physics_frame(1,{192,725});check(rng.state()==before,"finished text consumes no RNG");
 p.input(true,false);check(p.dialogue_active()&&p.dialogue_closing(),"confirmation starts Close animation before freeing UI");check(!p.dialogue_pose().text_visible,"source hides dialogue label on close");
 p.idle_frame(.299);check(p.dialogue_active(),"UI stays active through close");p.idle_frame(.002);check(p.dialogue_done()&&!p.dialogue_active(),"UI returns only after source animation length");
 check(p.begin_dialogue(0,"ABCDEFGH",{192,725}),p.error());p.idle_frame(.31);p.input(false,true);p.physics_frame(.1,{192,725});check(p.dialogue_pose().lines.back().text=="Are you okay, ABCDEFG?","source custom-name maximum applied");
 // Native Godot 3.6.2 source voice fixture (seed123): 116 calls, 348
 // raw draws, then randi724824953. This verifies unavailable hardware never
 // suppresses the source stream's draws or their ordering.
 SourceRandom oracle(123);HousePresentation q;check(q.begin(h,font.view(),oracle),q.error());check(q.begin_dialogue(0,"Ninten",{192,730}),q.error());q.take_audio_events();q.physics_frame(1.0/60,{192,730});check(!q.talking(),"source initial visible0 sees trailing WAIT and is silent");q.physics_frame(1.0/60,{192,730});auto voice=q.take_audio_events();check(voice.size()==1,"native first voice event");near(voice[0].pitch,.99191438388685480,1e-15,"native exact global rand_range pitch");q.idle_frame(.31);
 const uint32_t waits[]={22,62,79};uint32_t wait_index=0;
 for(unsigned guard=0;guard<300&&!q.dialogue_finished();++guard){q.input(false,true);q.physics_frame(1.0/60,{192,730});q.idle_frame(1.0/60);if(q.dialogue_stopped()){check(wait_index<3&&q.visible_characters()==waits[wait_index++],"native WAIT character counter");q.input(true,false);}}
 check(q.dialogue_finished()&&q.visible_characters()==116&&oracle.raw_draw_count()==348,"native final printing count and RNG draw count");check(oracle.randi()==724824953,"native next RNG state");const auto final=q.dialogue_pose();check(final.lines.size()==4,"native cumulative four dialogue lines");near(final.lines[0].y,0,0,"native final scroll15 removes initial blank line");near(final.lines[3].y,45,0,"native final text y after scroll");
 std::ifstream samples(argv[3]);check(bool(samples),"native AnimationTree reference available");std::string case_name,last_case,state;unsigned tick,expected_frame;int talk;float dx,dy;HouseNpcAnimation animation;unsigned samples_checked=0;
 while(samples>>case_name>>tick>>talk>>dx>>dy>>expected_frame>>state){if(case_name!=last_case){check(animation.begin(h,{0,1},h.npc(0).profile),"native animation case begins");last_case=case_name;}animation.request(talk!=0,{dx,dy});check(animation.idle_frame(1.0/60),"native animation step");if(animation.frame()!=expected_frame||animation.talking()!=(state=="Talk")){std::cerr<<case_name<<" tick "<<tick<<" expected frame "<<expected_frame<<" got "<<animation.frame()<<" expected "<<state<<" got "<<animation.talking()<<'\n';}check(animation.frame()==expected_frame&&animation.talking()==(state=="Talk"),"actual native AnimationTree frame/state");++samples_checked;}
 check(samples_checked==144,"complete native animation reference read");
 std::ifstream profile_samples(argv[4]);check(bool(profile_samples),"native profile animation reference available");last_case.clear();samples_checked=0;
 while(profile_samples>>case_name>>tick>>talk>>dx>>dy>>expected_frame>>state){
  check(case_name=="carol"||case_name=="mimmie"||case_name=="doll","known native actor reference");
  const uint32_t index=case_name=="carol"?0:case_name=="mimmie"?1:2;
  if(case_name!=last_case){check(animation.begin(h,{0,1},h.npc(index).profile),"native actor profile begins");last_case=case_name;}
  animation.request(talk!=0,{dx,dy});check(animation.idle_frame(1.0/60),"native profile animation step");
  if(animation.frame()!=expected_frame||animation.talking()!=(state=="Talk"))std::cerr<<case_name<<" tick "<<tick<<" expected frame "<<expected_frame<<" got "<<animation.frame()<<" expected "<<state<<" got "<<animation.talking()<<'\n';
  check(animation.frame()==expected_frame&&animation.talking()==(state=="Talk"),"actual native per-profile AnimationTree frame/state");++samples_checked;
 }
 check(samples_checked==720,"complete native actor reference read");
 // Per-actor source profiles preserve sheet geometry and initial state.
 check(h.count(HouseSection::Npcs)==4,"Original Carol, Mimmie and Doll prefix plus appended Minnie are present");
 auto mimmie=p.npc_pose(1),doll=p.npc_pose(2);
 near(mimmie.position.x,120,0,"source Mimmie position x");near(mimmie.position.y,88,0,"source Mimmie position y");near(mimmie.sprite_offset.y,-3,0,"Mimmie own frame auto-offset");
 check(mimmie.primary_resource!=pose.primary_resource&&mimmie.frame==1,"Mimmie own texture and Down Idle");
 near(doll.position.x,40,0,"source Doll position x");near(doll.position.y,40,0,"source Doll position y");near(doll.sprite_offset.y,-2,0,"Doll own frame auto-offset");check(doll.frame==0&&doll.shadow_resource==house_no_index,"Doll Idle first frame and no shadow");
 const auto minnie=p.npc_pose(3);near(minnie.position.x,472,0,"source Minnie initial position x");near(minnie.position.y,88,0,"source Minnie initial position y");check(minnie.primary_resource!=mimmie.primary_resource&&minnie.frame==1,"Appended Minnie uses her own texture and Down Idle");
 HouseNpcAnimation mimmie_animation,doll_animation,invalid_animation;
 check(!invalid_animation.begin(h,{0,1},h.count(HouseSection::Profiles)),"invalid profile rejected");
 check(mimmie_animation.begin(h,{0,1},h.npc(1).profile),"Mimmie profile begins");
 check(doll_animation.begin(h,{0,1},h.npc(2).profile),"Doll profile begins");
 for(unsigned tick=0;tick<240;++tick){doll_animation.request(tick%2,{float(tick%3)-1,float(tick%5)-2});check(doll_animation.idle_frame(1.0/60),"Doll idle source step");check(doll_animation.frame()==0&&!doll_animation.talking(),"Doll remains Idle until unimplemented script boundary");}
 mimmie_animation.request(false,{-1,0});mimmie_animation.idle_frame(1.0/60);check(mimmie_animation.frame()==6,"Mimmie profile uses original Left Idle");
 // Render the source Below-layer leaf at its child position plus offset.
 auto door=p.openable_door_pose(1,true);check(door.visible,"closed source leaf visible");near(door.position.x,64,0,"blocked door child world x");near(door.position.y,336,0,"blocked door child world y");near(door.offset.y,8,0,"source leaf offset");
 check(!p.openable_door_pose(1,false).visible,"Action hides original leaf");check(!p.openable_door_pose(h.count(HouseSection::OpenableDoors),true).visible,"invalid leaf safely invisible");
 // Door text is an independent script span without any NPC or name tag.
 SourceRandom silent_rng(456);HousePresentation scripted;check(scripted.begin(h,font.view(),silent_rng),scripted.error());
 check(!scripted.begin_dialogue(4,0,"Ninten"),"empty script span rejected");check(!scripted.begin_dialogue(UINT32_MAX,1,"Ninten"),"bad script span rejected");check(!scripted.begin_dialogue(4,UINT32_MAX,"Ninten"),"overflowing script span rejected");check(!scripted.begin_dialogue(0,5,"Ninten"),"mixed source phrase span rejected");
 check(scripted.begin_dialogue(4,1,"Ninten"),scripted.error());auto unnamed=scripted.dialogue_pose();check(unnamed.visible&&!unnamed.name_visible&&unnamed.speaker.empty(),"doorblocked has no speaker or name tag");
 scripted.idle_frame(.31);scripted.take_audio_events();const auto silent_before=silent_rng.raw_draw_count();scripted.input(false,true);
 for(unsigned tick=0;tick<30&&!scripted.dialogue_finished();++tick){check(scripted.physics_frame(1.0/60,{64,385}),scripted.error());check(scripted.idle_frame(1.0/60),scripted.error());}
 check(scripted.dialogue_finished()&&!scripted.dialogue_stopped(),"doorblocked source phrase completes without invented wait");
 check(silent_rng.raw_draw_count()==silent_before&&scripted.take_audio_events().empty(),"unnamed no-voice phrase consumes no voice RNG");check(scripted.npc_pose(0).frame==1&&scripted.npc_pose(1).frame==1&&scripted.npc_pose(2).frame==0,"no NPC is impersonated as door speaker");
 unnamed=scripted.dialogue_pose();std::string door_text;for(const auto&line:unnamed.lines){if(!door_text.empty())door_text+=' ';door_text+=line.text;}check(door_text=="The door won't open!","original blocked-door text retained");
 scripted.input(true,false);scripted.idle_frame(.31);check(scripted.dialogue_done(),"door text close returns control");check(scripted.begin_dialogue(0,"Ninten",{192,725}),"NPC wrapper remains usable after script span");check(scripted.dialogue_pose().name_visible&&scripted.dialogue_pose().speaker=="Carol","named dialogue restores source name tag");
 std::cout<<"House presentation: "<<checks<<" checks passed\n";return 0;
}
