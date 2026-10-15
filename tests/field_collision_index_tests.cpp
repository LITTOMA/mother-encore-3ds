#include "encore/field_scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;uint64_t old_cells=0;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Collision cache: %s\n",why);std::exit(1);}}
bool overlap(Vec2 a,Vec2 b,Vec2 c,Vec2 d){return a.x<=d.x&&b.x>=c.x&&a.y<=d.y&&b.y>=c.y;}
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
struct Expected {uint32_t shape;Vec2 offset;};
// Retain the previous uncached enumeration as the comparison oracle. In
// particular, this visits decorative cells in every empty origin-bound chunk.
std::vector<Expected> reference(FieldMapView map,const std::vector<bool>&layers,const std::vector<bool>&bodies,Vec2 lo,Vec2 hi){
 std::vector<Expected> out;
 auto push=[&](uint32_t i,Vec2 offset){const auto s=map.shape(i);if(overlap(lo,hi,{s.minimum.x+offset.x,s.minimum.y+offset.y},{s.maximum.x+offset.x,s.maximum.y+offset.y}))out.push_back({i,offset});};
 for(uint32_t i=0;i<layers.size();++i){if(!layers[i])continue;const auto l=map.layer(i);if(!(l.flags&uint32_t(FieldLayerFlag::Collides)))continue;
  for(uint32_t j=0;j<l.chunk_count;++j){const auto c=map.chunk(l.chunk_first+j);if(!overlap(lo,hi,c.minimum,c.maximum))continue;
   for(uint32_t k=0;k<c.count;++k){++old_cells;const auto cell=map.cell(c.first+k);const auto t=map.tile(cell.tile);for(uint32_t n=0;n<t.shape_count;++n)push(t.shape_first+n,{l.position.x+float(cell.x)*16.f,l.position.y+float(cell.y)*16.f});}
  }
 }
 for(uint32_t i=0;i<bodies.size();++i)if(bodies[i]){const auto b=map.body(i);for(uint32_t n=0;n<b.shape_count;++n)push(b.shape_first+n,{});}
 return out;
}
void compare(FieldCollision& collision,FieldMapView map,const std::vector<bool>&layers,const std::vector<bool>&bodies,Vec2 lo,Vec2 hi){
 const auto expected=reference(map,layers,bodies,lo,hi);std::vector<MotionObstacle> out;
 check(collision.collect(lo,hi,out)==(expected.size()<=4096),"unchanged candidate budget result");check(out.size()==expected.size(),"same obstacle count");
 for(size_t i=0;i<out.size();++i){const auto s=map.shape(expected[i].shape);const auto&v=out[i];const auto offset=expected[i].offset;
  check(v.count==s.count&&v.segment==(s.kind==uint16_t(FieldShapeKind::Segment))&&same(v.offset,offset),"same source shape order and offsets");
  check(same(v.minimum,{s.minimum.x+offset.x,s.minimum.y+offset.y})&&same(v.maximum,{s.maximum.x+offset.x,s.maximum.y+offset.y}),"same shape bounds");
  std::vector<Vec2> points;for(uint32_t k=0;k<s.count;++k)points.push_back(map.point(s.first+k));
  if(!v.segment){double area=0;for(size_t k=0;k<points.size();++k){const auto a=points[k],b=points[(k+1)%points.size()];area+=double(b.x-a.x)*double(b.y+a.y);}if(points.size()>=3&&area>0)std::reverse(points.begin(),points.end());}
  for(uint32_t k=0;k<s.count;++k)check(same(v.points[k],points[k]),"same normalized points");
  if(v.segment)check(same(v.normals[0],s.normal),"same segment normal");
  else for(uint32_t k=0;k<s.count;++k){const auto a=points[k],b=points[(k+1)%points.size()];Vec2 normal{b.y-a.y,-(b.x-a.x)};const float d=std::sqrt(normal.x*normal.x+normal.y*normal.y);if(d)normal={normal.x/d,normal.y/d};check(same(v.normals[k],normal),"same convex normals");}
 }
}
}
int main(int argc,char**argv){
 check(argc==2,"checked map argument required");std::string error;FieldMapData data;check(data.load_file(argv[1],error),error.c_str());const auto map=data.view();
 std::vector<bool> layers(map.count(FieldMapSection::Layers),true),bodies(map.count(FieldMapSection::Bodies),true);FieldCollision collision;
 check(!collision.bind({},layers,bodies,error),"invalid map rejected");auto bad=layers;bad.push_back(true);check(!collision.bind(map,bad,bodies,error),"wrong condition table rejected");
 check(collision.bind(map,layers,bodies,error),error.c_str());
 compare(collision,map,layers,bodies,{-1,-1},{1,1});check(old_cells>100000,"fixture reproduces origin scan of decorative cells");
 // Around both sides of the origin, the yard, chunk edges and distant terrain.
 for(int y=-1024;y<=1024;y+=128)for(int x=-1024;x<=1024;x+=128)compare(collision,map,layers,bodies,{float(x)-20,float(y)-20},{float(x)+20,float(y)+20});
 for(uint32_t i=0;i<map.count(FieldMapSection::Chunks);++i){const auto c=map.chunk(i);if(same(c.minimum,{})&&same(c.maximum,{}))continue;compare(collision,map,layers,bodies,c.minimum,c.minimum);compare(collision,map,layers,bodies,c.maximum,c.maximum);}
 compare(collision,map,layers,bodies,{-1000000,-1000000},{1000000,1000000});
 std::vector<MotionObstacle> extra;check(collision.collect({-1,-1},{1,1},extra),"origin base query");const auto count=extra.size();collision.set_body({0,0},{1,1});extra.clear();check(collision.collect({-1,-1},{1,1},extra)&&extra.size()==count+1,"live actor remains last");collision.set_extra_enabled(false);extra.clear();check(collision.collect({-1,-1},{1,1},extra)&&extra.size()==count,"live actor disable preserved");
 std::fill(layers.begin(),layers.end(),false);std::fill(bodies.begin(),bodies.end(),false);check(collision.bind(map,layers,bodies,error),"rebind inactive conditions");extra.clear();check(collision.collect({-1000000,-1000000},{1000000,1000000},extra)&&extra.empty(),"rebind removes old cache and actor");
 check(collision.bind(map,std::vector<bool>(layers.size(),true),std::vector<bool>(bodies.size(),true),error),"rebuild full cache");
 std::printf("Collision cache PASS: %u checks; reference decorative/collision cell visits %llu; source order, geometry, conditions, actor and budget preserved\n",checks,(unsigned long long)old_cells);
}
