#pragma once
#include "podunk_global_data_host.hpp"
#include "encore/field_character_load.hpp"

namespace encore::ctr {
// EnemySkill is an implicit Reference. Its actual bounded source body keeps
// all four declared fields; construction does not supply any Node lifecycle.
class PodunkEnemySkillObject final : public upstream::FieldCharacterEnemySkillReference {
  const upstream::FieldCharacterLoadData *source_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldGlobalExternalBinding binding_{};
  std::array<uint8_t,32> ir_{};
  std::map<std::string,upstream::GlobalYamlValue> fields_;
  bool complete_ = false;
  static bool fail(std::string &e, const char *s) { e=s; return false; }
public:
  PodunkEnemySkillObject() = default;
  PodunkEnemySkillObject(const PodunkEnemySkillObject &) = delete;
  PodunkEnemySkillObject &operator=(const PodunkEnemySkillObject &) = delete;
  ~PodunkEnemySkillObject() override {
    if (registry_ && binding_.object) {
      std::string e; registry_->retire_object(binding_.object,e);
    }
  }
  upstream::FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override { return "Reference"; }
  const upstream::FieldGlobalRegistry *registry() const override { return registry_; }
  bool checked_source_hash(std::string_view path, std::array<uint8_t,32> &out) const override {
    return source_ && source_->valid() && source_->ir_sha256()==ir_ &&
           path==binding_.source.script && source_->source_hash(path,out) &&
           out==binding_.source.script_sha;
  }
  static bool construct(const upstream::FieldCharacterLoadData &source,
                        upstream::FieldGlobalRegistry &registry,
                        const upstream::FieldCharacterEnemySkill &arguments,
                        std::shared_ptr<PodunkEnemySkillObject> &out,
                        std::string &e) {
    using namespace upstream;
    const auto &b=source.source_bindings();
    std::array<uint8_t,32> proof{};
    if (out || !source.valid() || !b.enemy_skill_id ||
        b.enemy_skill_native!="Reference" || b.enemy_skill_defaults.size()!=4 ||
        b.enemy_constructor_defaults.size()!=3 ||
        !source.source_hash(b.enemy_skill_script,proof))
      return fail(e,"EnemySkill checked actual source constructor absent");
    FieldObjectId id=0;
    if (!registry.allocate_object(id,e)) return false;
    auto object=std::make_shared<PodunkEnemySkillObject>();
    object->source_=&source; object->registry_=&registry; object->ir_=source.ir_sha256();
    FieldGlobalExternalSpec spec;
    spec.identity.upstream_commit=source.identity().upstream_commit;
    spec.identity.scene_id=spec.stable_id=b.enemy_skill_id;
    spec.identity.source_sha256=spec.source_sha=spec.script_sha=proof;
    spec.role=5; spec.native_class=b.enemy_skill_native;
    spec.source=spec.script=b.enemy_skill_script;
    object->binding_={id,spec,0x454e0050,2};
    for (const auto &d:b.enemy_skill_defaults) {
      GlobalYamlValue v;
      if (d.kind==1) { v.kind=4;v.string=d.string_value; }
      else if (d.kind==2) { v.kind=2;v.integer=d.integer_value; }
      else return fail(e,"EnemySkill source declared native type unsupported");
      if (!object->fields_.emplace(d.name,std::move(v)).second)
        return fail(e,"EnemySkill duplicate source member");
    }
    if (!registry.publish_native_reference(spec,id,object,e)) return false;
    // The source data controls assignment order and every actual field name.
    for (const auto &d:b.enemy_constructor_defaults) {
      auto p=object->fields_.find(d.name);
      if (p==object->fields_.end()) return fail(e,"EnemySkill source assignment target absent");
      if (d.name==b.enemy_id_field && p->second.kind==4)
        p->second.string=arguments.skill;
      else if (d.name==b.enemy_weight_field && p->second.kind==2)
        p->second.integer=arguments.weight;
      else if (d.name==b.enemy_cooldown_field && p->second.kind==2)
        p->second.integer=arguments.cooldown;
      else return fail(e,"EnemySkill source assignment opcode/field rejected");
    }
    object->complete_=true;out=std::move(object);e.clear();return true;
  }
  bool read_skill(upstream::FieldCharacterEnemySkill &out,
                  int64_t &remaining,std::string &e) const override {
    if (!complete_ || !registry_ || !source_ || !source_->valid() || source_->ir_sha256()!=ir_)
      return fail(e,"EnemySkill actual body/source unavailable");
    auto actual=registry_->native_reference(binding_.object);
    if (!actual || actual.get()!=static_cast<const upstream::FieldGlobalNativeReference *>(this))
      return fail(e,"EnemySkill actual ObjectDB Reference differs");
    const auto &b=source_->source_bindings();
    auto id=fields_.find(b.enemy_id_field),weight=fields_.find(b.enemy_weight_field),
         cooldown=fields_.find(b.enemy_cooldown_field),left=fields_.find(b.enemy_remaining_field);
    if (id==fields_.end() || weight==fields_.end() || cooldown==fields_.end() ||
        left==fields_.end() || id->second.kind!=4 || weight->second.kind!=2 ||
        cooldown->second.kind!=2 || left->second.kind!=2)
      return fail(e,"EnemySkill actual complete source fields rejected");
    out={id->second.string,weight->second.integer,cooldown->second.integer};
    remaining=left->second.integer;e.clear();return true;
  }
};

// The source global LOAD characters cursor uses this concrete factory and the
// same globalData objects. No copied Session or separate gameplay state exists.
class PodunkCharacterLoadHost final {
  const upstream::FieldCharacterLoadData *data_=nullptr;
  PodunkGlobalDataHost *owner_=nullptr;
  upstream::FieldGlobalRegistry *registry_=nullptr;
  upstream::FieldCharacterLoadRuntime loader_;
  bool failed_=false;
  std::map<upstream::FieldObjectId,std::weak_ptr<PodunkItemObject>> items_;
  std::map<upstream::FieldObjectId,std::weak_ptr<PodunkEnemySkillObject>> skills_;
  static bool fail(std::string &e,const char *s) {e=s;return false;}
public:
  PodunkCharacterLoadHost()=default;
  PodunkCharacterLoadHost(const PodunkCharacterLoadHost &)=delete;
  PodunkCharacterLoadHost &operator=(const PodunkCharacterLoadHost &)=delete;
  bool initialize(const upstream::FieldCharacterLoadData &data,
                  PodunkGlobalDataHost &owner,upstream::FieldGlobalRegistry &registry,
                  upstream::SourceRandom &random,std::vector<uint32_t> &uids,
                  upstream::LoadRngClockProvider clock,
                  upstream::FieldGlobalDataStatSignal signal,std::string &e) {
    using namespace upstream;
    if (data_ || failed_ || !data.valid() || !owner.runtime().data() ||
        !owner.runtime().ready_complete() || !clock || !signal ||
        !owner.runtime().initialize_character_load(data,e))
      return fail(e,"Character LOAD complete source owner/Ready absent");
    data_=&data;owner_=&owner;registry_=&registry;
    FieldCharacterLoadHost host;
    host.read=[this](uint32_t id,auto &out,auto &error) {
      return owner_->runtime().read_character_load(id,out,error);
    };
    host.publish=[this](const auto &value,auto &error) {
      return owner_->runtime().publish_character_load(value,error);
    };
    host.new_inventory=[this](uint32_t declaration,auto &out,auto &error) {
      return owner_->runtime().new_character_inventory(declaration,out,error);
    };
    host.new_item=[this](FieldObjectId inventory,const auto &value,
                        FieldGlobalDataItemReference &out,auto &error) {
      uint32_t declaration=0;
      auto *definitions=owner_->items_cache().definitions();
      std::shared_ptr<PodunkItemObject> item;
      if (!definitions || !owner_->runtime().character_inventory_owner(inventory,declaration,error) ||
          !PodunkInventoryHost::construct_loaded_global_item(*data_,*definitions,*registry_,declaration,value,item,error))
        return false;
      out={registry_,item->object,declaration,item->value,item};out.source_owner=item;
      items_[item->object]=item;return true;
    };
    host.new_enemy_skill=[this](const auto &value,FieldCharacterOwnedReference &out,auto &error) {
      std::shared_ptr<PodunkEnemySkillObject> skill;
      if (!PodunkEnemySkillObject::construct(*data_,*registry_,value,skill,error)) return false;
      out={registry_,skill->binding().object,skill,skill};skills_[out.object]=skill;return true;
    };
    host.stat_changed=std::move(signal);
    if (!loader_.initialize(data,*owner.runtime().data(),owner.runtime(),registry,
          owner.yaml_caches(),owner.items_cache(),random,uids,std::move(clock),std::move(host),e)) {
      failed_=true;return false;
    }
    return true;
  }
  // Call from the complete global LOAD source cursor after actual scalar and
  // KEY/STORAGE assignments. This never reports full global LOAD completion.
  bool load_cold_default(std::string &e) {
    if (!data_ || failed_) return fail(e,"Character LOAD actual cursor unavailable");
    if (!loader_.load_cold_default(e)) {failed_=true;return false;}return true;
  }
  bool characters_complete() const {return !failed_ && loader_.characters_complete();}
  const upstream::FieldCharacterLoadRuntime &runtime() const {return loader_;}
};
} // namespace encore::ctr
