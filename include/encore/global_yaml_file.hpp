#pragma once
#include "encore/global_packed_directory.hpp"
namespace encore::upstream {
struct GlobalYamlFileRecord {
  uint32_t role = 0;
  std::string source, bytes;
  std::array<uint8_t, 32> sha{};
  std::shared_ptr<const GlobalYamlValue> parsed;
};
class GlobalYamlFileData {
public:
  bool load(const uint8_t *, size_t, const GlobalYamlCachesData &,
            std::string &);
  bool load_file(const char *, const GlobalYamlCachesData &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &cache_ir_sha256() const { return caches_ir_; }
  const auto &bindings() const { return bindings_; }
  const auto &records() const { return records_; }
  const auto &owner_source() const { return owner_; }
  const auto &parser_source() const { return parser_; }
  const GlobalYamlFileRecord *record(std::string_view) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{}, caches_ir_{};
  std::string owner_, parser_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::vector<FieldGlobalExternalSpec> bindings_;
  std::vector<GlobalYamlFileRecord> records_;
};
class GlobalYamlFileReference final : public FieldGlobalNativeReference {
public:
  ~GlobalYamlFileReference() override;
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override {
    return binding_.source.native_class.c_str();
  }
  const FieldGlobalRegistry *registry() const override { return registry_; }
  bool checked_source_hash(std::string_view path,
                           std::array<uint8_t, 32> &out) const;
  bool file_exists(std::string_view, bool &, std::string &) const;
  bool open(std::string_view, uint32_t mode, std::string &);
  bool close(std::string &);
  bool get_as_text(bool skip_cr, std::string &, std::string &) const;
  bool get_line(std::string &, std::string &);
  bool eof_reached(bool &, std::string &) const;
  bool get_path(std::string &, std::string &) const;
  bool invoke_node_method(std::string_view, std::string &) const;
  bool is_open() const { return opened_ != nullptr; }
  size_t position() const { return position_; }

private:
  friend class GlobalYamlFileHost;
  friend class GlobalYamlSmartReader;
  const GlobalYamlFileData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldGlobalExternalBinding binding_{};
  const GlobalYamlFileRecord *opened_ = nullptr;
  std::array<uint8_t, 32> admitted_ir_{};
  size_t position_ = 0;
  mutable bool eof_ = false;
  bool available(std::string &) const;
};
class GlobalYamlSmartReader final : public FieldGlobalNativeReference {
public:
  ~GlobalYamlSmartReader() override;
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *native_class() const override {
    return binding_.source.native_class.c_str();
  }
  const FieldGlobalRegistry *registry() const override { return registry_; }
  bool checked_source_hash(std::string_view path,
                           std::array<uint8_t, 32> &out) const;
  bool open(std::string_view, std::string &);
  bool end_reached(bool &, std::string &);
  bool next_line(bool stripped, std::string &, std::string &);
  bool peek_line(bool stripped, std::string &, std::string &);
  bool close(std::string &);
  uint64_t line_number() const { return line_number_; }
  FieldObjectId member_file_object() const {
    return file_ ? file_->binding().object : 0;
  }

private:
  friend class GlobalYamlFileHost;
  const GlobalYamlFileData *data_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  FieldGlobalExternalBinding binding_{};
  std::array<uint8_t, 32> admitted_ir_{};
  std::shared_ptr<GlobalYamlFileReference> file_;
  std::string buffer_;
  uint64_t line_number_ = 0;
  bool available(std::string &) const;
  bool significant(std::string &, std::string &);
};
class GlobalYamlFileHost {
public:
  bool initialize(const GlobalYamlFileData &, const GlobalYamlCachesData &,
                  GlobalYamlCachesRuntime &, FieldGlobalRegistry &,
                  std::string &);
  bool make_file(uint32_t source_binding,
                 std::shared_ptr<GlobalYamlFileReference> &, std::string &);
  bool make_reader(std::shared_ptr<GlobalYamlSmartReader> &, std::string &);
  // Only checked cache sources; each source parse has a fresh local result.
  // No public get_json_data alias escapes this narrow Directory load port.
  // All three local References retire before actual owning cache insertion.
  bool actual_yaml_load(const GlobalPackedFile &, GlobalYamlCachesRuntime &,
                        std::string &);
  bool poisoned() const { return poisoned_; }

private:
  const GlobalYamlFileData *data_ = nullptr;
  const GlobalYamlCachesData *caches_data_ = nullptr;
  GlobalYamlCachesRuntime *caches_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  std::array<uint8_t, 32> admitted_ir_{};
  std::array<uint8_t, 32> admitted_caches_ir_{};
  bool busy_ = false, poisoned_ = false;
  bool get_json_data(std::string_view, const std::array<uint8_t, 32> &,
                     std::shared_ptr<GlobalYamlValue> &, std::string &);
  bool available(std::string &) const;
};
} // namespace encore::upstream
