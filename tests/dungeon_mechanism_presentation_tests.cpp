#include "encore/dungeon_mechanism_presentation.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool value,const char*line){++checks;if(!value){std::fprintf(stderr,"Mechanism presentation %u: %s (%s)\n",checks,line,error.c_str());std::exit(1);}}
#define CHECK(v) check((v),#v)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3||argc==4);std::vector<uint8_t>b;CHECK(encore::read_file(argv[1],b,512*1024,error));
 DungeonMechanismPresentationData data;CHECK(!data.valid()&&!data.bindings().valid()&&data.sprites().empty()&&data.owners().empty()&&data.shapes().empty()&&!data.sprite(1));
 DungeonMechanismDraw draw;draw.owner=17;DungeonMechanismRectangle rect;rect.center={12,13};
 CHECK(!data.draw(0,0,draw,error)&&draw.owner==17);CHECK(!data.rectangle(0,rect,error)&&rect.center.x==12);
 CHECK(!data.load_file(nullptr,error)&&!data.load(nullptr,b.size(),error));CHECK(data.load(b.data(),b.size(),error));CHECK(data.valid()&&data.bindings().graph().scene_id()==4&&data.owners().size()==14&&data.shapes().size()==17&&data.sprites().size()==4);
 CHECK(!data.load(b.data(),b.size(),error)&&data.valid()&&data.owners().size()==14);auto copy=data;CHECK(copy.bindings().graph().bytes_identity()==data.bindings().graph().bytes_identity());
 std::ifstream input(argv[2]);CHECK(bool(input));std::string line;unsigned owners=0,shapes=0;
 while(std::getline(input,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream s(line);char type;unsigned run;s>>type>>run;CHECK(run<2);
  if(type=='O'){
   uint32_t owner,profile,order,sort,w,h,bits;int32_t z;float px,py,x,y;s>>owner>>profile>>order>>sort>>px>>py>>x>>y>>w>>h>>bits>>z;CHECK(bool(s));CHECK(data.draw(owner,0,draw,error));CHECK(draw.owner==owner&&draw.profile==profile&&draw.order==order&&draw.parent_y_sort==bool(sort)&&draw.sort_y==py&&draw.top_left.x==x&&draw.top_left.y==y&&draw.width==w&&draw.height==h&&draw.bits==bits&&draw.z_index==z);
   const auto*texture=data.sprite(profile);CHECK(texture);for(uint32_t frame=0;frame<texture->frames;++frame){CHECK(data.draw(owner,frame,draw,error));CHECK(draw.x==frame%texture->columns*w&&draw.y==frame/texture->columns*h&&draw.x+w<=texture->width&&draw.y+h<=texture->height);}
   const auto before=draw;CHECK(!data.draw(owner,texture->frames,draw,error)&&draw.owner==before.owner&&draw.x==before.x&&draw.y==before.y);++owners;
  }else if(type=='S'){
   size_t index;uint32_t owner,role,layer,mask,bits;float bx,by,sx,sy,ex,ey,cx,cy,x0,y0,x1,y1,margin,oneway;s>>index>>owner>>role>>layer>>mask>>bits>>bx>>by>>sx>>sy>>ex>>ey>>cx>>cy>>x0>>y0>>x1>>y1>>margin>>oneway;CHECK(bool(s)&&index<data.shapes().size());const auto&shape=data.shapes()[index];CHECK(shape.owner==owner&&uint32_t(shape.role)==role&&shape.layer==layer&&shape.mask==mask&&shape.bits==bits&&shape.body_position.x==bx&&shape.body_position.y==by&&shape.shape_position.x==sx&&shape.shape_position.y==sy&&shape.extents.x==ex&&shape.extents.y==ey&&shape.safe_margin==margin&&shape.one_way_margin==oneway);CHECK(data.rectangle(index,rect,error));CHECK(rect.center.x==cx&&rect.center.y==cy&&rect.minimum.x==x0&&rect.minimum.y==y0&&rect.maximum.x==x1&&rect.maximum.y==y1);++shapes;
  }else CHECK(false);
 }
 CHECK(owners==28&&shapes==34);CHECK(!data.rectangle(data.shapes().size(),rect,error));CHECK(!data.draw(0,0,draw,error));
 auto reject=[&](const std::vector<uint8_t>&v){DungeonMechanismPresentationData trial;CHECK(!trial.load(v.data(),v.size(),error));CHECK(!trial.valid()&&trial.sprites().empty()&&trial.owners().empty()&&trial.shapes().empty()&&!trial.bindings().valid());};
 for(size_t size=0;size<b.size();++size){DungeonMechanismPresentationData trial;CHECK(!trial.load(b.data(),size,error));CHECK(!trial.valid());}
 const auto sprites=get(b,84),placements=get(b,100),geometry=get(b,116),nested=get(b,132);
 const std::vector<std::pair<size_t,uint32_t>>changes={{8,2},{20,2},{24,0},{24,5},{28,1},{32,0},{52,6},{56,1},{60,1},{64,2},{68,0},{72,0},{76,2},{84,0},{88,65},{92,111},{sprites,0},{sprites+20,0},{sprites+20,8388609},{sprites+60,1025},{sprites+68,0},{sprites+76,0},{sprites+84,65535},{sprites+104,32},{sprites+108,4097},{placements,0},{placements+4,0},{placements+24,0x7fc00000},{placements+28,0},{placements+32,2},{placements+36,1},{geometry,0},{geometry+4,4},{geometry+32,0},{geometry+36,0xbf800000},{geometry+40,0x100000},{geometry+48,16},{geometry+52,0x7f800000},{geometry+56,0xbf800000},{geometry+60,1},{nested+8,2},{nested+20,2}};
 for(const auto&item:changes){auto v=b;put(v,item.first,item.second);sign(v);reject(v);}
 for(size_t at:std::vector<size_t>{sprites+88,sprites+92,sprites+96,sprites+100,geometry+16,geometry+20,geometry+24,geometry+28}){auto v=b;put(v,at,0x7fc00000);sign(v);reject(v);}
 {auto v=b;put(v,sprites+112,get(b,sprites));sign(v);reject(v);}
 {auto v=b;put(v,sprites+112+4,get(b,sprites+4));put(v,sprites+112+8,get(b,sprites+8));sign(v);reject(v);}
 {auto v=b;const size_t text=get(b,68);v[text]=':';sign(v);reject(v);}
 {auto v=b;v.push_back(0);reject(v);}
 {auto v=b;v[180]^=1;reject(v);}
 for(const auto&s:data.sprites()){
  CHECK(!data.texture_matches(s.id,nullptr,s.texture_bytes,error));std::vector<uint8_t>bad(s.texture_bytes,0);CHECK(!data.texture_matches(s.id,bad.data(),bad.size(),error));
  if(argc==4){const std::string path=std::string(argv[3])+"/"+s.texture;std::vector<uint8_t>texture;CHECK(encore::read_file(path.c_str(),texture,8*1024*1024,error));CHECK(data.texture_matches(s.id,texture.data(),texture.size(),error));texture.back()^=1;CHECK(!data.texture_matches(s.id,texture.data(),texture.size(),error));}
 }
 std::printf("Mechanism presentation: %u checks; two native geometry replays, source sprite frames/order, exact binary32 transform composition, bounded atomic loading and negatives; no live scene/physics\n",checks);
}
