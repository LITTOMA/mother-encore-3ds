#pragma once
#include "encore/global_data_constructor.hpp"
#include "podunk_global_data_host.hpp"
#include "podunk_native_root.hpp"
namespace encore::ctr {
// Immutable source resources must outlive this Registry-owned actual singleton.
struct PodunkGlobalDataSingletonData {
  const upstream::FieldGlobalDataData *members = nullptr;
  const upstream::GlobalDataConstructorData *constructor = nullptr;
  const upstream::FieldGlobalFlagsData *flags = nullptr;
  const upstream::GlobalYamlCachesData *caches = nullptr;
  const upstream::GlobalPackedDirectoryData *directories = nullptr;
  const upstream::GlobalYamlFileData *files = nullptr;
  const upstream::FieldItemDefinitions *items = nullptr;
};
struct PodunkGlobalDataSingletonServices {
  upstream::FieldGlobalRegistry *registry = nullptr;
  PodunkNativeRoot *root = nullptr;
  upstream::SourceRandom *random = nullptr;
  std::vector<uint32_t> *uid_ledger = nullptr;
  upstream::LoadRngClockProvider clock;
  upstream::FieldGlobalFlagsRuntime::Emit flags_updated;
  upstream::GlobalYamlCachesRuntime::Warning warning;
  std::function<bool(std::string &, std::string &)> locale;
  // Actual Node signal channel; no fallback success receipt is supplied.
  std::function<bool(upstream::FieldObjectId, std::string_view, std::string &)> signal;
};
// Source constructor and original native root traversal drive this same owner.
// It supplies no global LOAD completion, scene entry or inferred source callback.
class PodunkGlobalDataSingleton final : public upstream::FieldGlobalExternalObject,
                                       public PodunkExternalNodeLifecycle,
                                       public PodunkUiGlobalDataOwner {
public:
  bool initialize(upstream::FieldObjectId, const upstream::FieldGlobalExternalSpec &,
                  PodunkGlobalDataSingletonData, PodunkGlobalDataSingletonServices,
                  std::string &);
  upstream::FieldGlobalExternalBinding binding() const override { return binding_; }
  bool state(upstream::FieldGlobalExternalState &, std::string &) const override;
  bool menu_flavor(std::string &, std::string &) const override;
  bool stage_parent(upstream::FieldObjectId, std::string &) override;
  bool native_notification(upstream::FieldTreePhase, std::string &) override;
  bool enter(upstream::FieldObjectId, std::string &) override;
  bool ready(std::string &) override;
  bool exit(std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool persist_append(upstream::FieldObjectId, std::string &) override;
  bool assign_stable_canvas(upstream::FieldObjectId, std::string &) override;
  PodunkGlobalDataHost &host() { return host_; }
  const PodunkGlobalDataHost &host() const { return host_; }
private:
  PodunkGlobalDataHost host_;
  upstream::FieldGlobalExternalBinding binding_{};
  PodunkGlobalDataSingletonData data_;
  PodunkGlobalDataSingletonServices services_;
  bool initialized_ = false, parented_ = false, unparented_ = false, failed_ = false;
  bool fail(std::string &, const char *) const;
  bool poison(std::string &);
  bool actual(std::string &) const;
};
// Composes with the other concrete source autoload factories. The Registry
// owns the returned singleton; this factory and its services must outlive it.
class PodunkGlobalDataFactory final : public PodunkUiExternalFactory {
public:
  bool initialize(const upstream::FieldGlobalRegistryData &,
                  PodunkGlobalDataSingletonData, PodunkGlobalDataSingletonServices,
                  std::string &);
  bool construct(upstream::FieldObjectId, const upstream::FieldGlobalExternalSpec &,
                 std::unique_ptr<upstream::FieldGlobalExternalObject> &,
                 std::string &) override;
  // Call after Registry::construct_autoload published this actual object and
  // before NativeRoot stages it. Construction never invents parent/Ready.
  bool register_native_root(std::string &);
  PodunkGlobalDataSingleton *globaldata() const;
private:
  const upstream::FieldGlobalRegistryData *registry_data_ = nullptr;
  PodunkGlobalDataSingletonData data_;
  PodunkGlobalDataSingletonServices services_;
  PodunkGlobalDataSingleton *object_ = nullptr;
  upstream::FieldObjectId id_ = 0;
};
} // namespace encore::ctr
