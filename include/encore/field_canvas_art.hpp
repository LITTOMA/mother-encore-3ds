#pragma once
#include "encore/field_node_tree.hpp"
namespace encore::upstream {
class FieldMapView;
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
  Landmark,
  Sparkles,
  Control
};
enum class FieldCanvasShader : uint32_t { Default, Outline, Distortion, Flash };
struct FieldCanvasPage {
  uint32_t id=0,x=0,y=0,width=0,height=0,bytes=0,crc=0;
  std::string path;
  std::array<uint8_t,32> output_sha{},crop_png_sha{};
};
struct FieldCanvasAsset {
  uint32_t id = 0, width = 0, height = 0, bytes = 0, crc = 0;
  std::string source, path;
  std::array<uint8_t, 32> source_sha{}, output_sha{};
  std::vector<FieldCanvasPage> pages;
};
struct FieldCanvasFrame {
  uint32_t texture=0;std::array<float,4>region{},margin{};
  bool filter_clip=false;std::string atlas_source;
};
struct FieldCanvasAnimation {
  std::string name;float speed=0;bool loop=false;
  std::vector<FieldCanvasFrame>frames;
};
enum class FieldCanvasUniformType:uint32_t { Bool,Int,Float,Vec2,Vec4,Sampler2D };
enum class FieldCanvasUniformProof:uint32_t { Material,ShaderDefault,Uninitialized };
struct FieldCanvasUniform {
  std::string name;FieldCanvasUniformType type=FieldCanvasUniformType::Float;
  FieldCanvasUniformProof proof=FieldCanvasUniformProof::Uninitialized;
  bool initialized=false;std::array<float,4>value{};int32_t integer=0;
};
struct FieldCanvasMaterial {
  bool present=false,local_to_scene=false;int32_t priority=0;
  std::string source;std::array<uint8_t,32>shader_code_sha{};
  std::vector<FieldCanvasUniform>uniforms;
};
struct FieldCanvasControlBoundary {
  uint32_t id=0,flags=0,owner_id=0;std::string node,native_class,owner_script;
  std::array<uint8_t,32>owner_sha{},native_properties_sha{};
  // The separate actual native Control owner authenticates the complete
  // source property digest. Label text/font are explicit additional borrows.
  std::string text,font_source;std::array<uint8_t,32>font_source_sha{};
  Vec2 size{};uint32_t align=0,valign=0;float percent_visible=0;
  bool autowrap=false,clip_text=false;
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
  FieldCanvasMaterial material;
  std::string animation,ready_method;float speed_scale=0,ready_min=0,ready_max=0;
  bool playing=false;std::vector<FieldCanvasAnimation>animations;
  FieldColor color{};
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
  uint32_t format()const{return format_;}
  const auto &control_boundaries()const{return controls_;}
  const FieldCanvasControlBoundary*control_boundary(uint32_t)const;
  const FieldCanvasRecord *record(uint32_t) const;
  const FieldCanvasAsset *texture(uint32_t) const;
  const FieldCanvasAsset &program() const { return program_; }
  float y_epsilon() const { return y_epsilon_; }
  float alpha_prune() const { return alpha_prune_; }
  bool pixel_snap() const { return pixel_snap_; }
  const auto &tree_ir_sha() const { return tree_ir_; }
  const auto &ir_sha256() const { return ir_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false, pixel_snap_ = false;uint32_t format_=0;
  FieldIdentity identity_{};
  std::string scene_;
  float y_epsilon_ = 0, alpha_prune_ = 0;
  std::array<uint8_t, 32> tree_ir_{}, ir_{};
  FieldCanvasAsset program_;
  std::vector<FieldCanvasAsset> textures_;
  std::vector<FieldCanvasRecord> records_;
  std::vector<FieldCanvasControlBoundary>controls_;
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
  // Texture, native ColorRect, or an actual native Label owner delegate.
  uint32_t primitive=0;
  std::vector<FieldCanvasUniform>material_uniforms;
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
  bool native_tile=false;uint32_t tile_draw=0,tile_quadrant=0;
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
struct FieldCanvasNativeTileChild {
  uint32_t draw_index=0,quadrant_order=0;Vec2 local_position{};int32_t z=0;
};
class FieldCanvasNativeOwner {
public:
  virtual ~FieldCanvasNativeOwner() = default;
  virtual const FieldCanvasArtData *canvas_data() const = 0;
  virtual const FieldNodeTreeRuntime *canvas_tree() const = 0;
  virtual bool sprite_snapshot(FieldObjectId, FieldCanvasAppearance &,
                               std::string &) const = 0;
  virtual bool animated_snapshot(FieldObjectId,std::string&,uint32_t&,
                                  FieldCanvasAppearance&,std::string&e)const {
    e="Canvas native AnimatedSprite owner unavailable";return false;
  }
  virtual bool color_rect_snapshot(FieldObjectId,Vec2&,FieldColor&,
                                    std::string&e)const {
    e="Canvas native ColorRect owner unavailable";return false;
  }
  virtual const FieldMapView*tile_map_data()const{return nullptr;}
  virtual bool tile_sort_children(FieldObjectId,std::vector<FieldCanvasNativeTileChild>&,
                                   std::string&e)const {
    e="Canvas actual TileMap native children unavailable";return false;
  }
};
class FieldCanvasControlOwner {
public:
  virtual ~FieldCanvasControlOwner()=default;
  virtual const FieldCanvasArtData*canvas_data()const=0;
  virtual const FieldNodeTreeRuntime*canvas_tree()const=0;
  virtual bool admit_control(const FieldCanvasControlBoundary&,FieldObjectId,
                            FieldObjectId,std::string&)const=0;
  virtual bool control_snapshot(const FieldCanvasControlBoundary&,FieldObjectId,
                               FieldObjectId,bool&drawable,std::string&)const=0;
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
  std::function<bool(const FieldCanvasRecord&,FieldObjectId,FieldObjectId,
                     std::vector<FieldCanvasUniform>&,std::string&)>material;
};
enum class FieldCanvasNativeCall:uint32_t {Ready,IdleInternal,SetProperty};
enum class FieldCanvasNativeSignal:uint32_t {FrameChanged,AnimationFinished};
struct FieldCanvasAnimationHost {
  std::function<bool(FieldObjectId,FieldCanvasNativeCall,std::string&)>source_call;
  std::function<bool(float,float,double&,std::string&)>random_range;
  std::function<bool(FieldObjectId,FieldCanvasNativeSignal,std::string&)>signal;
};
// The concrete native AnimatedSprite owner stores this actual state. These
// functions perform the audited native clock/setters, never an independent
// timer or a mirrored script VM. Source call/RNG/signal receipts are required.
struct FieldCanvasAnimatedState {
  const FieldCanvasRecord*source=nullptr;FieldObjectId object=0;
  uint32_t animation=0,frame=0;float timeout=0,speed_scale=0;
  bool playing=false,backwards=false,is_over=false;
};
bool field_canvas_animation_initialize(const FieldCanvasArtData&,
  FieldNodeTreeRuntime&,FieldObjectId,FieldCanvasAnimatedState&,std::string&);
bool field_canvas_animation_ready(FieldCanvasAnimatedState&,
  const FieldCanvasAnimationHost&,std::string&);
bool field_canvas_animation_idle(FieldCanvasAnimatedState&,float delta,
  bool update_pending,const FieldCanvasAnimationHost&,std::string&);
bool field_canvas_animation_set_frame(FieldCanvasAnimatedState&,int32_t,
  const FieldCanvasAnimationHost&,std::string&);
bool field_canvas_animation_set_playing(FieldCanvasAnimatedState&,bool,
  const FieldCanvasAnimationHost&,std::string&);
bool field_canvas_animation_set_speed(FieldCanvasAnimatedState&,float,
  const FieldCanvasAnimationHost&,std::string&);
bool field_canvas_animation_set_animation(FieldCanvasAnimatedState&,
  std::string_view,const FieldCanvasAnimationHost&,std::string&);
class FieldCanvasArtRuntime {
public:
  bool initialize(const FieldCanvasArtData &, const FieldNodeTreeData &,
                  FieldNodeTreeRuntime &, FieldCanvasArtHost, std::string &);
  bool bind_foreign(FieldCanvasForeignOwner &, std::string &);
  bool bind_native(FieldCanvasNativeOwner &, std::string &);
  bool bind_control(FieldCanvasControlOwner&,std::string&);
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
  FieldCanvasControlOwner*control_=nullptr;
  std::map<FieldObjectId, FieldObjectId> owners_;
  std::vector<FieldCanvasOrderSlot> slots_;
  bool bind(const FieldCanvasRecord &, FieldObjectId, FieldObjectId &,
            std::string &);
  bool command(FieldObjectId, std::vector<FieldCanvasDraw> &, std::string &);
};
} // namespace encore::upstream
