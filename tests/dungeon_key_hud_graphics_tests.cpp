// Actual core and one-icon production renderer. Tex3DS/C2D and font submission
// are explicit API doubles; no decoder, GPU, font pixels or emulator claim.
#include "dungeon_key_hud_renderer.hpp"
#include "encore/save_menu_data.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Key HUD line%d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
std::vector<uint8_t>read(const char*p){std::vector<uint8_t>b;CHECK(encore::read_file(p,b,1024*1024,error));return b;}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void word(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(i*8));}
void seal(std::vector<uint8_t>&b){word(b,16,encore::crc32(b.data()+32,b.size()-32));}
void write(const std::filesystem::path&p,const std::vector<uint8_t>&b){std::filesystem::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));CHECK(bool(f));}
struct Font {
 uint32_t bytes=0,crc=0;uint64_t generation=1;bool enabled=true,reject=false;
 struct Draw {std::string text;float x,y;uint32_t color;};mutable std::vector<Draw>draws;
 bool bottle_matches(uint32_t n,uint32_t c)const{return enabled&&bytes==n&&crc==c;}
 uint64_t bottle_generation()const{return enabled?generation:0;}
 bool draw_bottle(const std::string&s,float x,float y,uint32_t c,uint64_t g)const{if(reject||g!=bottle_generation())return false;draws.push_back({s,x,y,c});return true;}
};
}
int main(int argc,char**argv){CHECK(argc==6);auto bytes=read(argv[1]);DungeonKeyHudGraphicsData data;CHECK(data.load(bytes.data(),bytes.size(),error));const auto c=data.config();DungeonKeyIndicator hud;CHECK(hud.prepare(data.indicator(),101,error)&&data.matches(hud));CHECK(!DungeonKeyIndicatorData{}.same_contract({}));CHECK(data.indicator().same_contract(hud.data()));
 DungeonKeyHudGraphicsData from_file;CHECK(from_file.load_file(argv[1],error)&&from_file.matches(hud));CHECK(!from_file.load_file(nullptr,error)&&from_file.matches(hud));CHECK(!from_file.load_file("",error)&&from_file.matches(hud));CHECK(!from_file.load_file((std::string(argv[3])+"/missing.enckgx").c_str(),error)&&from_file.matches(hud));CHECK(!data.load(bytes.data(),1024*1024+1,error)&&data.matches(hud));
 // Failed loads preserve the already admitted immutable configuration.
 CHECK(!data.load(nullptr,bytes.size(),error));for(size_t i=0;i<bytes.size();++i)CHECK(!data.load(bytes.data(),i,error)&&data.matches(hud));
 for(unsigned at:{8u,20u,24u,28u,32u,36u,40u,44u,108u,112u,116u,120u,124u}){auto b=bytes;word(b,at,99);seal(b);CHECK(!data.load(b.data(),b.size(),error)&&data.matches(hud));}
 for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{48,0},{48,1025},{52,0},{52,1025},{56,0},{56,8*1024*1024+1},{64,23},{64,1024*1024+1}}){auto b=bytes;word(b,pair.first,pair.second);seal(b);CHECK(!data.load(b.data(),b.size(),error));}
 for(unsigned at=72;at<104;at+=4)for(uint32_t invalid:{0x7fc00000u,0x7f800000u,0xff800000u,0x47800000u}){auto b=bytes;word(b,at,invalid);seal(b);CHECK(!data.load(b.data(),b.size(),error));}
 for(unsigned at:{72u,76u}){auto b=bytes;word(b,at,0);seal(b);CHECK(!data.load(b.data(),b.size(),error));}
 const size_t path_at=u32(bytes.data()+40),nested_size=u32(bytes.data()+36);CHECK(path_at+c.texture.size()==bytes.size());for(uint8_t invalid:{uint8_t(0),uint8_t(31),uint8_t(127),uint8_t(255),uint8_t('/'),uint8_t(':'),uint8_t('\\')}){auto b=bytes;b[path_at]=invalid;seal(b);CHECK(!data.load(b.data(),b.size(),error));}
 {auto b=bytes;word(b,128+8,2);seal(b);CHECK(!data.load(b.data(),b.size(),error));}
 {auto b=bytes;b.push_back(0);word(b,12,uint32_t(b.size()));seal(b);CHECK(!data.load(b.data(),b.size(),error));}
 // A valid but different nested state cannot be mistaken for this HUD owner.
 auto other_bytes=bytes;double changed=321;std::memcpy(other_bytes.data()+128+64,&changed,8);word(other_bytes,128+16,encore::crc32(other_bytes.data()+128+32,nested_size-32));seal(other_bytes);DungeonKeyHudGraphicsData other;CHECK(other.load(other_bytes.data(),other_bytes.size(),error));DungeonKeyIndicator other_hud;CHECK(other_hud.prepare(other.indicator(),101,error)&&!data.matches(other_hud)&&!other.matches(hud));
 auto saved=read(argv[5]);SaveMenuData save;CHECK(!save.bytes_size()&&!save.bytes_crc());CHECK(save.load(saved.data(),saved.size(),error)&&save.bytes_size()==saved.size()&&save.bytes_crc()==encore::crc32(saved.data(),saved.size()));CHECK(save.bytes_size()==c.font_bytes&&save.bytes_crc()==c.font_crc);auto bad_save=saved;bad_save.back()^=1;CHECK(!save.load(bad_save.data(),bad_save.size(),error)&&save.bytes_size()==saved.size()&&save.bytes_crc()==c.font_crc);
 Font font;font.bytes=c.font_bytes;font.crc=c.font_crc;Font second=font;DungeonKeyHudRenderer renderer;
 CHECK(!renderer.prepare(data,other_hud,argv[2],error)&&!renderer.ready());const auto source=std::filesystem::path(argv[2])/c.texture;auto image=read(source.string().c_str());using namespace key_graphics_test;specs={{1,c.width,c.height,image}};
 const std::filesystem::path scratch(argv[3]);CHECK(!std::filesystem::exists(scratch));const auto copy=scratch/c.texture;write(copy,image);auto wrong=image;wrong[0]^=1;write(copy,wrong);CHECK(!renderer.prepare(data,hud,scratch.string().c_str(),error)&&!renderer.ready()&&live==0);write(copy,image);auto short_image=image;short_image.pop_back();write(copy,short_image);CHECK(!renderer.prepare(data,hud,scratch.string().c_str(),error)&&!renderer.ready()&&live==0);write(copy,image);
 for(unsigned fault_mode=1;fault_mode<=8;++fault_mode){target=imports+1;mode=fault_mode;CHECK(!renderer.prepare(data,hud,scratch.string().c_str(),error)&&!renderer.ready()&&live==0);target=mode=0;}
 CHECK(renderer.prepare(data,hud,scratch.string().c_str(),error)&&renderer.ready()&&live==1);CHECK(!renderer.prepare(data,hud,scratch.string().c_str(),error));CHECK(std::filesystem::remove(copy));const auto prepared_imports=imports;
 auto drawing=[&](float w,float h){CHECK(renderer.compose(hud,font,w,h,error));const auto frame=renderer.frame();calls.clear();font.draws.clear();CHECK(!renderer.draw(100,frame,font,error)&&!renderer.draw(101,frame-1,font,error)&&!renderer.draw(101,frame,second,error)&&calls.empty()&&font.draws.empty());CHECK(renderer.draw(101,frame,font,error));const auto&cmd=renderer.command();CHECK(cmd.visible==hud.view().visible&&cmd.label==hud.view().text);CHECK(calls.size()==size_t(cmd.visible)&&font.draws.size()==calls.size());if(cmd.visible){const auto&call=calls.back();const auto&text=font.draws.back();CHECK(call.x==std::floor(hud.view().position.x+w-c.reference.x+c.icon_offset.x+.5f)&&call.y==std::floor(hud.view().position.y+h-c.reference.y+c.icon_offset.y+.5f));CHECK(text.x==std::floor(hud.view().position.x+w-c.reference.x+c.text_offset.x+.5f)&&text.y==std::floor(hud.view().position.y+h-c.reference.y+c.text_offset.y+.5f)&&text.text==hud.view().text&&text.color==c.color);CHECK(call.sub.width==c.width&&call.sub.height==c.height&&call.filter==GPU_NEAREST&&call.wrap==GPU_CLAMP_TO_BORDER&&call.border==0);}CHECK(imports==prepared_imports&&live==1);};
 std::ifstream reference(argv[4]);std::string label;float ix,iy,tx,ty,sw,sh;unsigned rows=0;while(reference>>std::quoted(label)>>ix>>iy>>tx>>ty>>sw>>sh){CHECK(ix==c.icon_offset.x&&iy==c.icon_offset.y&&tx==c.text_offset.x&&ty==c.text_offset.y&&sw>0&&sh>0);++rows;}CHECK(rows==5);
 drawing(c.reference.x,c.reference.y);drawing(c.expanded.x,c.expanded.y);CHECK(!renderer.compose(hud,font,1,1,error)&&renderer.frame()==0);CHECK(!renderer.compose(other_hud,font,c.reference.x,c.reference.y,error));
 const auto policy=hud.data().keys().policy();for(int32_t n=policy.minimum+1;n<=policy.maximum;++n){CHECK(hud.update_counter(101,n,error));for(unsigned i=0;i<40;++i){CHECK(hud.advance(101,1./128,error));drawing(c.reference.x,c.reference.y);drawing(c.expanded.x,c.expanded.y);}}
 CHECK(renderer.compose(hud,font,c.reference.x,c.reference.y,error));auto old_frame=renderer.frame();++font.generation;calls.clear();font.draws.clear();CHECK(!renderer.draw(101,old_frame,font,error)&&calls.empty()&&font.draws.empty());drawing(c.reference.x,c.reference.y);font.crc^=1;CHECK(!renderer.compose(hud,font,c.reference.x,c.reference.y,error)&&!renderer.frame());font.crc^=1;font.enabled=false;CHECK(!renderer.compose(hud,font,c.reference.x,c.reference.y,error));font.enabled=true;
 CHECK(renderer.compose(hud,font,c.reference.x,c.reference.y,error));font.reject=true;CHECK(!renderer.draw(101,renderer.frame(),font,error));font.reject=false;draw_failure=true;CHECK(!renderer.draw(101,renderer.frame(),font,error));draw_failure=false;
 CHECK(hud.close(101,error)&&hud.advance(101,1,error));drawing(c.reference.x,c.reference.y);CHECK(!hud.view().visible&&calls.empty());old_frame=renderer.frame();CHECK(!renderer.retire(102,error)&&live==1);CHECK(renderer.retire(101,error)&&live==0&&!renderer.ready()&&!renderer.draw(101,old_frame,font,error));CHECK(!renderer.prepare(data,hud,argv[2],error));
 DungeonKeyIndicator next;CHECK(next.prepare(data.indicator(),102,error));{DungeonKeyHudRenderer owned;CHECK(owned.prepare(data,next,argv[2],error)&&live==1);}CHECK(live==0);
 auto dir=copy.parent_path();while(dir!=scratch){CHECK(std::filesystem::remove(dir));dir=dir.parent_path();}CHECK(std::filesystem::remove(scratch));
 std::printf("Key HUD:%u checks;original5 layouts,independent parser negatives,actual SAVE fingerprint,one admitted icon,borrowed font generation,no draw-time IO,epochs/retirement;explicit font/Tex3DS/C2D doubles,no GPU/pixel/emulator claim\n",checks);
}
