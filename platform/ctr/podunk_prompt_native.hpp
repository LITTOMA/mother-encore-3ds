#pragma once
#include "encore/field_equipment_data.hpp"
#include "encore/prompt_native.hpp"
#include "podunk_scene_loop.hpp"
#include "source_font_renderer.hpp"
namespace encore::ctr {
// Four actual native children per source Prompt. The existing source runtime
// owns the only clip clock and its source coroutine/persistent finished slot.
class PodunkPromptNative final : public PodunkSceneNativeMechanism,
                                 public PodunkSceneCanvasLeaf {
public:
  PodunkPromptNative();
  ~PodunkPromptNative();
  bool prepare(const upstream::PromptNativeData &,
               const upstream::FieldNodeTreeData &,
               const upstream::FieldPromptData &,
               upstream::FieldPromptRuntime &, upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, upstream::FieldObjectSignals &,
               const upstream::FieldGlobalDataRuntime &, PodunkPlayerHost &,
               upstream::FieldEquipmentView, SourceFontRenderer &,
               const char *asset_root, std::string &);
  // Apply after real 0068 source connections, before PromptRuntime.initialize.
  // Does not replace source connect/hide_signal/Canvas visibility callbacks.
  bool apply(upstream::FieldPromptHost &, std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             bool, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  // Invoke at the actual Label or Arrow canvas order slot, not once per root.
  bool draw_leaf(upstream::FieldObjectId, upstream::Vec2 camera, float depth,
                 bool snap, std::string &) const;
  bool snapshot(upstream::FieldObjectId, upstream::Vec2 &position,
                upstream::Vec2 &size, std::string &text, std::string &) const;
  bool finish_factory(std::string &) const;
  bool appearance(const upstream::FieldCanvasRecord &, upstream::FieldObjectId,
                  upstream::FieldCanvasAppearance &,
                  std::string &) const override;
  const upstream::FieldNodeTreeRuntime *canvas_tree() const override;
  const upstream::FieldGlobalRegistry *canvas_registry() const override;
  bool owns_drawable(upstream::FieldObjectId) const override;
  bool draw_leaf(const upstream::FieldCanvasOrderSlot &,
                 const upstream::FieldTransform &, bool,
                 std::string &) override;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace encore::ctr
