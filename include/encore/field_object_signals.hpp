#pragma once
#include "encore/field_global_registry.hpp"
#include <memory>
#include <set>

namespace encore::upstream {
// Native connection flags, independent of game content and resource families.
enum FieldSignalFlags : uint32_t {
  FieldSignalDeferred = 1, FieldSignalPersist = 2,
  FieldSignalOneShot = 4, FieldSignalReferenceCounted = 8
};
// The query resolves the real emitter's native/source declaration. A method
// call uses the same ObjectDB dispatch and MessageQueue as the scene owner.
class FieldObjectSignals {
public:
  using DeclarationQuery = std::function<bool(
      FieldObjectId, std::string_view, uint32_t &arguments, std::string &)>;
  using EmissionQuery=std::function<bool(FieldObjectId,std::string_view,size_t,std::string&)>;
  bool initialize(FieldGlobalRegistry &, DeclarationQuery, std::string &);
  bool bind_emission_policy(EmissionQuery,std::string&);
  bool connect(FieldObjectId, std::string_view, FieldObjectId, std::string_view,
               uint32_t flags, std::vector<FieldDeferredValue> binds,
               std::string &);
  bool disconnect(FieldObjectId, std::string_view, FieldObjectId,
                  std::string_view, std::string &);
  bool connected(FieldObjectId, std::string_view, FieldObjectId,
                 std::string_view, bool &, std::string &) const;
  bool emit(FieldObjectId, std::string_view,
            const std::vector<FieldDeferredValue> &, std::string &);
  bool block(FieldObjectId, bool, std::string &);
  // Called by the real ObjectDB deletion owner; never deletes a live object.
  bool release(FieldObjectId, std::string &);
  // Native Node duplication supplies actual original -> allocated clone IDs.
  // Only persistent connections are copied; other source Ref binds stay shared.
  bool duplicate_persistent(const std::map<FieldObjectId,FieldObjectId> &,
                            std::string &);
  const FieldGlobalRegistry *registry() const { return registry_; }
private:
  struct Target {
    FieldObjectId object = 0;
    const std::string *method = nullptr;
    bool operator<(const Target &b) const {
      return object != b.object ? object < b.object
                               : std::less<const std::string *>{}(method,b.method);
    }
  };
  struct Slot {
    uint32_t flags = 0;
    int64_t references = 0;
    std::vector<FieldDeferredValue> binds;
  };
  using Key = std::pair<FieldObjectId,std::string>;
  using Slots = std::map<Target,Slot>;
  bool declaration(FieldObjectId,std::string_view,uint32_t &,std::string &) const;
  const std::string *intern(std::string_view);
  FieldGlobalRegistry *registry_ = nullptr;
  DeclarationQuery declaration_;
  EmissionQuery emission_;
  std::map<std::string,std::unique_ptr<std::string>> names_;
  std::map<Key,Slots> signals_;
  std::set<FieldObjectId> blocked_;
};
} // namespace encore::upstream
