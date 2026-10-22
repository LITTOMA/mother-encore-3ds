#include "encore/dropped_item_actor.hpp"
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>

using namespace encore::upstream;
namespace {
size_t checks=0;
void check(bool ok,int line){++checks;if(!ok){std::cerr<<"Dropped lifecycle failure at "<<line<<"\n";std::exit(1);}}
#define CHECK(value) check(bool(value),__LINE__)
DroppedRequest pending(DroppedRequestKind kind,uint64_t instance=0){DroppedRequest r;r.kind=kind;r.epoch=1;r.instance=instance;return r;}
struct World {
 std::shared_ptr<DroppedItemsData>data=std::make_shared<DroppedItemsData>();
 SourceRandom random{1};std::vector<uint32_t>ledger;SessionSnapshot session;
 BreakableServices world;DroppedServices services;DroppedItemActor actor;std::string error;
 explicit World(const char*path){
  CHECK(data->load_file(path,error));const auto&owner=data->owners()[0];
  SessionCharacter member;member.character_id="holder";session.characters.push_back(member);session.party={member.character_id};
  world.epoch=1;world.scene=data->info().scene;world.parents.insert(owner.parent);
  BreakableContentState root;root.present=true;root.visible=true;root.has_interact=true;root.parent=owner.parent;root.position=owner.position;root.shapes={{102,true}};world.contents[owner.path]=root;
  services.data=data.get();services.epoch=1;services.world=&world;services.session=&session;services.random=&random;services.uid_ledger=&ledger;services.player={13,27};services.owners[17]=owner.id;services.identity_parents.insert(owner.parent);services.item_templates.insert(owner.template_id);services.dialogues={data->info().success_dialogue,data->info().full_dialogue};services.prompts[17]={31,true,false,true};services.shapes[17]={101,102};
  services.clock=[](LoadRngClockSample&s,std::string&e){s={100,200};e.clear();return true;};
 }
 bool begin(){return actor.prepare(data,data->owners()[0].id,1,17,services,error);}
 void rejected(const std::function<bool()>&action){
  const auto state=random.state(),draws=random.raw_draw_count();const auto old_ledger=ledger;
  const auto count=session.characters[0].inventory.size(),flags=session.object_flags.size(),requests=services.requests.size();
  const auto before=actor.pose();const auto position=world.contents.begin()->second.position;
  CHECK(!action());CHECK(!error.empty());
  CHECK(random.state()==state&&random.raw_draw_count()==draws&&ledger==old_ledger);
  CHECK(session.characters[0].inventory.size()==count&&session.object_flags.size()==flags&&services.requests.size()==requests);
  const auto&after=actor.pose();CHECK(after.scale.x==before.scale.x&&after.scale.y==before.scale.y&&after.rotation_degrees==before.rotation_degrees&&after.sparkle==before.sparkle&&after.timer_left==before.timer_left&&after.queued==before.queued&&after.removed==before.removed);
  CHECK(world.contents.begin()->second.position.x==position.x&&world.contents.begin()->second.position.y==position.y);
 }
};
}
int main(int argc,char**argv){
 CHECK(argc==2);
 // Prepared ownership must remain exact, including the separately held body
 // collider. Breakable's shape set contains only the Area collider.
 for(unsigned invalid=0;invalid<15;++invalid){
  World w(argv[1]);CHECK(w.begin());
  switch(invalid){
   case 0:w.services.epoch=2;break;
   case 1:w.world.epoch=2;break;
   case 2:w.services.owners[17]=0;break;
   case 3:w.services.item_templates.clear();break;
   case 4:w.services.dialogues.clear();break;
   case 5:w.services.identity_parents.clear();break;
   case 6:w.world.parents.clear();break;
   case 7:w.services.shapes[17]={102,102};break;
   case 8:w.world.contents.begin()->second.shapes[101]=true;break;
   case 9:w.world.contents.begin()->second.shapes.clear();break;
   case 10:w.world.contents.begin()->second.has_interact=false;break;
   case 11:w.world.contents.begin()->second.position.x=std::numeric_limits<float>::quiet_NaN();break;
   case 12:w.services.requests.push_back(pending(DroppedRequestKind(99)));break;
   case 13:w.session.object_flags.push_back({w.data->owners()[0].flag,true});break;
   case 14:w.services.prompts[17].token=32;break;
  }
  // The malformed transform itself is external input, not an actor mutation.
  if(invalid==11){const auto before=w.random.state();CHECK(!w.actor.interact(1,w.services,w.error));CHECK(w.random.state()==before&&w.ledger.empty()&&w.session.characters[0].inventory.empty());}
  else w.rejected([&]{return w.actor.interact(1,w.services,w.error);});
 }
 for(double delta:{0.0,-1.0,1.00001,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  World w(argv[1]);CHECK(w.begin());w.rejected([&]{return w.actor.advance_idle(1,delta,w.services,w.error);});
 }
 {
  World w(argv[1]);CHECK(w.begin());CHECK(w.actor.advance_idle(1,1.0/60,w.services,w.error));
  w.rejected([&]{return w.actor.advance_idle(1,1.0/60,w.services,w.error);});
  CHECK(w.actor.ready_signal(1,w.services,w.error));w.rejected([&]{return w.actor.ready_signal(1,w.services,w.error);});
  CHECK(w.actor.disappear(1,w.services,w.error));w.rejected([&]{return w.actor.disappear(1,w.services,w.error);});
 }
 // A clock callback can invalidate the prompt's map iterator. The actor must
 // reject after returning from the provider without an inventory/UID commit.
 for(unsigned invalid=0;invalid<4;++invalid){
  World w(argv[1]);CHECK(w.begin());
  w.services.clock=[&](LoadRngClockSample&s,std::string&e){
   s={100,200};e.clear();
   if(invalid==0)w.services.prompts.erase(17);
   if(invalid==1)w.services.prompts[17].token=32;
   if(invalid==2)w.services.prompts[17].visible=true;
   if(invalid==3)w.services.prompts[17].can_show=false;
   return true;
  };
  w.rejected([&]{return w.actor.interact(1,w.services,w.error);});
 }
 {
  World w(argv[1]);CHECK(w.begin());const auto state=w.random.state();
  w.services.clock=[&](LoadRngClockSample&s,std::string&e){s={100,200};e.clear();w.services.requests.assign(4093,pending(DroppedRequestKind::ForcePrompt,999));return true;};
  CHECK(!w.actor.interact(1,w.services,w.error));CHECK(w.random.state()==state&&w.ledger.empty()&&w.session.characters[0].inventory.empty()&&w.session.object_flags.empty());
  CHECK(w.services.requests.size()==4093&&!w.actor.pose().queued);
 }
 for(unsigned invalid=0;invalid<3;++invalid){
  World w(argv[1]);CHECK(w.begin());const auto state=w.random.state(),draws=w.random.raw_draw_count();
  w.services.clock=[&](LoadRngClockSample&s,std::string&e){s={100,200};e.clear();if(invalid==0)std::vector<SessionCharacter>().swap(w.session.characters);if(invalid==1)w.random.randi();if(invalid==2)w.ledger.push_back(123);return true;};
  CHECK(!w.actor.interact(1,w.services,w.error));CHECK(!w.error.empty()&&w.session.object_flags.empty()&&w.services.requests.empty()&&!w.services.has_global_item);
  CHECK(w.random.raw_draw_count()==draws+(invalid==1?1:0)&&w.ledger.size()==(invalid==2?1u:0u));if(invalid!=1)CHECK(w.random.state()==state);
  if(invalid==0)CHECK(w.session.characters.empty());else CHECK(w.session.characters[0].inventory.empty());
 }
 {
  World w(argv[1]);CHECK(w.begin());CHECK(w.data->load_file(argv[1],w.error));
  w.rejected([&]{return w.actor.interact(1,w.services,w.error);});
 }
 {
  World w(argv[1]);w.services.clock=[](LoadRngClockSample&,std::string&e){e="clock unavailable";return false;};CHECK(w.begin());
  w.rejected([&]{return w.actor.interact(1,w.services,w.error);});CHECK(w.error=="clock unavailable");
 }
 {
  World w(argv[1]);CHECK(w.begin());w.services.requests.push_back(pending(DroppedRequestKind::ForcePrompt,17));
  w.rejected([&]{return w.actor.leave(1,w.services,w.error);});w.services.requests.clear();
  const auto*slot=&w.world.contents.begin()->second;const auto*shapes=slot->shapes.begin().operator->();
  CHECK(w.actor.leave(1,w.services,w.error));CHECK(w.actor.pose().removed&&!slot->present&&slot==&w.world.contents.begin()->second&&shapes==slot->shapes.begin().operator->());
  CHECK(w.actor.leave(1,w.services,w.error)&&w.actor.flush_deferred(1,w.services,w.error));
 }
 std::cout<<checks<<" Dropped lifecycle failure/ownership checks passed\n";
}
