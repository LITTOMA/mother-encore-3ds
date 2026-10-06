#include "encore/global_yaml_file.hpp"
#include "encore/global_load.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
std::string trim(std::string s) {
  size_t first = 0, last = s.size();
  while (first < last && uint8_t(s[first]) <= 32)
    ++first;
  while (last > first && uint8_t(s[last - 1]) <= 32)
    --last;
  return s.substr(first, last - first);
}
std::string strip_indent(const std::string &s) {
  size_t n = 0;
  while (n < s.size() && s[n] == ' ')
    ++n;
  while (s.compare(n, 2, "- ") == 0)
    n += 2;
  return trim(s.substr(n));
}
std::shared_ptr<GlobalYamlValue> clone(const GlobalYamlValue &v) {
  auto r = std::make_shared<GlobalYamlValue>();
  r->kind = v.kind;
  r->boolean = v.boolean;
  r->integer = v.integer;
  r->real = v.real;
  r->string = v.string;
  for (const auto &x : v.array)
    r->array.push_back(clone(*x));
  for (const auto &x : v.dictionary)
    r->dictionary.emplace_back(x.first, clone(*x.second));
  return r;
}
} // namespace
bool GlobalYamlFileReference::checked_source_hash(
    std::string_view p, std::array<uint8_t, 32> &out) const {
  return data_ && data_->valid() && data_->ir_sha256() == admitted_ir_ &&
         data_->source_hash(p, out);
}
bool GlobalYamlSmartReader::checked_source_hash(
    std::string_view p, std::array<uint8_t, 32> &out) const {
  return data_ && data_->valid() && data_->ir_sha256() == admitted_ir_ &&
         data_->source_hash(p, out);
}
GlobalYamlFileReference::~GlobalYamlFileReference() {
  // _File::~_File deletes its backend before Object::~Object removes ObjectDB.
  opened_ = nullptr;
  position_ = 0;
  eof_ = false;
  if (registry_ && binding_.object) {
    std::string error;
    registry_->retire_object(binding_.object, error);
  }
}
const GlobalYamlFileRecord *GlobalYamlFileReference::admitted_record(
    std::string_view path) const {
  if (!documents_)
    return data_ ? data_->record(path) : nullptr;
  if (!documents_->valid() || documents_->ir_sha256() != documents_ir_ ||
      documents_->file_ir_sha256() != admitted_ir_ || !document_)
    return nullptr;
  for (const auto &d : documents_->documents())
    if (&d.file == document_ && (d.kind == 0 || d.kind == 2) &&
        path == "res://" + d.file.source)
      return document_;
  return nullptr;
}
bool GlobalYamlFileReference::available(std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      !registry_ || registry_->poisoned() ||
      registry_->native_reference(binding_.object).get() != this ||
      (documents_ && (!document_ || !admitted_record("res://" + document_->source))))
    return fail(e, "Actual File Reference owner unavailable");
  return true;
}
bool GlobalYamlFileReference::file_exists(std::string_view path, bool &out,
                                          std::string &e) const {
  if (!available(e) || path.substr(0, 6) != "res://")
    return fail(e, "File requires checked source res path");
  if (!admitted_record(path))
    return fail(e,
                "File existence outside admitted source scope rejected");
  out = true;
  e.clear();
  return true;
}
bool GlobalYamlFileReference::open(std::string_view path, uint32_t mode,
                                   std::string &e) {
  if (!available(e) || mode != 1 || path.substr(0, 6) != "res://")
    return fail(e, "File source READ mode/path rejected");
  auto *r = admitted_record(path);
  if (!r)
    return fail(e, "File open unknown source rejected before mutation");
  opened_ = nullptr;
  position_ = 0;
  eof_ = false; // Native open starts with close().
  opened_ = r;
  e.clear();
  return true;
}
bool GlobalYamlFileReference::close(std::string &e) {
  if (!available(e))
    return false;
  opened_ = nullptr;
  position_ = 0;
  eof_ = false;
  e.clear();
  return true;
}
bool GlobalYamlFileReference::get_path(std::string &out, std::string &e) const {
  if (!available(e) || !opened_)
    return fail(e, "File get_path requires opened backend");
  out = "res://" + opened_->source;
  e.clear();
  return true;
}
bool GlobalYamlFileReference::get_as_text(bool skip_cr, std::string &out,
                                          std::string &e) const {
  if (!available(e) || !opened_)
    return fail(e, "File get_as_text requires opened backend");
  eof_ = false;
  out = opened_->bytes;
  if (skip_cr)
    out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
  // _File::get_as_text seeks to zero, reads, restores position (which also
  // restores packed EOF). This cold get_json_data call always starts at zero.
  e.clear();
  return true;
}
bool GlobalYamlFileReference::get_line(std::string &out, std::string &e) {
  if (!available(e) || !opened_)
    return fail(e, "File get_line requires opened backend");
  auto byte = [this]() {
    if (position_ >= opened_->bytes.size()) {
      eof_ = true;
      return uint8_t(0);
    }
    return uint8_t(opened_->bytes[position_++]);
  };
  std::string line;
  auto c = byte();
  while (!eof_) {
    if (c == '\n' || c == 0)
      break;
    if (c != '\r')
      line.push_back(char(c));
    c = byte();
  }
  out = std::move(line);
  e.clear();
  return true;
}
bool GlobalYamlFileReference::eof_reached(bool &out, std::string &e) const {
  if (!available(e) || !opened_)
    return fail(e, "File eof_reached requires opened backend");
  out = eof_;
  e.clear();
  return true;
}
bool GlobalYamlFileReference::invoke_node_method(std::string_view,
                                                 std::string &e) const {
  return fail(e,
              "File is a native Reference, never a Node/Resource/Ready owner");
}
GlobalYamlSmartReader::~GlobalYamlSmartReader() {
  // Object::~Object deletes script instance/member Variants before removing
  // the Reader itself from ObjectDB; the member File therefore retires first.
  file_.reset();
  if (registry_ && binding_.object) {
    std::string e;
    registry_->retire_object(binding_.object, e);
  }
}
bool GlobalYamlSmartReader::available(std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      !registry_ || registry_->poisoned() ||
      registry_->native_reference(binding_.object).get() != this || !file_)
    return fail(e, "Actual SmartFileReader constructor/member unavailable");
  return true;
}
bool GlobalYamlSmartReader::open(std::string_view path, std::string &e) {
  if (!available(e))
    return false;
  bool exists = false;
  if (!file_->file_exists(path, exists, e) || !exists)
    return false;
  return file_->open(path, 1, e);
}
bool GlobalYamlSmartReader::significant(std::string &out, std::string &e) {
  if (!available(e))
    return false;
  bool end = false;
  while (file_->eof_reached(end, e) && !end) {
    std::string line;
    if (!file_->get_line(line, e))
      return false;
    ++line_number_;
    auto stripped = trim(line);
    if (!stripped.empty() && stripped[0] != '#') {
      out = std::move(line);
      return true;
    }
  }
  if (!e.empty())
    return false;
  out.clear();
  return true;
}
bool GlobalYamlSmartReader::peek_line(bool stripped, std::string &out,
                                      std::string &e) {
  if (!available(e))
    return false;
  if (buffer_.empty() && !significant(buffer_, e))
    return false;
  out = stripped ? strip_indent(buffer_) : buffer_;
  return true;
}
bool GlobalYamlSmartReader::next_line(bool stripped, std::string &out,
                                      std::string &e) {
  if (!available(e))
    return false;
  std::string line;
  if (buffer_.empty()) {
    if (!significant(line, e))
      return false;
  } else {
    line = std::move(buffer_);
    buffer_.clear();
  }
  out = stripped ? strip_indent(line) : line;
  return true;
}
bool GlobalYamlSmartReader::end_reached(bool &out, std::string &e) {
  std::string peek;
  if (!peek_line(false, peek, e))
    return false;
  bool end = false;
  if (!file_->eof_reached(end, e))
    return false;
  out = peek.empty() && end;
  return true;
}
bool GlobalYamlSmartReader::close(std::string &e) {
  if (!available(e))
    return false;
  return file_->close(e);
}
bool GlobalYamlFileHost::available(std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      !caches_data_ || !caches_data_->valid() ||
      caches_data_->ir_sha256() != admitted_caches_ir_ || !caches_ ||
      !registry_ || registry_->poisoned() || poisoned_)
    return fail(e, "Actual YAML File host unavailable");
  return true;
}
bool GlobalYamlFileHost::initialize(const GlobalYamlFileData &d,
                                    const GlobalYamlCachesData &c,
                                    GlobalYamlCachesRuntime &actual,
                                    FieldGlobalRegistry &g, std::string &e) {
  if (data_ || !d.valid() || !c.valid() || g.poisoned() || !actual.owner() ||
      !g.object_exists(actual.owner()) || actual.registry() != &g || actual.poisoned() ||
      d.identity().upstream_commit != c.identity().upstream_commit ||
      d.cache_ir_sha256() != c.ir_sha256())
    return fail(e, "YAML File actual cache/registry prerequisites rejected");
  if (d.records().size() != c.records().size())
    return fail(e, "YAML File incomplete cache closure");
  data_ = &d;
  caches_data_ = &c;
  caches_ = &actual;
  registry_ = &g;
  admitted_ir_ = d.ir_sha256();
  admitted_caches_ir_ = c.ir_sha256();
  return true;
}
bool GlobalYamlFileHost::make_file(
    uint32_t index, std::shared_ptr<GlobalYamlFileReference> &out,
    std::string &e) {
  return make_document_file(index, nullptr, 0, out, e);
}
bool GlobalYamlFileHost::make_document_file(
    uint32_t index, const GlobalLoadData *documents, uint32_t kind,
    std::shared_ptr<GlobalYamlFileReference> &out, std::string &e) {
  if (!available(e) || (index != 0 && index != 2) ||
      index >= data_->bindings().size())
    return fail(e, "YAML File constructor source point rejected");
  FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  auto file = std::make_shared<GlobalYamlFileReference>();
  file->data_ = data_;
  file->registry_ = registry_;
  file->binding_ = {id, data_->bindings()[index], 0x454e0052, 1};
  file->admitted_ir_ = admitted_ir_;
  if (documents) {
    file->documents_ = documents;
    file->documents_ir_ = documents->ir_sha256();
    for (const auto &d : documents->documents())
      if (d.kind == kind) file->document_ = &d.file;
    if (!file->document_ || !file->admitted_record("res://" + file->document_->source))
      return fail(e, "File typed save document owner rejected");
  }
  if (!registry_->publish_native_reference(file->binding_.source, id, file, e))
    return false;
  out = std::move(file);
  return true;
}
bool GlobalYamlFileHost::make_reader(
    std::shared_ptr<GlobalYamlSmartReader> &out, std::string &e) {
  return make_document_reader(nullptr, 0, out, e);
}
bool GlobalYamlFileHost::make_document_reader(
    const GlobalLoadData *documents, uint32_t kind,
    std::shared_ptr<GlobalYamlSmartReader> &out, std::string &e) {
  if (!available(e) || data_->bindings().size() != 3)
    return false;
  FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  auto reader = std::make_shared<GlobalYamlSmartReader>();
  reader->data_ = data_;
  reader->admitted_ir_ = admitted_ir_;
  reader->registry_ = registry_;
  reader->binding_ = {id, data_->bindings()[1], 0x454e0052, 1};
  if (!registry_->publish_native_reference(reader->binding_.source, id, reader,
                                           e))
    return false;
  // GDScript::_new creates its implicit Reference owner BEFORE initializer's
  // _file := File.new(). Neither instance is a Node nor gets a Ready callback.
  if (!make_document_file(2, documents, kind, reader->file_, e))
    return false;
  out = std::move(reader);
  return true;
}
bool GlobalYamlFileHost::get_json_data(std::string_view path,
                                       const std::array<uint8_t, 32> &sha,
                                       std::shared_ptr<GlobalYamlValue> &out,
                                       std::string &e,
                                       const GlobalLoadData *documents, uint32_t kind) {
  if (!available(e) || busy_ || path.substr(0, 6) != "res://")
    return fail(e, "YAML source load path/reentrancy rejected");
  const GlobalYamlFileRecord *record = nullptr;
  if (documents) {
    for (const auto &d : documents->documents())
      if (d.kind == kind && path == "res://" + d.file.source) record = &d.file;
  } else {
    record = data_->record(path);
  }
  if (!record || record->sha != sha)
    return fail(e, "YAML source path/SHA not admitted before construction");
  busy_ = true;
  struct Guard {
    bool &busy;
    ~Guard() { busy = false; }
  } guard{busy_};
  std::shared_ptr<GlobalYamlFileReference> outer;
  if (!make_document_file(0, documents, kind, outer, e)) {
    poisoned_ = true;
    return false;
  }
  bool exists = false;
  std::string unused;
  if (!outer->file_exists(path, exists, e) || !exists ||
      !outer->open(path, 1, e) || !outer->get_as_text(true, unused, e)) {
    poisoned_ = true;
    return false;
  }
  // Known audited cache inputs all have source yaml suffix. Invalid suffix is
  // rejected before SmartFileReader.new(), matching parse_file's early return.
  if (path.size() < 5 || path.substr(path.size() - 5) != ".yaml")
    return fail(e, "YAML parse capability requires original cache yaml suffix");
  std::shared_ptr<GlobalYamlValue> result;
  {
    std::shared_ptr<GlobalYamlSmartReader> reader;
    if (!make_document_reader(documents, kind, reader, e) || !reader->open(path, e)) {
      poisoned_ = true;
      return false;
    }
    bool end = false;
    if (!reader->end_reached(end, e) || end) {
      poisoned_ = true;
      return fail(e, "Checked native parser nonempty source rejected");
    }
    // Source text parsing was compiled by this exact original parser. Consume
    // its private line stream to EOF without another script VM, RNG or clock;
    // each parse receives a fresh actual Dictionary/Array, never cache id
    // aliases.
    while (!end) {
      std::string line;
      if (!reader->next_line(false, line, e) || !reader->end_reached(end, e)) {
        poisoned_ = true;
        return false;
      }
    }
    result = clone(*record->parsed);
    if (!reader->close(e)) {
      poisoned_ = true;
      return false;
    }
  }
  // Smart reader/member File have retired. Source outer File is still alive
  // until this get_json_data frame returns, and closes in its native
  // destructor.
  out = std::move(result);
  return true;
}
bool GlobalYamlFileHost::actual_global_load(
    const GlobalLoadData &documents, uint32_t kind,
    std::shared_ptr<GlobalYamlValue> &out, std::string &e) {
  if (!available(e) || !documents.valid() || busy_ ||
      (kind != 0 && kind != 2) ||
      documents.identity().upstream_commit != data_->identity().upstream_commit ||
      documents.file_ir_sha256() != admitted_ir_)
    return fail(e, "Global LOAD typed File scope/pin rejected");
  for (const auto &path : {data_->owner_source(), data_->parser_source()}) {
    std::array<uint8_t, 32> a{}, b{};
    if (!data_->source_hash(path, a) || !documents.source_hash(path, b) || a != b)
      return fail(e, "Global LOAD actual getter/parser source differs");
  }
  const GlobalLoadDocument *document = nullptr;
  for (const auto &d : documents.documents())
    if (d.kind == kind) {
      if (document) return fail(e, "Global LOAD duplicate document cursor");
      document = &d;
    }
  if (!document || !document->file.parsed ||
      document->getter_source != data_->owner_source() ||
      document->parser_source != data_->parser_source())
    return fail(e, "Global LOAD typed source document absent");
  return get_json_data("res://" + document->file.source, document->file.sha,
                       out, e, &documents, kind);
}
bool GlobalYamlFileHost::actual_yaml_load(const GlobalPackedFile &file,
                                          GlobalYamlCachesRuntime &actual,
                                          std::string &e) {
  if (!available(e) || &actual != caches_ || !actual.directory_open() ||
      actual.next_role() != file.role)
    return fail(e, "YAML load requires actual matching Directory/cache cursor");
  auto *r = data_->record(file.source);
  if (!r || r->role != file.role || r->sha != file.sha ||
      r->bytes.size() != file.size)
    return fail(e, "YAML Directory/source file binding rejected");
  std::shared_ptr<GlobalYamlValue> parsed;
  if (!get_json_data("res://" + file.source, file.sha, parsed, e) || !parsed)
    return false;
  // Only this source path/SHA crosses into the actual cache owner. Its existing
  // insertion API materializes its equal pristine compiled value. Public parsed
  // values are not asserted to be the cache's shared root without that
  // overload.
  if (!actual.insert_loaded_yaml(file.source, file.sha, e)) {
    poisoned_ = true;
    return false;
  }
  return true;
}
} // namespace encore::upstream
