#include "encore/global_packed_directory.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
GlobalPackedDirectoryReference::~GlobalPackedDirectoryReference() {
  // ObjectDB is weak. At last strong Ref destruction it cannot lock this
  // object.
  if (registry_ && binding_.object) {
    std::string e;
    registry_->retire_object(binding_.object, e);
  }
}
bool GlobalPackedDirectoryReference::available(std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      !registry_ || registry_->poisoned() || !binding_.object ||
      registry_->native_reference(binding_.object).get() != this)
    return fail(e, "Actual packed Directory Reference owner unavailable");
  return true;
}
bool GlobalPackedDirectoryReference::open(std::string_view source,
                                          std::string &e) {
  if (!available(e))
    return false;
  // Actual source cache calls are canonical absolute res paths; arbitrary
  // loose, editor, user and unrelated projected-root access is not admitted
  // here.
  if (source.substr(0, 6) != "res://")
    return fail(e, "Packed Directory requires source PCK res path");
  auto index = data_->find_directory(source.substr(6));
  if (index < 0 || !data_->directory_in_cache_scope(uint32_t(index)))
    return fail(e, "Packed Directory open path missing/outside admitted "
                   "file-tree projection");
  current_ = uint32_t(index);
  opened_ = true;
  listing_ = false;
  cdir_ = false;
  list_dirs_.clear();
  list_files_.clear();
  e.clear();
  return true;
}
bool GlobalPackedDirectoryReference::list_dir_begin(bool skip_navigation,
                                                    bool skip_hidden,
                                                    std::string &e) {
  if (!available(e) || !opened_)
    return fail(e,
                "Packed Directory source path must be opened before listing");
  skip_navigation_ = skip_navigation;
  skip_hidden_ = skip_hidden;
  list_dirs_.clear();
  list_files_.clear();
  const auto &dir = data_->directories()[current_];
  for (auto i : dir.directories) {
    auto path = data_->directories()[i].path;
    path.pop_back();
    auto at = path.find_last_of('/');
    list_dirs_.push_back(at == path.npos ? path : path.substr(at + 1));
  }
  for (auto i : dir.files)
    list_files_.push_back(data_->files()[i].name);
  // Native list_dir_begin does not reset the previously observed cdir.
  listing_ = true;
  e.clear();
  return true;
}
bool GlobalPackedDirectoryReference::get_next(std::string &out,
                                              std::string &e) {
  if (!available(e) || !opened_)
    return fail(e,
                "Packed Directory get_next requires actual opened Reference");
  if (!list_dirs_.empty()) {
    cdir_ = true;
    out = std::move(list_dirs_.front());
    list_dirs_.pop_front();
  } else if (!list_files_.empty()) {
    cdir_ = false;
    out = std::move(list_files_.front());
    list_files_.pop_front();
  } else
    out.clear();
  // PCK emits neither synthetic . / .. nor current_is_hidden=true. Thus both
  // native optional skip flags consume the exact same packed entries.
  e.clear();
  return true;
}
bool GlobalPackedDirectoryReference::current_is_dir(bool &out,
                                                    std::string &e) const {
  if (!available(e) || !opened_)
    return fail(e, "Packed Directory current_is_dir requires source open");
  out = cdir_;
  e.clear();
  return true;
}
bool GlobalPackedDirectoryReference::list_dir_end(std::string &e) {
  if (!available(e) || !opened_)
    return fail(e, "Packed Directory list_dir_end requires source open");
  list_dirs_.clear();
  list_files_.clear();
  listing_ = false;
  e.clear();
  return true;
}
bool GlobalPackedDirectoryReference::file(std::string_view name,
                                          const GlobalPackedFile *&out,
                                          std::string &e) const {
  if (!available(e) || !opened_)
    return fail(
        e, "Packed Directory source file lookup requires actual opened owner");
  for (auto i : data_->directories()[current_].files)
    if (data_->files()[i].name == name) {
      out = &data_->files()[i];
      e.clear();
      return true;
    }
  return fail(e, "Packed Directory actual source file absent");
}
const std::string &GlobalPackedDirectoryReference::current_directory() const {
  static const std::string empty;
  return data_ && current_ < data_->directories().size()
             ? data_->directories()[current_].path
             : empty;
}
bool GlobalPackedDirectoryReference::invoke_node_method(std::string_view,
                                                        std::string &e) const {
  return fail(
      e, "Directory Reference has no Node lifecycle/path/deferred methods");
}
bool GlobalPackedDirectoryHost::initialize(const GlobalPackedDirectoryData &d,
                                           const GlobalYamlCachesData &c,
                                           GlobalYamlCachesRuntime &runtime,
                                           FieldGlobalRegistry &registry,
                                           YamlLoad load, std::string &e) {
  if (data_ || !d.valid() || !c.valid() ||
      d.cache_ir_sha256() != c.ir_sha256() ||
      d.identity().upstream_commit != c.identity().upstream_commit ||
      d.identity().source_sha256 != c.identity().source_sha256 ||
      d.owner_source() != c.owner_source() || registry.poisoned() ||
      !runtime.owner() || runtime.poisoned() || runtime.directory_open() ||
      runtime.next_role() != 0 || !load)
    return fail(
        e,
        "Packed Directory actual source cache host/constructor order rejected");
  data_ = &d;
  caches_data_ = &c;
  caches_ = &runtime;
  registry_ = &registry;
  load_ = std::move(load);
  admitted_ir_ = d.ir_sha256();
  e.clear();
  return true;
}
bool GlobalPackedDirectoryHost::available(std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != admitted_ir_ ||
      !caches_data_ || !caches_data_->valid() ||
      data_->cache_ir_sha256() != caches_data_->ir_sha256() || !registry_ ||
      registry_->poisoned() || !caches_ || !caches_->owner() ||
      caches_->poisoned() || poisoned_)
    return fail(e, "Packed Directory actual host unavailable/poisoned");
  return true;
}
bool GlobalPackedDirectoryHost::make_reference(
    std::shared_ptr<GlobalPackedDirectoryReference> &out, std::string &e) {
  if (!available(e))
    return false;
  FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  auto object = std::make_shared<GlobalPackedDirectoryReference>();
  object->data_ = data_;
  object->registry_ = registry_;
  object->admitted_ir_ = admitted_ir_;
  auto &binding = object->binding_;
  binding.object = id;
  binding.family = 0x454e0051;
  binding.capability = 1;
  auto &s = binding.source;
  s.identity = data_->identity();
  s.stable_id = data_->class_id();
  s.role = 5;
  s.native_class = "Directory";
  s.source = data_->owner_source();
  s.script = data_->owner_source();
  s.source_sha = data_->identity().source_sha256;
  s.script_sha = s.source_sha;
  if (!registry_->publish_native_reference(s, id, object, e))
    return false;
  if (registry_->native_reference(id).get() != object.get() ||
      !registry_->object_exists(id))
    return fail(e,
                "Packed Directory ObjectDB did not publish actual Reference");
  out = std::move(object);
  e.clear();
  return true;
}
bool GlobalPackedDirectoryHost::push(const std::string &path, std::string &e) {
  if (stack_.size() >= 64)
    return fail(e, "Packed Directory source recursive depth unsupported");
  Frame frame;
  frame.path = path;
  if (!make_reference(frame.directory, e))
    return false;
  stack_.push_back(std::move(frame));
  return true;
}
bool GlobalPackedDirectoryHost::poison(std::string &e) {
  poisoned_ = true;
  if (e.empty())
    e = "Packed Directory original source cursor failed";
  return false;
}
bool GlobalPackedDirectoryHost::begin_init_caches(std::string &e) {
  if (!available(e) || started_ || finished_ || !stack_.empty() ||
      caches_->directory_open() || caches_->next_role() != 0)
    return fail(e, "Packed Directory source init loop repeated/out of order");
  started_ = true;
  role_ = 0;
  if (!caches_->begin_directory(role_, e) ||
      !push("res://" + data_->policies()[role_].directory, e))
    return poison(e);
  e.clear();
  return true;
}
bool GlobalPackedDirectoryHost::step(std::string &e) {
  if (!available(e) || !started_ || finished_ || stack_.empty())
    return fail(e, "Packed Directory source init cursor not runnable");
  auto &frame = stack_.back();
  if (frame.phase == 0) {
    if (!frame.directory->open(frame.path, e))
      return poison(e);
    frame.phase = 1;
  } else if (frame.phase == 1) {
    if (!frame.directory->list_dir_begin(false, false, e))
      return poison(e);
    frame.phase = 2;
  } else if (frame.phase == 2) {
    if (!frame.directory->get_next(frame.entry, e))
      return poison(e);
    frame.phase = frame.entry.empty() ? 5 : 3;
  } else if (frame.phase == 3) {
    if (!frame.directory->current_is_dir(frame.isdir, e))
      return poison(e);
    frame.phase = 4;
  } else if (frame.phase == 4) {
    if (frame.isdir) {
      auto path = frame.path + frame.entry + "/";
      frame.phase = 2;
      if (frame.entry != "." && frame.entry != ".." && !push(path, e))
        return poison(e);
    } else {
      if (frame.entry.size() >= data_->yaml_suffix().size() &&
          frame.entry.compare(frame.entry.size() - data_->yaml_suffix().size(),
                              data_->yaml_suffix().size(),
                              data_->yaml_suffix()) == 0) {
        const GlobalPackedFile *file = nullptr;
        if (!frame.directory->file(frame.entry, file, e) || !file ||
            file->role != role_)
          return poison(e);
        auto before = caches_->insertion_order(role_).size();
        auto next_role = caches_->next_role();
        if (!load_(*file, *caches_, e))
          return poison(e);
        auto expected = std::find_if(
            caches_data_->records().begin(), caches_data_->records().end(),
            [&](const auto &x) { return x.source == file->source; });
        const auto &order = caches_->insertion_order(role_);
        if (expected == caches_data_->records().end() ||
            caches_->next_role() != next_role || !caches_->directory_open() ||
            order.size() != before + 1 || order.back() != expected->name) {
          e = "Packed Directory YAML callback did not consume actual source "
              "cache entry";
          return poison(e);
        }
      }
      frame.phase = 2;
    }
  } else if (frame.phase == 5) {
    // Source has no explicit list_dir_end: local Ref destruction deletes d and
    // its listing queues on function return. Preserve a caller's retained Ref.
    stack_.pop_back();
    if (stack_.empty()) {
      if (!caches_->finish_directory(role_, e))
        return poison(e);
      ++role_;
      if (role_ == data_->policies().size()) {
        finished_ = true;
        if (!caches_->init_caches_complete()) {
          e = "Packed Directory real cache init closure incomplete";
          return poison(e);
        }
      } else if (!caches_->begin_directory(role_, e) ||
                 !push("res://" + data_->policies()[role_].directory, e))
        return poison(e);
    }
  } else {
    e = "Packed Directory unknown source cursor";
    return poison(e);
  }
  e.clear();
  return true;
}
bool GlobalPackedDirectoryHost::drive(std::string &e) {
  if (!started_ && !begin_init_caches(e))
    return false;
  while (!finished_)
    if (!step(e))
      return false;
  e.clear();
  return complete();
}
bool GlobalPackedDirectoryHost::complete() const {
  std::string e;
  return available(e) && finished_ && stack_.empty() &&
         caches_->init_caches_complete();
}
GlobalPackedDirectoryCursor GlobalPackedDirectoryHost::cursor() const {
  GlobalPackedDirectoryCursor c;
  c.role = role_;
  c.depth = uint32_t(stack_.size());
  if (!stack_.empty()) {
    const auto &f = stack_.back();
    c.phase = f.phase;
    c.directory = f.directory->binding().object;
    c.path = f.path;
    c.entry = f.entry;
    c.is_directory = f.isdir;
  }
  return c;
}
} // namespace encore::upstream
