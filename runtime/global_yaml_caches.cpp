#include "encore/global_yaml_caches.hpp"
#include "encore/field_global_data.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
std::shared_ptr<GlobalYamlValue> clone(const GlobalYamlValue &source) {
  auto out = std::make_shared<GlobalYamlValue>();
  out->kind = source.kind;
  out->boolean = source.boolean;
  out->integer = source.integer;
  out->real = source.real;
  out->string = source.string;
  for (const auto &x : source.array)
    out->array.push_back(clone(*x));
  for (const auto &x : source.dictionary)
    out->dictionary.emplace_back(x.first, clone(*x.second));
  return out;
}
} // namespace
bool GlobalYamlValue::truthy() const {
  switch (kind) {
  case 0:
    return false;
  case 1:
    return boolean;
  case 2:
    return integer != 0;
  case 3:
    return real != 0;
  case 4:
    return !string.empty();
  case 5:
    return !array.empty();
  case 6:
    return !dictionary.empty();
  default:
    return false;
  }
}
std::shared_ptr<GlobalYamlValue>
GlobalYamlValue::get(std::string_view key) const {
  if (kind != 6)
    return {};
  for (const auto &x : dictionary)
    if (x.first == key)
      return x.second;
  return {};
}
bool GlobalYamlCachesRuntime::initialize(
    const GlobalYamlCachesData &data, FieldObjectId owner,
    const FieldGlobalExternalSpec &spec, FieldGlobalRegistry &registry,
    const FieldGlobalDataRuntime &core, GlobalYamlItemsPort items,
    FlagsReady flags, Warning warning, std::string &e) {
  std::array<uint8_t, 32> source{};
  if (data_ || !data.valid() || !owner || spec.role != 3 ||
      spec.script != data.owner_source() ||
      spec.script_sha != data.identity().source_sha256 ||
      spec.identity.upstream_commit != data.identity().upstream_commit ||
      registry.poisoned() || core.globaldata_object() != owner ||
      !core.data() || core.data()->owner_source() != data.owner_source() ||
      !core.data()->source_hash(data.owner_source(), source) ||
      source != spec.script_sha || !items.actual_cache ||
      items.actual_cache->owner() != owner ||
      !items.actual_cache->definitions() ||
      items.actual_cache->definitions()->source_pin() !=
          data.identity().upstream_commit ||
      !items.insert || !items.finish || !flags || !warning)
    return fail(
        e, "Global YAML cache actual source constructor/ownership rejected");
  auto expected = data.expected_paths(4);
  const auto &definitions = *items.actual_cache->definitions();
  if (expected.size() != definitions.definitions().size() ||
      !definitions.global_constructor_scope() ||
      !items.actual_cache->insertion_order().empty() ||
      items.actual_cache->directory_admitted())
    return fail(e,
                "Global YAML cache independent full Items Directory mismatch");
  for (const auto &x : expected) {
    const FieldItemDefinition *item = nullptr;
    for (const auto &candidate : definitions.definitions())
      if (candidate.source == x.first)
        item = &candidate;
    std::array<uint8_t, 32> digest{};
    if (!item || !definitions.source_hash(x.first, digest) ||
        digest != x.second)
      return fail(e, "Global YAML cache Items source closure/proof rejected");
  }
  GlobalYamlFlagsReceipt receipt;
  if (!flags(receipt, e) || receipt.owner != owner ||
      receipt.source_cursor != 1 || !receipt.data || !receipt.runtime ||
      !receipt.data->valid() ||
      receipt.data->pin() != data.identity().upstream_commit ||
      receipt.data->owner_source() != data.owner_source() ||
      !receipt.data->source_hash(data.owner_source(), source) ||
      source != spec.script_sha ||
      receipt.runtime->state().normal != receipt.data->constructor() ||
      !receipt.runtime->state().objects.empty() ||
      !receipt.runtime->state().seen.empty())
    return fail(
        e, "Global YAML cache source preceding _init_flags cursor incomplete");
  // Real script construction executes within an allocated Registry slot before
  // external publication. Requiring object_exists here would reorder _init.
  data_ = &data;
  registry_ = &registry;
  owning_core_ = &core;
  owner_ = owner;
  admitted_ir_ = data.ir_sha256();
  items_ = std::move(items);
  warning_ = std::move(warning);
  e.clear();
  return true;
}
bool GlobalYamlCachesRuntime::available(std::string &e) const {
  std::array<uint8_t, 32> source{};
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      !registry_ || registry_->poisoned() || poisoned_ || !owning_core_ ||
      owning_core_->globaldata_object() != owner_ || !owning_core_->data() ||
      owning_core_->data()->owner_source() != data_->owner_source() ||
      !owning_core_->data()->source_hash(data_->owner_source(), source) ||
      source != data_->identity().source_sha256 || !items_.actual_cache ||
      items_.actual_cache->owner() != owner_)
    return fail(e, "Global YAML cache actual source owner unavailable");
  return true;
}
bool GlobalYamlCachesRuntime::poison(const char *s, std::string &e) {
  poisoned_ = true;
  return fail(e, s);
}
bool GlobalYamlCachesRuntime::begin_directory(uint32_t role, std::string &e) {
  if (!available(e))
    return false;
  if (open_ || role != cursor_ || role >= data_->policies().size())
    return fail(e, "Global YAML cache source _init Directory order rejected");
  open_ = true;
  e.clear();
  return true;
}
bool GlobalYamlCachesRuntime::insert_loaded_yaml(
    const std::string &source, const std::array<uint8_t, 32> &digest,
    std::string &e) {
  if (!available(e))
    return false;
  if (!open_)
    return fail(e, "Global YAML cache requires actual open Directory cursor");
  const GlobalYamlCacheRecord *record = nullptr;
  for (const auto &x : data_->records())
    if (x.source == source)
      record = &x;
  if (!record || record->role != cursor_ || record->source_sha != digest ||
      inserted_.count(source))
    return fail(e,
                "Global YAML cache source Directory entry/duplicate rejected");
  if (cursor_ == 4) {
    const auto &cache = *items_.actual_cache;
    const FieldItemDefinition *item = nullptr;
    for (const auto &x : cache.definitions()->definitions())
      if (x.source == source)
        item = &x;
    const auto size = cache.insertion_order().size();
    if (!item || !items_.insert(source, digest, e))
      return poison("Global YAML cache actual Items insertion failed", e);
    if (cache.insertion_order().size() != size + 1 ||
        cache.insertion_order().back() != item->id)
      return poison(
          "Global YAML cache callback did not mutate actual Items owner", e);
  } else {
    if (!record->parsed)
      return fail(e, "Global YAML cache original parsed source absent");
    values_.emplace(source, clone(*record->parsed));
  }
  inserted_.insert(source);
  order_[cursor_].push_back(record->name);
  e.clear();
  return true;
}
bool GlobalYamlCachesRuntime::finish_directory(uint32_t role, std::string &e) {
  if (!available(e))
    return false;
  if (!open_ || role != cursor_ || role >= 6)
    return fail(e,
                "Global YAML cache Directory completion source order rejected");
  const auto expected = data_->expected_paths(role);
  for (const auto &x : expected)
    if (!inserted_.count(x.first))
      return fail(
          e,
          "Global YAML cache actual Directory ended with missing source YAML");
  if (order_[role].size() != expected.size())
    return fail(e, "Global YAML cache actual Directory closure count rejected");
  if (role == 4) {
    const auto &proof = data_->policies()[role].closure;
    if (!items_.finish(expected, proof, e) ||
        !items_.actual_cache->directory_admitted() ||
        items_.actual_cache->directory_proof() != proof)
      return poison(
          "Global YAML cache Items source Directory completion not executed",
          e);
  }
  open_ = false;
  ++cursor_;
  e.clear();
  return true;
}
bool GlobalYamlCachesRuntime::init_caches_complete() const {
  std::string e;
  return available(e) && cursor_ == data_->policies().size() && !open_ &&
         inserted_.size() == data_->records().size() &&
         items_.actual_cache->directory_admitted() &&
         items_.actual_cache->directory_proof() == data_->policies()[4].closure;
}
const std::vector<std::string> &
GlobalYamlCachesRuntime::insertion_order(uint32_t role) const {
  static const std::vector<std::string> empty;
  return role < 6 ? order_[role] : empty;
}
bool GlobalYamlCachesRuntime::call(std::string_view method,
                                   const std::vector<std::string> &args,
                                   std::shared_ptr<GlobalYamlValue> &result,
                                   std::string &e) {
  if (!available(e))
    return false;
  const GlobalYamlGetter *getter = nullptr;
  for (const auto &x : data_->getters())
    if (x.method == method)
      getter = &x;
  if (!getter || args.size() != (getter->action == 3 ? 0u : 1u))
    return fail(e, "Global YAML cache unknown getter/argument shape rejected");
  auto out = std::make_shared<GlobalYamlValue>();
  if (getter->action == 3) {
    out->kind = 5;
    for (const auto &name : order_[getter->role]) {
      auto key = std::make_shared<GlobalYamlValue>();
      key->kind = 4;
      key->string = name;
      out->array.push_back(std::move(key));
    }
  } else {
    const auto &policy = data_->policies()[getter->role];
    auto found = values_.find(policy.directory + args[0] + ".yaml");
    if (getter->action == 2) {
      out->kind = 1;
      out->boolean = found != values_.end() &&
                     (!getter->truthiness || found->second->truthy());
    } else {
      out = found == values_.end() ? std::make_shared<GlobalYamlValue>()
                                   : found->second;
      if (found == values_.end())
        out->kind = policy.root_kind;
      if (out->truthy()) {
        if (!getter->mutation.empty()) {
          auto id = std::make_shared<GlobalYamlValue>();
          id->kind = 4;
          id->string = args[0];
          bool assigned = false;
          for (auto &x : out->dictionary)
            if (x.first == getter->mutation) {
              x.second = id;
              assigned = true;
              break;
            }
          if (!assigned)
            out->dictionary.emplace_back(getter->mutation, std::move(id));
        }
      } else {
        auto warning = getter->warning;
        auto at = warning.find("%s");
        if (at == warning.npos)
          return fail(e, "Global YAML source missing-warning format rejected");
        warning.replace(at, 2, args[0]);
        if (!warning_(warning, e))
          return poison("Global YAML source actual warning emission failed", e);
      }
    }
  }
  result = std::move(out);
  e.clear();
  return true;
}
} // namespace encore::upstream
