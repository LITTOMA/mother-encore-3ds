#include "encore/house_status_effects.hpp"
#include <algorithm>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool equal(const GlobalYamlValue &a, const GlobalYamlValue &b,
           uint32_t depth = 0) {
  if (depth > 32 || a.kind != b.kind)
    return false;
  switch (a.kind) {
  case 0:
    return true;
  case 1:
    return a.boolean == b.boolean;
  case 2:
    return a.integer == b.integer;
  case 4:
    return a.string == b.string;
  case 5:
    if (a.array.size() != b.array.size())
      return false;
    for (size_t i = 0; i < a.array.size(); ++i)
      if (!a.array[i] || !b.array[i] ||
          !equal(*a.array[i], *b.array[i], depth + 1))
        return false;
    return true;
  case 6:
    if (a.dictionary.size() != b.dictionary.size())
      return false;
    for (size_t i = 0; i < a.dictionary.size(); ++i) {
      const auto &x = a.dictionary[i];
      const auto &y = b.dictionary[i];
      if (x.first != y.first || !x.second || !y.second ||
          !equal(*x.second, *y.second, depth + 1))
        return false;
    }
    return true;
  default:
    return false;
  }
}
} // namespace
bool HouseStatusEffectsRuntime::initialize(const HouseStatusEffectsData &d,
                                           const FieldGlobalDataRuntime &c,
                                           const FieldGlobalRegistry &r,
                                           Getter g, std::string &e) {
  if (data_ || !d.valid() || !d.bridge() || !d.bridge()->characters() ||
      !c.constructor_complete() || c.registry() != &r ||
      !c.character_load_bound_to(*d.bridge()->characters()) || r.poisoned() ||
      !g)
    return fail(e, "Status effects actual source owner unavailable");
  data_ = &d;
  core_ = &c;
  registry_ = &r;
  ir_ = d.ir_sha256();
  getter_ = std::move(g);
  e.clear();
  return true;
}
bool HouseStatusEffectsRuntime::binds(const FieldGlobalDataRuntime &c,
                                      const FieldGlobalRegistry &r) const {
  return data_ && data_->valid() && data_->ir_sha256() == ir_ && core_ == &c &&
         registry_ == &r && !r.poisoned() && c.registry() == &r &&
         c.character_load_bound_to(*data_->bridge()->characters());
}
bool HouseStatusEffectsRuntime::boolean_effect(FieldObjectId character,
                                               HouseStatusBoolean role,
                                               bool &out,
                                               std::string &e) const {
  if (!core_ || !registry_ || !binds(*core_, *registry_) ||
      (role != HouseStatusBoolean::Sweat &&
       role != HouseStatusBoolean::Incapacitated))
    return fail(e, "Status effects unknown query/unbound actual owner");
  FieldGlobalDataObject body;
  if (!registry_->object_exists(character) ||
      !core_->constructed_body_alive(character) ||
      !core_->read_constructed_object(character, body, e))
    return false;
  FieldGlobalDataMemberState status;
  if (!core_->read_constructed_member(
          character, data_->bridge()->characters()->source_bindings().status,
          status, e))
    return false;
  if (status.kind != 5 || !status.value || status.value->kind != 5 ||
      !status.value->array.empty() ||
      (status.reference_array && !status.reference_array->values.empty()))
    return fail(e, "Status effects source Array is not an actual Node Array");
  bool next = false;
  if (!status.node_array) {
    if (!status.references.empty())
      return fail(e, "Status effects missing owning Node Array");
    out = false;
    e.clear();
    return true;
  }
  if (!status.node_array->values.empty() &&
      (body.declaration != data_->bridge()->declaration() || body.role != 0))
    return fail(e, "Status effects populated Character outside reviewed House "
                   "continuation");
  if (status.references.size() != status.node_array->values.size())
    return fail(e, "Status effects source Array representations differ");
  std::set<FieldObjectId> unique;
  for (size_t i = 0; i < status.node_array->values.size(); ++i) {
    auto id = status.node_array->values[i];
    if (!id || !unique.insert(id).second || status.references[i].second != id)
      return fail(e, "Status effects source Node identity/order rejected");
    HouseStatusActualData actual;
    if (!getter_(character, id, actual, e))
      return false;
    auto p =
        std::find_if(data_->policies().begin(), data_->policies().end(),
                     [&](const auto &v) { return v.id == actual.ailment; });
    auto tree = registry_->tree_owner(id);
    auto node = tree ? tree->state(id) : nullptr;
    auto descriptor = tree ? tree->descriptor(id) : nullptr;
    const auto &b = data_->bridge()->status();
    std::array<uint8_t, 32> proof{};
    if (p == data_->policies().end() || actual.registry != registry_ ||
        actual.tree != tree || actual.object != id || !node || !node->alive ||
        node->queued || !node->bound || !descriptor ||
        descriptor->script != b.script ||
        descriptor->native_class != b.native ||
        !data_->bridge()->source_hash(b.script, proof) ||
        descriptor->script_sha != proof || node->binding.family != 0x454e0060 ||
        node->binding.capability != 1 || actual.times != b.times ||
        !actual.data || !p->expected || !equal(*actual.data, *p->expected))
      return fail(e, "Status effects actual Node/cache value differs from "
                     "reviewed source");
    auto cases = actual.data->get(data_->effects_key());
    if (!cases)
      continue;
    if (cases->kind != 6 || cases->dictionary.size() != 1 ||
        cases->dictionary.front().first != data_->any_case() ||
        !cases->dictionary.front().second ||
        cases->dictionary.front().second->kind != 6)
      return fail(e, "Status effects unknown conditional branch");
    auto value = cases->dictionary.front().second->get(data_->query(role));
    if (value) {
      if (value->kind != 1)
        return fail(e,
                    "Status effects boolean query returned unsupported value");
      next = next || value->boolean;
    }
  }
  out = next;
  e.clear();
  return true;
}
} // namespace encore::upstream
