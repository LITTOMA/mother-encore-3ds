// Offline static-map route planner for emulator input tests. It never writes a
// guest/session and does not model moving actors, scene events or input timing.
#include "encore/field_scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <queue>
#include <set>
using namespace encore::upstream;
struct At {int x,y;bool operator<(At b)const{return x<b.x||(x==b.x&&y<b.y);}bool operator==(At b)const{return x==b.x&&y==b.y;}};
struct Node {int f,g;At at;uint64_t order;bool operator<(const Node&b)const{return f>b.f||(f==b.f&&order>b.order);}};
int main(int argc,char**argv){
 if(argc!=9){std::fprintf(stderr,"room map enabled-flags-file start-x start-y goal-x goal-y grid(4/6/8) required\n");return 2;}
 std::string error;RoomData room;FieldMapData data;
 if(!room.load_file(argv[1],error)||!data.load_file(argv[2],error)){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
 float numbers[5];for(int i=0;i<5;++i){char*end=nullptr;numbers[i]=std::strtof(argv[4+i],&end);if(!end||end==argv[4+i]||*end||!std::isfinite(numbers[i])||std::abs(numbers[i])>100000)return 2;}
 const Vec2 start{numbers[0],numbers[1]},goal{numbers[2],numbers[3]};const int grid=int(numbers[4]);if(numbers[4]!=grid||(grid!=4&&grid!=6&&grid!=8))return 2;
 std::set<std::string>enabled,known;for(uint32_t i=0;i<room.view().flag_count();++i)known.insert(std::string(room.view().string(room.view().flag(i).name_string)));
 std::ifstream in(argv[3],std::ios::binary|std::ios::ate);if(!in||in.tellg()<0||in.tellg()>65536)return 2;in.seekg(0);std::string line;while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(!known.count(line)||!enabled.insert(line).second)return 2;}
 const auto map=data.view();if(map.scene_id()!=room.view().scene().stable_id){std::fprintf(stderr,"Room/map scene mismatch\n");return 2;}auto active=[&](FieldSpan span){for(uint32_t i=0;i<span.count;++i){auto c=map.condition(span.first+i);const auto name=std::string(map.text(c.flag));if(!known.count(name)){std::fprintf(stderr,"unknown condition\n");std::exit(2);}if(bool(enabled.count(name))!=c.expected)return false;}return true;};
 std::vector<bool>layers,bodies;for(uint32_t i=0;i<map.count(FieldMapSection::Layers);++i)layers.push_back(active(map.layer(i).conditions));for(uint32_t i=0;i<map.count(FieldMapSection::Bodies);++i)bodies.push_back(active(map.body(i).conditions));
 FieldCollision collision;StaticMotionSolver solver;if(!collision.bind(map,layers,bodies,error)||!solver.configure_room(room.view(),{})){std::fprintf(stderr,"%s\n",error.c_str());return 2;}solver.attach_source(&collision);
 std::vector<FieldBoundary>blocked;for(uint32_t i=0;i<map.count(FieldMapSection::Boundaries);++i){auto b=map.boundary(i);if(active(b.conditions)&&b.kind!=uint16_t(FieldBoundaryKind::Confirm))blocked.push_back(b);}
 auto world=[&](At p){return Vec2{start.x+grid*p.x,start.y+grid*p.y};};auto clear=[&](Vec2 p){for(const auto&b:blocked)if(std::abs(p.x-b.center.x)<=b.extents.x+8&&std::abs(p.y-b.center.y)<=b.extents.y+10)return false;return true;};
 const At origin{0,0},target{int(std::round((goal.x-start.x)/grid)),int(std::round((goal.y-start.y)/grid))};const int padding=512/grid;const At lo{std::min(0,target.x)-padding,std::min(0,target.y)-padding},hi{std::max(0,target.x)+padding,std::max(0,target.y)+padding};
 auto heuristic=[&](At p){return std::abs(target.x-p.x)+std::abs(target.y-p.y);};
 struct Seen {int g;At previous;};std::map<At,Seen>seen;std::priority_queue<Node>queue;uint64_t order=0;seen[origin]={0,origin};queue.push({heuristic(origin),0,origin,order++});bool found=false;unsigned expanded=0;
 while(!queue.empty()&&expanded<250000){auto n=queue.top();queue.pop();if(n.g!=seen.at(n.at).g)continue;if(n.at==target){found=true;break;}++expanded;
  for(auto delta:{At{1,0},At{0,1},At{-1,0},At{0,-1}}){At next{n.at.x+delta.x,n.at.y+delta.y};if(next.x<lo.x||next.x>hi.x||next.y<lo.y||next.y>hi.y)continue;auto dest=world(next);if(!clear(dest))continue;auto old=seen.find(next);if(old!=seen.end()&&old->second.g<=n.g+1)continue;SlideResult result;
   if(!solver.slide(world(n.at),{float(delta.x*grid*60),float(delta.y*grid*60)},result)||std::abs(result.position.x-dest.x)>.001f||std::abs(result.position.y-dest.y)>.001f)continue;
   seen[next]={n.g+1,n.at};queue.push({n.g+1+heuristic(next),n.g+1,next,order++});
  }
 }
 if(!found){std::fprintf(stderr,"No bounded static route after %u expanded nodes\n",expanded);return 1;}
 std::vector<At>path;for(At at=target;;at=seen.at(at).previous){path.push_back(at);if(at==origin)break;}std::reverse(path.begin(),path.end());std::vector<At>corners;
 for(size_t i=0;i<path.size();++i)if(i==0||i+1==path.size()||path[i].x-path[i-1].x!=path[i+1].x-path[i].x||path[i].y-path[i-1].y!=path[i+1].y-path[i].y)corners.push_back(path[i]);
 std::printf("{\"scope\":\"static collision plan only; guest must follow and validate normal inputs\",\"expanded\":%u,\"grid\":%d,\"waypoints\":[",expanded,grid);
 for(size_t i=0;i<corners.size();++i){auto p=world(corners[i]);std::printf("%s[%.9g,%.9g]",i?",":"",p.x,p.y);}std::puts("]}");
}
