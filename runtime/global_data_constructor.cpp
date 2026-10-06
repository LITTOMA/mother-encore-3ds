#include "encore/global_data_constructor.hpp"
#include "encore/field_global_flags.hpp"
#include "encore/global_packed_directory.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool equal(const FieldGlobalDataDefault &a, const FieldGlobalDataDefault &b) {
  return a.name == b.name && a.kind == b.kind &&
         a.integer_value == b.integer_value && a.string_value == b.string_value;
}
bool same_spec(const FieldGlobalExternalSpec &a,
               const FieldGlobalExternalSpec &b) {
  return a.identity.scene_id == b.identity.scene_id &&
         a.identity.upstream_commit == b.identity.upstream_commit &&
         a.identity.source_sha256 == b.identity.source_sha256 &&
         a.stable_id == b.stable_id && a.role == b.role && a.name == b.name &&
         a.native_class == b.native_class && a.source == b.source &&
         a.script == b.script && a.source_sha == b.source_sha &&
         a.script_sha == b.script_sha;
}
std::shared_ptr<GlobalYamlValue> clone(const GlobalYamlValue &v) {
  auto p = std::make_shared<GlobalYamlValue>();
  p->kind = v.kind;
  p->boolean = v.boolean;
  p->integer = v.integer;
  p->real = v.real;
  p->string = v.string;
  for (const auto &x : v.array)
    p->array.push_back(clone(*x));
  for (const auto &x : v.dictionary)
    p->dictionary.emplace_back(x.first, clone(*x.second));
  return p;
}
FieldGlobalExternalSpec body_spec(const GlobalDataConstructorData &d,
                                  const GlobalDataConstructorObject &o) {
  FieldGlobalExternalSpec s;
  s.identity = d.identity();
  s.stable_id = o.id;
  s.role = 5;
  s.name = o.name;
  s.native_class = o.native;
  s.source = o.script;
  s.script = o.script;
  d.source_hash(o.script, s.source_sha);
  s.script_sha = s.source_sha;
  s.identity.source_sha256 = s.source_sha;
  return s;
}
class ActualCharacter final : public FieldGlobalNativeObject {
  FieldGlobalDataRuntime *core_;
  FieldGlobalRegistry *registry_;
  FieldGlobalExternalBinding binding_;

public:
  ActualCharacter(FieldGlobalDataRuntime &c, FieldGlobalRegistry &r,
                  FieldObjectId id, FieldGlobalExternalSpec s)
      : core_(&c), registry_(&r), binding_{id, std::move(s), 0x454e0053, 1} {}
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override { return "Object"; }
  const FieldGlobalRegistry *registry() const override { return registry_; }
  bool checked_source_hash(std::string_view p,
                           std::array<uint8_t, 32> &h) const override {
    return core_ && core_->constructor_source_hash(p, h);
  }
  bool alive() const override {
    return core_ && core_->constructed_body_alive(binding_.object);
  }
};
class ActualInventory final : public FieldGlobalNativeReference {
  FieldGlobalDataRuntime *core_;
  FieldGlobalRegistry *registry_;
  FieldGlobalExternalBinding binding_;

public:
  ActualInventory(FieldGlobalDataRuntime &c, FieldGlobalRegistry &r,
                  FieldObjectId id, FieldGlobalExternalSpec s)
      : core_(&c), registry_(&r), binding_{id, std::move(s), 0x454e0053, 1} {}
  ~ActualInventory() override {
    if (registry_) {
      std::string e;
      registry_->retire_object(binding_.object, e);
    }
  }
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override { return "Reference"; }
  const FieldGlobalRegistry *registry() const override { return registry_; }
  bool checked_source_hash(std::string_view p,
                           std::array<uint8_t, 32> &h) const override {
    return core_ && core_->constructor_source_hash(p, h);
  }
};
} // namespace

