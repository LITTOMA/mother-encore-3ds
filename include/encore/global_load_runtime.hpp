#pragma once
#include "encore/field_ui_manager.hpp"
#include "encore/global_load.hpp"
namespace encore::upstream {
struct GlobalLoadObjectArray {
  std::vector<FieldObjectId> values;
};
// Implemented by the actual global.gd owner. A LOAD caller must be at its
// source method cursor, after the real _ready prefix/settings/player work.
// This interface is not a bootstrap receipt factory or an inferred Ready.
class GlobalLoadGlobalOwner {
public:
  virtual ~GlobalLoadGlobalOwner() = default;
  virtual const FieldGlobalExternalObject &external() const = 0;
  virtual bool admit_cold_load_cursor(const GlobalLoadData &,
                                      const FieldGlobalDataRuntime &,
                                      std::string &) const = 0;
  virtual bool party_array(std::string_view source_member,
                           std::shared_ptr<const GlobalLoadObjectArray> &,
                           std::string &) const = 0;
  virtual bool clear_party(std::string_view, std::string &) = 0;
  virtual bool append_party(std::string_view, FieldObjectId, std::string &) = 0;
};
struct GlobalLoadHost {
  GlobalLoadGlobalOwner *global = nullptr;
  FieldUiManagerRuntime *ui = nullptr;
  GlobalYamlFileHost *files = nullptr;
  // Consumes this actually read/merged Dictionary on the same eight Objects;
  // the independent character resource supplies the checked cold capability.
  std::function<bool(const std::shared_ptr<GlobalYamlValue> &, std::string &)>
      characters;
  std::function<bool()> characters_complete;
  std::function<bool(FieldObjectId, const FieldOwnedItem &,
                     FieldGlobalDataItemReference &, std::string &)>
      new_item;
};
class GlobalLoadRuntime {
public:
  bool initialize(const GlobalLoadData &, FieldGlobalDataRuntime &,
                  FieldGlobalRegistry &, GlobalItemCache &, SourceRandom &,
                  std::vector<uint32_t> &, LoadRngClockProvider, GlobalLoadHost,
                  std::string &);
  bool load_cold_default(std::string &);
  bool complete() const;
  bool poisoned() const { return poisoned_; }
  size_t cursor() const { return cursor_; }

private:
  const GlobalLoadData *data_ = nullptr;
  FieldGlobalDataRuntime *core_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  GlobalItemCache *items_ = nullptr;
  SourceRandom *random_ = nullptr;
  std::vector<uint32_t> *uids_ = nullptr;
  LoadRngClockProvider clock_;
  GlobalLoadHost host_;
  std::array<uint8_t, 32> ir_{};
  size_t cursor_ = 0;
  bool begun_ = false, complete_ = false, poisoned_ = false;
  bool available(std::string &) const;
  bool owner_cursor(std::string &) const;
  bool assignment(const GlobalLoadAssignment &,
                  const std::shared_ptr<GlobalYamlValue> &, std::string &);
  bool inventory(size_t, const std::shared_ptr<GlobalYamlValue> &,
                 std::string &);
};
} // namespace encore::upstream
