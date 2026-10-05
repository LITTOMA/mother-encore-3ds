#pragma once
#include "encore/items_data.hpp"
#include <array>
namespace encore::upstream {
enum class FieldSection:uint16_t {Strings=1,Parameters,Bindings,Commands,Slots,Equipment,Resources,Layouts,Clips,Keys};
enum class FieldParameter:uint32_t {ReferenceWidth=1,ReferenceHeight,PlatformWidth,PlatformHeight,PauseColumns,SlotPitch,ListPitch,StatPitch,CursorMoveSeconds,CursorFps,CursorFrame0,CursorFrame1,CursorFrame2,CursorFrame3,MainLineHeight,NumberLineHeight,NumberSpacing,LoopAround,PauseColumnPitch,PauseRowPitch,ListRows,BoostWeight0,BoostWeight1,BoostWeight2,BoostWeight3,BoostWeight4,BoostWeight5,BoostWeight6,OpenMask,ConfirmMask,CancelMask,ScopeMask,OwnerId};
enum class FieldBinding:uint32_t {PauseTitle=1,EquipTitle,None,Empty,StatMaxHP,StatMaxPP,StatOffense,StatDefense,StatSpeed,StatIQ,StatGuts,Owner,MainFont,NumberFont,PauseOpenSound,PauseCloseSound,EquipOpenSound,EquipCloseSound,MoveSound,ConfirmSound,RestrictedSound,ClearSound,EquipSound,BackSound,Level,CashPattern,CashRight};
enum class FieldLayoutRole:uint32_t {PausePanel=1,PauseInside,PauseTitle,PauseCommand,PauseCash,EquipmentPanel,StatsPanel,Owner,EquipTitle,Portrait,SlotPanel,SlotLabel,SlotItem,ListPanel,ListItem,StatLabel,StatValue,StatProjected,StatIcon,DescriptionPanel,DescriptionText,Cursor,BoostEmpty,BoostBetter,BoostLower,PauseCursor,SlotCursor,CandidateCursor,CashLabel,CashValue,LevelLabel,LevelValue,PortraitEquipped,PortraitSuitable,PortraitBetter,PortraitLower,CashCents,ItemsTargetPanel,ItemsActionPanel};
enum class FieldClipRole:uint32_t {PauseOpen=1,PauseClose,EquipOpen,EquipClose,DescriptionOpen,DescriptionClose};
// Encoded source command operation; never infer it from localized labels.
enum class FieldCommandAction:uint32_t {Restricted=0,Equip=1,Items=2,Psi=3};
struct FieldCommand {uint32_t id=0,en=0,zh=0,enabled=0;FieldCommandAction action()const{return FieldCommandAction(enabled);}};
struct FieldSlot {uint32_t id=0,source=0,en=0,zh=0;};
struct FieldEquipment {uint32_t definition=0,source=0,slot=0;std::array<int32_t,7> boosts{};};
struct FieldResource {BattleResource image{};uint32_t file_bytes=0,crc32=0;};
struct FieldClip {uint32_t role=0,first_key=0,key_count=0;float duration=0;};
struct FieldKey {float time=0,value=0,ease=1;};
class FieldEquipmentView {
public:
 bool valid()const{return bytes_!=nullptr;}
 bool same_content(FieldEquipmentView other)const{return bytes_==other.bytes_&&size_==other.size_;}
 uint32_t count(FieldSection)const;std::string_view string(uint32_t)const;
 float parameter(FieldParameter)const;std::string_view binding(FieldBinding,bool chinese=false)const;
 FieldCommand command(uint32_t)const;FieldSlot slot(uint32_t)const;FieldEquipment equipment(uint32_t)const;
 FieldResource resource(uint32_t)const;ItemLayout layout(uint32_t)const;
 FieldClip clip(FieldClipRole)const;FieldKey key(uint32_t)const;
 std::string reviewed_commit()const;bool bind_items(ItemView,std::string&)const;
 bool verify_resources(const char*prefix,std::string&)const;
private:friend class FieldEquipmentData;const uint8_t*bytes_=nullptr;size_t size_=0;const uint8_t*record(FieldSection,uint32_t)const;
};
class FieldEquipmentData {
public:
 FieldEquipmentData()=default;FieldEquipmentData(const FieldEquipmentData&)=delete;FieldEquipmentData&operator=(const FieldEquipmentData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 FieldEquipmentView view()const{FieldEquipmentView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
