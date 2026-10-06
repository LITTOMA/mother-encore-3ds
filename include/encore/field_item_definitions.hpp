#pragma once
#include "encore/load_rng.hpp"
#include <array>
#include <map>
#include <string>
#include <vector>
namespace encore::upstream {
enum class FieldItemBindingKind:uint32_t {Present=1,Dropped,Door,Payphone,Programme,Initial};
enum class FieldItemFunction:uint32_t {Consume=1,Use,Equip,Transform};
struct FieldItemPendingValue {uint32_t field=0,kind=0;std::string key,text;int32_t integer=0;double real=0;};
struct FieldItemAction {FieldItemFunction function{};std::string name,textfail;bool pending=true;};
struct FieldItemDefinition {
 uint32_t id=0,doses=0,flags=0,legacy_domain=0,legacy_id=0;int32_t cost=0,value=0,heal_hp=0,heal_pp=0;std::array<int32_t,7>boost{};
 std::string source,item_name,name_key,sorting_key,description_key,article_key,slot,transform;
 std::vector<std::string>can_use,can_consume,status_heals;std::vector<FieldItemAction>actions;
 std::vector<FieldItemPendingValue>pending_metadata;int32_t unequipped_sort_score=0;std::map<std::string,std::string>sorting_translations;
 bool keyitem()const{return(flags&1)!=0;}bool is_food()const{return(flags&2)!=0;}bool target_all()const{return(flags&4)!=0;}bool reusable()const{return(flags&8)!=0;}
};
struct FieldItemBinding {FieldItemBindingKind kind{};uint32_t object_id=0,definition=0,operation=0;std::string scene,node,program,label;};
class FieldItemDefinitions {
public:
 bool global_constructor_scope()const{return format_==3;}const auto&constructor_sources()const{return constructor_sources_;}
 uint32_t god_storage_id()const{return god_storage_id_;}uint32_t god_storage_role()const{return god_storage_role_;}const auto&god_storage_member()const{return god_storage_member_;}const auto&god_storage_type_field()const{return god_storage_type_field_;}const auto&god_storage_items_field()const{return god_storage_items_field_;}
 uint32_t default_doses()const{return default_doses_;}
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}const auto&source_pin()const{return pin_;}
 const auto&definitions()const{return definitions_;}const auto&bindings()const{return bindings_;}const FieldItemDefinition*definition(uint32_t)const;const FieldItemDefinition*definition(const std::string&)const;const FieldItemDefinition*legacy(uint32_t domain,uint32_t id)const;
 const FieldItemBinding*binding(FieldItemBindingKind,const std::string&scene,uint32_t object)const;const FieldItemBinding*programme(const std::string&,const std::string&)const;
 uint32_t capacity(uint32_t role)const{return role<4?capacities_[role]:0;}bool unbounded(uint32_t role)const{return role<4&&(unbounded_mask_&(1u<<role));}uint32_t dose_step()const{return dose_step_;}uint32_t dose_drop_threshold()const{return dose_drop_threshold_;}bool source_hash(const std::string&,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;uint32_t format_=0,god_storage_id_=0,god_storage_role_=0;std::vector<std::string>constructor_sources_;std::string god_storage_member_,god_storage_type_field_,god_storage_items_field_;std::array<uint8_t,20>pin_{};std::array<uint32_t,4>capacities_{};uint32_t unbounded_mask_=0,default_doses_=0,dose_step_=0,dose_drop_threshold_=0,uid_protocol_=0;std::vector<FieldItemDefinition>definitions_;std::vector<FieldItemBinding>bindings_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
// UID is the source uint32 randi result, INCLUDING zero. Definition identity
// belongs to the separate stable resource domain and never substitutes for UID.
struct FieldOwnedItem {uint32_t definition=0,uid=0,doses=0;bool equipped=false;};
struct FieldItemInventory {uint32_t owner=0,role=0;std::vector<FieldOwnedItem>items;};
// Member rows include inactive source characters. party_order alone identifies
// current global.party order. KEY/STORAGE are distinct source inventory types.
struct FieldItemSnapshot {std::vector<FieldItemInventory>inventories;std::vector<uint32_t>party_order;};
enum class FieldItemResultKind:uint32_t {None=0,Owned,Transient,Reduced,Dropped,Transferred,TransferDropped};
struct FieldItemResult {FieldItemResultKind kind=FieldItemResultKind::None;FieldOwnedItem item;uint32_t owner=0,previous_owner=0;uint64_t seed=0,raw_draws=0;};
struct FieldItemDefinitionsHost {
 std::function<bool(const FieldItemDefinitions&,std::string&)>bind;
 std::function<bool(FieldItemSnapshot&,std::string&)>read;
 // Atomically publish ownership and actual global.item context. This callback
 // invokes no other gameplay/RNG effects. Shared RNG+generated UID ledger are
 // committed immediately after it returns, before subsequent source flags.
 std::function<bool(const FieldItemSnapshot&before,const FieldItemSnapshot&after,const FieldItemResult&,std::string&)>commit;
};
class FieldItemDefinitionsRuntime {
public:
 bool initialize(const FieldItemDefinitions&,SourceRandom&,std::vector<uint32_t>&generated_uid_ledger,LoadRngClockProvider,FieldItemDefinitionsHost,std::string&);
 bool inventory_space(bool&,std::string&)const;
 bool select_holder(FieldItemBindingKind,const std::string&scene,uint32_t source_object,bool give,FieldItemResult&,std::string&);
 bool grant_programme(const std::string&source_program,const std::string&label,FieldItemResult&,std::string&);
 bool query(FieldItemBindingKind,const std::string&scene,uint32_t source_object,bool&found,FieldItemResult&,std::string&)const;
 bool reduce_or_drop(uint32_t uid,FieldItemResult&,std::string&);bool drop(uint32_t uid,FieldItemResult&,std::string&);bool transfer(uint32_t uid,uint32_t target_owner,FieldItemResult&,std::string&);
 bool validate_snapshot(const FieldItemSnapshot&,std::string&)const;const FieldItemDefinitions*data()const{return data_;}
private:
 const FieldItemDefinitions*data_=nullptr;SourceRandom*random_=nullptr;std::vector<uint32_t>*ledger_=nullptr;LoadRngClockProvider clock_;FieldItemDefinitionsHost host_;
 bool read(FieldItemSnapshot&,std::string&)const;bool construct(const FieldItemBinding&,bool give,bool full_transient,FieldItemResult&,std::string&);
 bool remove(uint32_t,bool dose,FieldItemResult&,std::string&);
};
}
