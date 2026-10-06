#pragma once
#include "encore/global_yaml_caches.hpp"
#include <deque>
namespace encore::upstream {
struct GlobalPackedFile {
  std::string source, name;
  std::array<uint8_t, 32> sha{};
  uint64_t size = 0;
  uint32_t role = 0;
};
struct GlobalPackedDir {
  std::string path;
  std::vector<uint32_t> directories, files;
};
class GlobalPackedDirectoryData {
public:
  bool load(const uint8_t *, size_t, const GlobalYamlCachesData &,
            const FieldGlobalRegistryData &, std::string &);
  bool load_file(const char *, const GlobalYamlCachesData &,
                 const FieldGlobalRegistryData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &cache_ir_sha256() const { return cache_ir_; }
  const auto &owner_source() const { return owner_; }
  uint32_t class_id() const { return class_; }
  const std::string &yaml_suffix() const { return suffix_; }
  const auto &directories() const { return dirs_; }
  const auto &files() const { return files_; }
  const auto &policies() const { return policies_; }
  const auto &engine_sources() const { return engine_; }
  const auto &engine_commit() const { return engine_commit_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  int32_t find_directory(std::string_view) const;
  bool directory_in_cache_scope(uint32_t) const;

private:
  bool valid_ = false;
  uint32_t class_ = 0;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, cache_ir_{};
  std::string owner_, engine_commit_, suffix_;
  std::map<std::string, std::array<uint8_t, 32>> engine_;
  std::vector<GlobalPackedDir> dirs_;
  std::vector<GlobalPackedFile> files_;
  std::vector<GlobalYamlCachePolicy> policies_;
};
// A real native Reference, not a Node or Resource. The registry observes a weak
// reference; source local/shared owners determine its lifetime.
class GlobalPackedDirectoryReference final : public FieldGlobalNativeReference {
public:
  ~GlobalPackedDirectoryReference() override;
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override { return "Directory"; }
  const FieldGlobalRegistry *registry() const override { return registry_; }
  bool checked_source_hash(std::string_view p,
                           std::array<uint8_t, 32> &out) const override {
    return data_ && data_->valid() && data_->ir_sha256() == admitted_ir_ &&
           data_->source_hash(p, out);
  }
  bool open(std::string_view, std::string &);
  bool list_dir_begin(bool skip_navigation, bool skip_hidden, std::string &);
  bool get_next(std::string &, std::string &);
  bool current_is_dir(bool &, std::string &) const;
  bool list_dir_end(std::string &);
  bool file(std::string_view, const GlobalPackedFile *&, std::string &) const;
  bool invoke_node_method(std::string_view, std::string &) const;
  const std::string &current_directory() const;
  bool listing() const { return listing_; }

private:
  friend class GlobalPackedDirectoryHost;
  const GlobalPackedDirectoryData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldGlobalExternalBinding binding_{};
  std::array<uint8_t, 32> admitted_ir_{};
  uint32_t current_ = 0;
  bool opened_ = false, listing_ = false, cdir_ = false;
  bool skip_navigation_ = false, skip_hidden_ = false;
  std::deque<std::string> list_dirs_, list_files_;
  bool available(std::string &) const;
};
struct GlobalPackedDirectoryCursor {
  uint32_t role = 0, depth = 0, phase = 0;
  FieldObjectId directory = 0;
  std::string path, entry;
  bool is_directory = false;
};
class GlobalPackedDirectoryHost {
public:
  // get_json_data must be implemented by the actual typed source File/Parser
  // owner. Success alone is insufficient: the same cache must insert one value.
  using YamlLoad = std::function<bool(
      const GlobalPackedFile &, GlobalYamlCachesRuntime &, std::string &)>;
  bool initialize(const GlobalPackedDirectoryData &,
                  const GlobalYamlCachesData &, GlobalYamlCachesRuntime &,
                  FieldGlobalRegistry &, YamlLoad, std::string &);
  bool make_reference(std::shared_ptr<GlobalPackedDirectoryReference> &,
                      std::string &);
  bool begin_init_caches(std::string &);
  bool step(std::string &);
  bool drive(std::string &);
  bool complete() const;
  bool poisoned() const { return poisoned_; }
  GlobalPackedDirectoryCursor cursor() const;

private:
  struct Frame {
    std::shared_ptr<GlobalPackedDirectoryReference> directory;
    std::string path, entry;
    uint32_t phase = 0;
    bool isdir = false;
  };
  const GlobalPackedDirectoryData *data_ = nullptr;
  const GlobalYamlCachesData *caches_data_ = nullptr;
  GlobalYamlCachesRuntime *caches_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  YamlLoad load_;
  std::array<uint8_t, 32> admitted_ir_{};
  uint32_t role_ = 0;
  bool started_ = false, finished_ = false, poisoned_ = false;
  std::vector<Frame> stack_;
  bool available(std::string &) const;
  bool push(const std::string &, std::string &);
  bool poison(std::string &);
};
} // namespace encore::upstream
