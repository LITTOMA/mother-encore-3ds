// Caller supplies the actually admitted source space; no fixture Ready grants.
#include "encore/field_geometry_space.hpp"
#include "encore/field_scene_actions.hpp"
#include <cassert>
using namespace encore::upstream;
void field_live_polygon_manual_cases(FieldGeometrySpace&space,
    const FieldSceneActionsData&actions,uint32_t source_owner,uint32_t leaf,
    uint32_t tile_parent,uint32_t append_order){
 std::string error;bool attached=false,disabled=false;std::vector<std::vector<Vec2>>parts;
 assert(space.live_polygon_parts(source_owner,leaf,attached,disabled,parts,error));assert(attached&&!parts.empty());
 assert(!space.attach_leaf_polygon(actions,leaf,0,append_order,error));
 assert(!space.detach_leaf_polygon(actions,0,error));
 assert(space.detach_leaf_polygon(actions,leaf,error));
 assert(space.live_polygon_parts(source_owner,leaf,attached,disabled,parts,error)&&!attached&&parts.empty());
 assert(space.attach_leaf_polygon(actions,leaf,tile_parent,append_order,error));
 assert(space.live_polygon_parts(source_owner,leaf,attached,disabled,parts,error)&&!attached&&parts.empty());
 assert(space.detach_leaf_polygon(actions,leaf,error));
 assert(space.attach_leaf_polygon(actions,leaf,source_owner,append_order,error));
 assert(space.live_polygon_parts(source_owner,leaf,attached,disabled,parts,error)&&attached&&!parts.empty());
 auto previous=parts;assert(!space.live_polygon_parts(0,leaf,attached,disabled,parts,error));assert(parts.size()==previous.size());
}
