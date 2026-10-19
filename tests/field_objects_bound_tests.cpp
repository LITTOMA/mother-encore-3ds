#include "encore/field_objects.hpp"
#include "encore/field_scene.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Bound objects %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t x){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(x>>(8*i));}
void put_real(std::vector<uint8_t>&b,size_t at,float x){uint32_t bits;std::memcpy(&bits,&x,4);put(b,at,bits);}
void fix(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==5);std::vector<uint8_t>bytes,map_bytes;CHECK(encore::read_file(argv[1],bytes,1024*1024,error)&&encore::read_file(argv[2],map_bytes,8*1024*1024,error));
 FieldObjectsData data,old;FieldMapData map;RoomData room;CHECK(data.load(bytes.data(),bytes.size(),error)&&map.load(map_bytes.data(),map_bytes.size(),error)&&room.load_file(argv[3],error)&&old.load_file(argv[4],error));
 CHECK(data.capabilities()==2&&data.rendering_bound()&&map.view().capabilities()==6&&data.ready_state()==621950984495336987ULL);CHECK(data.bind(room.view(),map.view(),error));CHECK(!old.bind(room.view(),map.view(),error));
 {WorldLinksData links;CHECK(links.load_file("romfs/world/world.enclinks",error));FieldScene scene;CHECK(!scene.prepare(room.view(),map.view(),links,std::vector<bool>(room.view().flag_count(),false),{1432,4719},{0,-1},{400,240},error)&&error.find("not yet admitted")!=error.npos);}
 const auto&d=data.objects()[0];CHECK(d.resolved==6&&d.initial==22&&d.flags==511&&d.body_extents.x==9&&d.body_extents.y==8&&d.body_center.x==1432&&d.body_center.y==4664);
 CHECK(d.body==kFieldNone&&d.body_layer==4&&d.body_mask==0&&data.player_layer()==1&&data.player_mask()==4353);
 CHECK(map.view().item(d.draw).first==d.id&&map.view().item(d.draw).kind==7&&map.view().text(map.view().notice(d.notice).path)==d.path);
 FieldObjects objects;SourceRandom random(9);const auto untouched=random.state();CHECK(objects.prepare(data,room.view(),error));CHECK(!objects.enter_ready(1,false,random,error)&&random.state()==untouched&&!objects.state(1)->ready);
 for(bool flag:{false,true}){
  CHECK(objects.prepare(data,room.view(),error)&&objects.enter_ready(1,flag,error)&&random.state()==untouched);CHECK(objects.state(1)->frame==6&&!objects.state(1)->visible&&objects.state(1)->alive&&objects.state(1)->queued==flag);
  CHECK(!objects.enter_ready(1,flag,error)&&objects.idle_frame(error));CHECK(objects.state(1)->visible==!flag);
  if(!flag){CHECK(objects.advance_idle(1./60,error)&&objects.state(1)->alive);CHECK(objects.queue_free(d.path,error)&&objects.state(1)->alive&&objects.state(1)->visible);}
  CHECK(objects.flush_queued(error)&&!objects.state(1)->alive&&!objects.state(1)->visible&&random.state()==untouched);
 }
 CHECK(objects.prepare(old,room.view(),error)&&!objects.enter_ready(1,false,error)&&!objects.state(1)->ready); // legacy still requires its live draw
 const size_t at=word(bytes,100);auto reject=[&](std::vector<uint8_t>b){fix(b);CHECK(!data.load(b.data(),b.size(),error)&&data.valid()&&data.objects()[0].resolved==6);};
 const size_t policy=word(bytes,84);
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{20,0},{20,1},{20,3},{108,96},{policy+56,0},{policy+60,1u<<20},{at+8,255},{at+8,1023},{at+8,509},{at+28,0},{at+28,41},{at+96,0},{at+100,kFieldNone},{at+104,kFieldNone},{at+108,1},{at+112,0x7fc00000},{at+116,0x7f800000},{at+120,0},{at+124,0xbf800000},{at+128,1},{at+128,1u<<20},{at+132,1u<<20}}){auto b=bytes;put(b,edit.first,edit.second);reject(b);}
 // Valid binary metadata with wrong joins must fail binding without replacing
 // the accepted object owner. Every map identity is checked independently.
 for(size_t delta:{size_t(100),size_t(104),size_t(48)}){
  auto b=bytes;put(b,at+delta,delta==48?0x3f800000:UINT32_MAX-1);fix(b);FieldObjectsData other;CHECK(other.load(b.data(),b.size(),error)&&!other.bind(room.view(),map.view(),error));
 }
 {auto b=bytes;put(b,policy+60,4357);put(b,at+96,map.view().count(FieldMapSection::Bodies));fix(b);FieldObjectsData other;CHECK(other.load(b.data(),b.size(),error)&&!other.bind(room.view(),map.view(),error));}
 {auto b=bytes;put(b,policy+60,4357);put(b,at+96,0);fix(b);FieldObjectsData other;CHECK(other.load(b.data(),b.size(),error)&&!other.bind(room.view(),map.view(),error));}
 // Exercise the optional colliding-body schema with a synthetic binding to
 // one already admitted rectangle. This does not change source Shovel data.
 bool rectangle_tested=false;
 for(uint32_t i=0;i<map.view().count(FieldMapSection::Bodies)&&!rectangle_tested;++i){const auto body=map.view().body(i);const auto s=map.view().shape(body.shape_first);
  if(body.shape_count!=1||body.conditions.count||s.kind!=uint16_t(FieldShapeKind::Convex)||s.count!=4)continue;
  bool rectangle=true;for(uint32_t k=0;k<4;++k){const auto p=map.view().point(s.first+k);rectangle&=(p.x==s.minimum.x||p.x==s.maximum.x)&&(p.y==s.minimum.y||p.y==s.maximum.y);}if(!rectangle)continue;
  auto b=bytes;put(b,policy+60,4357);put(b,at+96,i);put_real(b,at+112,(s.minimum.x+s.maximum.x)/2);put_real(b,at+116,(s.minimum.y+s.maximum.y)/2);put_real(b,at+120,(s.maximum.x-s.minimum.x)/2);put_real(b,at+124,(s.maximum.y-s.minimum.y)/2);fix(b);FieldObjectsData other;CHECK(other.load(b.data(),b.size(),error)&&other.bind(room.view(),map.view(),error));
  put_real(b,at+120,(s.maximum.x-s.minimum.x)/2+1);fix(b);CHECK(other.load(b.data(),b.size(),error)&&!other.bind(room.view(),map.view(),error));rectangle_tested=true;
 }
 CHECK(rectangle_tested);
 const size_t items=word(map_bytes,64+13*16+4),record=items+d.draw*32;
 auto reject_map=[&](std::vector<uint8_t>b){fix(b);CHECK(!map.load(b.data(),b.size(),error)&&map.view().capabilities()==6);};
 for(uint32_t cap:{0u,1u,2u,3u,4u,5u,7u}){auto b=map_bytes;put(b,20,cap);reject_map(b);}
 for(uint32_t id:{0u,kFieldNone}){auto b=map_bytes;put(b,record+4,id);reject_map(b);}
 {auto b=map_bytes;put(b,record+8,2);reject_map(b);}
 {auto b=map_bytes;put(b,record,6);put(b,record+4,63);reject_map(b);} // canonical6 needs an object
 {auto b=map_bytes;put(b,record,8);reject_map(b);}
 {auto b=map_bytes;put(b,record+4,2);fix(b);FieldMapData other;CHECK(other.load(b.data(),b.size(),error)&&!data.bind(room.view(),other.view(),error));}
 CHECK(data.bind(room.view(),map.view(),error));
 std::printf("Bound field objects: %u checks; source map geometry/draw/notice joins, offline ready sample without live RNG, suppression/deferred removal, legacy and unknown-version rejection; unpublished, no frontend claim\n",checks);
}
