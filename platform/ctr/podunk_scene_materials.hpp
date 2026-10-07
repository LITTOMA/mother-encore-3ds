#pragma once
#include "encore/field_melody_background.hpp"
#include "encore/field_prompts.hpp"
#include "encore/field_scene_materials.hpp"
#include "podunk_scene_native.hpp"
class FieldCanvasArtRenderer;
namespace encore::ctr {
// Original effective ShaderMaterial Resources; no Node/script Ready admission.
class PodunkSceneMaterials final : public PodunkSceneMaterialOwner {
public:
  using DefaultDraw =
      std::function<bool(const upstream::FieldCanvasDraw &, upstream::Vec2,
                         float, float, std::string &)>;
  PodunkSceneMaterials();
  ~PodunkSceneMaterials();
  PodunkSceneMaterials(const PodunkSceneMaterials &) = delete;
  PodunkSceneMaterials &operator=(const PodunkSceneMaterials &) = delete;
  bool prepare(const upstream::FieldSceneMaterialsData &,
               const upstream::FieldCanvasArtData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &,
               const upstream::FieldPromptData &,
               upstream::FieldPromptRuntime &,
               const upstream::FieldMelodyBackgroundData &,
               const upstream::FieldMelodyBackgroundRuntime &,
               const char *asset_root, DefaultDraw, std::string &);
  const upstream::FieldGlobalRegistry *registry() const override;
  bool bind_images(const FieldCanvasArtRenderer &, std::string &) override;
  bool material(upstream::FieldObjectId, const upstream::FieldCanvasRecord &,
                PodunkSceneMaterialState &, std::string &) const override;
  // Invoke exactly once from actual GPU frame begin, after fence. Shader TIME
  // belongs to the renderer-wide scene clock, not this component's animation.
  bool begin_frame(uint64_t epoch, float global_shader_time,
                   std::string &) override;
  bool shader_parameter(upstream::FieldObjectId material, std::string_view name,
                        std::vector<float> &, std::string &) const;
  bool draw(const upstream::FieldCanvasDraw &, upstream::Vec2, float, float,
            std::string &) override;
  bool shutdown(std::string &);

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace encore::ctr
