#include "encore/field_objects.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool v,const char*s){++checks;if(!v){std::fprintf(stderr,"Field objects %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t x){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(x>>(8*i));}
void fix(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,1024*1024,error));FieldObjectsData d;CHECK(d.load(bytes.data(),bytes.size(),error));
 CHECK(d.valid()&&d.scene_id()==2&&d.width()==32&&d.height()==32&&d.objects().size()==1&&d.frames().size()==6&&d.animation_frames().size()==40);
 const auto original=d.texture();const auto&obj=d.objects()[0];CHECK(obj.id==1&&obj.kind==1&&obj.path=="Objects/Items/Item"&&obj.ready_flag=="got_shovel"&&obj.flags==255&&obj.ready_delay==1);
 CHECK(obj.position.x==1432&&obj.position.y==4664&&obj.main_offset.x==0&&obj.main_offset.y==0&&obj.effect_offset.x==4&&obj.effect_offset.y==-4);
 CHECK(obj.initial==22&&obj.count==40&&obj.first==0&&obj.fps==12&&obj.random_min==0&&obj.random_max==47);
 CHECK(d.frames()[0].width==20&&d.frames()[0].height==20);for(uint32_t i=1;i<6;++i)CHECK(d.frames()[i].width==7&&d.frames()[i].height==7);
 RoomData room;CHECK(room.load_file(argv[2],error)&&d.bind(room.view(),error));RoomData empty;CHECK(!d.bind(empty.view(),error)&&d.valid());
 const size_t pool=word(bytes,68),policy=word(bytes,84),object=word(bytes,100),rect=word(bytes,116),sequence=word(bytes,132);
 auto reject=[&](std::vector<uint8_t>b,bool checksum=true){if(checksum)fix(b);CHECK(!d.load(b.data(),b.size(),error));CHECK(d.valid()&&d.texture()==original&&d.objects()[0].path=="Objects/Items/Item");};
 CHECK(!d.load(nullptr,bytes.size(),error)&&d.valid());for(size_t n:{size_t(0),size_t(63),size_t(144),bytes.size()-1})CHECK(!d.load(bytes.data(),n,error)&&d.valid());
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{8,0},{8,2},{12,uint32_t(bytes.size()-1)},{20,0},{20,2},{24,0},{24,UINT32_MAX},{28,1},{52,4},{56,1},{60,1},{64,2},{68,UINT32_MAX},{72,0},{76,2},{88,2},{104,0},{104,129},{108,95},{120,4097},{136,8193},
 {policy,0},{policy,31},{policy,2048},{policy+4,0},{policy+8,1},{policy+12,1},{policy+16,UINT32_MAX},{policy+20,0},{policy+56,1},{policy+60,1},
 {object,0},{object,UINT32_MAX},{object+4,0},{object+4,2},{object+8,256},{object+12,6},{object+16,UINT32_MAX},{object+20,0},{object+20,41},{object+24,40},{object+28,1},{object+32,UINT32_MAX},{object+36,0},{object+40,UINT32_MAX},{object+44,0},
 {object+48,0x7fc00000},{object+52,0x7f800000},{object+56,0x7fc00000},{object+64,0x7fc00000},{object+72,0},{object+72,1},{object+72,0x7fc00000},{object+76,0xbf800000},{object+80,0},{object+80,0x7f800000},{object+84,0},{object+84,2},{object+88,1},{object+92,1},
 {rect,UINT32_MAX},{rect+4,32},{rect+8,0},{rect+8,UINT32_MAX},{rect+12,33},{sequence,6}}){auto b=bytes;put(b,edit.first,edit.second);reject(b);}
 {auto b=bytes;b[0]^=1;reject(b);}{auto b=bytes;b[32]^=1;reject(b);}{auto b=bytes;b.back()^=1;reject(b,false);}
 {auto b=bytes;for(unsigned i=0;i<32;++i)b[policy+24+i]=0;reject(b);}
 for(char c:{'/',':','\\','\0'}){auto b=bytes;const auto start=word(bytes,object+32);b[pool+start]=uint8_t(c);reject(b);}
 {auto b=bytes;const auto start=word(bytes,object+40);b[pool+start]='/';reject(b);}
 {auto b=bytes;put(b,object+8,255^uint32_t(FieldObjectFlag::RandomFrame));reject(b);}
 {auto b=bytes;put(b,object+8,255^uint32_t(FieldObjectFlag::SuppressAtReady));reject(b);}
 {auto b=bytes;for(size_t at=sequence;at<b.size();at+=4)put(b,at,1);reject(b);}
 {auto b=bytes;put(b,object+20,39);reject(b);}
 // Two records must not alias a stable owner or source node path.
 {auto b=bytes;std::vector<uint8_t>copy(bytes.begin()+object,bytes.begin()+object+96);b.insert(b.begin()+rect,copy.begin(),copy.end());put(b,12,uint32_t(b.size()));put(b,104,2);put(b,116,uint32_t(rect+96));put(b,132,uint32_t(sequence+96));reject(b);put(b,object+96,2);reject(b);}
 // Canonical layout rejects padding payload and unexplained trailing bytes.
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b);}
 {auto b=bytes;const auto end=pool+word(bytes,72);CHECK(end<=policy);if(end<policy){b[end]=1;reject(b);}}
 std::printf("Field object metadata: %u checks; actual source visual/ready fields, scope binding, unknown/missing/version/type/range/finite/unconsumed content and transactional rejection; no lifecycle/frontend claim\n",checks);
}
