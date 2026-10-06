#pragma once
#include "encore/global_ready.hpp"
#include "encore/localization.hpp"
#include "encore/slot_preference.hpp"
#include "encore/startup_settings.hpp"
#include "podunk_global_load_host.hpp"
namespace encore::ctr {
// Existing Main supplies its actual GPU-safe language operation. Readback is
// from that SAME LocaleSelection; a callback merely returning true is rejected.
class PodunkGlobalReadyPreferences {
public:
  using SelectNativeLocale =
      std::function<bool(const std::string &, bool, std::string &)>;
  bool initialize(const upstream::GlobalReadyData &, const char *romfs_root,
                  upstream::FieldGlobalRegistry &,
                  const upstream::NativeInputData &,
                  upstream::NativeInputAdapter &,
                  const upstream::StartupSettingsData &,
                  const upstream::SessionSettings &,
                  const upstream::LocaleCatalog &, upstream::LocaleSelection &,
                  uint32_t max_slots, uint32_t &actual_selected_slot,
                  SelectNativeLocale, std::string &);
  bool localized_inputs(std::string &);
  bool load_settings(std::string &);
  bool inputs_complete() const { return inputs_complete_; }
  bool settings_complete() const { return settings_complete_; }

private:
  const upstream::GlobalReadyData *data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  const upstream::NativeInputData *input_ = nullptr;
  upstream::NativeInputAdapter *adapter_ = nullptr;
  const upstream::StartupSettingsData *settings_ = nullptr;
  const upstream::SessionSettings *session_ = nullptr;
  const upstream::LocaleCatalog *locales_ = nullptr;
  upstream::LocaleSelection *selection_ = nullptr;
  uint32_t max_slots_ = 0;
  uint32_t *selected_slot_ = nullptr;
  SelectNativeLocale select_;
  std::string root_;
  bool inputs_complete_ = false, settings_complete_ = false;
};

enum class PodunkGlobalReadyDomain { ColdSourceBootstrap = 1 };
// This is the actual source method cursor, not a second global object. Use it
// as GlobalLoadHost's owner; external() returns the Registry-owned original.
// Existing House sessions must not call this cold source LOAD.
class PodunkGlobalReady final : public upstream::GlobalLoadGlobalOwner {
public:
  bool initialize(PodunkGlobalReadyDomain, const upstream::GlobalReadyData &,
                  const upstream::GlobalLoadData &,
                  upstream::FieldGlobalConstructorRuntime &,
                  upstream::FieldGlobalDataRuntime &,
                  upstream::FieldGlobalRegistry &,
                  upstream::GlobalLoadGlobalOwner &actual_global,
                  upstream::PlayerInitializationRuntime &,
                  PodunkGlobalLoadHost &, PodunkGlobalReadyPreferences &,
                  std::string &);
  bool source_ready(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                    const upstream::FieldNodeBinding &, std::string &);
  bool binds(const upstream::FieldGlobalConstructorRuntime &,
             const upstream::FieldGlobalRegistry &) const;
  // Bind constructor-live owners now. Actual LOAD initialization is deferred
  // until global.gd's real Ready cursor, after globalData's earlier Ready.
  bool bind_cold_loader(const upstream::FieldCharacterLoadData &,
                        upstream::FieldUiManagerRuntime &,
                        PodunkGlobalDataHost &, upstream::SourceRandom &,
                        std::vector<uint32_t> &, upstream::LoadRngClockProvider,
                        upstream::FieldGlobalDataStatSignal, std::string &);
  const upstream::FieldGlobalExternalObject &external() const override;
  bool admit_cold_load_cursor(const upstream::GlobalLoadData &,
                              const upstream::FieldGlobalDataRuntime &,
                              std::string &) const override;
  bool party_array(std::string_view,
                   std::shared_ptr<const upstream::GlobalLoadObjectArray> &,
                   std::string &) const override;
  bool clear_party(std::string_view, std::string &) override;
  bool append_party(std::string_view, upstream::FieldObjectId,
                    std::string &) override;
  bool complete() const { return complete_ && !poisoned_; }
  bool poisoned() const { return poisoned_; }
  size_t cursor() const { return cursor_; }

private:
  const upstream::GlobalReadyData *data_ = nullptr;
  const upstream::GlobalLoadData *load_data_ = nullptr;
  upstream::FieldGlobalConstructorRuntime *global_ = nullptr;
  upstream::FieldGlobalDataRuntime *characters_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  upstream::GlobalLoadGlobalOwner *actual_ = nullptr;
  upstream::PlayerInitializationRuntime *player_ = nullptr;
  PodunkGlobalLoadHost *load_ = nullptr;
  PodunkGlobalReadyPreferences *preferences_ = nullptr;
  const upstream::FieldCharacterLoadData *characters_data_ = nullptr;
  upstream::FieldUiManagerRuntime *ui_ = nullptr;
  PodunkGlobalDataHost *data_host_ = nullptr;
  upstream::SourceRandom *random_ = nullptr;
  std::vector<uint32_t> *uids_ = nullptr;
  upstream::LoadRngClockProvider clock_;
  upstream::FieldGlobalDataStatSignal stat_signal_;
  upstream::FieldNodeTreeRuntime *ready_tree_ = nullptr;
  size_t cursor_ = 0;
  bool running_ = false, complete_ = false, poisoned_ = false;
  bool owner(std::string &) const;
  bool reject(std::string &, const char *);
};
} // namespace encore::ctr
