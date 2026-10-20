#pragma once
// Explicit public SDK doubles. No T3X decoder, GPU, pixels or audibility.
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
struct C3D_Tex {unsigned id=0,border=9,filter=0,wrap=0;};
struct C3D_TexCube;
struct Tex3DS_SubTexture {uint16_t width=0,height=0;float left=0,top=1,right=1,bottom=0;bool rotated=false;};
struct KeyTestMetadata {Tex3DS_SubTexture sub;};
using Tex3DS_Texture=KeyTestMetadata*;
struct C2D_Image {C3D_Tex*tex;const Tex3DS_SubTexture*subtex;};
constexpr unsigned GPU_NEAREST=1,GPU_CLAMP_TO_BORDER=2;
namespace key_graphics_test {
struct Spec {uint32_t profile=0,width=0,height=0;std::vector<uint8_t>bytes;};
struct Call {unsigned profile;float x,y;Tex3DS_SubTexture sub;unsigned filter,wrap,border;};
inline std::vector<Spec>specs;inline std::vector<Call>calls;
inline unsigned imports=0,live=0,frees=0,flushes=0,binds=0,target=0,mode=0,bound=0;
inline bool draw_failure=false;
inline bool fault(){return target==imports;}
}
inline Tex3DS_Texture Tex3DS_TextureImport(const void*bytes,size_t size,C3D_Tex*tex,C3D_TexCube*,bool){
 using namespace key_graphics_test;++imports;const Spec*spec=nullptr;
 for(const auto&s:specs)if(s.bytes.size()==size&&std::equal(s.bytes.begin(),s.bytes.end(),static_cast<const uint8_t*>(bytes)))spec=&s;
 if(!spec||(fault()&&mode==1))return nullptr;
 auto*m=new KeyTestMetadata;m->sub.width=spec->width;m->sub.height=spec->height;
 unsigned w=8,h=8;while(w<spec->width*2)w*=2;while(h<spec->height*2)h*=2;
 m->sub.left=.125f;m->sub.top=.875f;m->sub.right=.125f+float(spec->width)/w;m->sub.bottom=.875f-float(spec->height)/h;
 if(fault()&&mode==4)++m->sub.width;if(fault()&&mode==5)--m->sub.height;if(fault()&&mode==6)m->sub.rotated=true;
 if(fault()&&mode==7)m->sub.right=std::numeric_limits<float>::quiet_NaN();if(fault()&&mode==8)m->sub.left=m->sub.right;
 tex->id=spec->profile;++live;return m;
}
inline size_t Tex3DS_GetNumSubTextures(Tex3DS_Texture){using namespace key_graphics_test;return fault()&&mode==2?2:1;}
inline const Tex3DS_SubTexture*Tex3DS_GetSubTexture(Tex3DS_Texture m,size_t){using namespace key_graphics_test;return fault()&&mode==3?nullptr:&m->sub;}
inline bool Tex3DS_SubTextureRotated(const Tex3DS_SubTexture*s){return s->rotated;}
inline void Tex3DS_TextureFree(Tex3DS_Texture p){delete p;}
inline void C3D_TexDelete(C3D_Tex*p){if(p->id){--key_graphics_test::live;++key_graphics_test::frees;p->id=0;}}
inline void C3D_TexSetFilter(C3D_Tex*p,unsigned a,unsigned b){p->filter=a==b?a:0;}
inline void C3D_TexSetWrap(C3D_Tex*p,unsigned a,unsigned b){p->wrap=a==b?a:0;}
inline void C2D_Flush(){++key_graphics_test::flushes;}
inline void C3D_TexBind(unsigned unit,C3D_Tex*p){key_graphics_test::bound=unit?0:p->id;++key_graphics_test::binds;}
inline bool C2D_DrawImageAt(C2D_Image image,float x,float y,float){
 using namespace key_graphics_test;if(draw_failure||bound!=image.tex->id)return false;
 calls.push_back({image.tex->id,x,y,*image.subtex,image.tex->filter,image.tex->wrap,image.tex->border});return true;
}
