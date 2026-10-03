#include "encore/items_menu.hpp"
#include "encore/battle_entry.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
namespace {float mix(float a,float b,float t){return a+(b-a)*t;}}
bool InventoryState::initialize(ItemView data){
 if(!data.valid()||data.count(ItemSection::Instances)>data.metadata().capacity)return false;
 std::vector<ItemInstance>next;next.reserve(data.metadata().capacity);
 for(uint32_t i=0;i<data.count(ItemSection::Instances);++i)next.push_back(data.initial_instance(i));
 data_=data;instances_=std::move(next);return true;
}
bool InventoryState::restore(ItemView data,const std::vector<ItemInstance>& instances,std::string& error){
 auto reject=[&](const char*message){error=message;return false;};
 if(!data.valid())return reject("Inventory restore requires checked item content");
 if(instances.size()>data.metadata().capacity)return reject("Inventory restore capacity exceeded");
 std::set<uint32_t>uids,equipment_slots;
 for(const auto&item:instances){
  if(item.definition>=data.count(ItemSection::Definitions))return reject("Inventory restore unknown definition");
  if(!uids.insert(item.id).second)return reject("Inventory restore duplicate UID");
  if(item.equipped>1||!item.doses||item.doses>65535)return reject("Inventory restore invalid doses/equipped value");
  if(item.equipped){const auto definition=data.definition(item.definition);
   if(!(definition.flags&uint32_t(ItemDefinitionFlag::Equipment))||!equipment_slots.insert(definition.equipment_slot).second)return reject("Inventory restore incompatible equipped instances");
  }
 }
 auto next=instances;data_=data;instances_=std::move(next);error.clear();return true;
}
bool InventoryState::can_use(uint32_t i)const{
 if(!valid()||i>=size())return false;
 const auto& item=instances_[i];const auto d=data_.definition(item.definition);
 return d.can_use&&((d.heal_hp>0&&d.max_hp_boost==0)||(d.heal_pp>0&&d.max_pp_boost==0)||((d.flags&uint32_t(ItemDefinitionFlag::Equipment))&&!item.equipped));
}
bool BattleItemsMenu::fail(const char*error){error_=error;return false;}
bool BattleItemsMenu::initialize(InventoryState& inventory){
 if(!inventory.valid())return fail("Missing checked inventory");
 *this=BattleItemsMenu{};inventory_=&inventory;data_=inventory.content();
 const auto info=data_.layout(data_.layout_for(ItemLayoutRole::InfoPanel));
 info_y_=info_start_=info_target_=info.rect.y;
 return true;
}
void BattleItemsMenu::retarget_cursor(bool transition){
 const auto grid=data_.parameter(ItemParameter::GridShape),offset=data_.parameter(ItemParameter::CursorOffset);
 const auto origin=data_.layout(data_.layout_for(ItemLayoutRole::Grid)).rect;
 const auto columns=uint32_t(grid.x);const uint32_t local=selection_-row_offset_*columns;
 cursor_target_={origin.x+float(local%columns)*grid.z+offset.x,origin.y+float(local/columns)*grid.w+offset.y};
 cursor_start_=cursor_;move_time_=0;if(!transition){const auto initial=data_.layout(data_.layout_for(ItemLayoutRole::Cursor));cursor_target_={initial.rect.x+float(local%columns)*grid.z,initial.rect.y+float(local/columns)*grid.w};cursor_=cursor_start_=cursor_target_;}
}
bool BattleItemsMenu::open(bool reset){
 if(!inventory_||!data_.valid())return fail("Items menu has no session inventory");
 if(reset){selection_=row_offset_=0;retarget_cursor(false);}
 active_=true;closing_=false;menu_time_=0;repeat_remaining_=0;result_=ItemMenuResult::None;
 info_start_=info_y_;info_time_=0;
 const auto info=data_.layout(data_.layout_for(ItemLayoutRole::InfoPanel));
 info_target_=info.rect.y-((info_visible_&&inventory_->size())?data_.parameter(ItemParameter::InfoMotion).y:0);
 sounds_.push_back(ItemSoundEvent::Open);return true;
}
void BattleItemsMenu::move(int x,int y){
 const auto size=inventory_->size();if(!size)return;
 const auto shape=data_.parameter(ItemParameter::GridShape);const uint32_t columns=uint32_t(shape.x),page_rows=uint32_t(shape.y),rows=(size+columns-1)/columns;
 int row=int(selection_/columns),col=int(selection_%columns);uint32_t next=selection_;
 if(y){row+=y;if(row<0)row=int(rows)-1;else if(row>=int(rows))row=0;col=std::min(col,int(std::min(columns,size-uint32_t(row)*columns))-1);next=uint32_t(row)*columns+uint32_t(col);}
 else if(x){col+=x;if(col<0)col=int(columns)-1;else if(col>=int(columns))col=0;
  if(size%columns&&uint32_t(row)==rows-1&&selection_%columns==0&&uint32_t(row)*columns+uint32_t(col)>=size)next=size>1?size-2:0;
  else next=uint32_t(row)*columns+std::min(uint32_t(col),std::min(columns,size-uint32_t(row)*columns)-1);
 }
 if(next==selection_)return;
 selection_=next;row=int(selection_/columns);
 if(uint32_t(row)<row_offset_)row_offset_=uint32_t(row);else if(uint32_t(row)>=row_offset_+page_rows)row_offset_=uint32_t(row)-page_rows+1;
 retarget_cursor(true);repeat_remaining_=data_.parameter(ItemParameter::CursorMotion).y;sounds_.push_back(ItemSoundEvent::Move);
}
bool BattleItemsMenu::input(int x,int y,bool confirm,bool cancel,bool scope,bool navigation_pulse){
 if(x<-1||x>1||y<-1||y>1)return fail("Invalid inventory direction");
 if(!active_)return true;
 if(cancel){active_=false;closing_=true;menu_time_=0;result_=ItemMenuResult::Back;sounds_.push_back(ItemSoundEvent::Close);info_start_=info_y_;info_target_=data_.layout(data_.layout_for(ItemLayoutRole::InfoPanel)).rect.y;info_time_=0;return true;}
 if(scope){info_visible_=!info_visible_;info_start_=info_y_;info_time_=0;info_target_=data_.layout(data_.layout_for(ItemLayoutRole::InfoPanel)).rect.y-(info_visible_?data_.parameter(ItemParameter::InfoMotion).y:0);}
 if(navigation_pulse||repeat_remaining_<=0)move(x,y);
 if(confirm&&inventory_->size()){
  if(inventory_->can_use(selection_)){result_=ItemMenuResult::Selected;sounds_.push_back(ItemSoundEvent::Confirm);}
  else{result_=ItemMenuResult::Restricted;sounds_.push_back(ItemSoundEvent::Disabled);}
 }
 return true;
}
bool BattleItemsMenu::idle_frame(double delta){
 if(!std::isfinite(delta)||delta<0||delta>1)return fail("Invalid inventory delta");
 if(!data_.valid())return false;
 const float dt=float(delta);repeat_remaining_=std::max(0.0f,repeat_remaining_-dt);menu_time_+=dt;cursor_time_+=dt;move_time_+=dt;info_time_+=dt;
 const auto motion=data_.parameter(ItemParameter::CursorMotion);float t=std::min(move_time_/motion.x,1.0f);const float quart=1-std::pow(1-t,4.0f);
 cursor_={mix(cursor_start_.x,cursor_target_.x,quart),mix(cursor_start_.y,cursor_target_.y,quart)};
 t=std::min(info_time_/data_.parameter(ItemParameter::InfoMotion).x,1.0f);info_y_=mix(info_start_,info_target_,1-(1-t)*(1-t));
 if(closing_&&menu_time_>=data_.clip(data_.clip_for(ItemClipRole::Close)).duration)closing_=false;
 return true;
}
void BattleItemsMenu::sample_clip(uint32_t index,float time,uint32_t layout,ItemMenuPose&pose)const{
 if(index==item_no_index)return;
 const auto clip=data_.clip(index);if(clip.loop&&clip.duration>0)time=std::fmod(time,clip.duration);
 for(uint32_t i=0;i<clip.track_count;++i){const auto track=data_.track(clip.first_track+i);if(track.target!=layout||!track.key_count)continue;
  uint32_t key=0;while(key+1<track.key_count&&data_.key(track.first_key+key+1).time<=time)++key;
  const auto a=data_.key(track.first_key+key);auto v=a.value;
  if(track.interpolation==0&&key+1<track.key_count){const auto b=data_.key(track.first_key+key+1);const float t=battle_ease((time-a.time)/(b.time-a.time),a.ease);v={mix(v.x,b.value.x,t),mix(v.y,b.value.y,t),mix(v.z,b.value.z,t),mix(v.w,b.value.w,t)};}
  switch(ItemProperty(track.property)){
   case ItemProperty::Position:pose.rect.x=v.x;pose.rect.y=v.y;break;case ItemProperty::PositionX:pose.rect.x=v.x;break;case ItemProperty::PositionY:pose.rect.y=v.x;break;
   case ItemProperty::Scale:pose.scale={v.x,v.y};break;case ItemProperty::Alpha:pose.color.w=v.x;break;case ItemProperty::Visible:pose.visible=v.x!=0;break;case ItemProperty::Rect:pose.rect=v;break;
   case ItemProperty::Frame:pose.frame=uint32_t(v.x);break;case ItemProperty::Offset:pose.offset={v.x,v.y};break;
  }
 }
}
ItemMenuPose BattleItemsMenu::pose(uint32_t index)const{
 if(!data_.valid()||index>=data_.count(ItemSection::Layouts))return {};
 const auto l=data_.layout(index);ItemMenuPose p{l,l.rect,l.color,{1,1},{},l.frame,(l.flags&uint32_t(ItemLayoutFlag::Visible))!=0};
 if(!visible()){p.visible=false;return p;}
 sample_clip(data_.clip_for(closing_?ItemClipRole::Close:ItemClipRole::Open),menu_time_,index,p);
 sample_clip(data_.clip_for(ItemClipRole::CursorIdle),cursor_time_,index,p);
 if(index==data_.layout_for(ItemLayoutRole::Cursor)){p.rect.x=cursor_.x;p.rect.y=cursor_.y;p.visible=p.visible&&inventory_->size()>0;}
 if(l.role==uint32_t(ItemLayoutRole::InfoPanel))p.rect.y=info_y_;
 return p;
}
}
