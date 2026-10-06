#pragma once
#include "encore/field_global_flags.hpp"
#include "encore/global_item_cache.hpp"
#include <memory>
namespace encore::upstream {
class FieldGlobalDataRuntime;
// Only YAML native data types, not an instruction set or script VM.
struct GlobalYamlValue {
  uint32_t kind = 0;
  bool boolean = false;
  int64_t integer = 0;
  double real = 0;
  std::string string;
  std::vector<std::shared_ptr<GlobalYamlValue>> array;
  std::vector<std::pair<std::string, std::shared_ptr<GlobalYamlValue>>>
      dictionary;
  bool truthy() const;
  std::shared_ptr<GlobalYamlValue> get(std::string_view) const;
};
struct GlobalYamlCachePolicy {
  uint32_t role = 0, root_kind = 0;
  std::string name, member, directory;
  std::array<uint8_t, 32> closure{};
};
struct GlobalYamlCacheRecord {
  uint32_t role = 0;
  std::string source, name;
  std::array<uint8_t, 32> source_sha{};
  std::shared_ptr<const GlobalYamlValue> parsed;
};
struct GlobalYamlGetter {
  std::string method, mutation, warning;
  uint32_t role = 0, action = 0;
  bool truthiness = false;
};
class GlobalYamlCachesData {
public:
  bool load(const uint8_t *, size_t, const FieldGlobalExternalSpec &,
            std::string &);
  bool load_file(const char *, const FieldGlobalExternalSpec &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &owner_source() const { return owner_; }
  const auto &policies() const { return policies_; }
  const auto &records() const { return records_; }
  const auto &getters() const { return getters_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  std::vector<std::pair<std::string, std::array<uint8_t, 32>>>
  expected_paths(uint32_t role) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  std::string owner_;
  std::vector<GlobalYamlCachePolicy> policies_;
  std::vector<GlobalYamlCacheRecord> records_;
  std::vector<GlobalYamlGetter> getters_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct GlobalYamlFlagsReceipt {
  FieldObjectId owner = 0;
  const FieldGlobalFlagsData *data = nullptr;
  const FieldGlobalFlagsRuntime *runtime = nullptr;
  // Actual source _init_flags completion cursor; not a fabricated Ready bit.
  uint32_t source_cursor = 0;
};
struct GlobalYamlItemsPort {
  const GlobalItemCache *actual_cache = nullptr;
  std::function<bool(const std::string &, const std::array<uint8_t, 32> &,
                     std::string &)>
      insert;
  std::function<bool(
      const std::vector<std::pair<std::string, std::array<uint8_t, 32>>> &,
      const std::array<uint8_t, 32> &, std::string &)>
      finish;
};
// Owns five cache values and the original six-directory initialization cursor.
// Caller must execute the actual Directory cursor; canonical resource order is
// only a closure manifest. Finishing initialization does not grant Ready.
class GlobalYamlCachesRuntime {
public:
  using FlagsReady =
      std::function<bool(GlobalYamlFlagsReceipt &, std::string &)>;
  using Warning = std::function<bool(const std::string &, std::string &)>;
  bool initialize(const GlobalYamlCachesData &, FieldObjectId,
                  const FieldGlobalExternalSpec &, FieldGlobalRegistry &,
                  const FieldGlobalDataRuntime &, GlobalYamlItemsPort,
                  FlagsReady, Warning, std::string &);
  bool begin_directory(uint32_t role, std::string &);
  bool insert_loaded_yaml(const std::string &, const std::array<uint8_t, 32> &,
                          std::string &);
  // Only at actual Directory.get_next()=="", after any source recursive calls.
  bool finish_directory(uint32_t role, std::string &);
  bool call(std::string_view method, const std::vector<std::string> &arguments,
            std::shared_ptr<GlobalYamlValue> &result, std::string &);
  bool init_caches_complete() const;
  bool directory_open() const { return open_; }
  uint32_t next_role() const { return cursor_; }
  FieldObjectId owner() const { return owner_; }
  const FieldGlobalRegistry *registry() const { return registry_; }
  const std::vector<std::string> &insertion_order(uint32_t role) const;
  bool poisoned() const { return poisoned_; }

private:
  const GlobalYamlCachesData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  const FieldGlobalDataRuntime *owning_core_ = nullptr;
  FieldObjectId owner_ = 0;
  std::array<uint8_t, 32> admitted_ir_{};
  GlobalYamlItemsPort items_;
  Warning warning_;
  bool open_ = false, poisoned_ = false;
  uint32_t cursor_ = 0;
  std::array<std::vector<std::string>, 6> order_;
  std::map<std::string, std::shared_ptr<GlobalYamlValue>> values_;
  std::set<std::string> inserted_;
  bool available(std::string &) const;
  bool poison(const char *, std::string &);
};
} // namespace encore::upstream
