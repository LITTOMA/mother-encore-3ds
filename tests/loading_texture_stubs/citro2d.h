#pragma once
// Public API test double only. The host test does not decode T3X or render GPU
// images; actual SDK parity is checked by loading_texture_arm_selfcheck.cpp.
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
struct C3D_Tex {
    void* data = nullptr;
    int fmt=0;
    uint32_t border = 0xffffffff;
    size_t size = 0;
    int wrap_s = 0, wrap_t = 0;
};
struct C3D_TexCube;
struct Tex3DS_SubTexture {
    uint16_t width, height;
    float left, top, right, bottom;
};
struct TextureMetadata;
using Tex3DS_Texture = TextureMetadata*;
using decompressCallback = ssize_t (*)(void*, void*, size_t);
struct C2D_Image { C3D_Tex* tex; const Tex3DS_SubTexture* subtex; };
constexpr int GPU_RGBA8=0;
constexpr int GPU_CLAMP_TO_BORDER = 7;
inline void C3D_TexFlush(C3D_Tex*){}
Tex3DS_Texture Tex3DS_TextureImportCallback(C3D_Tex*, C3D_TexCube*, bool, decompressCallback, void*);
Tex3DS_Texture Tex3DS_TextureImport(const void*, size_t, C3D_Tex*, C3D_TexCube*, bool);
void Tex3DS_TextureFree(Tex3DS_Texture);
void C3D_TexDelete(C3D_Tex*);
void C3D_TexSetWrap(C3D_Tex*, int, int);
size_t Tex3DS_GetNumSubTextures(Tex3DS_Texture);
const Tex3DS_SubTexture* Tex3DS_GetSubTexture(Tex3DS_Texture, size_t);
