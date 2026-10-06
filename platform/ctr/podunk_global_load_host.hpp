#pragma once
#include "encore/global_load_runtime.hpp"
#include "podunk_character_load_host.hpp"
namespace encore::ctr {
// Composes the source LOAD method body with the SAME real global/globalData,
// File, UI, Character and Inventory owners. This supplies no missing bootstrap.
class PodunkGlobalLoadHost final {
  const upstream::GlobalLoadData *data_ = nullptr;
  const upstream::FieldCharacterLoadData *characters_data_ = nullptr;
  PodunkGlobalDataHost *owner_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkCharacterLoadHost characters_;
  upstream::GlobalLoadRuntime loader_;
  std::map<upstream::FieldObjectId, std::weak_ptr<PodunkItemObject>> items_;
  bool failed_ = false;
  static bool fail(std::string &e,const char *s) { e=s;return false; }
public:
  PodunkGlobalLoadHost() = default;
  PodunkGlobalLoadHost(const PodunkGlobalLoadHost &) = delete;
  PodunkGlobalLoadHost &operator=(const PodunkGlobalLoadHost &) = delete;
  bool initialize(const upstream::GlobalLoadData &data,
                  const upstream::FieldCharacterLoadData &characters,
                  upstream::GlobalLoadGlobalOwner &global,
                  upstream::FieldUiManagerRuntime &ui,
                  PodunkGlobalDataHost &owner,
                  upstream::FieldGlobalRegistry &registry,
                  upstream::SourceRandom &random,std::vector<uint32_t> &uids,
                  upstream::LoadRngClockProvider clock,
                  upstream::FieldGlobalDataStatSignal signal,std::string &e) {
    using namespace upstream;
    if(data_ || failed_ || !data.valid() || !characters.valid() ||
       data.characters_ir_sha256()!=characters.ir_sha256() ||
       data.identity().upstream_commit!=characters.identity().upstream_commit ||
       !owner.runtime().ready_complete() || !clock || !signal)
      return fail(e,"Global LOAD concrete source owners/dependencies rejected");
    data_=&data;characters_data_=&characters;owner_=&owner;registry_=&registry;
    if(!characters_.initialize(characters,owner,registry,random,uids,clock,
                               std::move(signal),e)) {
      failed_=true;return false;
    }
    GlobalLoadHost host;
    host.global=&global;host.ui=&ui;host.files=&owner.yaml_files();
    host.characters=[this](const std::shared_ptr<GlobalYamlValue> &saved,
                           std::string &error) {
      // The runtime just admitted each actual merged Character subroot
      // against the independent cold capability. Execute original setters
      // on the same owners; never install a cloned gameplay projection.
      if(!saved || saved->kind!=6)
        return fail(error,"Global LOAD actual merged source Dictionary absent");
      return characters_.load_cold_default(error);
    };
    host.characters_complete=[this]() { return characters_.characters_complete(); };
    host.new_item=[this](FieldObjectId inventory,const FieldOwnedItem &value,
                         FieldGlobalDataItemReference &out,std::string &error) {
      uint32_t declaration=0;
      const auto *definitions=owner_->items_cache().definitions();
      std::shared_ptr<PodunkItemObject> item;
      if(!definitions ||
         !owner_->runtime().load_inventory_owner(inventory,declaration,error) ||
         !PodunkInventoryHost::construct_loaded_global_item(*characters_data_,
            *definitions,*registry_,declaration,value,item,error)) return false;
      out={registry_,item->object,declaration,item->value,item};
      out.source_owner=item;items_[item->object]=item;return true;
    };
    if(!loader_.initialize(data,owner.runtime(),registry,owner.items_cache(),
                           random,uids,std::move(clock),std::move(host),e)) {
      failed_=true;return false;
    }
    e.clear();return true;
  }
  bool load_cold_default(std::string &e) {
    if(!data_ || failed_) return fail(e,"Global LOAD concrete owner unavailable");
    // The real global.gd cursor admits actual bootstrap prerequisites
    // BEFORE any File/Reader allocation or RNG consumption.
    if(!loader_.load_cold_default(e)) {
      failed_=loader_.poisoned();return false;
    }
    return true;
  }
  bool complete() const {return !failed_ && loader_.complete();}
  const auto &runtime() const {return loader_;}
};
} // namespace encore::ctr
