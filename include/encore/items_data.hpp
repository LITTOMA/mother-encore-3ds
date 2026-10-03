#pragma once
#include "encore/battle_data.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// Execution schema only. Content, bindings, initial state and tuning are external.
constexpr uint32_t item_no_index=UINT32_MAX;
enum class ItemSection:uint16_t {Strings=1,Metadata,Definitions,Instances,Resources,Layouts,Parameters,Clips,Tracks,Keys,Sounds};
enum class ItemDefinitionFlag:uint32_t {Equipment=1};
enum class ItemLayoutFlag:uint32_t {Visible=1,ClipChildren=2,Centered=4,BehindParent=8};
enum class ItemLayoutRole:uint32_t {Container=1,Panel,Grid,ItemLabel,ItemIcon,Equipped,Cursor,InfoPanel,Description,Scrollbar,ScrollBackground,ScrollThumb,Hint};
enum class ItemDrawKind:uint32_t {Container=1,Sprite,Rectangle,NinePatch,Text};
enum class ItemParameter:uint32_t {SourceViewport=1,PlatformViewport,GridShape,LabelSize,CursorOffset,CursorMotion,InfoMotion,DisabledColor,NormalColor,ScrollColor,InputBinding,InputRepeat,Count};
enum class ItemClipRole:uint32_t {Open=1,Close,CursorIdle,Count};
enum class ItemProperty:uint32_t {Position=1,PositionX,PositionY,Scale,Alpha,Visible,Rect,Frame,Offset};
enum class ItemSoundEvent:uint32_t {Open=1,Move,Close,Disabled,Confirm,Count};
struct ItemMetadata {uint32_t capacity=0,owner=0,flags=0,reserved=0;};
struct ItemDefinition {
 uint32_t id=0,source=0,name=0,description=0,icon=item_no_index,equipment_slot=item_no_index;
 int32_t heal_hp=0,heal_pp=0,max_hp_boost=0,max_pp_boost=0;
 uint32_t flags=0,can_use=0;
};
struct ItemInstance {uint32_t id=0,definition=0,equipped=0,doses=0;};
struct ItemLayout {
 uint32_t id=0,role=0,parent=item_no_index,kind=0,resource=item_no_index,frame=0,flags=0;
 Vec2 anchor{};BattleValue rect{},color{};uint32_t patch[4]{};
};
struct ItemClip {uint32_t id=0,role=0,first_track=0,track_count=0;float duration=0;uint32_t loop=0;};
struct ItemTrack {uint32_t target=0,property=0,interpolation=0,first_key=0,key_count=0;};
using ItemKey=BattleKey;
struct ItemSound {uint32_t event=0,path=0,audio_id=0,flags=0;};
class ItemView {
public:
 bool valid()const{return bytes_!=nullptr;}explicit operator bool()const{return valid();}
 uint32_t count(ItemSection)const;std::string_view string(uint32_t)const;
 ItemMetadata metadata()const;ItemDefinition definition(uint32_t)const;
 ItemInstance initial_instance(uint32_t)const;ItemInstance instance(uint32_t i)const{return initial_instance(i);}
 BattleResource resource(uint32_t)const;ItemLayout layout(uint32_t)const;
 BattleValue parameter(ItemParameter)const;ItemClip clip(uint32_t)const;
 ItemTrack track(uint32_t)const;ItemKey key(uint32_t)const;ItemSound sound(uint32_t)const;
 uint32_t clip_for(ItemClipRole)const;uint32_t layout_for(ItemLayoutRole)const;
 ItemSound sound_for(ItemSoundEvent)const;
private:
 friend class ItemData;const uint8_t*bytes_=nullptr;size_t size_=0;
 const uint8_t*record(ItemSection,uint32_t)const;
};
// Views borrow this owner's bytes. A successful reload invalidates earlier views;
// a rejected reload preserves them. The input buffer is copied only after validation.
class ItemData {
public:
 ItemData()=default;ItemData(const ItemData&)=delete;ItemData&operator=(const ItemData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 ItemView view()const{ItemView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
