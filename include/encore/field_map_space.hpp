#pragma once
#include "encore/field_map.hpp"
#include "encore/field_scene_actions.hpp"
#include <unordered_map>

namespace encore::upstream {
class HouseReentryData;
class FieldNodeTreeRuntime;
// Live TileMap state consumes the existing source map and checked Reparenter
// bindings. It never interprets script or approves the whole source scene.
class FieldMapSpace {
public:
  bool configure(const FieldMapView &, const FieldSceneActionsData &,
                 std::string &);
  // Retire the actual Podunk TileMaps and bind the full source House tree.
  // Only the complete checked placed-cell proof can authorize an empty tile
  // collision result. House collision bodies live in FieldGeometrySpace.
  bool replace_house_tiles(const HouseReentryData &,
                           const FieldNodeTreeRuntime &old_tree,
                           uint64_t old_root, FieldNodeTreeRuntime &house_tree,
                           std::string &);
  bool admit_reparent(uint32_t leaf, uint32_t parent, std::string &) const;
  // Source remove_child is immediate; add_child is invoked by the global queue.
  // Native child ordering comes from the actual tree, not an independent
  // counter.
  bool remove_leaf(uint32_t leaf, uint32_t actual_parent, std::string &);
  bool append_leaf(uint32_t leaf, uint32_t parent, uint32_t native_canvas_order,
                   std::string &);
  bool set_collision(uint32_t leaf, uint32_t role, uint32_t value,
                     std::string &);
  bool set_canvas_visibility(uint32_t canvas, bool, std::string &);
  bool commit_deleted(uint32_t canvas, std::string &);
  bool collect_draws(FieldMapRect, const FieldMapGateQuery &, size_t,
                     std::vector<uint32_t> &, std::string &) const;
  bool collision_polygons(FieldMapRect, uint32_t mask,
                          const FieldMapGateQuery &, size_t,
                          std::vector<uint32_t> &, std::string &) const;
  FieldMapCanvas canvas(uint32_t index) const;
  FieldMapLayer map(uint32_t index) const;
  FieldMapDraw draw(uint32_t index) const;
  FieldMapPolygon polygon(uint32_t index) const;
  FieldMapShapeTransform shape_transform(uint32_t index) const;
  Vec2 point(uint32_t polygon_index, uint32_t point_in_polygon) const;
  Vec2 sort_anchor(uint32_t draw_index) const;
  const FieldMapView *source() const { return source_; }
  const HouseReentryData *house_source() const { return house_; }

private:
  struct CanvasState {
    uint32_t parent = 0, order = 0;
    Vec2 local{}, world{};
    bool attached = true, visible = true, deleted = false;
  };
  struct LayerState {
    uint32_t layer = 0, mask = 0;
    Vec2 delta{};
    bool moved = false;
  };
  bool active(uint32_t, const FieldMapGateQuery &, bool drawing, bool &,
              std::string &) const;
  bool query(FieldMapRect, uint32_t, const FieldMapGateQuery &, bool drawing,
             size_t, std::vector<uint32_t> &, std::string &) const;
  const FieldMapView *source_ = nullptr;
  const FieldSceneActionsData *actions_ = nullptr;
  const HouseReentryData *house_ = nullptr;
  FieldNodeTreeRuntime *house_tree_ = nullptr;
  std::vector<uint64_t> house_tiles_;
  std::vector<CanvasState> canvases_;
  std::vector<LayerState> layers_;
  std::unordered_map<uint32_t, uint32_t> canvas_ids_, map_ids_;
};
} // namespace encore::upstream
