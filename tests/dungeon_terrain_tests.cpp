#include "encore/dungeon_terrain.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Terrain line%d:%s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void word(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){word(b,16,encore::crc32(b.data()+32,b.size()-32));}
size_t section(const std::vector<uint8_t>&b,unsigned id){return u(b.data()+64+(id-1)*16+4);}
}
int main(int argc,char**argv){CHECK(argc==4);std::vector<uint8_t>blob;CHECK(encore::read_file(argv[1],blob,8*1024*1024,error));DungeonTerrainData data;CHECK(data.load(blob.data(),blob.size(),error));CHECK(data.valid()&&data.graphics().terrain_only()&&data.info().scene==data.graphics().scene_id());const auto before=data.graphics().bytes_identity();
 CHECK(!data.load(nullptr,blob.size(),error));CHECK(!data.load(blob.data(),8*1024*1024+1,error));for(size_t n=0;n<blob.size();++n)CHECK(!data.load(blob.data(),n,error)&&data.graphics().bytes_identity()==before);
 auto invalid=[&](std::vector<uint8_t>b){CHECK(!data.load(b.data(),b.size(),error)&&data.graphics().bytes_identity()==before);};
 auto change=[&](size_t at,uint32_t v){auto b=blob;word(b,at,v);seal(b);invalid(std::move(b));};
 for(unsigned at:{8u,20u,24u,28u,52u,56u,60u})change(at,99);change(32,0);
 for(unsigned i=0;i<9;++i){const size_t at=64+i*16;change(at,99);change(at+4,1);change(at+8,0xffffffffu);change(at+12,99);}
 {auto b=blob;b.back()^=1;invalid(std::move(b));}{auto b=blob;b.push_back(0);word(b,12,uint32_t(b.size()));seal(b);invalid(std::move(b));}
 const auto meta=section(blob,3),layers=section(blob,4),cells=section(blob,5),defs=section(blob,6),nav=section(blob,7),points=section(blob,8),indices=section(blob,9),graphics=section(blob,2);
 for(auto item:std::vector<std::pair<size_t,uint32_t>>{{meta,0xffffffffu},{meta+16,0},{meta+20,2},{layers,0},{layers+4,99},{layers+40,4},{layers+44,1},{layers+64,1},{layers+68,0},{layers+72,99},{layers+76,1},{cells,99},{cells+8,1u<<29},{cells+16,4},{cells+20,0xffffffffu},{cells+24,0xffffffffu},{cells+28,1},{defs+48,3},{defs+52,0xffffffffu},{defs+56,1},{defs+64,1},{defs+68,1},{nav,0xffffffffu},{nav+4,1},{nav+8,2},{nav+12,1},{nav+16,2},{nav+28,1},{indices,99}})change(item.first,item.second);
 for(unsigned at:{16u,20u,24u,28u})change(layers+at,0x7fc00000u);
 for(size_t at=20;at<44;at+=4)change(defs+at,0x7f800000u);change(points,0xff800000u);change(nav+20,0x7fc00000u);
 {auto b=blob;b[section(blob,1)]=':';seal(b);invalid(std::move(b));}
 // Nested format/capability/pin and the ordinary-map player contract remain distinct.
 for(unsigned at:{8u,20u,28u,52u}){auto b=blob;word(b,graphics+at,99);word(b,graphics+16,encore::crc32(b.data()+graphics+32,u(b.data()+64+16+8)-32));seal(b);invalid(std::move(b));}
 const auto gfx_size=u(blob.data()+64+16+8);std::vector<uint8_t>g(blob.begin()+graphics,blob.begin()+graphics+gfx_size);FieldMapData field;CHECK(field.load(g.data(),g.size(),error)&&field.view().terrain_only());g[4]='A';g[5]='P';g[6]='0';g[7]='1'; // ENCTAP01 is unknown, never a scene fallback.
 CHECK(!field.load(g.data(),g.size(),error)&&field.view().terrain_only());g.assign(blob.begin()+graphics,blob.begin()+graphics+gfx_size);const char ordinary[]="ENCMAP01";for(unsigned i=0;i<8;++i)g[i]=uint8_t(ordinary[i]);CHECK(!field.load(g.data(),g.size(),error));
 // New terrain paths reject scene behavior even when the nested container is valid.
 const std::vector<uint8_t>clean(blob.begin()+graphics,blob.begin()+graphics+gfx_size);
 auto graphics_invalid=[&](std::vector<uint8_t>b){seal(b);CHECK(!field.load(b.data(),b.size(),error)&&field.view().terrain_only());};
 {auto b=clean;word(b,20,2);graphics_invalid(std::move(b));}
 const auto item_offset=section(clean,14);unsigned sorted=0;for(;sorted<u(clean.data()+64+13*16+8);++sorted)if(clean[item_offset+sorted*32]==uint8_t(FieldItemKind::SortedLayerCells))break;
 CHECK(sorted<u(clean.data()+64+13*16+8));
 {auto b=clean;const auto at=item_offset+sorted*32;word(b,at,uint32_t(FieldItemKind::Player));word(b,at+4,0);word(b,at+8,0);graphics_invalid(std::move(b));}
 {auto b=clean;const auto at=item_offset+sorted*32;word(b,at,uint32_t(FieldItemKind::SortedLayer));word(b,at+4,0);graphics_invalid(std::move(b));}
 // Repack canonical section spans, then add a scene-sprite record. The terrain
 // behavior check must reject it before attempting to interpret a sprite.
 {std::vector<uint8_t>b(clean.begin(),clean.begin()+64+21*16);for(unsigned i=0;i<21;++i){const size_t at=64+i*16;const auto count=u(clean.data()+at+8),stride=u(clean.data()+at+12),offset=u(clean.data()+at+4);if(i==12){while(b.size()%4)b.push_back(0);word(b,at+4,uint32_t(b.size()));word(b,at+8,1);b.resize(b.size()+stride,0);}else if(count){while(b.size()%4)b.push_back(0);word(b,at+4,uint32_t(b.size()));b.insert(b.end(),clean.begin()+offset,clean.begin()+offset+size_t(count)*stride);}}word(b,12,uint32_t(b.size()));graphics_invalid(std::move(b));}
 CHECK(data.load_file(argv[1],error));const auto current=data.graphics().bytes_identity();CHECK(!data.load_file(nullptr,error)&&!data.load_file("",error)&&data.graphics().bytes_identity()==current);
 std::ifstream reference(argv[2]);unsigned layer,tid,h,v,t,ax,ay,resolved;int x,y;size_t count=0;while(reference>>layer>>x>>y>>tid>>h>>v>>t>>ax>>ay>>resolved){bool present=false;TerrainCell cell;CHECK(data.cell(layer,x,y,present,cell,error)&&present);CHECK(cell.layer==layer&&cell.x==x&&cell.y==y&&cell.tile==tid&&cell.flags==(h|(v<<1))&&!t&&cell.auto_x==ax&&cell.auto_y==ay);CHECK(bool(data.definition(tid))==bool(resolved)&&(cell.graphics_tile!=kFieldNone)==bool(resolved));++count;}
 CHECK(count==data.cells().size()&&data.layers().size()==5&&data.definitions().size()==30&&data.navigation().size()==1);
 bool present=true;TerrainCell held;held.tile=123;CHECK(!data.cell(0,0,0,present,held,error)&&present&&held.tile==123);CHECK(!data.cell(data.layers()[0].id,40000,0,present,held,error)&&present&&held.tile==123);CHECK(data.cell(data.layers()[0].id,32767,32767,present,held,error)&&!present&&held.tile==123);
 CHECK(data.cell(2,-42,1,present,held,error)&&present&&held.tile==11&&held.graphics_tile==kFieldNone&&!data.definition(11));const auto*spikes=data.definition(24);CHECK(spikes&&spikes->nav_count==1);const auto&polygon=data.navigation()[spikes->nav_first];CHECK(polygon.point_count==4&&polygon.index_count==4);CHECK(data.navigation_points()[polygon.point_first].x==48&&data.navigation_points()[polygon.point_first].y==16);
 DungeonTerrainData optional;CHECK(optional.load_file(argv[3],error)&&optional.navigation().empty()&&optional.navigation_points().empty()&&optional.navigation_indices().empty());CHECK(optional.cells().size()==data.cells().size()&&optional.graphics().count(FieldMapSection::Cells)==data.graphics().count(FieldMapSection::Cells));
 std::printf("Terrain:%u checks;12248 original native cell lookups,missing11 retained,navigation metadata,independent terrain scope,parser/ownership/encoding negatives;no root/navsolver/GPU/emulator/hardware claim\n",checks);
}
