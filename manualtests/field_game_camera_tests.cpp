// Handwritten source/format cases. Manual only; this file is compiled, not run.
#include "encore/field_game_camera.hpp"
#include <cassert>
#include <cstring>
#include <vector>
using namespace encore::upstream;
void field_game_camera_parser_cases(const std::vector<uint8_t>&bytes,const FieldIdentity&id){FieldGameCameraData d;std::string e;assert(d.load(bytes.data(),bytes.size(),id,e));auto old=d.records().size();assert(old);for(size_t offset:{size_t(0),size_t(8),size_t(24),size_t(28),size_t(36),size_t(40),size_t(60),size_t(124),size_t(128)}){auto b=bytes;b[offset]^=1;assert(!d.load(b.data(),b.size(),id,e));assert(d.valid()&&d.records().size()==old);}assert(!d.load(bytes.data(),127,id,e));assert(!d.load(bytes.data(),bytes.size()-1,id,e));auto extra=bytes;extra.push_back(0);assert(!d.load(extra.data(),extra.size(),id,e));assert(!d.scene_admitted());}
void field_game_camera_lifecycle_cases(FieldGameCameraRuntime&r,uint32_t id){Vec2 before=r.state(id)->position;assert(r.reset(id));assert(r.state(id)->position.x==before.x&&r.state(id)->position.y==before.y);int64_t result=0;assert(r.adjust_camareas(id,1,result));assert(result==1);assert(r.adjust_camareas(id,-1,result));assert(result==0);assert(r.set_camarea_offset(id,{4,5}));Vec2 off;assert(r.get_offset_with_camerea_offset(id,off));assert(off.x==8&&off.y==10);uint64_t token;assert(r.return_offset(id,0,token));assert(r.step_tween(token,0));assert(r.tweens().at(token).running);assert(r.step_tween(token,.016f));assert(!r.tweens().at(token).running);}
