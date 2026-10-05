#pragma once
#include "loading_texture.hpp"
#include "encore/field_payphone.hpp"
#include "encore/field_geometry.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
namespace field_payphone_renderer_detail {
inline uint32_t rotate(uint32_t v,unsigned n){return(v>>n)|(v<<(32-n));}
// Integrity machinery only. The digest binds converted tex3ds bytes to the
// checked resource; all sprite pixels, grids and transforms come from data.
inline std::array<uint8_t,32> sha256(const uint8_t*data,size_t size){
 static constexpr uint32_t constants[64]={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 const size_t blocks=(size+9+63)/64;
 for(size_t block=0;block<blocks;++block){
  uint8_t raw[64]{};for(size_t j=0;j<64;++j){const size_t at=block*64+j;if(at<size)raw[j]=data[at];else if(at==size)raw[j]=0x80;}
  if(block+1==blocks){const uint64_t bits=uint64_t(size)*8;for(unsigned j=0;j<8;++j)raw[63-j]=uint8_t(bits>>(j*8));}
  uint32_t w[64];for(unsigned j=0;j<16;++j)w[j]=uint32_t(raw[j*4])<<24|uint32_t(raw[j*4+1])<<16|uint32_t(raw[j*4+2])<<8|raw[j*4+3];
  for(unsigned j=16;j<64;++j){const auto a=w[j-15],b=w[j-2];w[j]=w[j-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[j-7]+(rotate(b,17)^rotate(b,19)^(b>>10));}
  uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
  for(unsigned j=0;j<64;++j){const auto t1=v+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[j]+w[j],t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
  h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
 }
 std::array<uint8_t,32>out{};for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)out[i*4+j]=uint8_t(h[i]>>(24-j*8));return out;
}
}
class FieldPayphoneRenderer {
 const encore::upstream::FieldPayphoneData*data_=nullptr;encore::ctr::LoadingSpriteSheet sheet_=nullptr;mutable bool bind_=true;
 static bool fail(std::string&e,const char*s){e=s;return false;}
public:
 FieldPayphoneRenderer()=default;FieldPayphoneRenderer(const FieldPayphoneRenderer&)=delete;FieldPayphoneRenderer&operator=(const FieldPayphoneRenderer&)=delete;~FieldPayphoneRenderer(){free();}
 void free(){if(sheet_)encore::ctr::loading_sprite_sheet_free(sheet_);sheet_=nullptr;data_=nullptr;bind_=true;}
 bool load(const encore::upstream::FieldPayphoneData&d,const std::string&prefix,std::string&e){free();if(!d.valid()||prefix.empty())return fail(e,"Payphone GPU data unadmitted");auto&t=d.texture();FILE*f=std::fopen((prefix+t.path).c_str(),"rb");if(!f)return fail(e,"Payphone atlas unavailable");std::vector<uint8_t>bytes;uint8_t block[8192];bool good=true;for(;;){auto n=std::fread(block,1,sizeof block,f);if(n>16*1024*1024-bytes.size()){good=false;break;}bytes.insert(bytes.end(),block,block+n);if(n<sizeof block){good=!std::ferror(f);break;}}if(std::fclose(f))good=false;if(!good||bytes.empty()||field_payphone_renderer_detail::sha256(bytes.data(),bytes.size())!=t.sha)return fail(e,"Payphone bounded atlas/SHA read rejected");sheet_=new(std::nothrow)encore::ctr::LoadingSpriteSheetData;if(!sheet_)return fail(e,"Payphone atlas allocation failed");sheet_->metadata=Tex3DS_TextureImport(bytes.data(),bytes.size(),&sheet_->texture,nullptr,false);if(!sheet_->metadata){delete sheet_;sheet_=nullptr;return fail(e,"Actual payphone tex3ds import failed");}sheet_->source_path=encore::ctr::loading_texture_key((prefix+t.path).c_str());if(encore::ctr::loading_sprite_sheet_count(sheet_)!=1){free();return fail(e,"Payphone atlas image count rejected");}auto image=encore::ctr::loading_sprite_sheet_get_image(sheet_,0);if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=t.width||image.subtex->height!=t.height||image.subtex->right<=image.subtex->left||image.subtex->top<=image.subtex->bottom){free();return fail(e,"Payphone source atlas dimensions/UV rejected");}for(float v:{image.subtex->left,image.subtex->right,image.subtex->top,image.subtex->bottom})if(!std::isfinite(v)||v<0||v>1){free();return fail(e,"Payphone atlas UV invalid");}sheet_->texture.border=0;C3D_TexSetFilter(&sheet_->texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&sheet_->texture,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);data_=&d;e.clear();return true;}
 // sprite_world is the admitted main child's actual Canvas transform: its
 // source position has already been applied. Only source offset/center are
 // added here, avoiding the previous parent/local double positioning class.
 bool draw(const encore::upstream::FieldPayphoneRuntime&r,uint32_t id,const encore::upstream::FieldGeometryTransform&sprite_world,encore::upstream::Vec2 camera,float depth,bool ancestors_visible,bool pixel_snap,uint32_t color,std::string&e)const{
  auto d=data_?data_->record(id):nullptr;auto s=r.instance(id);if(!sheet_||!d||!s||!s->ready||!std::isfinite(depth)||depth<0||depth>1)return fail(e,"Payphone GPU live source binding rejected");for(float v:{float(sprite_world.x.x),float(sprite_world.x.y),float(sprite_world.y.x),float(sprite_world.y.y),float(sprite_world.origin.x),float(sprite_world.origin.y),float(camera.x),float(camera.y)})if(!std::isfinite(v))return fail(e,"Payphone Canvas transform invalid");if(!s->visible||s->deleted||!ancestors_visible){e.clear();return true;}auto t=data_->texture();auto image=encore::ctr::loading_sprite_sheet_get_image(sheet_,0);auto sub=*image.subtex;const auto columns=t.columns,rows=t.rows,frame=data_->idle_frame();if(!columns||!rows||frame>=uint64_t(columns)*rows)return fail(e,"Payphone source grid rejected");const float width=float(t.width)/columns,height=float(t.height)/rows,du=(sub.right-sub.left)/columns,dv=(sub.top-sub.bottom)/rows;sub.left+=du*(frame%columns);sub.right=sub.left+du;sub.top-=dv*(frame/columns);sub.bottom=sub.top-dv;sub.width=uint16_t(width);sub.height=uint16_t(height);
  const float sx=std::hypot(float(sprite_world.x.x),float(sprite_world.x.y)),sy=std::hypot(float(sprite_world.y.x),float(sprite_world.y.y));if(sx==0||sy==0){e.clear();return true;}if(std::abs(sprite_world.x.x*sprite_world.y.x+sprite_world.x.y*sprite_world.y.y)>1e-5f*sx*sy)return fail(e,"Payphone unsupported skew transform");const bool flipy=sprite_world.x.x*sprite_world.y.y-sprite_world.x.y*sprite_world.y.x<0;float cx=d->sprite_offset.x,cy=d->sprite_offset.y;if(!(d->sprite_flags&1)){cx+=width*.5f;cy+=height*.5f;}float x=sprite_world.origin.x+sprite_world.x.x*cx+sprite_world.y.x*cy-camera.x,y=sprite_world.origin.y+sprite_world.x.y*cx+sprite_world.y.y*cy-camera.y;if(pixel_snap){x=std::floor(x+.5f);y=std::floor(y+.5f);}if(d->sprite_flags&2)std::swap(sub.left,sub.right);if(bool(d->sprite_flags&4)!=flipy)std::swap(sub.top,sub.bottom);image.subtex=&sub;if(bind_){C2D_Flush();C3D_TexBind(0,image.tex);bind_=false;}C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,0);if(!C2D_DrawImageAtRotated(image,x,y,depth,std::atan2(float(sprite_world.x.y),float(sprite_world.x.x)),&tint,sx,sy))return fail(e,"Payphone GPU submission rejected");e.clear();return true;
 }
};
