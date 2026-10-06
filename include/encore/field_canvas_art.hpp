#pragma once
#include "encore/field_node_tree.hpp"
namespace encore::upstream {
enum class FieldCanvasOwner : uint32_t {
  Native,
  Grass,
  Dandelion,
  Butterfly,
  Character,
  Emotes,
  Prompt,
  Enemy,
  Present,
  DeadBush,
  OpenableDoor,
  Jump,
  Phone,
  Dropped,
  Vending,
  Melody,
  Birds,
  Npc,
  Landmark
};
enum class FieldCanvasShader : uint32_t { Default, Outline, Distortion, Flash };
struct FieldCanvasAsset {
  uint32_t id = 0, width = 0, height = 0, bytes = 0, crc = 0;
  std::string source, path;
  std::array<uint8_t, 32> source_sha{}, output_sha{};
};
struct FieldCanvasRecord {
  uint32_t id = 0, kind = 0, flags = 0, texture = 0, hframes = 0, vframes = 0,
           frame = 0, stretch = 0, owner_id = 0;
  bool centered = false, flip_h = false, flip_v = false;
  Vec2 offset{}, size{};
  FieldCanvasOwner owner = FieldCanvasOwner::Native;
  FieldCanvasShader shader = FieldCanvasShader::Default;
  std::string node, owner_script, shader_source;
  std::array<uint8_t, 32> owner_sha{};
};
class FieldCanvasArtData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  bool scene_admitted() const { return false; }
  FieldIdentity identity() const { return identity_; }
  const std::string &source_scene() const { return scene_; }
  const auto &records() const { return records_; }
  const auto &textures() const { return textures_; }
  const FieldCanvasRecord *record(uint32_t) const;
  const FieldCanvasAsset *texture(uint32_t) const;
  const FieldCanvasAsset &program() const { return program_; }
  float y_epsilon() const { return y_epsilon_; }
  float alpha_prune() const { return alpha_prune_; }
  bool pixel_snap() const { return pixel_snap_; }
  const auto &tree_ir_sha() const { return tree_ir_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false, pixel_snap_ = false;
  FieldIdentity identity_{};
  std::string scene_;
  float y_epsilon_ = 0, alpha_prune_ = 0;
  std::array<uint8_t, 32> tree_ir_{};
  FieldCanvasAsset program_;
  std::vector<FieldCanvasAsset> textures_;
  std::vector<FieldCanvasRecord> records_;
  std::map<uint32_t, size_t> record_index_, texture_index_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
enum class FieldCanvasAction : uint32_t { Hidden, Default, Delegate };
struct FieldCanvasAppearance {
  FieldCanvasAction action = FieldCanvasAction::Default;
  uint32_t texture = 0, hframes = 0, vframes = 0, frame = 0;
  Vec2 offset{}, size{};
  bool centered = false, flip_h = false, flip_v = false;
};
struct FieldCanvasDraw {
  FieldObjectId object = 0, owner_object = 0;
  uint32_t source = 0, owner_source = 0, texture = 0, frame = 0;
  FieldCanvasOwner owner = FieldCanvasOwner::Native;
  FieldCanvasShader shader = FieldCanvasShader::Default;
  FieldCanvasAction action = FieldCanvasAction::Default;
  int32_t z = 0;
  uint64_t order = 0;
  std::array<Vec2, 4> vertices{};
  FieldTransform world{};
  std::array<float, 4> source_rect{};
  FieldColor color{};
  bool flip_h = false, flip_v = false, pixel_snap = false;
};
struct FieldCanvasOrderSlot {
  FieldObjectId object = 0;
  uint32_t source = 0, class_index = 0, flags = 0;
  int32_t z = 0;
  uint64_t order = 0, native_order = 0;
  FieldTransform world{};
  FieldColor color{};
  FieldIdentity identity{};
  bool foreign = false, foreign_drawable = false;
};
// Implemented by an actual owning source consumer. Containers and drawable
// nodes keep their original identities and participate in the same tree order.
// This grants neither constructor nor Ready and is not a callback roster.
class FieldCanvasForeignOwner {
public:
  virtual ~FieldCanvasForeignOwner() = default;
  virtual bool admit(FieldObjectId, const FieldNodeDescriptor &,
                     const FieldIdentity &, const FieldNodeTreeRuntime &,
                     bool &drawable, std::string &) const = 0;
};
// Real native Sprite state; this is independent of scripted appearance/shader
// ownership and does not grant construction or Ready.
class FieldCanvasNativeOwner {
public:
  virtual ~FieldCanvasNativeOwner() = default;
  virtual const FieldCanvasArtData *canvas_data() const = 0;
  virtual const FieldNodeTreeRuntime *canvas_tree() const = 0;
  virtual bool sprite_snapshot(FieldObjectId, FieldCanvasAppearance &,
                               std::string &) const = 0;
};
struct FieldCanvasArtHost {
  // Each dynamic appearance must bind the actual consumer, source node and
  // script digest. Delegation is explicit and never erases shader ownership.
  std::function<bool(const FieldCanvasRecord &, FieldObjectId, FieldObjectId,
                     std::string &)>
      bind_owner;
  std::function<bool(const FieldCanvasRecord &, FieldObjectId, FieldObjectId,
                     FieldCanvasAppearance &, std::string &)>
      appearance;
};
class FieldCanvasArtRuntime {
public:
  bool initialize(const FieldCanvasArtData &, const FieldNodeTreeData &,
                  FieldNodeTreeRuntime &, FieldCanvasArtHost, std::string &);
  bool bind_foreign(FieldCanvasForeignOwner &, std::string &);
  bool bind_native(FieldCanvasNativeOwner &, std::string &);
  // Generates one ordered command stream; platform draws defaults or invokes
  // the typed shader owner's renderer at the same z/tree slot. No frame logic.
  bool collect(std::vector<FieldCanvasDraw> &, std::string &);
  // All visible Canvas nodes, including independently admitted TileMap,
  // Label and AnimatedSprite families. A parent can merge their commands
  // at the source slot without drawing all maps before all Sprite layers.
  const auto &canvas_order() const { return slots_; }
  bool ready() const { return data_ && tree_; }
  void clear();

private:
  const FieldCanvasArtData *data_ = nullptr;
  const FieldNodeTreeData *source_ = nullptr;
  FieldNodeTreeRuntime *tree_ = nullptr;
  FieldCanvasArtHost host_;
  FieldCanvasForeignOwner *foreign_ = nullptr;
  FieldCanvasNativeOwner *native_ = nullptr;
  std::map<FieldObjectId, FieldObjectId> owners_;
  std::vector<FieldCanvasOrderSlot> slots_;
  bool bind(const FieldCanvasRecord &, FieldObjectId, FieldObjectId &,
            std::string &);
  bool command(FieldObjectId, std::vector<FieldCanvasDraw> &, std::string &);
};
} // namespace encore::upstream
