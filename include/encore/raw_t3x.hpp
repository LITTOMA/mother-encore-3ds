#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace encore {
// Checked subset emitted by the project's tex3ds -f rgba8 -z none recipes.
// Format reference: devkitPro/citro3d source/tex3ds.c, public T3X header and
// subtexture decoding; libctru's type-0 decompression header. Unknown formats
// must obtain a worker-side decoder before they can enter this pipeline.
struct RawT3x {
    uint32_t texture_width=0,texture_height=0,image_width=0,image_height=0;
    float left=0,top=0,right=0,bottom=0;size_t offset=21,size=0;
};
inline bool inspect_raw_t3x(const uint8_t* data,size_t header_size,size_t size,RawT3x& out,std::string& error){
    const auto fail=[&](const char* text){error=text;return false;};
    if(!data||header_size<21||size<21)return fail("Truncated raw T3X header");
    const auto u16=[&](size_t i){return uint32_t(data[i])|(uint32_t(data[i+1])<<8);};
    if(u16(0)!=1||(data[2]&0xc0)||data[3]!=0||data[4]!=0)return fail("Prewarm requires single 2D RGBA8 T3X without mipmaps");
    RawT3x value;value.texture_width=1u<<((data[2]&7)+3);value.texture_height=1u<<(((data[2]>>3)&7)+3);
    value.image_width=u16(5);value.image_height=u16(7);
    if(!value.image_width||!value.image_height||value.image_width>value.texture_width||value.image_height>value.texture_height)return fail("Raw T3X image dimensions rejected");
    const auto left=u16(9),top=u16(11),right=u16(13),bottom=u16(15);
    if(left>=right||bottom>=top||right>1024||top>1024||uint64_t(right-left)*value.texture_width!=uint64_t(value.image_width)*1024||uint64_t(top-bottom)*value.texture_height!=uint64_t(value.image_height)*1024)return fail("Raw T3X subtexture bounds or rotation rejected");
    if(data[17])return fail("Compressed T3X needs a worker-side decoder");
    value.size=size_t(data[18])|(size_t(data[19])<<8)|(size_t(data[20])<<16);
    if(value.size!=size_t(value.texture_width)*value.texture_height*4||size!=value.offset+value.size)return fail("Raw T3X payload length rejected");
    value.left=left/1024.f;value.top=top/1024.f;value.right=right/1024.f;value.bottom=bottom/1024.f;out=value;return true;
}
inline bool parse_raw_t3x(const uint8_t* data,size_t size,RawT3x& out,std::string& error){return inspect_raw_t3x(data,size,size,out,error);}
}
