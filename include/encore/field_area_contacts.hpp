#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace encore::upstream {
struct FieldAreaEvent {uint32_t index=0,owner=0;bool entered=false;};
struct FieldAreaEvents {std::array<FieldAreaEvent,128> values{};size_t count=0;};
// A checked pair set, not a physics server. Publish the previous physics
// sample before node processing; sample current geometry after movement.
// Disabling either shape cancels an unpublished enter. Idle-only changes do
// not create a new physics sample. Owners and geometry remain resource data.
class FieldAreaContacts {
public:
 bool initialize(const std::vector<uint32_t>&owners,std::string&);
 bool begin(const uint8_t*enabled,size_t,FieldAreaEvents&,std::string&);
 bool end(const uint8_t*enabled,const uint8_t*overlap,size_t,std::string&);
 bool ready()const{return count_!=0;}bool sampling()const{return sampling_;}
 size_t count()const{return count_;}
 bool inside(size_t i)const{return i<count_&&inside_[i];}
private:
 std::array<uint32_t,128>owners_{};
 std::array<uint8_t,128>pending_{},inside_{};
 size_t count_=0;bool sampling_=false;
};
}
