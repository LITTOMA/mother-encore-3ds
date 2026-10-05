#pragma once
#include "encore/collision.hpp"
#include "encore/field_map.hpp"
#include "encore/scene_motion.hpp"

namespace encore::upstream {
// Static TileMap collision only. The scene host additionally owns actor/NPC
// bodies, Ready/deletion, Areas, cached rays and the shared source event order.
class FieldMapMotion final : public SceneMotionBackend {
public:
    bool configure(const FieldMapView&,const RoomView& actor_source,uint32_t collision_mask,
                   FieldMapGateQuery,std::string&);
    bool admitted()const override{return map_&&map_->valid()&&solver_.valid();}
    std::string_view source_scene()const override{return map_?map_->source_scene():std::string_view{};}
    std::string_view reviewed_commit()const override{return commit_;}
    bool slide(Vec2,Vec2,SlideResult&)const override;
    const std::string&error()const{return error_;}
private:
    bool geometry(FieldMapRect)const;
    const FieldMapView*map_=nullptr;
    ConvexPolygon actor_;Vec2 minimum_{},maximum_{};
    uint32_t mask_=0;float margin_=0;
    FieldMapGateQuery gates_;
    std::string commit_;
    mutable StaticMotionSolver solver_;
    mutable std::vector<uint32_t> polygons_,pairs_;
    mutable std::vector<StaticObstacle> obstacles_;
    mutable std::string error_;
};
}
