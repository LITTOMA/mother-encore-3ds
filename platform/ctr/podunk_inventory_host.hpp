#pragma once
#include "audio_player.hpp"
#include "encore/field_global_registry.hpp"
#include "encore/field_openable_door.hpp"
#include "field_goods_renderer.hpp"
#include <memory>
#include <set>

namespace encore::ctr {
// A live source Item Reference, not a Node or Resource and not a saved UID.
// The source inventory and global.item keep the same object alive. Its handle
// comes from the process ObjectDB allocator; UID zero remains a valid item.
class PodunkInventoryHost;
struct PodunkItemObject {
private:
  friend class PodunkInventoryHost;
  upstream::FieldGlobalRegistry *allocator_ = nullptr;
  upstream::FieldObjectId allocated_ = 0;

public:
  PodunkItemObject() = default;
  PodunkItemObject(const PodunkItemObject &) = delete;
  PodunkItemObject &operator=(const PodunkItemObject &) = delete;
  PodunkItemObject(PodunkItemObject &&) = delete;
  PodunkItemObject &operator=(PodunkItemObject &&) = delete;
  upstream::FieldObjectId object = 0;
  upstream::FieldOwnedItem value{};
  uint32_t owner = 0;
  // The process registry must outlive every source Reference. IDs are never
  // reused; releasing the last actual Ref removes its reserved ObjectDB slot.
  ~PodunkItemObject() {
    if (allocator_ && allocated_) {
      std::string error;
      allocator_->retire_object(allocated_, error);
    }
  }
};
struct PodunkInventoryOwnerObject {
  uint32_t owner = 0;
  upstream::FieldObjectId object = 0;
};
enum class PodunkInventoryAudioMode { Required, UnavailableNDSP };
struct PodunkInventorySnapshot {
  std::array<uint8_t, 20> source_pin{};
  std::array<uint8_t, 32> inventory_identity{};
  upstream::FieldInventoryState state;
  std::vector<PodunkInventoryOwnerObject> owners;
  std::vector<std::shared_ptr<PodunkItemObject>> items;
  std::shared_ptr<PodunkItemObject> context;
};
struct PodunkInventoryOps {
  // Must check actual globaldata.characters.ninten/key_items/storage owners.
  // It is invoked again on activation, never replaced with legacy save flags.
  std::function<bool(const upstream::FieldInventoryData &,
                     const std::vector<PodunkInventoryOwnerObject> &,
                     const upstream::FieldInventoryState &, std::string &)>
      owners;
  std::function<bool(upstream::FieldObjectId)> owner_exists;
  std::function<bool(const upstream::FieldGoodsTextContext &, std::string &,
                     std::string &)>
      format;
  std::function<bool(const std::string &, std::string &, std::string &)>
      key_name;
  std::function<bool(bool, std::string &)> description;
};
class PodunkInventoryHost final {
  const upstream::FieldInventoryData *data_ = nullptr;
  const upstream::FieldItemDefinitions *defs_ = nullptr;
  const upstream::FieldGoodsData *goods_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::FieldInventoryRuntime inventory_;
  upstream::FieldItemDefinitionsRuntime definitions_;
  upstream::FieldGoodsMenu menu_;
  FieldGoodsRenderer renderer_;
  ItemDetailsRenderer details_;
  SourceFontRenderer *main_font_ = nullptr, *number_font_ = nullptr;
  BattleRenderer *main_text_ = nullptr, *number_text_ = nullptr;
  const upstream::LocaleSelection *locale_ = nullptr;
  AudioPlayer *audio_ = nullptr;
  const upstream::AudioBank *audio_bank_ = nullptr;
  PodunkInventoryAudioMode audio_mode_ = PodunkInventoryAudioMode::Required;
  PodunkInventoryOps ops_;
  std::vector<PodunkInventoryOwnerObject> owners_;
  std::map<upstream::FieldObjectId, std::shared_ptr<PodunkItemObject>> objects_;
  std::shared_ptr<PodunkItemObject> context_;
  std::vector<std::shared_ptr<PodunkItemObject>> shop_items_;
  std::map<std::string, uint32_t> sounds_;
  bool prepared_ = false, graphics_ = false, active_ = false, poisoned_ = false;
  bool external_context_ = false;
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }
  static bool same(const upstream::FieldOwnedItem &a,
                   const upstream::FieldOwnedItem &b) {
    return a.uid == b.uid && a.definition == b.definition &&
           a.doses == b.doses && a.equipped == b.equipped;
  }
  bool live(std::string &e) const {
    if (!active_ || !prepared_ || poisoned_)
      return fail(e, "Podunk owning inventory is not active");
    if (!inventory_.validate(inventory_.state(), e) ||
        !ops_.owners(*data_, owners_, inventory_.state(), e))
      return false;
    for (const auto &o : owners_)
      if (!ops_.owner_exists(o.object))
        return fail(e, "Actual Inventory Reference owner was retired");
    for (const auto &r : inventory_.state().items.inventories)
      for (const auto &i : r.items) {
        auto ref = find_uid(i.uid);
        if (!actual(ref, e) || ref->owner != r.owner || !same(ref->value, i))
          return fail(e, "Live Item Reference and owning inventory diverged");
      }
    return true;
  }
  std::shared_ptr<PodunkItemObject> find_uid(uint32_t uid) const {
    for (const auto &v : objects_)
      if (v.second->value.uid == uid)
        return v.second;
    return {};
  }
  bool actual(const std::shared_ptr<PodunkItemObject> &i,
              std::string &e) const {
    if (!i || !i->object || i->allocator_ != registry_ ||
        i->allocated_ != i->object)
      return fail(e, "Source Item Reference is not owned by actual ObjectDB");
    return true;
  }
  using ObjectTable =
      std::map<upstream::FieldObjectId, std::shared_ptr<PodunkItemObject>>;
  bool plan_objects(const upstream::FieldItemSnapshot &state,
                    const upstream::FieldOwnedItem *context, ObjectTable &out,
                    std::string &e) {
    ObjectTable next = objects_;
    auto stage = [&](const upstream::FieldOwnedItem &i, uint32_t owner) {
      if (!defs_->definition(i.definition))
        return fail(e, "Source Item Reference definition unknown");
      for (const auto &v : next)
        if (v.second->value.uid == i.uid) {
          if (v.second->value.definition != i.definition ||
              !actual(v.second, e))
            return fail(e, "Source UID aliases a different actual Item");
          return true;
        }
      upstream::FieldObjectId id = 0;
      if (!registry_->allocate_object(id, e))
        return false;
      auto ref = std::make_shared<PodunkItemObject>();
      ref->object = ref->allocated_ = id;
      ref->allocator_ = registry_;
      ref->value = i;
      ref->owner = owner;
      next.emplace(id, ref);
      return true;
    };
    for (const auto &r : state.inventories)
      for (const auto &i : r.items)
        if (!stage(i, r.owner))
          return false;
    if (context && context->definition && !stage(*context, 0))
      return false;
    out.swap(next);
    e.clear();
    return true;
  }
  bool reconcile(const upstream::FieldItemSnapshot &s,
                 const upstream::FieldItemResult *result, std::string &e) {
    std::set<uint32_t> uids;
    for (const auto &o : s.inventories)
      for (const auto &i : o.items) {
        if (!uids.insert(i.uid).second || !defs_->definition(i.definition))
          return fail(e, "Item ObjectDB source snapshot rejected");
        auto ref = find_uid(i.uid);
        if (!ref)
          return fail(
              e, "Source actual Item creation was not staged before commit");
        else if (ref->value.definition != i.definition || !actual(ref, e))
          return fail(e, "Item live UID changed source definition");
        ref->value = i;
        ref->owner = o.owner;
      }
    if (result && result->kind != upstream::FieldItemResultKind::None) {
      auto ref = find_uid(result->item.uid);
      if (!ref)
        return fail(e, "Source result Item Reference construction missing");
      if (!uids.count(result->item.uid)) {
        ref->value = result->item;
        ref->owner = 0;
      }
      context_ = ref;
      external_context_ = false;
    } else if (inventory_.has_context() && !external_context_) {
      auto ref = find_uid(inventory_.context().uid);
      if (!ref)
        return fail(e, "Original global.item actual Reference missing");
      ref->value = inventory_.context();
      context_ = ref;
    }
    for (auto it = objects_.begin(); it != objects_.end();) {
      if (!uids.count(it->second->value.uid)) {
        it->second->owner = 0;
        // Other actual source Reference owners may retain the object. The
        // ObjectDB owner decides retirement; this store never reuses handles.
        if (it->second != context_ && it->second.use_count() == 1) {
          it = objects_.erase(it);
          continue;
        }
      }
      ++it;
    }
    e.clear();
    return true;
  }
  bool sync(std::string &e) {
    if (!reconcile(inventory_.state().items, nullptr, e)) {
      poisoned_ = true;
      return false;
    }
    return true;
  }
  bool sound(const std::string &source, std::string &e) {
    auto it = sounds_.find(source);
    if (it != sounds_.end() && audio_ &&
        audio_mode_ == PodunkInventoryAudioMode::UnavailableNDSP &&
        !audio_->available() && R_FAILED(audio_->dsp_result())) {
      e.clear();
      return true;
    }
    if (it == sounds_.end() || !audio_ || !audio_->prepared(it->second))
      return fail(e, "Goods audio source was not admitted/prepared");
    return audio_->play(it->second, AudioLane::Effect, e);
  }

