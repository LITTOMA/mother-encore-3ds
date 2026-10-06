#include "encore/field_ui_preloads.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
const FieldUiPreloadRecipe *FieldUiPreloadsData::entry(uint32_t id) const {
  for (const auto &e : entries_)
    if (e.preload.id == id)
      return &e;
  return nullptr;
}
bool FieldUiPreloadsData::source_hash(std::string_view p,
                                      std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
const FieldUiPreloadRecipe *FieldUiPreloadRecipes::entry(uint32_t id) const {
  return data_ ? data_->entry(id) : nullptr;
}
bool FieldUiPreloadRecipes::initialize(
    std::shared_ptr<const FieldUiPreloadsData> d, const FieldUiManagerData &ui,
    std::shared_ptr<const FieldNodeRecipeData> dialogue, std::string &e) {
  std::array<uint8_t, 32> proof;
  if (data_ || !d || !d->valid() || !ui.valid() ||
      d->identity().upstream_commit != ui.identity().upstream_commit ||
      d->source_script() != ui.source_script() ||
      !d->source_hash(ui.source_script(), proof) ||
      proof != ui.identity().source_sha256)
    return fail(e, "UI preloads actual source/pin identity rejected");
  std::map<uint32_t, std::shared_ptr<const FieldNodeRecipeData>> candidate;
  for (const auto &entry : d->entries()) {
    const auto &p = entry.preload;
    auto it = std::find_if(ui.preloads().begin(), ui.preloads().end(),
                           [&](const auto &v) { return v.id == p.id; });
    if (it == ui.preloads().end() || it->name != p.name || it->path != p.path ||
        it->native_class != p.native_class || it->sha != p.sha || it->onready)
      return fail(e, "UI complete source preload declaration rejected");
    auto owner = entry.borrowed ? dialogue : entry.recipe;
    if (!owner || !owner->valid() || owner->source_scene() != p.path ||
        owner->identity().scene_id != entry.scene_id ||
        owner->identity().source_sha256 != p.sha ||
        owner->identity().upstream_commit != ui.identity().upstream_commit ||
        owner->ir_sha256() != entry.ir_sha ||
        owner->records().size() != entry.node_count)
      return fail(e, "UI actual full recipe owner/source/IR/count rejected");
    if (!candidate.emplace(p.id, owner).second)
      return fail(e, "UI duplicate source resource owner rejected");
  }
  data_ = std::move(d);
  recipes_ = std::move(candidate);
  e.clear();
  return true;
}
bool FieldUiPreloadRecipes::load_recipe(
    const FieldUiPreload &p, std::shared_ptr<const FieldNodeRecipeData> &out,
    std::string &e) const {
  const auto *source = entry(p.id);
  auto owner = recipes_.find(p.id);
  if (!complete() || !source || owner == recipes_.end() ||
      p.name != source->preload.name || p.path != source->preload.path ||
      p.sha != source->preload.sha ||
      p.native_class != source->preload.native_class || p.onready)
    return fail(
        e,
        "UI preload outside checked complete source recipe ownership rejected");
  out = owner->second;
  e.clear();
  return true;
}
} // namespace encore::upstream
