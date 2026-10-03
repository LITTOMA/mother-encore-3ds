#pragma once
#include "../loading_texture_stubs/citro2d.h"
constexpr int GPU_NEAREST=3;
void C3D_TexSetFilter(C3D_Tex*,int,int);
bool Tex3DS_SubTextureRotated(const Tex3DS_SubTexture*);
bool C2D_DrawImageAt(C2D_Image,float,float,float);
