#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
struct C3D_Tex {unsigned id=0;};
struct Tex3DS_SubTexture {uint16_t width=0,height=0;float left=0,top=1,right=1,bottom=0;bool rotated=false;};
struct C2D_Image {C3D_Tex*tex;const Tex3DS_SubTexture*subtex;};
namespace draw_test {
struct Spec {unsigned id=0,width=0,height=0;bool rotated=false,missing=false;};
struct Call {unsigned id=0;float x=0,y=0;Tex3DS_SubTexture sub;};
inline std::map<std::string,Spec>specs;
inline std::vector<Call>calls;
inline unsigned live=0;
}
constexpr unsigned GPU_NEAREST=0;
inline void C3D_TexSetFilter(C3D_Tex*,unsigned,unsigned){}
inline bool Tex3DS_SubTextureRotated(const Tex3DS_SubTexture*s){return s->rotated;}
inline uint32_t C2D_Color32(unsigned r,unsigned g,unsigned b,unsigned a){return r|g<<8|b<<16|a<<24;}
inline void C2D_DrawRectSolid(float,float,float,float,float,uint32_t){}
inline void C2D_DrawImageAt(C2D_Image image,float x,float y,float){draw_test::calls.push_back({image.tex->id,x,y,*image.subtex});}
