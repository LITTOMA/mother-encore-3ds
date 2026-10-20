#pragma once
#include "encore/field_data.hpp"
namespace encore::upstream {
// ENCDKEY1 format1/capability1 is a detached key/lock state component.
// It neither admits nor activates a dungeon, animation, audio or save policy.
enum class DungeonKeyKind:uint32_t {Pickup=1,Lock=2};
struct DungeonKeyPolicy {
 FieldText region,locked_dialogue;
 int32_t initial=0,minimum=0,maximum=0;
};
struct DungeonKeyNode {
 uint32_t id=0;DungeonKeyKind kind=DungeonKeyKind::Pickup;
 FieldText path,flag;int32_t delta=0;uint32_t flags=0;
};
class DungeonKeysView {
public:
 bool valid()const{return p_!=nullptr;}
 uint32_t scene_id()const;uint32_t node_count()const;
 DungeonKeyPolicy policy()const;DungeonKeyNode node(uint32_t)const;
 std::string_view text(FieldText)const;
 const void*bytes_identity()const{return p_;}
private:
 friend class DungeonKeysData;
 uint32_t count(uint32_t)const;const uint8_t*record(uint32_t,uint32_t)const;
 const uint8_t*p_=nullptr;
};
class DungeonKeysData {
public:
 DungeonKeysData()=default;DungeonKeysData(const DungeonKeysData&)=delete;DungeonKeysData&operator=(const DungeonKeysData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 DungeonKeysView view()const{DungeonKeysView v;if(!bytes_.empty())v.p_=bytes_.data();return v;}
private:std::vector<uint8_t>bytes_;
};
}
