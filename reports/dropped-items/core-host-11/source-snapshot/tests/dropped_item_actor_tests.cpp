#include "encore/dropped_item_actor.hpp"
#include <cstdlib>
#include <iostream>

using namespace encore::upstream;
namespace {
size_t checks=0;
void check(bool ok,int line){++checks;if(!ok){std::cerr<<"Dropped actor failure at "<<line<<"\n";std::exit(1);}}
#define CHECK(value) check(bool(value),__LINE__)
}
int main(int argc,char**argv){
 CHECK(argc==2);std::string error;auto data=std::make_shared<DroppedItemsData>();CHECK(data->load_file(argv[1],error));
 const auto&owner=data->owners()[0];SourceRandom random(1);std::vector<uint32_t>ledger;SessionSnapshot session;
 SessionCharacter member;member.character_id="source-holder";session.characters.push_back(member);session.party={member.character_id};
 BreakableServices world;world.epoch=1;world.scene=data->info().scene;world.parents.insert(owner.parent);
 BreakableContentState root;root.present=true;root.visible=true;root.has_interact=true;root.parent=owner.parent;root.position=owner.position;root.shapes={{102,true}};world.contents[owner.path]=root;
 DroppedServices services;services.data=data.get();services.epoch=1;services.world=&world;services.session=&session;services.random=&random;services.uid_ledger=&ledger;services.player={13,27};services.owners[17]=owner.id;services.identity_parents.insert(owner.parent);services.item_templates.insert(owner.template_id);services.dialogues={data->info().success_dialogue,data->info().full_dialogue};services.prompts[17]={31,true,false,true};services.shapes[17]={101,102};
 services.clock=[](LoadRngClockSample&s,std::string&e){s={100,200};e.clear();return true;};
 DroppedItemActor actor;CHECK(actor.prepare(data,owner.id,1,17,services,error));
 CHECK(actor.valid()&&actor.waiting_ready()&&!world.contents[owner.path].visible&&ledger.empty());
 (void)random.randi(); // Match the original observer's explicit diagnostic draw.
 const auto before=random.state();CHECK(!actor.prepare(data,owner.id,1,17,services,error));CHECK(random.state()==before);
 CHECK(actor.interact(1,services,error));CHECK(session.characters[0].inventory.size()==1&&ledger.size()==1);
 CHECK(world.object_flags[owner.flag]&&session.object_flags.size()==1&&session.object_flags[0].value);
 CHECK(services.has_global_item&&services.global_item.kind==ItemHolderGrantKind::Acquired);
 CHECK(services.requests.size()==3&&services.requests[0].kind==DroppedRequestKind::ForcePrompt&&services.requests[1].kind==DroppedRequestKind::FreePrompt&&services.requests[2].kind==DroppedRequestKind::Dialogue);
 services.requests.clear();CHECK(actor.ready_signal(1,services,error));
 CHECK(world.contents[owner.path].visible);
 for(uint64_t step=0;step<30&&!actor.pose().removed;++step){
  services.idle_step=step;services.frame=step;
  CHECK(actor.advance_idle(1,double(float(1.0/60.0)),services,error));
  if(actor.pose().queued){CHECK(!actor.flush_deferred(1,services,error));services.requests.clear();CHECK(actor.flush_deferred(1,services,error));}
 }
 CHECK(actor.pose().removed&&!world.contents[owner.path].present&&!services.owners.count(17));
 CHECK(actor.pose().scale.y==1e-5f);CHECK(actor.flush_deferred(1,services,error));
 CHECK(!actor.interact(1,services,error));
 std::cout<<checks<<" Dropped actor checks passed\n";
}
