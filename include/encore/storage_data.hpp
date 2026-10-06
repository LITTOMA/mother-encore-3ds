#pragma once
#include "encore/items_data.hpp"
#include <array>
namespace encore::upstream {
enum class StorageSection:uint16_t {Strings=1,Policies,Parameters,Bindings,Resources,Layouts,Equipment};
enum class StorageParameter:uint32_t {StorageCapacity=1,InventoryCapacity,Rows,RowPitch,WarnSeconds,EquippedScoreDelta,LoopAround,CursorSeconds,ReferenceWidth,ReferenceHeight,PlatformWidth,PlatformHeight,CursorX,CursorY,CursorWidth,CursorHeight,CursorFps,CursorFrame0,CursorFrame1,CursorFrame2,CursorFrame3,CursorMoveSeconds,BoostWeight0,BoostWeight1,BoostWeight2,BoostWeight3,BoostWeight4,BoostWeight5,BoostWeight6,TextLineHeight,CounterCharacterSpacing};
enum class StorageLayoutRole:uint32_t {Container=1,Panel,OnHandList,StoredList,Title,Counter,Portrait,ItemLabel,Cursor,Description,Prompt,Yes,No,Separator,Scrollbar,Equipped,ItemIcon,IconFrame,DescriptionText,ScrollThumb,Highlight,PortraitEquipped,PortraitSuitable,PortraitBetter,PortraitLower,PortraitFull,QuestionCursor};
enum class StorageBinding:uint32_t {TitleEn=1,TitleZh,UnequipEn,UnequipZh,EquipEn,EquipZh,StorageFullEn,StorageFullZh,InventoryFullEn,InventoryFullZh,YesEn,YesZh,NoEn,NoZh,Owner,MoveSound,ConfirmSound,RestrictedSound,ClearSound,EquipSound,CounterPattern,CounterFont,MainFont};
struct StoragePolicy {uint32_t definition=0,source=0,doses=0,max_count=0,rank_en=0,rank_zh=0,min_doses=0;};
struct StorageEquipment {uint32_t definition=0;std::array<int32_t,7> boosts{};};
class StorageView {
public:
 bool valid()const{return bytes_!=nullptr;}
 bool same_content(StorageView other)const{return bytes_==other.bytes_&&size_==other.size_;}
 uint32_t count(StorageSection)const;std::string_view string(uint32_t)const;
 StoragePolicy policy(uint32_t)const;StorageEquipment equipment(uint32_t)const;
 float parameter(StorageParameter)const;std::string_view binding(StorageBinding)const;
 BattleResource resource(uint32_t)const;ItemLayout layout(uint32_t)const;
 std::string reviewed_commit()const;
 bool bind_items(ItemView,std::string&)const;
private:friend class StorageData;const uint8_t*bytes_=nullptr;size_t size_=0;const uint8_t*record(StorageSection,uint32_t)const;
};
class StorageData {
public:
 StorageData()=default;StorageData(const StorageData&)=delete;StorageData&operator=(const StorageData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 StorageView view()const{StorageView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
