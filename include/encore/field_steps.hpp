#pragma once
#include "encore/field_data.hpp"
#include "encore/audio_data.hpp"
#include <array>
namespace encore::upstream {
// ENCSTEP1 format1/capability1: original MOVE run-loop selection and rectangular
// stepping areas. This codec does not activate a scene or play any audio.
struct FieldStepPolicy {uint32_t default_sound=0;FieldText shadow,slot;};
struct FieldStepSound {FieldText name,source;std::array<uint8_t,32>sha256{};uint32_t id=0;};
struct FieldStepArea {FieldText path;uint32_t enter=0,exit=kFieldNone;Vec2 center{},extents{};FieldText enter_shadow,exit_shadow;uint32_t layer=0,mask=0;bool enabled=false;};
class FieldStepsView {
public:
 bool valid()const{return p_!=nullptr;}uint32_t scene_id()const;
 uint32_t sound_count()const;uint32_t area_count()const;
 FieldStepPolicy policy()const;FieldStepSound sound(uint32_t)const;FieldStepArea area(uint32_t)const;
 std::string_view text(FieldText)const;uint32_t find_sound(std::string_view)const;
 bool matches(const AudioBank&,std::string&)const;
 const void*bytes_identity()const{return p_;}
private:
 friend class FieldStepsData;const uint8_t*record(uint32_t,uint32_t)const;uint32_t count(uint32_t)const;
 const uint8_t*p_=nullptr;
};
class FieldStepsData {
public:
 FieldStepsData()=default;FieldStepsData(const FieldStepsData&)=delete;FieldStepsData&operator=(const FieldStepsData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 FieldStepsView view()const{FieldStepsView v;if(!bytes_.empty())v.p_=bytes_.data();return v;}
private:std::vector<uint8_t>bytes_;
};
}
