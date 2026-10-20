#pragma once
#include "encore/field_data.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace encore::upstream {
// Post-idle Camarea bounds. Caller supplies a checked FieldCamera and finite
// positive viewport. Source Vector2 components promote to GDScript binary64;
// native Camera2D integer setters truncate after viewport size expansion.
inline std::array<double,4> field_camera_limits(const FieldCamera&area,Vec2 viewport){
 const double width=std::max(double(area.width),double(viewport.x));
 const double height=std::max(double(area.height),double(viewport.y));
 const double left=std::trunc(area.has_rect?double(area.left):double(area.center.x)-width*.5);
 const double top=std::trunc(area.has_rect?double(area.top):double(area.center.y)-height*.5);
 return{left,top,std::trunc(left+width),std::trunc(top+height)};
}
}
