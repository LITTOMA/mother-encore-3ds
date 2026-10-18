#include "room_fixture.hpp"
#include "encore/field_scene.hpp"
#include "encore/field_scene_resources.hpp"
#include "encore/resource_catalog.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"World run audio %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
struct Unready:WalkAudioSink {bool walk_audio_ready()const override{return false;}bool apply_walk_calls(const WalkAudioCalls&,std::string&)override{return false;}};
struct Reject:WalkAudioSink {bool walk_audio_ready()const override{return true;}bool apply_walk_calls(const WalkAudioCalls&,std::string&)override{return false;}};
}
int main(int argc,char**argv){
 CHECK(argc==4);const std::string root=argv[1];FieldStepsData data;CHECK(data.load_file(argv[2],error));auto view=data.view();
 struct Expected {int tap=0,running=0,playing=0,paused=0;std::string events;};std::vector<Expected>reference;std::ifstream reference_file(argv[3]);CHECK(bool(reference_file));std::string line;CHECK(bool(std::getline(reference_file,line)));
 while(std::getline(reference_file,line)){std::istringstream row(line);std::string action;int argument;Expected e;CHECK(bool(row>>action>>argument>>e.tap>>e.running>>e.playing>>e.paused>>e.events));reference.push_back(e);}CHECK(reference.size()==16);
 auto compare=[&](unsigned index,OpeningWorld&w,FieldRunLoop&loop,bool capture=true){const auto&e=reference.at(index);CHECK(w.player().tap_run==bool(e.tap)&&w.player().running==bool(e.running)&&loop.running()==bool(e.running)&&loop.playing()==bool(e.playing)&&w.house_paused()==bool(e.paused));if(capture){std::ostringstream events;for(const auto&event:loop.events()){if(events.tellp()>0)events<<',';events<<unsigned(event.kind);}CHECK((loop.events().empty()?"-":events.str())==e.events);}loop.clear_events();};
 auto initialize=[&](OpeningWorld&w,FieldRunLoop&loop){CHECK(w.initialize(encore_test::room()));CHECK(loop.initialize(view,view.text(view.sound(view.policy().default_sound).name),error));CHECK(w.bind_walk_audio(loop));CHECK(!w.bind_walk_audio(loop));};
 {OpeningWorld w;FieldRunLoop loop;Unready unready;CHECK(!w.bind_walk_audio(unready));CHECK(w.initialize(encore_test::room()));CHECK(!w.bind_walk_audio(unready));}
 {OpeningWorld w;FieldRunLoop loop;initialize(w,loop);CHECK(w.advance({1,0}));CHECK(!loop.playing());CHECK(w.advance({1,0,true}));CHECK(w.player().running&&loop.running()&&loop.playing());compare(0,w,loop);CHECK(w.pause_for_house());compare(1,w,loop);CHECK(!w.player().running&&!loop.running()&&!loop.playing());const auto count=loop.events().size();CHECK(w.advance({1,0,true}));CHECK(!loop.playing()&&loop.events().size()==count);CHECK(w.unpause_from_house());compare(2,w,loop);CHECK(!loop.playing());}
 {OpeningWorld w;FieldRunLoop loop;initialize(w,loop);CHECK(w.advance({0,0,true}));CHECK(w.player().crouch);CHECK(w.advance({}));CHECK(w.player().tap_run&&!w.player().running&&!loop.playing());CHECK(w.advance({}));CHECK(w.player().running&&loop.running()&&loop.playing());compare(4,w,loop,false);CHECK(w.pause_for_house());compare(5,w,loop);CHECK(w.player().tap_run&&w.player().running&&loop.running()&&loop.playing());CHECK(w.advance({}));compare(6,w,loop);CHECK(w.player().running&&loop.running()&&!loop.playing());CHECK(w.unpause_from_house());compare(7,w,loop);CHECK(loop.playing());CHECK(w.pause_for_house(true));compare(8,w,loop);CHECK(!w.player().tap_run&&!w.player().running&&!loop.running()&&!loop.playing());CHECK(w.unpause_from_house());compare(9,w,loop);CHECK(!loop.playing());CHECK(w.initialize(encore_test::room()));const auto count=loop.events().size();CHECK(w.advance({1,0,true}));CHECK(loop.events().size()==count);}
 {OpeningWorld w;Reject reject;CHECK(w.initialize(encore_test::room()));CHECK(w.bind_walk_audio(reject));const auto old=w.player();CHECK(!w.advance({1,0}));CHECK(!w.healthy()&&w.player().position.x==old.position.x&&w.player().position.y==old.position.y);}
 ResourceCatalog catalog;WorldLinksData links;RoomData domain;ItemData items;AudioBank bank;
 CHECK(catalog.load_file((root+"/system/native.encresources").c_str(),error));CHECK(links.load_file((root+"/world/world.enclinks").c_str(),error));CHECK(domain.load_file((root+"/world/house/opening.encroom").c_str(),error));CHECK(items.load_file((root+"/ui/opening.encitems").c_str(),error));CHECK(bank.load_file((root+"/sound/banks/opening.encaudio").c_str(),error));
 FieldSceneResources resources;CHECK(resources.load(catalog,links,2,root.c_str(),domain.view(),items.view(),error));
 const auto step=resources.steps();CHECK(step.valid()&&step.scene_id()==2);std::vector<bool>flags(domain.view().flag_count(),false);FieldScene field;
 CHECK(!field.bind_steps(step,bank,"wood",error));CHECK(field.prepare(resources.room(),resources.map(),links,flags,{80,80},{0,1},{400,240},error));
 CHECK(!field.bind_steps(view,bank,"wood",error));CHECK(!field.bind_steps(step,AudioBank{},"wood",error));CHECK(!field.bind_steps(step,bank,"unmapped",error));CHECK(!field.run_audio().prepared());
 CHECK(field.bind_steps(step,bank,"stone",error));CHECK(!field.bind_steps(step,bank,"wood",error));CHECK(field.run_audio().content().bytes_identity()==step.bytes_identity());CHECK(field.finish_transition());
 const auto wood=step.find_sound("wood"),stone=step.find_sound("stone");CHECK(field.run_audio().wanted_sound()==stone);WalkInput input;CHECK(field.before_physics(input));CHECK(field.run_audio().wanted_sound()==stone);
 // Only this explicit schema fixture binds Mayor rectangles to Podunk. The
 // following teleports test callback ownership/order, not original traversal.
 auto visit=[&](Vec2 p){CHECK(field.world.warp_same_scene(p,{0,1}));WalkInput next;CHECK(field.before_physics(next));};
 visit({8,12});CHECK(field.run_audio().wanted_sound()==wood);CHECK(field.run_audio().events().empty());visit({8,12});CHECK(field.run_audio().wanted_sound()==wood);
 visit({80,80});CHECK(field.run_audio().wanted_sound()==wood);visit({8,-581});CHECK(field.run_audio().wanted_sound()==stone);CHECK(field.run_audio().events().empty());visit({80,80});CHECK(field.run_audio().wanted_sound()==wood);
 // Contact selection keeps the current registered stream until a MOVE run
 // call, including while paused; reset removes the old borrowed sink.
 CHECK(field.prepare(resources.room(),resources.map(),links,flags,{80,80},{0,1},{400,240},error));CHECK(!field.run_audio().prepared());CHECK(field.finish_transition());CHECK(field.world.advance({1,0}));
 std::printf("World run audio: %u checks; live MOVE sink, held/tap/forced pause and resume, rejected binding, metadata-only area callback fixture; no DSP or original-scene pass\n",checks);
}