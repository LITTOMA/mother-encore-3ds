#include "dropped_reference_support.hpp"
#include <cmath>
namespace {
struct Cover {
 std::string parent;std::array<uint32_t,2>position{};
 bool visible=false,sprite_visible=false,playing=false,disabled=false,timer_stopped=false;
 uint32_t frame=0,animation=0,time=0,body=0,layer=0,mask=0,timer=0;
 std::vector<std::pair<uint64_t,bool>>audio;
 struct Rumble {uint64_t frame=0;uint32_t device=0;std::array<double,3>value{};};
 std::vector<Rumble>rumble;
};
double wide(Reader&r){uint64_t bits=r.word();bits|=uint64_t(r.word())<<32;double value;std::memcpy(&value,&bits,8);CHECK(std::isfinite(value));return value;}
Cover cover(Reader&r){
 Cover c;c.parent=r.text();for(auto&v:c.position)v=r.word();c.visible=r.boolean();c.sprite_visible=r.boolean();c.frame=r.word();c.animation=r.word();CHECK(c.animation<=3);c.playing=r.boolean();c.time=r.word();c.disabled=r.boolean();c.body=r.word();c.layer=r.word();c.mask=r.word();c.timer=r.word();c.timer_stopped=r.boolean();
 auto n=r.word();CHECK(n<=16);while(n--){const auto frame=r.word();const auto value=r.boolean();c.audio.emplace_back(frame,value);}
 n=r.word();CHECK(n<=1);while(n--){Cover::Rumble v;v.frame=r.word();v.device=r.word();for(auto&value:v.value)value=wide(r);c.rumble.push_back(v);}return c;
}
struct Expected {Row item;std::string parent;bool physics=false,body=false,area=false;Cover cover;};
}
int main(int argc,char**argv){
 CHECK(argc==2);const std::string directory=argv[1];std::string error;Reader input;
 CHECK(encore::read_file((directory+"/native.bin").c_str(),input.bytes,16*1024*1024,error));
 CHECK(input.bytes.size()>=12&&!std::memcmp(input.bytes.data(),"ENCDRPB2",8));input.at=8;const auto cases=input.word();CHECK(cases==11);
 auto data=std::make_shared<DroppedItemsData>();BreakablesData breaks;
 CHECK(data->load_file((directory+"/source.encdrp").c_str(),error)&&breaks.load_file((directory+"/breakables.encbrk").c_str(),error));CHECK(data->compatible(breaks,error));
 for(uint32_t index=0;index<cases;++index){
  current=input.text();const auto owner_id=input.word(),cover_id=input.word(),seed=input.word(),tail=input.word();auto initial_count=input.word();CHECK(initial_count<=32);std::vector<float>initial_deltas;while(initial_count--){const auto delta=real(input.word());CHECK(std::isfinite(delta)&&delta>0&&delta<=1);initial_deltas.push_back(delta);}const auto distance=real(input.word());CHECK(std::isfinite(distance)&&distance>=0);
  const auto action=input.integer(),repeat=input.integer();const auto flagged=input.boolean();auto n=input.word();CHECK(n<=32);std::vector<uint32_t>fill;while(n--)fill.push_back(input.word());const auto filler=input.text();const BreakablePoint player{real(input.word()),real(input.word())};
  n=input.word();CHECK(n>0&&n<=10000);std::vector<Expected>rows;while(n--){Expected v;v.item=row(input);if(v.item.alive){v.parent=input.text();v.physics=input.boolean();v.body=input.boolean();v.area=input.boolean();}v.cover=cover(input);rows.push_back(v);}
  const auto*owner=data->owner(owner_id);const auto*container=breaks.owner(cover_id);CHECK(owner&&container&&container->content<breaks.contents().size()&&breaks.contents()[container->content].path==owner->path);const auto*family=breaks.family(container->family);CHECK(family);
  SessionSnapshot session;for(size_t i=0;i<fill.size();++i){SessionCharacter c;c.character_id="holder-"+std::to_string(i);for(uint32_t j=0;j<fill[i];++j)c.inventory.push_back({filler,false,1,uint32_t(1000+100*i+j)});session.party.push_back(c.character_id);session.characters.push_back(std::move(c));}
  if(flagged)session.object_flags.push_back({owner->flag,true});
  SourceRandom random(seed);std::vector<uint32_t>ledger;BreakableServices world;world.epoch=1;world.scene=data->info().scene;world.sounds.insert(breaks.policy().sound);for(const auto&parent:{owner->parent,container->parent,container->drop_parent,container->new_parent})world.parents.insert(parent);if(flagged)world.object_flags[owner->flag]=true;
  BreakableContentState root;root.present=true;root.visible=true;root.has_interact=true;root.parent=owner->parent;root.position=owner->position;root.shapes={{102,false}};world.contents[owner->path]=root;
  DroppedServices services;services.data=data.get();services.epoch=1;services.world=&world;services.session=&session;services.random=&random;services.uid_ledger=&ledger;services.player=player;services.owners[17]=owner->id;services.identity_parents=world.parents;for(const auto&t:data->templates())services.item_templates.insert(t.id);services.dialogues={data->info().success_dialogue,data->info().full_dialogue};services.prompts[17]={31,true,false,true};services.shapes[17]={101,102};services.clock=[](LoadRngClockSample&s,std::string&e){s={100,200};e.clear();return true;};
  BreakableActor object;CHECK(object.prepare(breaks,cover_id,1,world,error));DroppedItemActor actor;CHECK(actor.prepare(data,owner_id,1,17,services,error));std::vector<std::string>dialogues;
  const auto drain=[&]{for(const auto&request:services.requests){CHECK(request.epoch==1&&request.instance==17&&request.owner==owner_id);if(request.kind==DroppedRequestKind::Dialogue)dialogues.push_back(request.target);}services.requests.clear();};
  drain();
  // The independently observed process callbacks record any internal steps
  // between add_child and the first idle-frame observation. Source instances
  // created inside an idle signal already receive that frame's child processing.
  for(size_t step=0;step<initial_deltas.size()&&!actor.pose().removed;++step){services.idle_step=step;CHECK(actor.advance_idle(1,double(initial_deltas[step]),services,error));drain();CHECK(actor.flush_deferred(1,services,error));}
  CHECK(actor.flush_deferred(1,services,error));if(!actor.pose().removed)CHECK(actor.ready_signal(1,services,error));
  const auto*storage=session.storage.data();const auto*slot=&world.contents.at(owner->path);const auto*shape=&slot->shapes.at(102);
  for(const auto&expected:rows){
   const auto&item=expected.item;frame=item.frame;services.frame=uint64_t(std::max(frame,0));services.idle_step=initial_deltas.size()+item.idle;services.prompts[17].visible=item.prompt_visible;
   if(frame==0)CHECK(object.hit(1,{container->position.x+distance,container->position.y},world,error));
   if((frame==action||frame==repeat)&&!actor.pose().removed)CHECK(actor.interact(1,services,error));
   drain();CHECK(item.alive==!actor.pose().removed);CHECK(ledger.size()==item.ledger&&dialogues==item.dialogues&&session.key_items.size()==item.keys&&session.party.size()==item.party.size());
   for(size_t i=0;i<item.party.size();++i)CHECK(session.characters[i].inventory.size()==item.party[i]);
   bool present=false,flag=false;for(const auto&value:session.object_flags)if(value.id==owner->flag){CHECK(!present);present=true;flag=value.value;}CHECK(present==item.flag_present&&flag==item.flag);CHECK(services.has_global_item==item.global);
   if(item.global){const auto&value=services.global_item.item;CHECK(value.item_id==item.item&&value.doses==item.doses&&value.equipped==item.equipped);const auto uid=std::find(ledger.begin(),ledger.end(),value.uid);CHECK(uid!=ledger.end()&&size_t(uid-ledger.begin())==item.uid);}
   if(item.alive){
    const auto&pose=actor.pose();same(slot->position.x,item.transform[0],"position.x");same(slot->position.y,item.transform[1],"position.y");same(pose.scale.x,item.transform[2],"scale.x");same(pose.scale.y,item.transform[3],"scale.y");
    if(slot->visible!=item.visible||pose.sprite_visible!=item.sprite_visible||pose.sparkle!=item.sparkle||pose.timer_paused!=item.timer_paused||pose.queued!=item.queued)std::cerr<<"Actual/expected visible="<<slot->visible<<'/'<<item.visible<<" sprite="<<pose.sprite_visible<<'/'<<item.sprite_visible<<" sparkle="<<pose.sparkle<<'/'<<item.sparkle<<" paused="<<pose.timer_paused<<'/'<<item.timer_paused<<" queued="<<pose.queued<<'/'<<item.queued<<"\n";
    CHECK(slot->visible==item.visible&&pose.sprite_visible==item.sprite_visible&&pose.sparkle==item.sparkle&&pose.timer_paused==item.timer_paused&&pose.queued==item.queued);
    same(pose.rotation_degrees,item.rotation,"rotation");same(float(pose.timer_left),item.timer,"timer");same(pose.animation_speed,item.speed,"animation-speed");same(pose.animation_time,item.time,"animation-time");CHECK((!pose.timer_running)==item.timer_stopped);CHECK(pose.animation==(item.animation?data->clip(DroppedClipRole(item.animation))->id:0));
    CHECK(slot->parent==expected.parent&&slot->physics==expected.physics&&slot->shapes.at(102)==expected.area&&!expected.body);
   }
   const auto&c=expected.cover;CHECK(object.parent()==c.parent&&object.root_visible()==c.visible&&object.animation().pose().visible==c.sprite_visible&&object.animation().pose().frame==c.frame);same(object.position().x,c.position[0],"cover.x");same(object.position().y,c.position[1],"cover.y");
   const auto clip=c.animation==1?family->break_clip:c.animation==2?family->ready_clip:c.animation==3?family->reset_clip:UINT32_MAX;CHECK(object.animation().assigned_clip()==clip&&object.animation().playing()==c.playing);if(c.animation)same(float(object.animation().position()),c.time,"cover.animation-time");CHECK(object.body_disabled()==c.disabled&&object.body_layer()==c.body&&object.hit_layer()==c.layer&&object.hit_mask()==c.mask);
   std::vector<const BreakableRequest*>audio,rumble;for(const auto&r:world.requests){CHECK(r.epoch==1&&r.owner==cover_id);if(r.kind==BreakableRequestKind::Audio)audio.push_back(&r);else if(r.kind==BreakableRequestKind::Rumble)rumble.push_back(&r);else CHECK(false);}
   CHECK(audio.size()==c.audio.size()&&rumble.size()==c.rumble.size());for(size_t i=0;i<audio.size();++i)CHECK(audio[i]->frame==c.audio[i].first&&bool(audio[i]->value[0])==c.audio[i].second&&audio[i]->identity==breaks.policy().sound);for(size_t i=0;i<rumble.size();++i){CHECK(rumble[i]->frame==c.rumble[i].frame&&rumble[i]->identity==c.rumble[i].device);for(size_t k=0;k<3;++k)CHECK(rumble[i]->value[k]==c.rumble[i].value[k]);}
   CHECK(session.storage.data()==storage&&slot==&world.contents.at(owner->path)&&shape==&slot->shapes.at(102));
   if(frame==-1)CHECK(random.randi()==tail);
   if(frame>=0){
    CHECK(object.advance_idle(1,double(real(item.delta)),world,error));
    const auto emitted=world.requests;
    for(const auto&command:emitted)if(command.kind==BreakableRequestKind::ContentReentered){
     const auto count=world.requests.size();const auto state=random.state();
     for(unsigned invalid=0;invalid<6;++invalid){auto bad=command;if(invalid==0)bad.owner=0;if(invalid==1)bad.epoch=2;if(invalid==2)++bad.frame;if(invalid==3)++bad.identity;if(invalid==4)bad.value[0]=1;if(invalid==5)bad.kind=BreakableRequestKind(99);CHECK(!actor.content_reentered(1,bad,breaks,services,error));CHECK(world.requests.size()==count&&random.state()==state);}
     auto unknown=command;unknown.kind=BreakableRequestKind(99);world.requests.push_back(unknown);CHECK(!actor.content_reentered(1,command,breaks,services,error));CHECK(world.requests.size()==count+1);world.requests.pop_back();
     world.requests.push_back(command);CHECK(!actor.content_reentered(1,command,breaks,services,error));CHECK(world.requests.size()==count+1);world.requests.pop_back();
     CHECK(actor.content_reentered(1,command,breaks,services,error));CHECK(world.requests.size()==count-1&&random.state()==state);
     CHECK(!actor.content_reentered(1,command,breaks,services,error));CHECK(world.requests.size()==count-1&&random.state()==state);
    }
    if(!actor.pose().removed){CHECK(actor.advance_idle(1,double(real(item.delta)),services,error));drain();CHECK(actor.flush_deferred(1,services,error));}
   }
  }
  CHECK(actor.leave(1,services,error)&&object.leave(1,true,world,error));
 }
 CHECK(input.at==input.bytes.size());std::cout<<checks<<" original coffin/DroppedItem composition checks passed\n";
}
