#include "encore/field_global_constructor.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool FieldGlobalConstructorRuntime::live(std::string &e) const {
  return data_ && registry_ && owner_ && registry_->object_exists(owner_)
             ? true
             : fail(e, "Global constructor owner not live");
}
bool FieldGlobalConstructorRuntime::initialize(
    const FieldGlobalConstructorData &d, FieldGlobalRegistry &r,
    FieldObjectId owner, FieldObjectId transition, std::string &e) {
  if (data_ || !d.valid() || !owner || !transition || !r.object_exists(owner) ||
      !r.object_exists(transition) || owner == transition)
    return fail(e, "Global fields actual objects missing");
  data_ = &d;
  registry_ = &r;
  owner_ = owner;
  for (const auto &f : d.fields()) {
    auto role = FieldGlobalMemberRole(f.role);
    switch (f.kind) {
    case FieldGlobalLiteralKind::StringArray:
      if (role == FieldGlobalMemberRole::PartySpace)
        party_space_ = std::make_shared<FieldGlobalPartySpaceArray>();
      else
        arrays_[role] = std::make_shared<GlobalLoadObjectArray>();
      break;
    case FieldGlobalLiteralKind::Null:
      objects_[role] = 0;
      break;
    case FieldGlobalLiteralKind::SourceNode:
      objects_[role] = transition;
      break;
    case FieldGlobalLiteralKind::Boolean:
      booleans_[role] = f.boolean;
      break;
    case FieldGlobalLiteralKind::String:
      strings_[role] = f.string_value;
      break;
    default:
      return fail(e, "Global field execution opcode rejected");
    }
  }
  return true;
}
bool FieldGlobalConstructorRuntime::array(
    FieldGlobalMemberRole role,
    std::shared_ptr<const GlobalLoadObjectArray> &out, std::string &e) const {
  if (!live(e))
    return false;
  auto f = arrays_.find(role);
  if (f == arrays_.end())
    return fail(e, "Global source member is not Array");
  out = f->second;
  return true;
}
bool FieldGlobalConstructorRuntime::clear_array(FieldGlobalMemberRole role,
                                                std::string &e) {
  if (!live(e))
    return false;
  auto f = arrays_.find(role);
  if (f == arrays_.end())
    return fail(e, "Global clear unknown Array");
  f->second->values.clear();
  return true;
}
bool FieldGlobalConstructorRuntime::valid_object(FieldGlobalMemberRole role,
                                                 FieldObjectId id,
                                                 std::string &e) const {
  if (!id || !registry_->object_exists(id))
    return fail(e, "Global Array actual ObjectID missing");
  if (role == FieldGlobalMemberRole::Party ||
      role == FieldGlobalMemberRole::PartyNpcs) {
    if (!characters_)
      return fail(e, "Global party Character owner missing");
    const auto &objects = characters_->objects();
    auto f = std::find_if(objects.begin(), objects.end(),
                          [id](const auto &x) { return x.object == id; });
    if (f == objects.end() || f->kind != 1 ||
        f->role != (role == FieldGlobalMemberRole::Party ? 0u : 1u))
      return fail(e, "Global party source Character class differs");
  }
  return true;
}
bool FieldGlobalConstructorRuntime::append_array(FieldGlobalMemberRole role,
                                                 FieldObjectId id,
                                                 std::string &e) {
  if (!live(e))
    return false;
  auto f = arrays_.find(role);
  if (f == arrays_.end() || !valid_object(role, id, e))
    return fail(e, "Global append invalid Array/object");
  f->second->values.push_back(id);
  return true;
}
bool FieldGlobalConstructorRuntime::erase_array_first(FieldGlobalMemberRole role,
                                                      FieldObjectId id,
                                                      std::string &e) {
  if (!live(e)) return false;
  auto f = arrays_.find(role);
  if (f == arrays_.end() || !f->second)
    return fail(e, "Global erase unknown Array");
  // Godot Array.erase does not dereference its Variant element. In particular,
  // source cleanup may erase a node after queue_free has destroyed that node.
  auto at = std::find(f->second->values.begin(), f->second->values.end(), id);
  if (at != f->second->values.end()) f->second->values.erase(at);
  e.clear();
  return true;
}
bool FieldGlobalConstructorRuntime::assign_array(FieldGlobalMemberRole role,
                                                 std::vector<FieldObjectId> v,
                                                 std::string &e) {
  if (!live(e) || arrays_.find(role) == arrays_.end())
    return fail(e, "Global assignment unknown Array");
  for (auto id : v)
    if (!valid_object(role, id, e))
      return false;
  auto root = std::make_shared<GlobalLoadObjectArray>();
  root->values = std::move(v);
  arrays_[role] = std::move(root);
  return true;
}
bool FieldGlobalConstructorRuntime::party_space(
    std::shared_ptr<const FieldGlobalPartySpaceArray> &out,
    std::string &e) const {
  if (!live(e) || !party_space_)
    return fail(e, "Global PartySpace actual Array missing");
  out = party_space_;
  return true;
}
bool FieldGlobalConstructorRuntime::resize_party_space(size_t size,
                                                       std::string &e) {
  if (!live(e) || !party_space_ || size > party_space_->values.max_size())
    return fail(e, "Global PartySpace resize rejected");
  party_space_->values.resize(size);
  return true;
}
bool FieldGlobalConstructorRuntime::push_front_party_space(Vec2 value,
                                                           std::string &e) {
  if (!live(e) || !party_space_ || !std::isfinite(value.x) ||
      !std::isfinite(value.y) ||
      party_space_->values.size() == party_space_->values.max_size())
    return fail(e, "Global PartySpace Vector2 append rejected");
  party_space_->values.insert(party_space_->values.begin(), {true, value});
  return true;
}
bool FieldGlobalConstructorRuntime::pop_back_party_space(
    FieldGlobalPartySpaceValue &out, std::string &e) {
  if (!live(e) || !party_space_)
    return fail(e, "Global PartySpace actual Array missing");
  if (party_space_->values.empty()) {
    out = {};
    return true;
  }
  out = party_space_->values.back();
  party_space_->values.pop_back();
  return true;
}
bool FieldGlobalConstructorRuntime::object(FieldGlobalMemberRole role,
                                           FieldObjectId &out,
                                           std::string &e) const {
  if (!live(e))
    return false;
  auto f = objects_.find(role);
  if (f == objects_.end())
    return fail(e, "Global member not Object");
  out = f->second;
  return true;
}
bool FieldGlobalConstructorRuntime::set_object(FieldGlobalMemberRole role,
                                               FieldObjectId id,
                                               std::string &e) {
  if (!live(e) || objects_.find(role) == objects_.end() ||
      role == FieldGlobalMemberRole::SceneTransition)
    return fail(e, "Global Object assignment rejected");
  if (id && !registry_->object_exists(id))
    return fail(e, "Global Object assignment missing ObjectDB");
  objects_[role] = id;
  return true;
}
bool FieldGlobalConstructorRuntime::boolean(FieldGlobalMemberRole role,
                                            bool &out, std::string &e) const {
  if (!live(e))
    return false;
  auto f = booleans_.find(role);
  if (f == booleans_.end())
    return fail(e, "Global member not boolean");
  out = f->second;
  return true;
}
bool FieldGlobalConstructorRuntime::set_boolean(FieldGlobalMemberRole role,
                                                bool v, std::string &e) {
  if (!live(e) || booleans_.find(role) == booleans_.end())
    return fail(e, "Global boolean assignment rejected");
  booleans_[role] = v;
  return true;
}
bool FieldGlobalConstructorRuntime::string(FieldGlobalMemberRole role,
                                           std::string &out,
                                           std::string &e) const {
  if (!live(e))
    return false;
  auto f = strings_.find(role);
  if (f == strings_.end())
    return fail(e, "Global member not string");
  out = f->second;
  return true;
}
bool FieldGlobalConstructorRuntime::set_string(FieldGlobalMemberRole role,
                                               std::string v, std::string &e) {
  if (!live(e) || strings_.find(role) == strings_.end())
    return fail(e, "Global String assignment rejected");
  strings_[role] = std::move(v);
  return true;
}
bool FieldGlobalConstructorRuntime::bind_characters(
    const FieldGlobalDataRuntime &c, std::string &e) {
  if (!live(e) || !c.globaldata_object() ||
      !registry_->object_exists(c.globaldata_object()) || !c.data() ||
      c.data()->identity().upstream_commit != data_->identity().upstream_commit)
    return fail(e, "Global party globalData owner rejected");
  characters_ = &c;
  return true;
}
bool FieldGlobalConstructorRuntime::party_role(std::string_view name,
                                               FieldGlobalMemberRole &out,
                                               std::string &e) const {
  if (!live(e))
    return false;
  for (auto role :
       {FieldGlobalMemberRole::Party, FieldGlobalMemberRole::PartyNpcs})
    if (data_->member(role)->name == name) {
      out = role;
      return true;
    }
  return fail(e, "Global LOAD unknown party member");
}
bool FieldGlobalConstructorRuntime::party_array(
    std::string_view name, std::shared_ptr<const GlobalLoadObjectArray> &out,
    std::string &e) const {
  FieldGlobalMemberRole role{};
  return party_role(name, role, e) && array(role, out, e);
}
bool FieldGlobalConstructorRuntime::clear_party(std::string_view name,
                                                std::string &e) {
  FieldGlobalMemberRole role{};
  return party_role(name, role, e) && clear_array(role, e);
}
bool FieldGlobalConstructorRuntime::append_party(std::string_view name,
                                                 FieldObjectId id,
                                                 std::string &e) {
  FieldGlobalMemberRole role{};
  return party_role(name, role, e) && append_array(role, id, e);
}
} // namespace encore::upstream
