#include "dropped_reference_support.hpp"
int main(int argc,char**argv){
 CHECK(argc==2);const std::string directory=argv[1];std::string error;Reader input;
 CHECK(encore::read_file((directory+"/native.bin").c_str(),input.bytes,16*1024*1024,error));
 CHECK(input.bytes.size()>=12&&!std::memcmp(input.bytes.data(),"ENCDRPR1",8));input.at=8;const auto cases=input.word();CHECK(cases==19);
 for(uint32_t index=0;index<cases;++index){
  current=input.text();const auto resource=input.text();CHECK(resource.find('/')==std::string::npos&&resource.find('\\')==std::string::npos&&resource.find("..") == std::string::npos);
  const auto owner_id=input.word(),seed=input.word(),rng_tail=input.word(),mode=input.word();CHECK(mode==1||mode==2);
  const int action=input.integer(),repeat=input.integer(),pause=input.integer(),unpause=input.integer(),move=input.integer();const bool flagged=input.boolean();
  auto n=input.word();CHECK(n<=32);std::vector<uint32_t>fill;while(n--)fill.push_back(input.word());const auto filler=input.text();
  const BreakablePoint player{real(input.word()),real(input.word())},moved{real(input.word()),real(input.word())};
  n=input.word();CHECK(n>0&&n<=10000);std::vector<Row>rows;while(n--)rows.push_back(row(input));
  auto data=std::make_shared<DroppedItemsData>();CHECK(data->load_file((directory+"/"+resource).c_str(),error));const auto*owner=data->owner(owner_id);CHECK(owner);
  SessionSnapshot session;for(size_t i=0;i<fill.size();++i){SessionCharacter c;c.character_id="source-holder-"+std::to_string(i);for(uint32_t j=0;j<fill[i];++j)c.inventory.push_back({filler,false,1,uint32_t(1000+100*i+j)});session.party.push_back(c.character_id);session.characters.push_back(std::move(c));}
  if(flagged)session.object_flags.push_back({owner->flag,true});
  SourceRandom random(seed);std::vector<uint32_t>ledger;BreakableServices world;world.epoch=1;world.scene=data->info().scene;world.parents.insert(owner->parent);if(flagged)world.object_flags[owner->flag]=true;
  BreakableContentState root;root.present=true;root.visible=true;root.has_interact=true;root.parent=owner->parent;root.position=owner->position;root.shapes={{102,true}};world.contents[owner->path]=root;
  DroppedServices services;services.data=data.get();services.epoch=1;services.world=&world;services.session=&session;services.random=&random;services.uid_ledger=&ledger;services.player=player;services.owners[17]=owner->id;services.identity_parents.insert(owner->parent);for(const auto&t:data->templates())services.item_templates.insert(t.id);services.dialogues={data->info().success_dialogue,data->info().full_dialogue};services.prompts[17]={31,true,false,true};services.shapes[17]={101,102};
  services.clock=[](LoadRngClockSample&s,std::string&e){s={100,200};e.clear();return true;};
  DroppedItemActor actor;CHECK(actor.prepare(data,owner_id,1,17,services,error));std::vector<std::string>dialogues;
  const auto drain=[&]{for(const auto&request:services.requests){CHECK(request.epoch==1&&request.instance==17&&request.owner==owner_id);if(request.kind==DroppedRequestKind::Dialogue)dialogues.push_back(request.target);}services.requests.clear();};
  for(const auto&expected:rows){
   frame=expected.frame;services.frame=uint64_t(std::max(frame,0));services.idle_step=expected.idle;services.prompts[17].visible=expected.prompt_visible;
   if(frame==1&&actor.waiting_ready())CHECK(actor.ready_signal(1,services,error));
   if(!actor.pose().removed){
    if(frame==action){if(mode==1)CHECK(actor.interact(1,services,error));else CHECK(actor.disappear(1,services,error));}
    if(frame==repeat)CHECK(actor.interact(1,services,error));
    if(frame==pause)CHECK(actor.player_pause(1,true,services,error));
    if(frame==unpause)CHECK(actor.player_pause(1,false,services,error));
   }
   if(frame==move)services.player=moved;
   drain();CHECK(expected.alive==!actor.pose().removed);
   CHECK(ledger.size()==expected.ledger&&dialogues==expected.dialogues&&session.key_items.size()==expected.keys&&session.party.size()==expected.party.size());
   for(size_t i=0;i<expected.party.size();++i)CHECK(session.characters[i].inventory.size()==expected.party[i]);
   bool present=false,flag=false;for(const auto&f:session.object_flags)if(f.id==owner->flag){CHECK(!present);present=true;flag=f.value;}
   CHECK(present==expected.flag_present&&flag==expected.flag);
   CHECK(services.has_global_item==expected.global);
   if(expected.global){const auto&item=services.global_item.item;CHECK(item.item_id==expected.item&&item.doses==expected.doses&&item.equipped==expected.equipped);const auto uid=std::find(ledger.begin(),ledger.end(),item.uid);CHECK(uid!=ledger.end()&&size_t(uid-ledger.begin())==expected.uid);}
   if(expected.alive){
    const auto&pose=actor.pose();const auto&slot=world.contents.at(owner->path);
    same(slot.position.x,expected.transform[0],"position.x");same(slot.position.y,expected.transform[1],"position.y");same(pose.scale.x,expected.transform[2],"scale.x");same(pose.scale.y,expected.transform[3],"scale.y");
    CHECK(slot.visible==expected.visible&&pose.sprite_visible==expected.sprite_visible&&pose.sparkle==expected.sparkle&&pose.timer_paused==expected.timer_paused&&pose.queued==expected.queued);
    same(pose.rotation_degrees,expected.rotation,"rotation");same(float(pose.timer_left),expected.timer,"timer");same(pose.animation_speed,expected.speed,"animation-speed");same(pose.animation_time,expected.time,"animation-time");
    CHECK((!pose.timer_running)==expected.timer_stopped);
    CHECK(pose.animation==(expected.animation?data->clip(DroppedClipRole(expected.animation))->id:0));
   }
   if(frame==-1)CHECK(random.randi()==rng_tail);
   if(frame>=0&&!actor.pose().removed){
    if(real(expected.delta)>0)CHECK(actor.advance_idle(1,double(real(expected.delta)),services,error));
    drain();CHECK(actor.flush_deferred(1,services,error));
   }
  }
  CHECK(actor.leave(1,services,error));
 }
 CHECK(input.at==input.bytes.size());
 std::cout<<checks<<" original DroppedItem state checks passed\n";
}
