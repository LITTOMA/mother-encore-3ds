#include "house_return_inventory.hpp"
#include <algorithm>
#include <cstring>

namespace encore::ctr {
using namespace upstream;
namespace {
struct BusinessCall {
  size_t &depth;
  explicit BusinessCall(size_t &d):depth(d){++depth;}
  ~BusinessCall(){--depth;}
};
bool same(DrawerItemTemplate a,DrawerItemTemplate b) {
  return a.id==b.id&&a.source==b.source&&a.doses==b.doses&&a.key_item==b.key_item;
}
}
bool HouseReturnInventoryOwner::reject(std::string &e,const char *text)const {
  error_=e=text; return false;
}
bool HouseReturnInventoryOwner::source(PodunkDrawerInventoryState &out,
    FieldObjectId &character,uint32_t &inventory,std::string &nickname,
    std::string &e)const {
  if(!input_.inventory||!input_.global||!input_.globaldata||!input_.session||
     !input_.session->valid()||!input_.drawer||
     !input_.inventory->drawer_state(out,e))
    return reject(e,"House Drawer actual source owners unavailable");
  const auto &data=input_.globaldata->runtime();
  const auto *definitions=data.data();
  if(!definitions||!definitions->valid()||
     input_.globaldata->live_inventory()!=input_.inventory||data.registry()!=out.registry||
     input_.global->registry()!=out.registry||!input_.global->data()||
     input_.global->data()->identity().upstream_commit!=out.data->source_pin()||
     definitions->identity().upstream_commit!=out.data->source_pin()||
     !data.character_load_bound_to(*out.characters)||
     !std::equal(out.data->source_pin().begin(),out.data->source_pin().end(),
                 input_.drawer.reviewed_commit())||
     !definitions->bind_inventory(*out.data,*out.definitions,e))
    return reject(e,"House Drawer source pin/Character/Inventory ownership differs");
  const FieldGlobalDataDeclaration *first=nullptr;
  for(const auto &d:definitions->declarations())
    if(d.id==definitions->first_character())first=&d;
  if(!first||first->kind!=1||first->role!=0||first->name.empty()||
     input_.session->defaults().party.size()!=1||
     input_.session->leader_id()!=first->name)
    return reject(e,"House ItemReceiver singleton source Character policy unavailable");
  const auto &binding=out.characters->source_bindings();
  const auto same_script=[&](const std::string &path) {
    std::array<uint8_t,32> a{},b{};
    return definitions->source_hash(path,a)&&out.definitions->source_hash(path,b)&&a==b;
  };
  std::array<uint8_t,32> a{},b{};
  if(!same_script(first->script)||!same_script(binding.inventory_script)||
     !same_script(binding.item_script)||
     !out.characters->source_hash(first->script,a)||!definitions->source_hash(first->script,b)||a!=b||
     !out.constructor_definitions->source_hash(binding.inventory_script,a)||
     !definitions->source_hash(binding.inventory_script,b)||a!=b||
     !out.characters->source_hash(binding.item_script,a)||
     !out.constructor_definitions->source_hash(binding.item_script,b)||a!=b||
     !same_script("Scripts/global/text_tools.gd")||
     !same_script("Data/save_new_game.yaml"))
    return reject(e,"House singleton ItemReceiver/Item/Inventory source closure differs");
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if(!input_.global->array(FieldGlobalMemberRole::Party,party,e)||!party||
     party->values.size()!=1)
    return reject(e,"House ItemReceiver requires the actual singleton global.party");
  character=party->values.front();
  FieldGlobalDataObject actor;
  if(!input_.globaldata->read_object(character,actor,e)||
     actor.declaration!=first->id||actor.kind!=1||actor.role!=0||!actor.inventory||
     !data.character_nickname(first->id,nickname,e)||nickname.empty())
    return reject(e,"House ItemReceiver is not the actual first source Character");
  const auto *normal=out.data->role(0);
  if(!normal||out.actual.state.items.party_order.size()!=1||
     out.actual.state.items.party_order.front()!=normal->id)
    return reject(e,"House source Inventory party order differs from global.party");
  size_t matches=0;
  for(const auto &owner:out.actual.owners)
    if(owner.owner==normal->id&&owner.object==actor.inventory)++matches;
  if(matches!=1)return reject(e,"House Character borrows a different actual Inventory");
  inventory=normal->id;
  bool space=false;
  if(!input_.inventory->source_drawer_space(space,e))return false;
  FieldObjectId context=0;
  if(!input_.global->object(FieldGlobalMemberRole::Item,context,e)||
     context!=input_.inventory->global_item())
    return reject(e,"House global.item is not the retained actual Item context");
  e.clear(); return true;
}
bool HouseReturnInventoryOwner::frame(HouseReturnDialogueReceipt &out,
                                      std::string &e)const {
  if(!prepared_||business_depth_||!input_.dialogue||!input_.world||!input_.runtime||
     !input_.dialogue->inventory_frame(out,e)||out.world!=input_.world||
     out.runtime!=input_.runtime||input_.runtime->world_owner()!=input_.world||
     input_.runtime->drawer_effects()!=this||
     input_.runtime->drawer_source().reviewed_commit()!=input_.drawer.reviewed_commit()||
     input_.world->house_programme_owner()!=input_.dialogue||
     out.world_generation!=input_.world->source_generation())
    return reject(e,"House Drawer actual World/runtime/source call receipt rejected");
  PodunkDrawerInventoryState actual;FieldObjectId actor=0;uint32_t inventory=0;
  std::string nickname;
  if(!source(actual,actor,inventory,nickname,e)||out.registry!=actual.registry||
     out.random!=actual.random||out.uid_ledger!=actual.uid_ledger||
     input_.runtime->source_player_nickname()!=nickname)
    return reject(e,"House Drawer source receiver/entropy/text owners differ");
  PodunkMickHouseNativeState session;
  auto *house=input_.dialogue->context().house;
  if(!out.session||!house||house!=out.house||
     !out.session->house_native_state(*house,session,e)||
     session.registry!=actual.registry||session.global_data!=&input_.globaldata->runtime()||
     session.random!=actual.random||session.uid_ledger!=actual.uid_ledger)
    return reject(e,"House Drawer did not retain the same actual source session owners");
  e.clear(); return true;
}
bool HouseReturnInventoryOwner::prepare(HouseReturnInventoryInput in,std::string &e) {
  if(prepared_||bound_||business_depth_||!in.dialogue||!in.world||!in.runtime||
     in.runtime->world_owner()!=in.world)
    return reject(e,"House Drawer preparation scope rejected");
  input_=in;
  PodunkDrawerInventoryState actual;FieldObjectId actor=0;uint32_t inventory=0;
  std::string nickname;
  if(!source(actual,actor,inventory,nickname,e)||
     !input_.inventory->prepare_drawer_audio(input_.drawer,e)) {
    input_={}; return false;
  }
  // Source text consumers use the same live Character nickname. This is a
  // validation, not a replacement of the source Character or party Array.
  if(input_.runtime->source_player_nickname()!=nickname) {
    input_={}; return reject(e,"House Drawer printer nickname differs from source Character");
  }
  prepared_=true;e.clear();return true;
}
bool HouseReturnInventoryOwner::bind(std::string &e) {
  HouseReturnDialogueReceipt receipt;
  if(bound_||!frame(receipt,e)||
     !input_.world->bind_house_inventory_owner(*this,e))return false;
  bound_=true;e.clear();return true;
}
bool HouseReturnInventoryOwner::unbind(std::string &e) {
  if(!bound_||business_depth_||!input_.dialogue->source_frame_closed(*input_.world,e)||
     !input_.world->unbind_house_inventory_owner(*this,e))return false;
  bound_=false;e.clear();return true;
}
bool HouseReturnInventoryOwner::observe_inventory(OpeningHouseInventoryState &out,
                                                  std::string &e)const {
  HouseReturnDialogueReceipt actual;
  if(!frame(actual,e))return false;
  OpeningHouseInventoryState next;
  next.world=input_.world;next.programme_owner=input_.dialogue;
  next.room=input_.world->content();next.drawer=input_.drawer;
  next.effects=const_cast<HouseReturnInventoryOwner*>(this);
  next.programme=input_.world->story_program_index();
  next.generation=input_.world->source_generation();next.source_call_closed=true;
  out=next;e.clear();return true;
}
bool HouseReturnInventoryOwner::validate_item(DrawerItemTemplate t,std::string_view n,
                                              std::string &e) {
  PodunkDrawerInventoryState actual;FieldObjectId actor=0;uint32_t inventory=0;
  std::string nickname;
  if(!source(actual,actor,inventory,nickname,e))return false;
  size_t matches=0;
  for(uint32_t i=0;i<input_.drawer.count(DrawerSection::Templates);++i)
    if(same(t,input_.drawer.item_template(i))&&input_.drawer.string(t.source)==n)++matches;
  const auto *d=actual.definitions->definition(std::string(n));
  const auto *constructor=actual.constructor_definitions->definition(std::string(n));
  const auto *policy=d?actual.data->policy(d->id):nullptr;
  std::array<uint8_t,32> sha{};
  if(matches!=1||!d||!constructor||!policy||d->id!=constructor->id||
     d->source!=constructor->source||d->doses!=t.doses||constructor->doses!=t.doses||
     d->keyitem()||constructor->keyitem()||t.key_item||
     !actual.constructor_definitions->source_hash(d->source,sha)||sha!=policy->source_sha)
    return reject(e,"House Drawer actual Item template/source/default arguments rejected");
  e.clear();return true;
}
bool HouseReturnInventoryOwner::validate_sound(std::string_view n,std::string &e) {
  if(!prepared_)return reject(e,"House Drawer sound owner unprepared");
  bool found=false;
  for(uint32_t i=0;i<input_.drawer.count(DrawerSection::Commands);++i) {
    const auto c=input_.drawer.command(i);
    if(c.opcode==uint32_t(DrawerOpcode::PlaySound)&&input_.drawer.string(c.a)==n)found=true;
  }
  if(!found)return reject(e,"House Drawer sound is outside its actual source");
  return input_.inventory->source_drawer_sound(n,false,e);
}
bool HouseReturnInventoryOwner::inventory_space()const {
  HouseReturnDialogueReceipt r;bool space=false;std::string e;
  RoomCommand c;
  if(!bound_||!frame(r,e)||!command(DialogueActionKind::BranchInventorySpace,c,e)||
     !input_.inventory->source_drawer_space(space,e)) {
    error_=e.empty()?"House source Inventory space receipt unavailable":e;return false;
  }
  return space;
}
bool HouseReturnInventoryOwner::command(DialogueActionKind kind,RoomCommand &out,
                                         std::string &e)const {
  const auto &w=*input_.world;
  const auto room=w.content();const auto programme=w.story_program_index();
  const auto next=w.story_next_command_index();
  if(w.house_inventory_owner()!=this||w.story_status()!=DialogueStatus::Running||
     w.story_generation()!=w.source_generation()||programme>=room.program_count()||
     !next||next>room.program(programme).command_count)
    return reject(e,"House Inventory effect has no actual running VM cursor");
  const auto p=room.program(programme);
  const auto c=room.command(p.first_command+next-1);
  if(c.opcode!=uint16_t(kind)||
     input_.drawer.string(input_.drawer.binding().source_path)!=
       std::string("Data/Dialogue/")+std::string(room.string(p.source_path_string))+".yaml"||
     !w.admit_house_inventory_command(programme,next-1,e))
    return reject(e,"House Inventory effect differs from the actual Drawer source command");
  out=c;e.clear();return true;
}
bool HouseReturnInventoryOwner::grant_item(DrawerItemTemplate t,std::string_view n,
                                           std::string &e) {
  HouseReturnDialogueReceipt r;
  RoomCommand c;
  if(!bound_||!frame(r,e)||!command(DialogueActionKind::GrantInventoryItem,c,e)||
     c.target_index>=input_.drawer.count(DrawerSection::Templates)||
     !same(t,input_.drawer.item_template(c.target_index))||!validate_item(t,n,e))
    return reject(e,"House Item grant requires its exact running source template cursor");
  PodunkDrawerInventoryState actual;FieldObjectId actor=0;uint32_t inventory=0;
  std::string nickname;
  if(!source(actual,actor,inventory,nickname,e))return false;
  BusinessCall call(business_depth_);
  return input_.inventory->source_drawer_grant(input_.drawer,t,n,inventory,*input_.global,e);
}
bool HouseReturnInventoryOwner::item_receiver(FieldObjectId &out,std::string &name,
                                              std::string &e)const {
  HouseReturnDialogueReceipt r;
  if(!bound_||!frame(r,e))return false;
  PodunkDrawerInventoryState actual;FieldObjectId actor=0;uint32_t inventory=0;
  std::string nickname;
  if(!source(actual,actor,inventory,nickname,e)||!actual.actual.context||
     actual.actual.context->owner!=inventory)
    return reject(e,"House ItemReceiver has no actual owned global.item");
  FieldOwnedItem value;
  if(!actual.actual.context->read_item(value,e))return false;
  const auto row=std::find_if(actual.actual.state.items.inventories.begin(),
      actual.actual.state.items.inventories.end(),[&](const auto &x){return x.owner==inventory;});
  if(row==actual.actual.state.items.inventories.end()||
     std::none_of(row->items.begin(),row->items.end(),[&](const auto &x){
       return x.uid==value.uid&&x.definition==value.definition&&x.doses==value.doses&&x.equipped==value.equipped;}))
    return reject(e,"House ItemReceiver context is not in the actual source Inventory");
  out=actor;name=nickname;e.clear();return true;
}
bool HouseReturnInventoryOwner::validate_text(uint32_t id,std::string &e) {
  if(!prepared_)return reject(e,"House Drawer text owner unprepared");
  return input_.runtime->validate_text(id,e);
}
bool HouseReturnInventoryOwner::validate_flag(std::string_view n,std::string &e) {
  if(!prepared_)return reject(e,"House Drawer flag owner unprepared");
  return input_.runtime->validate_flag(n,e);
}
bool HouseReturnInventoryOwner::flag(std::string_view n,bool &v,std::string &e) {
  if(!prepared_)return reject(e,"House Drawer flag owner unprepared");
  return input_.runtime->flag(n,v,e);
}
// Those effects belong to the real Room VM/native Dialogue consumer. Calling
// legacy Drawer execution through this owner would execute the source twice.
bool HouseReturnInventoryOwner::show_text(uint32_t,std::string &e) {
  return reject(e,"House Drawer text requires the same native Room dialogue cursor");
}
bool HouseReturnInventoryOwner::play_sound(std::string_view,std::string &e) {
  return reject(e,"House Drawer audio requires the same World Room command cursor");
}
bool HouseReturnInventoryOwner::set_flag(std::string_view,bool,std::string &e) {
  return reject(e,"House Drawer flags require the same World Room command cursor");
}
} // namespace encore::ctr
