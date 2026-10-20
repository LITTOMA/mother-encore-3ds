#include "encore/dungeon_rooms.hpp"
#include "encore/field_camera.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error,label;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Dungeon rooms %s %u: %s (%s)\n",label.c_str(),checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==4);DungeonRoomsData data;std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,65536,error));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.rooms().size()==18&&data.scene()==4&&data.floor()==0&&!data.source().empty());
 CHECK(!data.load(bytes.data(),bytes.size(),error)&&data.valid());DungeonRoomsData empty;CHECK(!empty.valid()&&empty.scene()==0&&empty.rooms().empty()&&empty.name().empty()&&!empty.room(1));
 for(size_t n=0;n<bytes.size();++n){DungeonRoomsData trial;CHECK(!trial.load(bytes.data(),n,error)&&!trial.valid()&&trial.rooms().empty());}
 auto reject=[&](std::vector<uint8_t>b){DungeonRoomsData bad;sign(b);CHECK(!bad.load(b.data(),b.size(),error)&&!bad.valid()&&bad.rooms().empty());};
 const auto policy=get(bytes,84),rows=get(bytes,100),pool=get(bytes,68);
 for(const auto&p:std::vector<std::pair<size_t,uint32_t>>{{8,2},{12,0},{20,0},{20,2},{24,0},{24,5},{28,1},{32,0},{52,2},{56,1},{60,1},{68,0},{72,0},{76,2},{84,0},{88,2},{92,60},{100,0},{104,0},{104,129},{108,76},{policy,0},{policy+4,128},{policy+4,0xffffff7f},{policy+32,100000001},{policy+32,10000000},{policy+48,1},{rows,0},{rows+4,1},{rows+24,0x7fc00000},{rows+32,0},{rows+40,0x3f800000},{rows+56,0x100000},{rows+60,0x100000},{rows+64,4},{rows+68,1},{rows+72,1},{rows+76,1}}){auto b=bytes;put(b,p.first,p.second);reject(b);}
 {auto b=bytes;put(b,rows+80,get(bytes,rows));reject(b);}
 {auto b=bytes;put(b,rows+80+8,get(bytes,rows+8));put(b,rows+80+12,get(bytes,rows+12));reject(b);}
 for(uint8_t v:{uint8_t(0),uint8_t(':'),uint8_t(255)}){auto b=bytes;b[pool]=v;reject(b);}
 std::ifstream geometry(argv[3]);CHECK(geometry);unsigned view,index,layer,mask,monitoring,monitorable,visible,geometries=0;float x,y,w,h;
 while(geometry>>view>>index>>x>>y>>w>>h>>layer>>mask>>monitoring>>monitorable>>visible){CHECK(view<2&&index<data.rooms().size());const auto&r=data.rooms()[index];CHECK(r.index==index&&r.center.x==x&&r.center.y==y&&r.size.x==w&&r.size.y==h&&r.layer==layer&&r.mask==mask&&r.bits==(monitoring|(monitorable<<1))&&!visible);++geometries;}
 CHECK(geometry.eof()&&geometries==36);
 DungeonRoomState state;std::vector<DungeonRoomFlag>flags;for(size_t i=0;i<data.rooms().size();++i)flags.push_back({data.rooms()[i].id,i==0});
 CHECK(!state.prepare({},1,flags,error)&&!state.prepare(data,0,flags,error)&&!state.prepare(data,1,{},error));
 {auto bad=flags;bad[1]=bad[0];CHECK(!state.prepare(data,1,bad,error)&&!state.valid());bad[1].owner=0;CHECK(!state.prepare(data,1,bad,error));}
 std::ifstream reference(argv[2]);CHECK(reference);std::string line;unsigned seen=0;uint64_t epoch=0;
 while(std::getline(reference,line)){
  unsigned vi,action,ri,active,signals,inside,visited,has_relative,offsets;int room;std::array<int32_t,4>limits;float ox,oy,rx,ry;std::istringstream in(line);
  CHECK(in>>vi>>action>>ri>>active>>room>>signals>>inside>>visited>>limits[0]>>limits[1]>>limits[2]>>limits[3]>>ox>>oy>>has_relative>>rx>>ry>>offsets);std::string trailing;CHECK(!(in>>trailing)&&vi<2&&ri<18);label=std::to_string(seen);
  const auto id=data.rooms()[ri].id;const Vec2 viewport=vi?Vec2{400,240}:Vec2{320,180};
  if(action==0){if(state.valid())CHECK(state.retire(epoch,error));++epoch;CHECK(state.prepare(data,epoch,flags,error)&&!state.prepare(data,epoch+1,flags,error));}
  else if(action==1)CHECK(state.enter(epoch,id,true,error));
  else if(action==2)CHECK(state.idle_frame(epoch,viewport,error));
  else if(action==3)CHECK(state.exit(epoch,id,true,error));
  else if(action==4)CHECK(state.enter(epoch,id,false,error)&&state.exit(epoch,id,false,error));
  else if(action==5)CHECK(state.enter(epoch,id,true,error)&&state.exit(epoch,id,true,error)&&state.enter(epoch,id,true,error));
  else CHECK(false);
  CHECK(state.active_count()==active&&state.current_index()==room&&state.limits()==limits&&state.offset().x==ox&&state.offset().y==oy);
  unsigned got_inside=0,got_visited=0,got_signals=0,got_offsets=0;
  for(size_t i=0;i<data.rooms().size();++i){got_inside|=unsigned(state.inside(data.rooms()[i].id))<<i;got_visited|=unsigned(state.visited(data.rooms()[i].id))<<i;}
  CHECK(got_inside==inside&&got_visited==visited);
  for(const auto&e:state.take_events()){CHECK(e.epoch==epoch&&data.room(e.owner));if(e.kind==DungeonRoomEventKind::ObjectFlag)++got_signals;if(e.kind==DungeonRoomEventKind::Offset)++got_offsets;}
  CHECK(got_signals==signals&&got_offsets==offsets);
  Vec2 relative{9,9};if(has_relative)CHECK(state.relative_position({7,11},relative,error)&&relative.x==rx&&relative.y==ry);
  else CHECK(!state.relative_position({7,11},relative,error)&&relative.x==9&&relative.y==9);
  const auto before=state.event_count(),pending=state.pending();
  CHECK(!state.enter(epoch+100,id,true,error)&&!state.exit(epoch+100,id,true,error)&&!state.idle_frame(epoch+100,viewport,error));
  CHECK(!state.enter(epoch,0,true,error)&&!state.exit(epoch,0,true,error));
  CHECK(!state.idle_frame(epoch,{0,240},error)&&!state.idle_frame(epoch,{std::numeric_limits<float>::quiet_NaN(),240},error));
  CHECK(state.pending()==pending&&state.event_count()==before&&state.active_count()==active&&state.limits()==limits);++seen;
 }
 CHECK(reference.eof()&&seen==140);CHECK(state.retire(epoch,error)&&!state.valid()&&!state.retire(epoch,error)&&!state.idle_frame(epoch,{400,240},error));CHECK(!state.prepare(data,epoch,flags,error)&&state.prepare(data,epoch+1,flags,error));++epoch;
 const auto id=data.rooms()[0].id;CHECK(!state.exit(epoch,id,true,error)&&state.enter(epoch,id,true,error));const auto pending=state.pending(),events=state.event_count();CHECK(!state.enter(epoch,id,true,error)&&state.pending()==pending&&state.event_count()==events);CHECK(state.exit(epoch,id,true,error));state.take_events();
 // Capacity failure keeps all entered/visited/room/limits and queued callbacks.
 for(unsigned i=1;i<512;++i){CHECK(state.enter(epoch,id,true,error)&&state.exit(epoch,id,true,error));state.take_events();}
 CHECK(state.pending()==512&&!state.enter(epoch,id,true,error)&&!state.inside(id)&&!state.active_count()&&state.pending()==512&&state.event_count()==0);CHECK(state.idle_frame(epoch,{400,240},error)&&state.pending()==0&&state.take_events().empty());
 auto all_visited=flags;for(auto&f:all_visited)f.visited=true;DungeonRoomState full;CHECK(full.prepare(data,epoch+1,all_visited,error));
 for(unsigned i=0;i<818;++i)CHECK(full.enter(epoch+1,id,true,error)&&full.exit(epoch+1,id,true,error)&&full.idle_frame(epoch+1,{400,240},error));
 CHECK(full.event_count()==4090&&full.enter(epoch+1,id,true,error)&&full.enter(epoch+1,data.rooms()[1].id,true,error)&&full.event_count()==4096);
 const size_t count=full.event_count(),callbacks=full.pending();const uint32_t current=full.current_owner();const auto old_limits=full.limits();
 CHECK(!full.idle_frame(epoch+1,{400,240},error)&&full.event_count()==count&&full.pending()==callbacks&&full.current_owner()==current&&full.limits()==old_limits&&full.active_count()==2);
 CHECK(!full.enter(epoch+1,data.rooms()[2].id,true,error)&&full.event_count()==count&&full.pending()==callbacks&&!full.inside(data.rooms()[2].id));
 // Common projection is the same routine called by the live FieldScene.
 FieldCamera area;area.center={-0.5f,-0.5f};area.width=100;area.height=100;auto bounds=field_camera_limits(area,{400,240});CHECK(bounds[0]==-200&&bounds[1]==-120&&bounds[2]==200&&bounds[3]==120);
 area.has_rect=true;area.left=-8.75f;area.top=-3.25f;bounds=field_camera_limits(area,{400,240});CHECK(bounds[0]==-8&&bounds[1]==-3&&bounds[2]==392&&bounds[3]==237);
 std::printf("Dungeon rooms: %u checks;140 original method/idle observations,36 hidden-parent native shapes,two viewports,visited/current/relative position,negative-limit truncation,ABA callbacks,codec negative cases,phase/capacity/epoch retirement; shared live-field camera projection\n",checks);
}
