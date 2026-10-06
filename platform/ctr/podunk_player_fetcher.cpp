#include "podunk_player_fetcher.hpp"
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool PodunkPlayerFetcherSprites::initialize(
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, PodunkPlayerAnimation &a,
    PodunkPlayerVisualNative &bat, PodunkPlayerFetcherTextures &textures,
    std::string &e) {
  if (tree_ || t.object_domain() != r.kernel() || bat.tree() != &t ||
      bat.registry() != &r || textures.registry() != &r)
    return fail(
        e, "Fetcher native Sprite services must share actual Tree/Registry");
  tree_ = &t;
  registry_ = &r;
  animation_ = &a;
  bat_ = &bat;
  textures_ = &textures;
  e.clear();
  return true;
}
bool PodunkPlayerFetcherSprites::read(FieldObjectId id,
                                      PlayerFetcherSpriteState &out,
                                      std::string &e) const {
  if (!tree_ || !registry_ || registry_->tree_owner(id).get() != tree_ ||
      !registry_->object_exists(id))
    return fail(e, "Fetcher actual native Sprite ObjectDB owner absent");
  auto *node = tree_->descriptor(id);
  auto *state = tree_->state(id);
  if (!node || node->native_class != "Sprite" || !state || !state->alive ||
      !(state->flags & 1))
    return fail(e, "Fetcher native Sprite/Canvas target rejected");
  PlayerFetcherSpriteState s;
  if (id == bat_->object()) {
    PlayerVisualNativeState v;
    if (bat_->tree() != tree_ || bat_->registry() != registry_ ||
        !bat_->state(v, e) || !v.constructed)
      return fail(e, "Fetcher actual Bat native state absent");
    s.columns = v.columns;
    s.rows = v.rows;
    s.frame = v.frame;
    if (v.texture && !textures_->texture(id, v.texture, s.texture, e))
      return false;
  } else {
    const auto *v = animation_->sprite(id);
    if (!v || v->object != id)
      return fail(e, "Fetcher actual ordinary Sprite native owner absent");
    s.texture = v->texture;
    s.columns = v->columns;
    s.rows = v->rows;
    s.frame = v->frame;
  }
  // Source Sprite.visible is local visibility, not visible_in_tree().
  s.visible = bool(state->flags & 2);
  if (s.texture && !registry_->source_resource(s.texture))
    return fail(e, "Fetcher native Texture is not a live actual Resource");
  out = s;
  e.clear();
  return true;
}
} // namespace encore::ctr
