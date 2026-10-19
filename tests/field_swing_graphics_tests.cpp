#include "encore/field_swing_graphics.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
void check(bool ok,const char*s){++checks;if(!ok){std::fprintf(stderr,"Swing graphics %u: %s (%s)\n",checks,s,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3||argc==5);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,64*1024,error));FieldSwingGraphicsData data;FieldSwingData action,empty;CHECK(!data.valid()&&!data.frame(4)&&!data.bind(empty,error));CHECK(action.load_file(argv[2],error));CHECK(!data.bind(action,error));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.valid()&&data.bind(action,error));CHECK(!data.bind(empty,error));CHECK(data.frames().size()==40&&data.meta().initial_frame==4);CHECK(data.meta().frame_width==64&&data.meta().frame_height==64&&data.meta().source_columns==10&&data.meta().source_rows==10&&data.meta().atlas_columns==5);
 CHECK(data.layers()[0].path=="graphics/world/player/swing/ninten.t3x"&&data.layers()[1].path=="graphics/world/player/swing/bat.t3x");CHECK(data.layers()[0].position.y==-4&&data.layers()[0].flags==1&&data.layers()[1].flags==9);CHECK(data.layers()[0].width==320&&data.layers()[0].height==512&&data.layers()[0].parent==0&&data.layers()[1].parent==1);
 for(uint32_t i=0;i<40;++i){const auto id=(i/5)*10+i%5;const auto*f=data.frame(id);CHECK(f&&f->x==i%5*64&&f->y==i/5*64&&f->width==64&&f->height==64);}CHECK(!data.frame(99));
 auto unchanged=[&](){CHECK(data.valid()&&data.frames().size()==40&&data.meta().initial_frame==4&&data.layers()[0].position.y==-4&&data.bind(action,error));};
 auto reject=[&](std::vector<uint8_t>b){seal(b);CHECK(!data.load(b.data(),b.size(),error));unchanged();};auto mutation=[&](size_t at,uint32_t value){auto b=bytes;put(b,at,value);reject(std::move(b));};
 for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));unchanged();}CHECK(!data.load(nullptr,bytes.size(),error));CHECK(!data.load(bytes.data(),65537,error));unchanged();
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,0},{8,2},{12,0},{20,0},{20,2},{24,1},{28,1},{32,0},{52,3},{56,1},{60,1},{64,2},{68,0},{68,129},{72,0},{72,2},{76,1},{80,1},{84,0xffffffff},{88,1},{88,3},{92,1},{104,0},{104,1025},{108,1},{116,0},{120,2049},{124,2}})mutation(r.first,r.second);
 {auto b=bytes;b[16]^=1;CHECK(!data.load(b.data(),b.size(),error));unchanged();}
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b);}
 const auto meta=get(bytes,68),layers=get(bytes,84),frames=get(bytes,100),strings=get(bytes,116);
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,100},{4,0},{4,1025},{8,0},{12,0},{12,257},{16,0},{16,257},{20,0},{20,17},{24,39},{28,1}})mutation(meta+r.first,r.second);
 {auto b=bytes;put(b,meta+12,256);put(b,meta+16,256);reject(b);}
 {auto b=bytes;put(b,meta,99);seal(b);FieldSwingGraphicsData other;CHECK(!other.load(b.data(),b.size(),error));}
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,0},{0,2},{4,321},{8,511},{12,1},{16,16},{20,1},{24,0x7fc00000},{28,0x7f800000},{32,0x45800800},{40,1},{44,0},{44,513},{48,0},{52,0xffffffff},{120,0},{120,8*1024*1024+1},{128,1},{128+12,0}})mutation(layers+r.first,r.second);
 for(size_t at:{size_t(56),size_t(88)}){auto b=bytes;for(unsigned i=0;i<32;++i)b[layers+at+i]=0;reject(b);}
 for(auto r:std::vector<std::pair<size_t,uint32_t>>{{0,100},{4,1},{8,0},{12,0},{12+4,0}})mutation(frames+r.first,r.second);
 for(unsigned char c:std::array<unsigned char,4>{{0,':','\\',127}}){auto b=bytes;b[strings+10]=c;reject(b);}
 // Well-formed metadata from a different source still cannot bind this action.
 for(auto at:std::vector<size_t>{size_t(layers+28),size_t(layers+16),size_t(frames+19*12)}){auto b=bytes;if(at==layers+28)put(b,at,0);else if(at==layers+16)put(b,at,9);else put(b,at,35);seal(b);FieldSwingGraphicsData other;CHECK(other.load(b.data(),b.size(),error));CHECK(!other.bind(action,error));unchanged();}
 {auto b=bytes;const auto source=strings+get(b,layers+48);b[source+get(b,layers+52)-5]='X';seal(b);FieldSwingGraphicsData other;CHECK(other.load(b.data(),b.size(),error));CHECK(!other.bind(action,error));}
 // The integrity API deliberately accepts opaque bytes; it does not certify a
 // T3X format. Genuine converted files are checked separately when supplied.
 const std::vector<uint8_t>payload{1,2,3};auto b=bytes;put(b,layers+120,3);put(b,layers+124,encore::crc32(payload.data(),payload.size()));seal(b);FieldSwingGraphicsData integrity;CHECK(integrity.load(b.data(),b.size(),error));CHECK(integrity.texture_matches(1,payload.data(),payload.size(),error));CHECK(!integrity.texture_matches(0,payload.data(),payload.size(),error));CHECK(!integrity.texture_matches(3,payload.data(),payload.size(),error));CHECK(!integrity.texture_matches(1,nullptr,3,error));CHECK(!integrity.texture_matches(1,payload.data(),2,error));auto corrupt=payload;corrupt[1]^=1;CHECK(!integrity.texture_matches(1,corrupt.data(),corrupt.size(),error));
 if(argc==5)for(unsigned role=1;role<=2;++role){std::vector<uint8_t>actual;CHECK(encore::read_file(argv[role+2],actual,8*1024*1024,error));CHECK(data.texture_matches(role,actual.data(),actual.size(),error));actual.back()^=1;CHECK(!data.texture_matches(role,actual.data(),actual.size(),error));}
 std::printf("Swing graphics: %u checks; source frame/layer binding, malformed schemas/spans, atomic rollback and texture integrity; genuine textures supplied=%u; no draw claim\n",checks,unsigned(argc==5));
}
