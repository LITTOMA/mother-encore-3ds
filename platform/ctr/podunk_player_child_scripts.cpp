#include "podunk_player_child_scripts.hpp"
#include <algorithm>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool PodunkPlayerArrowNative::prepare(
    const PlayerChildScriptsData &d, const PlayerInitializationData &p,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, FieldObjectId root,
    PodunkPlayerAnimation &animation, PodunkPlayerNativeMedia &media,
    std::string &e) {
  if (data_ || !d.valid() || !p.valid() ||
      d.player_ir_sha256() != p.ir_sha256() || !r.object_exists(root) ||
      r.tree_owner(root).get() != &t || t.object_domain() != r.kernel() ||
      !t.descriptor(root) ||
      t.descriptor(root)->id != p.recipe().records()[0].id)
    return fail(e, "Player arrow native actual root scope rejected");
  std::vector<uint32_t> ids;
  for (const auto &v : d.arrows().records())
    ids.push_back(v.id);
  for (const auto &v : d.arrows().sprites())
    ids.push_back(v.id);
  for (const auto &v : d.arrows().players())
    ids.push_back(v.id);
  auto animation_ids = animation.animation_objects();
  std::map<uint32_t, FieldObjectId> objects;
  for (auto source : ids) {
    auto *s = p.recipe().record(source);
    FieldObjectId actual = 0;
    if (!s || !t.get_node(root, s->path, actual, e) || !t.descriptor(actual) ||
        t.descriptor(actual)->id != source ||
        t.descriptor(actual)->native_class != s->native_class ||
        t.descriptor(actual)->script_sha != s->script_sha ||
        !r.object_exists(actual) || r.tree_owner(actual).get() != &t)
      return fail(e, "Player arrow native actual child identity rejected");
    if (s->native_class == "AnimationPlayer" &&
        std::find(animation_ids.begin(), animation_ids.end(), actual) ==
            animation_ids.end())
      return fail(e, "Player arrow actual AnimationPlayer owner missing");
    if (s->native_class == "AnimatedSprite" && !media.owns(actual))
      return fail(e, "Player arrow actual AnimatedSprite owner missing");
    objects.emplace(source, actual);
  }
  data_ = &d;
  player_ = &p;
  tree_ = &t;
  registry_ = &r;
  animation_ = &animation;
  media_ = &media;
  objects_ = std::move(objects);
  return true;
}
bool PodunkPlayerArrowNative::bind(const FieldCameraArrowsData &d,
                                   std::string &e) {
  return data_ && &d == &data_->arrows()
             ? true
             : fail(e, "Player arrow checked pack owner rejected");
}
bool PodunkPlayerArrowNative::actual(uint32_t source, FieldObjectId &id,
                                     std::string &e) const {
  auto i = objects_.find(source);
  if (!data_ || i == objects_.end() || !registry_->object_exists(i->second) ||
      registry_->tree_owner(i->second).get() != tree_ ||
      !tree_->descriptor(i->second) ||
      tree_->descriptor(i->second)->id != source)
    return fail(e, "Player arrow actual native lifetime rejected");
  id = i->second;
  return true;
}
bool PodunkPlayerArrowNative::root(uint32_t source, bool &visible,
                                   Vec2 &position, std::string &e) const {
  FieldObjectId id = 0;
  if (!data_ || !data_->arrows().record(source) || !actual(source, id, e))
    return false;
  auto *s = tree_->state(id);
  if (!s || !s->alive)
    return fail(e, "Player arrow native root missing");
  visible = (s->flags & 1) != 0;
  position = s->local[2];
  return true;
}
bool PodunkPlayerArrowNative::sprite(uint32_t source,
                                     FieldArrowSpriteState &out,
                                     std::string &e) const {
  FieldObjectId id = 0;
  if (!data_ || !data_->arrows().sprite(source) || !actual(source, id, e))
    return false;
  PodunkPlayerAnimatedState native;
  if (!media_->animated_state(id, native, e))
    return false;
  auto *s = tree_->state(id);
  if (!s || !s->alive)
    return fail(e, "Player arrow native Sprite missing");
  out = {};
  out.id = source;
  out.frame = native.frame;
  out.alive = true;
  out.playing = native.playing;
  out.visible = native.visible;
  out.position = s->local[2];
  out.offset = native.offset;
  return true;
}
bool PodunkPlayerArrowNative::play(uint32_t source, std::string_view name,
                                   float speed, bool from_end, std::string &e) {
  FieldObjectId id = 0;
  if (!data_ || !data_->arrows().player(source) || !actual(source, id, e))
    return false;
  return animation_->play(id, name, speed, from_end, e);
}
bool PodunkPlayerArrowNative::assigned(uint32_t source, std::string &name,
                                       std::string &e) const {
  FieldObjectId id = 0;
  if (!data_ || !data_->arrows().player(source) || !actual(source, id, e))
    return false;
  return animation_->assigned(id, name, e);
}
bool PodunkPlayerArrowNative::frame(uint32_t source, int32_t value,
                                    std::string &e) {
  FieldObjectId id = 0;
  if (!data_ || !data_->arrows().sprite(source) || !actual(source, id, e))
    return false;
  auto count = data_->arrows().frames().size();
  if (!count)
    return fail(e, "Player arrow checked frames absent");
  return media_->set_frame(
      id, uint32_t(std::clamp<int32_t>(value, 0, int32_t(count - 1))), e);
}
bool PodunkPlayerArrowNative::visible(uint32_t source, bool value,
                                      std::string &e) {
  FieldObjectId id = 0;
  if (!data_ || !data_->arrows().sprite(source) || !actual(source, id, e))
    return false;
  return media_->set_visible(id, value, e);
}
bool PodunkPlayerArrowNative::rebind_tree(FieldNodeTreeRuntime &t,
                                          std::string &e) {
  if (!data_ || t.object_domain() != registry_->kernel())
    return fail(e, "Player arrow persistent ObjectDB domain rejected");
  for (const auto &entry : objects_) {
    auto *d = t.descriptor(entry.second);
    if (!registry_->object_exists(entry.second) ||
        registry_->tree_owner(entry.second).get() != &t || !d ||
        d->id != entry.first)
      return fail(e, "Player arrow persistent same ObjectID transfer rejected");
  }
  tree_ = &t;
  return true;
}
} // namespace encore::ctr
