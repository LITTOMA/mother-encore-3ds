#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check_result(bool ok,const char* why,unsigned line){++checks;if(!ok){std::cerr<<"House NPC restore check "<<checks<<" at line "<<line<<": "<<why<<'\n';std::exit(1);}}
#define check(ok,why) do{const bool passed=(ok);check_result(passed,(why),__LINE__);}while(false)
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
uint32_t npc(HouseView h,std::string_view path){for(uint32_t i=0;i<h.count(HouseSection::Npcs);++i)if(h.string(h.npc(i).source_path)==path)return i;check(false,"Required source NPC missing");return house_no_index;}
}
int main(int argc,char**argv){
 check(argc==4,"Room, house and battle-font packs required");std::string error;RoomData rd;HouseData hd;BattleData bd;
 check(rd.load_file(argv[1],error),error.c_str());check(hd.load_file(argv[2],error),error.c_str());check(bd.load_file(argv[3],error),error.c_str());
 const auto room=rd.view();const auto house=hd.view();const auto font=bd.view();const double dt=double(float(1.0/60));
 const auto carol=npc(house,"Objects/npc"),mimmie=npc(house,"Objects/npc2"),doll=npc(house,"Objects/npcdoll"),minnie=npc(house,"Objects/npc3");
 const auto mi=house.npc(mimmie).room_actor_index,di=house.npc(doll).room_actor_index,ni=house.npc(minnie).room_actor_index;
 SourceRandom random(123);HousePresentation probe;check(probe.begin(house,font,random),probe.error());
 check(!probe.restore_npc_pose(house.count(HouseSection::Npcs),{0,0},{0,1}),"Unknown restoration rejects");
 check(!probe.restore_npc_pose(carol,{std::numeric_limits<float>::infinity(),0},{0,1}),"Nonfinite restoration rejects");
 check(!probe.set_npc_visible(house.count(HouseSection::Npcs),true),"Unknown visibility rejects");
 const auto initial=probe.npc_pose(carol);const auto revision=probe.npc_frame_revision(carol);
 check(probe.restore_npc_pose(carol,{300,700},{0,-1})&&probe.idle_frame(dt),probe.error());
 check(probe.npc_frame_revision(carol)==revision+1,"Actual frame change increments signal revision");
 check(probe.restore_npc_pose(carol,{300,700},{0,1})&&probe.idle_frame(dt),probe.error());
 check(probe.npc_pose(carol).frame==initial.frame&&probe.npc_frame_revision(carol)==revision+2,"A return to the original frame preserves intervening signal evidence");
 check(probe.set_npc_replaced(carol,true)&&!probe.npc_pose(carol).visible,"Actor replacement hides original sprite");
 check(probe.set_npc_visible(carol,false)&&probe.set_npc_replaced(carol,false)&&!probe.npc_pose(carol).visible,"Original visibility remains independent of replacement");
 check(probe.set_npc_visible(carol,true)&&probe.npc_pose(carol).visible,"Original can become visible again");
 const auto doll_revision=probe.npc_frame_revision(doll);const auto doll_frame=probe.npc_pose(doll).frame;
 check(probe.restore_npc_pose(doll,{40,40},{0,1}),probe.error());
 for(unsigned i=0;i<120;++i)check(probe.idle_frame(dt),probe.error());
 check(probe.npc_pose(doll).frame==doll_frame&&probe.npc_frame_revision(doll)>doll_revision,"Looped source Doll Idle emits frame_changed even for the same numeric frame");
 // Original Godot3.6.2 discrete-key signal frames, preserved in
 // reports/doll-postwin-cleanup-reference-2/reference.json. Equal-frame
 // boundary writes are observable and must not be replaced with a timeout.
 for(unsigned variant=0;variant<3;++variant){
  HouseNpcAnimation animation;check(animation.begin(house,{0,1},house.npc(variant==0?doll:minnie).profile),"Native signal case begins");
  for(unsigned frame=0;frame<120;++frame){const auto before=animation.frame_revision();check(animation.idle_frame(dt),"Native signal animation step");
   const bool expected=variant==0?(frame%15==0||frame%15==14):variant==1?(frame%12==0||frame%12==11):(frame==0||(frame>=2&&(frame-2)%12<=1));
   check((animation.frame_revision()!=before)==expected,"Discrete key-write signal matches original engine frame");
   if(frame==1&&variant==2)animation.request(false,{-1,0});
  }
 }

 OpeningWorld world;HousePresentation hp;HouseRuntime hr;
 check(world.initialize(room),world.error());world.attach_random(random);check(hp.begin(house,font,random),hp.error());check(hr.initialize(house,world,hp),hr.error());
 auto step=[&](WalkInput input=WalkInput{},bool accept=false,bool cancel=false){
  check(hr.before_physics(input),hr.error());
  check(hp.physics_frame(dt,world.player().position),hp.error());
  check(world.advance(input),world.error());
  check(hr.after_physics(),hr.error());
  check(world.idle_frame(dt),world.error());
  check(hp.idle_frame(dt),hp.error());
  const bool ok=hr.idle_frame(dt,accept,cancel);
  if(!ok)std::cerr<<"stage="<<unsigned(world.stage())<<" phrase="<<world.phrase()<<" dialogue="<<world.pending_dialogue_id()<<" presentation="<<hp.error()<<'\n';
  check(ok,hr.error());
 };
 auto advance_text=[&]{const bool accept=hp.dialogue_active()&&!hp.dialogue_closing()&&(hp.dialogue_finished()||hp.dialogue_stopped());step({},accept,!accept);};
 // Enter the real source House trigger so its pending state is exercised.
 check(world.warp_same_scene({128,169},{0,-1}),world.error());
 for(unsigned i=0;i<2400&&world.stage()!=OpeningStage::BattleRequested;++i)advance_text();
 check(world.stage()==OpeningStage::BattleRequested&&hr.story_pending(),"Source attack reaches battle while retaining House story ownership");
 check(world.accept_battle_entry(),world.error());
 for(unsigned i=0;i<10;++i)step();
 check(!world.actor_bound(mi)&&!world.actor_restore_requested(mi)&&hp.npc_pose(mimmie).visible,"Attack Mimmie changed Down to Up and completed source cleanup during battle");
 check(world.body_enabled(house.npc(mimmie).body_id),"Restored attack Mimmie collision enabled");
 check(world.actor_bound(di)&&!world.actor_restore_requested(di)&&!hp.npc_pose(doll).visible&&!world.body_enabled(house.npc(doll).body_id),"Attack releases retained Doll without restoring its hidden original");
 // This focused lifecycle test supplies the battle-return landing boundary;
 // the separate battle integration test validates victory and rewards.
 const auto controlled=room.scene().player_instance_index;
 check(world.land_battle_player(world.player().direction,world.actor(controlled).frame),world.error());
 check(world.begin_battle_continuation("Podunk/cutscenes/doll_defeated"),world.error());
 for(unsigned i=0;i<2400&&world.stage()!=OpeningStage::Walking;++i)advance_text();
 check(world.stage()==OpeningStage::Walking&&world.story_completed(),"Eight source post-win phrases finish");
 check(!hr.story_pending()&&!hr.story_executing()&&!hr.blocks_player()&&hr.phase()==HousePhase::Idle,"Post-win completion releases House story ownership and final input");
 check(same(hp.npc_pose(mimmie).position,world.actor(mi).position)&&same(hp.npc_pose(minnie).position,world.actor(ni).position)&&same(hp.npc_pose(doll).position,world.actor(di).position),"Original positions copy on final input before the next idle");
 check(world.actor_bound(mi)&&!hp.npc_pose(mimmie).visible&&!world.body_enabled(house.npc(mimmie).body_id),"Changed-frame restoration has not skipped its idle wait");
 step();check(world.actor_bound(mi)&&!world.body_enabled(house.npc(mimmie).body_id),"First idle observes frame_changed and preserves hidden original");
 step();check(!world.actor_bound(mi)&&!world.actor_restore_requested(mi)&&hp.npc_pose(mimmie).visible&&world.body_enabled(house.npc(mimmie).body_id),"Following idle frees proxy and restores original collision");
 const auto restored=hp.npc_pose(mimmie).position;
 check(same(restored,{112,88}),"Source Mimmie relative movement restores at 112,88");
 for(unsigned i=0;i<120;++i)step();
 check(!world.actor_bound(di)&&!world.actor_restore_requested(di)&&world.body_enabled(house.npc(doll).body_id),"Doll constant-Idle restore completes after source loop key writes and idle");
 check(!world.actor_bound(ni)&&!world.actor_restore_requested(ni)&&world.body_enabled(house.npc(minnie).body_id),"Minnie unchanged Down Idle also completes on original loop signal");
 check(same(hp.npc_pose(minnie).position,{472,88}),"Minnie receives script position without invented event-position teleport");
 check(world.story_flag("doll_defeated")&&world.story_flag("pillow_attack")&&!world.story_flag("poltergeist")&&!world.story_flag("doll_melody"),"Only source post-win flags are applied");
 check(same(house.npc(mimmie).position,{120,88}),"Immutable exported NPC position remains unchanged");
 OpeningWorld unmoved;check(unmoved.initialize(room),unmoved.error());
 check(unmoved.warp_same_scene({150,88},{-1,0})&&world.warp_same_scene({150,88},{-1,0}),"Position collision comparison players");
 for(unsigned i=0;i<40;++i){check(unmoved.advance({-1,0}),unmoved.error());check(world.advance({-1,0}),world.error());}
 check(std::abs((unmoved.player().position.x-world.player().position.x)-8)<.001f,"Restored Mimmie collider moves eight source units and survives disable/re-enable");

 // Move a voiced NPC well away from its exported geometry, then exercise
 // directional interaction against the translated center and player facing.
 OpeningWorld interaction;HousePresentation ip;HouseRuntime ir;
 check(interaction.initialize(room),interaction.error());check(ip.begin(house,font,random),ip.error());check(ir.initialize(house,interaction,ip),ir.error());
 const Vec2 relocated{300,704};check(ip.restore_npc_pose(carol,relocated,{0,1}),ip.error());
 check(interaction.warp_same_scene({300,731},{0,-1}),interaction.error());
 check(ir.idle_frame(dt,true,false),ir.error());
 check(ir.phase()==HousePhase::Dialogue&&ir.active_object()==carol,"Interaction ray follows restored NPC position");
 check(same(interaction.player().direction,{0,-1}),"Player faces restored position rather than old exported location");
 check(same(house.npc(carol).position,initial.position),"Translated interaction leaves content bytes unchanged");
 OpeningWorld looking;HousePresentation lp;HouseRuntime lr;
 check(looking.initialize(room),looking.error());check(lp.begin(house,font,random),lp.error());check(lr.initialize(house,looking,lp),lr.error());
 check(lp.restore_npc_pose(carol,relocated,{0,1}),lp.error());check(looking.warp_same_scene({328,704},{-1,0}),looking.error());check(lr.after_physics(),lr.error());
 WalkInput still;check(lr.before_physics(still),lr.error());
 check(lp.physics_frame(dt,looking.player().position)&&lp.idle_frame(dt)&&lp.physics_frame(dt,looking.player().position)&&lp.idle_frame(dt),lp.error());
 const auto right_idle=house.clip(house.clip_for(HouseClipRole::NpcIdleRight,house.npc(carol).profile));
 check(lp.npc_pose(carol).frame==house.key(right_idle.first_key).frame,"View area follows restored NPC position and faces nearby player");
 std::cout<<"House NPC restoration: "<<checks<<" checks, source signal waits, collision lifecycle and translated interaction\n";
}
