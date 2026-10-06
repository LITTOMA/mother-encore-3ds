#pragma once
#include "encore/field_global_registry.hpp"
#include <functional>
#include <map>
#include <utility>

namespace encore::upstream {
using FieldFlagDictionary = std::vector<std::pair<std::string, bool>>;
struct FieldFlagProjection {
  FieldFlagDictionary normal, objects, seen;
};
struct FieldFlagSourceProfile {
  std::string source;
  FieldFlagProjection flags;
};
class FieldGlobalFlagsData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const std::array<uint8_t, 20> &pin() const { return pin_; }
  const std::array<uint8_t, 32> &content_hash() const { return content_; }
  const std::string &owner_source() const { return owner_source_; }
  const FieldFlagDictionary &constructor() const { return constructor_; }
  const std::vector<FieldFlagSourceProfile> &profiles() const {
    return profiles_;
  }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  std::array<uint8_t, 20> pin_{};
  std::array<uint8_t, 32> content_{};
  std::string owner_source_;
  FieldFlagDictionary constructor_;
  std::vector<FieldFlagSourceProfile> profiles_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
// Owns exactly the original three boolean dictionaries. The surrounding
// actual globaldata constructor owns this instance; it does not approve any
// other autoload constructor/Ready, party, scene or complete save schema.
class FieldGlobalFlagsRuntime {
public:
  using Emit = std::function<bool(std::string &)>;
  bool initialize(const FieldGlobalFlagsData &, const FieldGlobalExternalSpec &,
                  Emit, std::string &);
  bool read(bool object, std::string_view, bool &present, bool &value,
            std::string &) const;
  bool seen(std::string_view, bool &, std::string &) const;
  bool mark_seen(std::string_view, std::string &);
  // Raw source dictionary mutation used by FieldSceneHost, whose source
  // wrapper emits separately. It never emits from a setter twice.
  bool write(bool object, std::string_view, bool, std::string &);
  bool set_normal(std::string_view, bool, bool emit, std::string &);
  bool set_object(std::string_view, bool, bool emit, std::string &);
  bool emit(std::string &);
  // global._load_save preserves registered normal names/default-false and
  // replaces the object/seen dictionaries; extra saved normal keys are a
  // specifically audited source no-op, not a generic unknown-content ignore.
  bool load_source(const FieldFlagProjection &, std::string &);
  bool load_profile(size_t source_profile, std::string &);
  bool encode_save(std::vector<uint8_t> &, std::string &) const;
  bool restore_save(const uint8_t *, size_t, std::string &);
  const FieldFlagProjection &state() const { return state_; }
  uint64_t revision() const { return revision_; }

private:
  const FieldGlobalFlagsData *data_ = nullptr;
  FieldFlagProjection state_;
  Emit emit_;
  std::array<uint8_t, 20> admitted_pin_{};
  std::array<uint8_t, 32> admitted_content_{};
  uint64_t revision_ = 0;
  bool poisoned_ = false;
  bool available(std::string &) const;
  bool assign(FieldFlagDictionary &, std::string_view, bool, std::string &);
};
} // namespace encore::upstream
