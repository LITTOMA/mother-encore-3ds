#pragma once
#include "encore/movement.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
constexpr uint32_t house_inspection_no_index=UINT32_MAX;
enum class HouseInspectionSection:uint16_t {Strings=1,Objects,Overrides};
enum class HouseInspectionTurn:uint32_t {X=1,Y=2};
struct HouseInspectionObject {
 uint32_t id=0,source_path=0,default_dialogue=0,appear_flag=0,disappear_flag=0,player_turn=0;
 uint32_t first_override=0,override_count=0,collision_mask=0,seen_key=0,default_dialogue_index=house_inspection_no_index;
 Vec2 position{},interact_center{},interact_extents{},prompt_offset{};
};
struct HouseInspectionOverride {uint32_t object=0,flag=0,dialogue=0,dialogue_index=house_inspection_no_index;};
class HouseInspectionView {
public:
 // Views borrow the owning data's bytes. Do not reload while consumers use one.
 bool valid()const{return bytes_!=nullptr;}explicit operator bool()const{return valid();}
 uint32_t count(HouseInspectionSection)const;std::string_view string(uint32_t)const;
 HouseInspectionObject object(uint32_t)const;HouseInspectionOverride override_dialogue(uint32_t)const;
 const uint8_t*reviewed_commit()const{return bytes_?bytes_+32:nullptr;}
private:
 friend class HouseInspectionData;const uint8_t*bytes_=nullptr;size_t size_=0;
 const uint8_t*record(HouseInspectionSection,uint32_t)const;
};
class HouseInspectionData {
public:
 HouseInspectionData()=default;HouseInspectionData(const HouseInspectionData&)=delete;HouseInspectionData&operator=(const HouseInspectionData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 HouseInspectionView view()const{HouseInspectionView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