bool FieldGlobalDataRuntime::constructor_available(std::string &e) const {
  if (!constructor_data_ || !constructor_data_->valid() ||
      constructor_data_->ir_sha256() != constructor_ir_ || !data_ ||
      !data_->valid() || data_->ir_sha256() != admitted_ir_ || !registry_ ||
      registry_->poisoned() || poisoned_)
    return fail(e, "globalData full source constructor unavailable/reloaded");
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::constructor_source_hash(
    std::string_view p, std::array<uint8_t, 32> &out) const {
  std::string e;
  return constructor_available(e) && constructor_data_->source_hash(p, out);
}
bool FieldGlobalDataRuntime::constructed_body_alive(FieldObjectId id) const {
  std::string e;
  return constructor_available(e) &&
         std::any_of(objects_.begin(), objects_.end(),
                     [=](const auto &o) { return o.object == id; });
}
bool FieldGlobalDataRuntime::initialize_constructor(
    const GlobalDataConstructorData &d, std::string &e) {
  if (constructor_data_ || !data_ || poisoned_ || prefix_ ||
      god_storage_complete_ || !d.valid() || !registry_ ||
      d.legacy_ir_sha256() != admitted_ir_ ||
      d.owner_source() != data_->owner_source() ||
      d.identity().upstream_commit != data_->identity().upstream_commit ||
      d.objects().size() != objects_.size())
    return fail(
        e, "globalData full constructor must precede actual caches/LOAD/Ready");
  std::vector<std::vector<FieldGlobalDataDefault>> body_fields;
  for (size_t i = 0; i < objects_.size(); ++i) {
    const auto &o = objects_[i];
    const auto &a = data_->declarations()[i];
    const auto &b = d.objects()[i];
    std::array<uint8_t, 32> x{}, y{};
    if (o.declaration != b.id || a.id != b.id || a.kind != b.kind ||
        a.role != b.role || a.name != b.name || a.native != b.native ||
        a.script != b.script || o.fields.size() > b.defaults.size() ||
        !data_->source_hash(a.script, x) || !d.source_hash(b.script, y) ||
        x != y)
      return fail(
          e, "globalData actual empty Object constructor identity differs");
    for (size_t k = 0; k < o.fields.size(); ++k)
      if (!equal(o.fields[k], b.defaults[k]))
        return fail(e, "globalData source field already modified before "
                       "constructor closure");
    body_fields.push_back(b.defaults);
  }
  std::vector<FieldGlobalDataMemberState> members;
  for (const auto &decl : d.declarations()) {
    FieldGlobalDataMemberState m;
    m.kind = decl.kind;
    m.adapter = decl.adapter;
    m.owner_role = decl.owner_role;
    m.vector = decl.vector;
    if (decl.adapter == 0 || decl.adapter == 6) {
      if (decl.kind <= 6) {
        if (!decl.value)
          return fail(e, "globalData source value missing");
        m.value = clone(*decl.value);
      } else if (decl.kind != 7) {
        return fail(e, "globalData plain source value type unsupported");
      }
    } else if (decl.adapter == 1) {
      for (const auto &r : decl.references) {
        auto o =
            std::find_if(objects_.begin(), objects_.end(), [&](const auto &v) {
              return v.declaration == r.second;
            });
        if (o == objects_.end() || o->kind != 1)
          return fail(e,
                      "globalData character dictionary actual Object absent");
        m.references.emplace_back(r.first, o->object);
      }
    } else if (decl.adapter == 2) {
      auto o =
          std::find_if(objects_.begin(), objects_.end(), [&](const auto &v) {
            return v.declaration == decl.reference_id;
          });
      if (o == objects_.end() || o->kind != 2)
        return fail(e, "globalData declared Inventory Reference absent");
      m.references.emplace_back(decl.name, o->object);
    } else if (decl.adapter != 3 && decl.adapter != 4 && decl.adapter != 5)
      return fail(e, "globalData source member adapter unsupported");
    members.push_back(std::move(m));
  }
  constructor_data_ = &d;
  constructor_ir_ = d.ir_sha256();
  members_ = std::move(members);
  for (size_t i = 0; i < objects_.size(); ++i) {
    objects_[i].fields = std::move(body_fields[i]);
    for (const auto &f : objects_[i].fields)
      if (f.kind == 3 || f.kind == 4) {
        auto value = std::make_shared<GlobalYamlValue>();
        value->kind = f.kind == 3 ? 5 : 6;
        objects_[i].collections.emplace(f.name, std::move(value));
      }
  }
  for (size_t i = 0; i < objects_.size(); ++i) {
    const auto &o = objects_[i];
    const auto &decl = d.objects()[i];
    if (o.kind == 1) {
      auto adapter = std::make_unique<ActualCharacter>(
          *this, *registry_, o.object, body_spec(d, decl));
      const auto spec = adapter->binding().source;
      if (!registry_->publish_native_object(spec, o.object, std::move(adapter),
                                            e)) {
        poisoned_ = true;
        return false;
      }
    } else if (!publish_inventory_body(o.object, e)) {
      poisoned_ = true;
      return false;
    }
  }
  e.clear();
  return true;
}

bool FieldGlobalDataRuntime::publish_inventory_body(FieldObjectId id,
                                                    std::string &e) {
  if (!constructor_available(e))
    return false;
  auto o = std::find_if(objects_.begin(), objects_.end(),
                        [=](const auto &v) { return v.object == id; });
  if (o == objects_.end() || o->kind != 2 ||
      std::any_of(inventory_references_.begin(), inventory_references_.end(),
                  [=](const auto &v) { return v->binding().object == id; }))
    return fail(e, "globalData Inventory actual body absent/repeated");
  const GlobalDataConstructorObject *decl = nullptr;
  for (const auto &v : constructor_data_->objects())
    if (v.kind == 2 && v.role == o->role)
      decl = &v;
  // GodStorage and LOAD NORMAL use the same audited implicit Inventory class.
  if (!decl)
    for (const auto &v : constructor_data_->objects())
      if (v.kind == 2) {
        decl = &v;
        break;
      }
  if (!decl)
    return fail(e, "globalData Inventory source class proof unavailable");
  auto spec = body_spec(*constructor_data_, *decl);
  spec.stable_id = o->declaration;
  if (decl->role != o->role) {
    auto step = std::find_if(
        constructor_data_->ready_steps().begin(),
        constructor_data_->ready_steps().end(),
        [&](const auto &s) { return s.kind == 3 && s.role == o->role; });
    if (step == constructor_data_->ready_steps().end())
      return fail(e, "globalData unknown dynamic Inventory constructor cursor");
    spec.name = step->member;
  }
  auto ref =
      std::make_shared<ActualInventory>(*this, *registry_, id, std::move(spec));
  if (!registry_->publish_native_reference(ref->binding().source, id, ref, e))
    return false;
  inventory_references_.push_back(std::move(ref));
  e.clear();
  return true;
}

bool FieldGlobalDataRuntime::complete_constructor(
    const FieldGlobalFlagsData &f, FieldGlobalFlagsRuntime &flags,
    const GlobalYamlCachesData &c, GlobalYamlCachesRuntime &caches,
    const GlobalPackedDirectoryHost &dirs, GlobalItemCache &items,
    std::string &e) {
  if (!constructor_available(e))
    return false;
  if (constructor_closed_ || source_inside_ || !f.valid() || !c.valid() ||
      c.ir_sha256() != constructor_data_->cache_ir_sha256() ||
      f.pin() != data_->identity().upstream_commit ||
      c.identity().upstream_commit != f.pin() ||
      f.owner_source() != data_->owner_source() ||
      c.owner_source() != f.owner_source() || caches.owner() != owner_ ||
      items.owner() != owner_ || dirs.cache_runtime() != &caches ||
      !dirs.data() || dirs.data()->cache_ir_sha256() != c.ir_sha256() ||
      !dirs.complete() || !caches.init_caches_complete() ||
      !items.directory_admitted() || !items.definitions_loaded() ||
      flags.state().normal != f.constructor() ||
      !flags.state().objects.empty() || !flags.state().seen.empty())
    return fail(e, "globalData flags/six actual Directory constructors not "
                   "complete/same owner");
  std::array<uint8_t, 32> a{}, b{}, v{};
  if (!f.source_hash(f.owner_source(), a) ||
      !c.source_hash(c.owner_source(), b) ||
      !constructor_data_->source_hash(c.owner_source(), v) || a != b || a != v)
    return fail(e, "globalData constructor source closure differs");
  for (const auto &s : constructor_data_->constructor_steps()) {
    if (s.kind == 1)
      continue;
    if (s.kind != 2)
      return fail(e, "globalData unknown constructor execution step");
    auto p = std::find_if(c.policies().begin(), c.policies().end(),
                          [&](const auto &x) { return x.role == s.role; });
    if (p == c.policies().end() || s.member != p->member ||
        s.argument != std::string("res://") + p->directory)
      return fail(e, "globalData actual six Directory policy binding differs");
  }
  constructor_flags_data_ = &f;
  constructor_flags_ = &flags;
  constructor_cache_data_ = &c;
  constructor_caches_ = &caches;
  constructor_directories_ = &dirs;
  constructor_items_ = &items;
  flags_ir_ = f.content_hash();
  cache_ir_ = c.ir_sha256();
  constructor_closed_ = true;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::constructor_complete() const {
  std::string e;
  return constructor_available(e) && constructor_closed_ &&
         constructor_flags_data_ && constructor_flags_data_->valid() &&
         constructor_flags_data_->content_hash() == flags_ir_ &&
         constructor_cache_data_ && constructor_cache_data_->valid() &&
         constructor_cache_data_->ir_sha256() == cache_ir_ &&
         constructor_caches_ && constructor_caches_->owner() == owner_ &&
         constructor_caches_->init_caches_complete() &&
         constructor_directories_ &&
         constructor_directories_->cache_runtime() == constructor_caches_ &&
         constructor_directories_->complete() && constructor_items_ &&
         constructor_items_->owner() == owner_ &&
         constructor_items_->directory_admitted();
}
bool FieldGlobalDataRuntime::source_binding(const FieldGlobalExternalBinding &b,
                                            std::string &e) const {
  if (!constructor_available(e) || b.object != owner_ ||
      b.family != 0x454e0053 || b.capability != 1 ||
      !same_spec(b.source, source_spec_) || !registry_->object_exists(owner_))
    return fail(e,
                "globalData lifecycle actual external source owner mismatch");
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::source_stage_parent(
    const FieldGlobalExternalBinding &b, FieldObjectId p, std::string &e) {
  if (!source_binding(b, e))
    return false;
  FieldObjectId actual = 0;
  std::string lookup;
  bool listed = registry_->resolve_path(registry_->root(), source_spec_.name,
                                        actual, lookup) &&
                actual == owner_;
  if (p) {
    if (p != registry_->root() || !listed || source_parent_ || source_inside_)
      return fail(e,
                  "globalData actual source root child insertion not observed");
  } else if (source_inside_ || !source_parent_ || listed)
    return fail(e, "globalData unparent precedes actual source exit/removal");
  source_parent_ = p;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::source_enter(const FieldGlobalExternalBinding &b,
                                          FieldObjectId p, std::string &e) {
  if (!source_binding(b, e) || !constructor_complete() || source_inside_ ||
      !p || p != source_parent_ || p != registry_->root())
    return fail(
        e, "globalData Enter requires full actual source construction/parent");
  FieldObjectId actual = 0;
  if (!registry_->resolve_path(p, source_spec_.name, actual, e) ||
      actual != owner_)
    return fail(e,
                "globalData actual source parent/child unavailable at Enter");
  source_inside_ = true;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::source_begin_ready(
    const FieldGlobalExternalBinding &b, FieldObjectId p, std::string &e) {
  if (!source_binding(b, e) || !constructor_complete() || !source_inside_ ||
      p != source_parent_ || !p || !source_ready_first_ ||
      source_ready_started_ || source_ready_ || god_storage_complete_)
    return fail(e, "globalData actual first Ready cursor unavailable");
  source_ready_started_ = true;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::source_finish_ready(
    const FieldGlobalExternalBinding &b, FieldObjectId p, std::string &e) {
  if (!source_binding(b, e) || !constructor_complete() || !source_inside_ ||
      p != source_parent_ || !p || !source_ready_started_ || source_ready_ ||
      !god_storage_complete_ || !god_storage_object_ ||
      !registry_->native_reference(god_storage_object_))
    return fail(e,
                "globalData Ready body actual GodStorage Reference incomplete");
  for (const auto &i : god_items_)
    if (!i.actual_owner || !registry_->object_exists(i.object) ||
        !registry_->native_reference(i.object))
      return fail(e, "globalData Ready GodStorage actual Item Reference "
                     "unregistered/dead");
  for (size_t i = 0; i < members_.size(); ++i)
    if (members_[i].adapter == 3) {
      if (constructor_data_->declarations()[i].name != god_storage_member_)
        return fail(e, "globalData Ready GodStorage member source differs");
      members_[i].references = {{god_storage_member_, god_storage_object_}};
    }
  source_ready_ = true;
  source_ready_first_ = false;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::source_exit(const FieldGlobalExternalBinding &b,
                                         FieldObjectId p, std::string &e) {
  if (!source_binding(b, e) || !source_inside_ || p != source_parent_ || !p ||
      (source_ready_started_ && !source_ready_))
    return fail(e, "globalData Exit lacks source membership/completed Ready");
  source_inside_ = false;
  source_ready_ = false;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::source_state(FieldGlobalExternalState &out,
                                          std::string &e) const {
  if (!constructor_available(e))
    return false;
  out = {};
  out.name = source_spec_.name;
  out.parent = source_parent_;
  out.inside = source_inside_;
  out.ready = source_ready_;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::ready_complete() const {
  return constructor_complete() && source_inside_ && source_ready_;
}
bool FieldGlobalDataRuntime::read_global_member(std::string_view name,
                                                FieldGlobalDataMemberState &out,
                                                std::string &e) const {
  if (!constructor_available(e))
    return false;
  const auto &ds = constructor_data_->declarations();
  auto i = std::find_if(ds.begin(), ds.end(),
                        [&](const auto &d) { return d.name == name; });
  if (i == ds.end())
    return fail(e, "globalData unknown source member");
  auto v = members_[size_t(i - ds.begin())];
  if (v.adapter == 3 && god_storage_complete_) {
    if (i->name != god_storage_member_ ||
        !registry_->native_reference(god_storage_object_))
      return fail(e, "globalData actual GodStorage member binding rejected");
    v.references = {{god_storage_member_, god_storage_object_}};
    v.kind = 8;
  } else if (v.adapter == 4) {
    if (!constructor_complete())
      return fail(e, "globalData actual cache member not initialized");
    v.caches = constructor_caches_;
    v.items = constructor_items_;
  } else if (v.adapter == 5) {
    if (!constructor_complete())
      return fail(e, "globalData actual flags member not initialized");
    v.flags = constructor_flags_;
  }
  out = std::move(v);
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::write_global_scalar(std::string_view name,
                                                 const GlobalYamlValue &v,
                                                 std::string &e) {
  if (!constructor_available(e))
    return false;
  const auto &ds = constructor_data_->declarations();
  auto i = std::find_if(ds.begin(), ds.end(),
                        [&](const auto &d) { return d.name == name; });
  if (i == ds.end() || i->constant || i->adapter != 0 || i->kind < 1 ||
      i->kind > 4 || v.kind != i->kind)
    return fail(e, "globalData source scalar assignment type/member rejected");
  auto next = clone(v);
  const auto &p = constructor_data_->text_speed();
  if (!i->setter.empty()) {
    if (i->name != p.member || i->setter != p.setter || v.kind != 3 ||
        !std::isfinite(v.real) || p.speeds.empty() || !p.fallback_divisor ||
        p.speeds.size() / p.fallback_divisor >= p.speeds.size() ||
        p.comparison != 1)
      return fail(e, "globalData unsupported source setter");
    if (v.real <= p.threshold)
      next->real = p.speeds[p.speeds.size() / p.fallback_divisor];
    else {
      double closest = p.closest_initial, delta = p.delta_initial;
      for (double speed : p.speeds) {
        double d = std::abs(speed - v.real);
        if (d < delta) {
          delta = d;
          closest = speed;
        }
      }
      next->real = closest;
    }
  } else if (v.kind == 3 && !std::isfinite(v.real))
    return fail(e, "globalData non-finite scalar rejected");
  members_[size_t(i - ds.begin())].value = std::move(next);
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::menu_flavor(std::string &out,
                                         std::string &e) const {
  FieldGlobalDataMemberState v;
  if (!read_global_member(
          constructor_data_ ? constructor_data_->menu_flavor_member() : "", v,
          e))
    return false;
  if (v.adapter || v.kind != 4 || !v.value)
    return fail(e, "globalData menu flavor actual String unavailable");
  out = v.value->string;
  e.clear();
  return true;
}
bool FieldGlobalDataRuntime::read_constructed_member(
    FieldObjectId id, std::string_view name, FieldGlobalDataMemberState &out,
    std::string &e) const {
  if (!constructor_available(e) || !registry_->object_exists(id))
    return fail(e, "globalData actual member Object unavailable");
  auto o = std::find_if(objects_.begin(), objects_.end(),
                        [=](const auto &v) { return v.object == id; });
  if (o == objects_.end())
    return fail(e, "globalData source body not owned");
  auto field = std::find_if(o->fields.begin(), o->fields.end(),
                            [&](const auto &f) { return f.name == name; });
  if (field == o->fields.end())
    return fail(e, "globalData unknown source Character/Inventory member");
  FieldGlobalDataMemberState value;
  auto scalar = std::make_shared<GlobalYamlValue>();
  if (field->kind == 1) {
    value.kind = scalar->kind = 4;
    scalar->string = field->string_value;
    value.value = scalar;
  } else if (field->kind == 2) {
    value.kind = scalar->kind = 2;
    scalar->integer = field->integer_value;
    value.value = scalar;
  } else if (field->kind == 6) {
    value.kind = scalar->kind = 1;
    scalar->boolean = field->integer_value != 0;
    value.value = scalar;
  } else if (field->kind == 3 || field->kind == 4) {
    value.kind = field->kind == 3 ? 5 : 6;
    auto collection = o->collections.find(field->name);
    if (collection != o->collections.end())
      value.value = collection->second;
    if (o->kind == 2 && field->kind == 3) {
      for (auto item : o->item_objects) {
        if (!registry_->native_reference(item))
          return fail(e, "globalData Inventory member Item Reference dead");
        value.references.emplace_back("", item);
      }
    } else if (!value.value)
      return fail(e, "globalData actual source collection owner unavailable");
  } else if (field->kind == 5) {
    const FieldObjectId ref = o->inventory;
    auto decl =
        std::find_if(constructor_data_->objects().begin(),
                     constructor_data_->objects().end(),
                     [&](const auto &v) { return v.id == o->declaration; });
    if (decl == constructor_data_->objects().end())
      return fail(e, "globalData unknown Object class member binding");
    // Both the typed backing reference and its audited source getter observe
    // this Character's one actual inventory, never a projection/saved ID.
    if (ref && !registry_->native_reference(ref))
      return fail(e, "globalData Character inventory Reference dead");
    value.kind = ref ? 8 : 0;
    if (ref)
      value.references.emplace_back(field->name, ref);
  } else
    return fail(e, "globalData source member native type unsupported");
  out = std::move(value);
  e.clear();
  return true;
}
} // namespace encore::upstream