  bool prepare_bank_asset(uint32_t id, std::string &e) {
    if (audio_mode_ == PodunkInventoryAudioMode::UnavailableNDSP) {
      if (audio_->available() || !R_FAILED(audio_->dsp_result()))
        return fail(e, "Audio unavailable mode became inconsistent");
      e.clear();
      return true;
    }
    const auto *playing = audio_->checked_bank();
    upstream::AudioAsset expected{}, actual{};
    if (!playing || !audio_bank_->find(id, expected) ||
        !playing->find(id, actual) ||
        expected.source_path != actual.source_path ||
        expected.source_sha256 != actual.source_sha256 ||
        expected.pcm_crc != actual.pcm_crc ||
        expected.pcm_bytes != actual.pcm_bytes ||
        expected.sample_rate != actual.sample_rate ||
        expected.frames != actual.frames ||
        expected.channels != actual.channels)
      return fail(e, "Goods audio owners do not share checked asset identity");
    return audio_->prepare(id, e);
  }

public:
  PodunkInventoryHost() = default;
  PodunkInventoryHost(const PodunkInventoryHost &) = delete;
  PodunkInventoryHost &operator=(const PodunkInventoryHost &) = delete;
  ~PodunkInventoryHost() { free_graphics(); }
  // One actual source Item.new cursor; UID fallback/default RNG has already
  // executed at the caller's source cursor. Never draws/reassigns that UID.
  static bool construct_source_item(
      const upstream::FieldInventoryData &data,
      const upstream::FieldItemDefinitions &defs,
      upstream::FieldGlobalRegistry &registry, uint32_t owner,
      const upstream::FieldOwnedItem &item,
      std::shared_ptr<PodunkItemObject> &out, std::string &e) {
    const auto *o = data.owner(owner);
    const auto *d = defs.definition(item.definition);
    if (!data.valid() || !data.bind_definitions(defs, e) || !o || !d ||
        !item.doses || item.doses > d->doses ||
        (o->role == 1 && !d->keyitem()) ||
        (o->role == 0 && d->keyitem()) ||
        (item.equipped && (o->role != 0 || d->slot.empty())))
      return fail(e, "Source Item constructor definition/doses/owner rejected");
    upstream::FieldObjectId id = 0;
    if (!registry.allocate_object(id, e))
      return false;
    auto actual = std::make_shared<PodunkItemObject>();
    actual->object = actual->allocated_ = id;
    actual->allocator_ = &registry;
    actual->owner = owner;
    actual->value = item;
    out = std::move(actual);
    e.clear();
    return true;
  }
  static bool check_source_item(const std::shared_ptr<PodunkItemObject> &item,
                                const upstream::FieldGlobalRegistry &registry) {
    return item && item->object && item->allocated_ == item->object &&
           item->allocator_ == &registry;
  }
  // Called at the original source reconstruction cursor, on caller-owned RNG
  // and ledger candidates. Only KEY/STORAGE/Ninten relative order is admitted;
  // this is not full global.LOAD and draws no inactive/GodStorage items.
  static bool source_initial_domain(const upstream::FieldInventoryData &data,
                                    const upstream::FieldItemDefinitions &defs,
                                    upstream::SourceRandom &random,
                                    std::vector<uint32_t> &ledger,
                                    upstream::LoadRngClockProvider clock,
                                    upstream::FieldInventoryState &out,
                                    std::string &e) {
    upstream::FieldInventoryState seed;
    seed.level = data.initial_level();
    if (!data.valid() || !data.bind_definitions(defs, e) || !data.role(0))
      return false;
    for (const auto &o : data.owners())
      seed.items.inventories.push_back({o.id, o.role, {}});
    seed.items.party_order = {data.role(0)->id};
    upstream::FieldInventoryRuntime candidate;
    if (!candidate.initialize(data, defs, seed, e) ||
        !candidate.source_initial(random, ledger, std::move(clock), e))
      return false;
    out = candidate.state();
    e.clear();
    return true;
  }
  // Call only at the source Item construction cursor, after
  // source_initial_domain or the admitted restore below. It allocates actual
  // Reference handles from the SAME process ObjectDB counter. No UID
  // offset/hash identities are used.
  static bool construct_domain_objects(
      const upstream::FieldInventoryData &data,
      const upstream::FieldItemDefinitions &defs,
      const upstream::FieldInventoryState &state,
      const std::vector<PodunkInventoryOwnerObject> &owners,
      upstream::FieldGlobalRegistry &registry,
      const std::function<bool(upstream::FieldObjectId)> &owner_exists,
      PodunkInventorySnapshot &out, std::string &e) {
    upstream::FieldInventoryRuntime check;
    if (!owner_exists || !check.initialize(data, defs, state, e) ||
        owners.size() != data.owners().size())
      return false;
    std::set<uint32_t> ownerids;
    std::set<upstream::FieldObjectId> ownerobjects;
    for (const auto &o : owners)
      if (!data.owner(o.owner) || !o.object || !owner_exists(o.object) ||
          !ownerids.insert(o.owner).second ||
          !ownerobjects.insert(o.object).second)
        return fail(e, "Source construction actual inventory owners absent");
    PodunkInventorySnapshot next;
    next.source_pin = data.source_pin();
    next.inventory_identity = data.identity();
    next.state = state;
    next.owners = owners;
    for (auto role : {1u, 2u, 0u})
      for (const auto &r : state.items.inventories)
        if (r.role == role)
          for (const auto &i : r.items) {
            upstream::FieldObjectId id = 0;
            if (!registry.allocate_object(id, e))
              return false;
            auto ref = std::make_shared<PodunkItemObject>();
            ref->object = ref->allocated_ = id;
            ref->allocator_ = &registry;
            ref->value = i;
            ref->owner = r.owner;
            next.items.push_back(ref);
          }
    out = std::move(next);
    e.clear();
    return true;
  }
  static bool restore_domain(const upstream::FieldInventoryData &data,
                             const upstream::FieldItemDefinitions &defs,
                             const uint8_t *bytes, size_t size,
                             upstream::SourceRandom &random,
                             std::vector<uint32_t> &ledger,
                             upstream::LoadRngClockProvider clock,
                             upstream::FieldInventoryState &out,
                             std::string &e) {
    upstream::FieldInventoryState seed;
    seed.level = data.initial_level();
    if (!data.valid() || !data.role(0))
      return false;
    for (const auto &o : data.owners())
      seed.items.inventories.push_back({o.id, o.role, {}});
    seed.items.party_order = {data.role(0)->id};
    upstream::FieldInventoryRuntime candidate;
    if (!candidate.initialize(data, defs, seed, e) ||
        !candidate.restore(bytes, size, random, ledger, std::move(clock), e))
      return false;
    out = candidate.state();
    e.clear();
    return true;
  }
  bool prepare(const upstream::FieldInventoryData &data,
               const upstream::FieldItemDefinitions &defs,
               const upstream::FieldGoodsData &goods,
               const PodunkInventorySnapshot &snapshot,
               upstream::SourceRandom &random, std::vector<uint32_t> &ledger,
               upstream::LoadRngClockProvider clock,
               upstream::FieldGlobalRegistry &registry, AudioPlayer &audio,
               const upstream::AudioBank &bank,
               PodunkInventoryAudioMode audio_mode, PodunkInventoryOps ops,
               std::string &e) {
    if (data_ || snapshot.source_pin != data.source_pin() ||
        snapshot.inventory_identity != data.identity() ||
        !goods.bind_inventory(data, e) || !data.bind_definitions(defs, e) ||
        !ops.owners || !ops.owner_exists || !ops.format || !ops.key_name ||
        !ops.description || !clock)
      return fail(
          e, "Podunk actual owning inventory snapshot/host identity rejected");
    if (!inventory_.initialize(data, defs, snapshot.state, e) ||
        !ops.owners(data, snapshot.owners, snapshot.state, e))
      return false;
    data_ = &data;
    defs_ = &defs;
    goods_ = &goods;
    registry_ = &registry;
    if (!bank.count() ||
        (audio_mode != PodunkInventoryAudioMode::Required &&
         audio_mode != PodunkInventoryAudioMode::UnavailableNDSP) ||
        (audio_mode == PodunkInventoryAudioMode::UnavailableNDSP &&
         (audio.available() || !R_FAILED(audio.dsp_result()))))
      return fail(e,
                  "Goods audio mode lacks actual NDSP unavailability evidence");
    audio_ = &audio;
    audio_bank_ = &bank;
    audio_mode_ = audio_mode;
    ops_ = std::move(ops);
    owners_ = snapshot.owners;
    std::set<uint32_t> ownerids, uids;
    std::set<upstream::FieldObjectId> objectids;
    for (const auto &o : owners_)
      if (!data.owner(o.owner) || !o.object || !ops_.owner_exists(o.object) ||
          !ownerids.insert(o.owner).second ||
          !objectids.insert(o.object).second)
        return fail(e, "Inventory actual owner ObjectDB mapping rejected");
    if (owners_.size() != data.owners().size())
      return fail(e, "Inventory actual owner mapping incomplete");
    for (const auto &row : snapshot.state.items.inventories)
      for (const auto &i : row.items) {
        auto ref = std::find_if(
            snapshot.items.begin(), snapshot.items.end(),
            [&](const auto &r) { return r && r->value.uid == i.uid; });
        if (ref == snapshot.items.end() || !same((*ref)->value, i) ||
            (*ref)->owner != row.owner || !uids.insert(i.uid).second ||
            !objectids.insert((*ref)->object).second || !actual(*ref, e))
          return fail(
              e, "Inventory supplied actual Item Reference mapping rejected");
        objects_.emplace((*ref)->object, *ref);
      }
    if (objects_.size() != snapshot.items.size())
      return fail(e, "Inventory detached/foreign Item Reference supplied");
    if (snapshot.context) {
      if (!actual(snapshot.context, e) ||
          !defs_->definition(snapshot.context->value.definition))
        return fail(e, "Source global.item Reference proof rejected");
      auto owned = find_uid(snapshot.context->value.uid);
      if (owned && owned != snapshot.context)
        return fail(e, "Source Item Reference UID alias is unsupported");
      context_ = snapshot.context;
      objects_.emplace(context_->object, context_);
    }
    auto item_host = inventory_.definitions_host();
    auto commit = item_host.commit;
    item_host.commit = [this, commit](const auto &before, const auto &after,
                                      const auto &r, std::string &error) {
      ObjectTable staged;
      if (!plan_objects(after, &r.item, staged, error) ||
          !commit(before, after, r, error))
        return false;
      objects_.swap(staged);
      if (!reconcile(after, &r, error)) {
        poisoned_ = true;
        return false;
      }
      return true;
    };
    if (!definitions_.initialize(defs, random, ledger, std::move(clock),
                                 std::move(item_host), e))
      return false;
    upstream::FieldGoodsHost host;
    host.bind = [this](const auto &g, const auto &i, std::string &error) {
      return &g == goods_ && &i == data_ && g.bind_inventory(i, error);
    };
    host.format = [this](const auto &c, std::string &out, std::string &error) {
      return ops_.format(c, out, error);
    };
    host.key_name = [this](const auto &k, std::string &out,
                           std::string &error) {
      return ops_.key_name(k, out, error);
    };
    host.sound = [this](const auto &s, std::string &error) {
      return sound(s, error);
    };
    host.description = [this](bool v, std::string &error) {
      return ops_.description(v, error);
    };
    if (!menu_.initialize(goods, inventory_, definitions_, std::move(host), e))
      return false;
    prepared_ = true;
    e.clear();
    return true;
  }
  bool prepare_audio(std::string &e) {
    const auto *bank = audio_bank_;
    if (!prepared_ || !bank)
      return fail(e, "Goods real checked NDSP bank absent");
    std::set<std::string> paths;
    for (uint32_t i = 0; i < 5; ++i)
      paths.insert(goods_->sound(i));
    for (uint32_t i = 0; i < 3; ++i)
      paths.insert(data_->sound(i));
    std::map<std::string, uint32_t> found;
    for (const auto &p : paths) {
      std::array<uint8_t, 32> hash{};
      if (p.empty() || !goods_->source_hash(p, hash))
        return fail(e, "Goods source audio proof absent");
      uint32_t id = 0;
      for (uint32_t i = 0; i < bank->count(); ++i) {
        auto a = bank->asset(i);
        if (a.source_path == p || (a.source_path.substr(0, 6) == "res://" &&
                                   a.source_path.substr(6) == p)) {
          if (id || a.source_sha256 != hash)
            return fail(e, "Goods audio source SHA/uniqueness mismatch");
          id = a.stable_id;
        }
      }
      if (!id || !prepare_bank_asset(id, e))
        return fail(e, "Goods original sound missing from checked bank");
      found.emplace(p, id);
    }
    sounds_.swap(found);
    e.clear();
    return true;
  }
  bool load_graphics(upstream::ItemDetailsView details,
                     upstream::FieldEquipmentView field,
                     const FieldEquipmentRenderer &art,
                     SourceFontRenderer &main_font,
                     SourceFontRenderer &number_font, BattleRenderer &main_text,
                     BattleRenderer &number_text,
                     const upstream::LocaleSelection &locale, const char *root,
                     const std::array<uint32_t, 8> &source,
                     const std::vector<std::array<uint32_t, 8>> &palettes,
                     double threshold, uint32_t base, std::string &e) {
    auto *main = main_font.catalog().selected();
    auto *numbers = number_font.catalog().selected();
    const auto *catalog = locale.catalog();
    const int index = catalog ? catalog->locale_index(locale.code()) : -1;
    const auto authored = std::find_if(
        main_font.catalog().faces().begin(), main_font.catalog().faces().end(),
        [&](const auto &f) {
          return f.source == field.binding(upstream::FieldBinding::MainFont);
        });
    if (!prepared_ || active_ || !field.valid() || !main || !numbers ||
        !catalog || index < 0 ||
        authored == main_font.catalog().faces().end() ||
        main->source != catalog->locales()[size_t(index)].font ||
        numbers->source != field.binding(upstream::FieldBinding::NumberFont) ||
        !details.bind_field_items(*defs_, e))
      return fail(e, "Goods source font/field details owner mismatch");
    if (!main_font.admit_selected_font(e) ||
        !number_font.admit_selected_font(e) ||
        !details_.load_field(details, *defs_, root, e) ||
        !renderer_.load(*goods_, *data_, art, details_, root, source, palettes,
                        threshold, base, e))
      return false;
    main_font_ = &main_font;
    number_font_ = &number_font;
    main_text_ = &main_text;
    number_text_ = &number_text;
    locale_ = &locale;
    main_text_->attach_source_font(main_font_);
    number_text_->attach_source_font(number_font_);
    graphics_ = true;
    e.clear();
    return true;
  }
  bool activate(std::string &e) {
    if (!prepared_ || !graphics_ || sounds_.empty() || poisoned_ || active_ ||
        !ops_.owners(*data_, owners_, inventory_.state(), e))
      return fail(e, "Goods actual source owners/preparation not admitted");
    for (const auto &o : owners_)
      if (!ops_.owner_exists(o.object))
        return fail(
            e,
            "Actual Inventory Reference owner was retired before activation");
    for (const auto &v : objects_)
      if (!actual(v.second, e))
        return false;
    active_ = true;
    e.clear();
    return true;
  }
  bool open(const std::string &nickname, bool description, bool chinese,
            std::string &e) {
    if (!live(e) || !locale_ || chinese != (locale_->code() == "zh_Hans_CN"))
      return fail(e, "Goods locale does not match actual font owner");
    details_.set_locale(chinese ? "zh_Hans_CN" : "en");
    details_.set_nickname(nickname);
    return menu_.open(nickname, description, chinese, e);
  }
  bool input(upstream::FieldGoodsInput input, std::string &e) {
    return live(e) && menu_.input(input, e) && sync(e);
  }
  bool idle(double dt, std::string &e) {
    return live(e) && menu_.idle(dt, e) && sync(e);
  }
  bool draw(float width, float height, std::string &e, float left = 0,
            float top = 0) const {
    if (!live(e) || !graphics_ || !main_font_ || !number_font_ || !locale_ ||
        !main_font_->catalog().selected())
      return false;
    const int locale_index = locale_->catalog()->locale_index(locale_->code());
    if (locale_index < 0 ||
        main_font_->catalog().selected()->source !=
            locale_->catalog()->locales()[size_t(locale_index)].font)
      return fail(
          e, "Goods active font was changed outside checked locale boundary");
    if (!renderer_.draw(menu_, *main_text_, *number_text_, width, height, left,
                        top)) {
      e = renderer_.error();
      return false;
    }
    e.clear();
    return true;
  }
  std::vector<upstream::FieldGoodsEvent> take_events() {
    return menu_.take_events();
  }
  bool visible() const { return menu_.visible(); }
  PodunkInventoryAudioMode audio_mode() const { return audio_mode_; }
  void deactivate() { active_ = false; }
  const upstream::FieldInventoryState &state() const {
    return inventory_.state();
  }
  const upstream::FieldGoodsMenu &menu() const { return menu_; }
  upstream::FieldItemDefinitionsRuntime *items() {
    return active_ && !poisoned_ ? &definitions_ : nullptr;
  }
  bool encode_save(std::vector<uint8_t> &bytes, std::string &e) const {
    return live(e) && inventory_.encode_save(bytes, e);
  }
  bool snapshot(PodunkInventorySnapshot &out, std::string &e) const {
    if (!live(e))
      return false;
    PodunkInventorySnapshot next;
    next.source_pin = data_->source_pin();
    next.inventory_identity = data_->identity();
    next.state = inventory_.state();
    next.owners = owners_;
    for (const auto &r : next.state.items.inventories)
      for (const auto &i : r.items) {
        auto ref = find_uid(i.uid);
        if (!actual(ref, e))
          return false;
        next.items.push_back(ref);
      }
    next.context = context_;
    out = std::move(next);
    e.clear();
    return true;
  }
  bool object_exists(upstream::FieldObjectId id) const {
    return id && objects_.count(id) != 0;
  }
  std::shared_ptr<PodunkItemObject> object(upstream::FieldObjectId id) const {
    auto it = objects_.find(id);
    return it == objects_.end() ? nullptr : it->second;
  }
  upstream::FieldObjectId global_item() const {
    return context_ ? context_->object : 0;
  }
  bool item_object(uint32_t uid, upstream::FieldObjectId &out,
                   std::string &e) const {
    if (!live(e))
      return false;
    auto ref = find_uid(uid);
    if (!actual(ref, e))
      return false;
    out = ref->object;
    e.clear();
    return true;
  }
  upstream::FieldShopHost
  shop_host(const upstream::FieldShopData &shop,
            std::function<bool(const std::string &, std::string &)> close) {
    auto host = inventory_.shop_host(
        shop, [this](const auto &s, std::string &e) { return sound(s, e); },
        std::move(close));
    auto pending =
        std::make_shared<std::vector<std::shared_ptr<PodunkItemObject>>>();
    host.source_items = [this, &shop, pending](const auto &items,
                                               std::string &e) {
      if (!live(e) || !shop_items_.empty() ||
          items.size() != shop.offers().size())
        return fail(e, "Shop source preview construction scope rejected");
      pending->clear();
      std::vector<std::shared_ptr<PodunkItemObject>> next;
      std::set<uint32_t> preview_uids;
      for (size_t n = 0; n < items.size(); ++n) {
        const auto &i = items[n];
        const auto *p = shop.policy(shop.offers()[n]);
        if (!p || i.definition != p->id || i.doses != p->doses || i.equipped ||
            find_uid(i.uid) || !preview_uids.insert(i.uid).second)
          return fail(e, "Shop source preview UID/offer/order rejected");
        upstream::FieldObjectId id = 0;
        if (!registry_->allocate_object(id, e))
          return false;
        auto ref = std::make_shared<PodunkItemObject>();
        ref->object = ref->allocated_ = id;
        ref->allocator_ = registry_;
        ref->value = i;
        next.push_back(ref);
      }
      pending->swap(next);
      e.clear();
      return true;
    };
    auto commit = host.commit;
    host.commit = [this, commit, pending](const auto &before, const auto &after,
                                          const auto &r, std::string &e) {
      if (!live(e)) {
        pending->clear();
        return false;
      }
      ObjectTable staged;
      if (r.action == upstream::FieldShopAction::Ready) {
        staged = objects_;
        for (const auto &ref : *pending)
          staged.emplace(ref->object, ref);
      } else if (!plan_objects(after.items,
                               r.context_item.definition ? &r.context_item
                                                         : nullptr,
                               staged, e))
        return false;
      if (!commit(before, after, r, e)) {
        pending->clear();
        return false;
      }
      objects_.swap(staged);
      if (r.action != upstream::FieldShopAction::Ready)
        external_context_ = false;
      if (r.action == upstream::FieldShopAction::Ready)
        shop_items_.swap(*pending);
      pending->clear();
      return sync(e);
    };
    return host;
  }
  // Invoke only after the actual source ShopUI queue_free has flushed.
  bool release_shop_items(std::string &e) {
    if (!live(e))
      return false;
    shop_items_.clear();
    return sync(e);
  }
  bool prepare_shop_audio(const upstream::FieldShopData &shop, std::string &e) {
    if (!prepared_ || shop.source_pin() != data_->source_pin() || !shop.valid())
      return fail(e, "Shop source identity not admitted");
    const auto *bank = audio_bank_;
    if (!bank)
      return fail(e, "Actual Shop audio bank absent");
    auto next = sounds_;
    for (uint32_t n = 0; n < 5; ++n) {
      const auto &p = shop.sound(n);
      std::array<uint8_t, 32> hash{};
      if (!defs_->source_hash(p, hash))
        return fail(e, "Shop source sound proof absent");
      uint32_t id = 0;
      for (uint32_t i = 0; i < bank->count(); ++i) {
        auto a = bank->asset(i);
        if (a.source_path == p || (a.source_path.substr(0, 6) == "res://" &&
                                   a.source_path.substr(6) == p)) {
          if (id || a.source_sha256 != hash)
            return fail(e, "Shop source audio SHA/uniqueness mismatch");
          id = a.stable_id;
        }
      }
      if (!id || !prepare_bank_asset(id, e))
        return fail(e, "Actual Shop audio resource unavailable");
      next[p] = id;
    }
    sounds_.swap(next);
    e.clear();
    return true;
  }
  bool bind_payphone(const upstream::FieldPayphoneData &phone,
                     upstream::FieldPayphoneHost &host, std::string &e) {
    if (!live(e) || !inventory_.bind_payphone_inventory(phone, host, e))
      return false;
    auto reduce = host.reduce_or_drop;
    host.reduce_or_drop = [this, reduce](auto uid, auto doses,
                                         std::string &error) {
      return live(error) && reduce(uid, doses, error) && sync(error);
    };
    return true;
  }
  // The same actual owner inventory is used by OpenableDoor; source item
  // argument is a live Item handle, not a uint32 saved UID.
  bool inventory_find(upstream::FieldObjectId owner, std::string_view name,
                      upstream::FieldObjectId &out, std::string &e) const {
    if (!live(e))
      return false;
    out = 0;
    auto o = std::find_if(owners_.begin(), owners_.end(),
                          [&](const auto &v) { return v.object == owner; });
    if (o == owners_.end())
      return fail(e, "Door uses a foreign actual inventory");
    for (const auto &r : inventory_.state().items.inventories)
      if (r.owner == o->owner)
        for (const auto &i : r.items)
          if (defs_->definition(i.definition)->item_name == name)
            return item_object(i.uid, out, e);
    e.clear();
    return true;
  }
  bool bind_openable_inventory(const upstream::FieldOpenableDoorData &door,
                               upstream::FieldOpenableHost &host,
                               std::string &e) {
    if (!live(e) || !door.valid() ||
        door.identity().upstream_commit != data_->source_pin() || !host.bind)
      return fail(e, "Openable actual inventory/source host absent");
    for (const auto &r : door.records()) {
      if (r.key == door.none() || r.key == door.bash())
        continue;
      const auto *d = defs_->definition(r.key);
      std::array<uint8_t, 32> a{}, b{};
      if (!d || !door.source_hash(d->source, a) ||
          !defs_->source_hash(d->source, b) || a != b)
        return fail(e, "Openable key source definition not admitted");
    }
    auto candidate = host;
    auto bind = host.bind;
    candidate.bind = [this, &door, bind](const auto &d, std::string &error) {
      return &d == &door && live(error) && bind(d, error);
    };
    candidate.party_inventories = [this](std::vector<uint64_t> &party,
                                         uint64_t &keys, std::string &error) {
      if (!live(error))
        return false;
      party.clear();
      keys = 0;
      for (auto id : inventory_.state().items.party_order) {
        auto o = std::find_if(owners_.begin(), owners_.end(),
                              [&](const auto &v) { return v.owner == id; });
        if (o == owners_.end())
          return fail(error, "Actual party inventory reference absent");
        party.push_back(o->object);
      }
      for (const auto &o : owners_)
        if (data_->owner(o.owner)->role == 1)
          keys = o.object;
      return keys ? true : fail(error, "Actual key inventory reference absent");
    };
    candidate.inventory_find = [this](uint64_t owner, std::string_view name,
                                      uint64_t &item, std::string &error) {
      return inventory_find(owner, name, item, error);
    };
    candidate.item_name = [this](uint64_t id, std::string &name,
                                 std::string &error) {
      auto ref = object(id);
      if (!live(error) || !actual(ref, error))
        return false;
      auto *d = defs_->definition(ref->value.definition);
      if (!d)
        return fail(error, "Actual Item data absent");
      name = d->item_name;
      error.clear();
      return true;
    };
    candidate.item_owner = [this](uint64_t id, uint64_t &owner,
                                  std::string &error) {
      if (!live(error))
        return false;
      auto ref = object(id);
      if (!actual(ref, error))
        return false;
      owner = 0;
      for (auto role : {1u, 0u})
        for (const auto &r : inventory_.state().items.inventories)
          if (r.role == role)
            for (const auto &i : r.items)
              if (i.uid == ref->value.uid) {
                auto o = std::find_if(
                    owners_.begin(), owners_.end(),
                    [&](const auto &v) { return v.owner == r.owner; });
                if (o == owners_.end())
                  return fail(error, "Actual Item owner absent");
                owner = o->object;
                error.clear();
                return true;
              }
      error.clear();
      return true;
    };
    candidate.inventory_drop = [this](uint64_t owner, uint64_t id,
                                      bool &removed, std::string &error) {
      removed = false;
      if (!live(error))
        return false;
      auto ref = object(id);
      if (!actual(ref, error))
        return false;
      auto o = std::find_if(owners_.begin(), owners_.end(),
                            [&](const auto &v) { return v.object == owner; });
      if (o == owners_.end() || ref->owner != o->owner)
        return fail(error,
                    "Actual Door key reference belongs to another inventory");
      upstream::FieldItemResult result;
      if (!definitions_.drop(ref->value.uid, result, error))
        return false;
      removed = result.kind == upstream::FieldItemResultKind::Dropped;
      return true;
    };
    candidate.set_global_item = [this](uint64_t id, std::string &error) {
      if (!live(error))
        return false;
      auto ref = object(id);
      if (!actual(ref, error))
        return false;
      context_ = ref;
      external_context_ = true;
      error.clear();
      return true;
    };
    host = std::move(candidate);
    e.clear();
    return true;
  }
  void free_graphics() {
    renderer_.free();
    details_.free();
    graphics_ = false;
    active_ = false;
  }
};
} // namespace encore::ctr
