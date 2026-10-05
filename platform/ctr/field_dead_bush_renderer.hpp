#pragma once
#include "loading_texture.hpp"
#include "encore/field_dead_bush.hpp"
#include "encore/field_geometry.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
namespace field_bush_renderer_detail {
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
// Real GPU sprites from the checked source atlas. The Host supplies the actual
// Canvas transform (including source parent flips), visibility, tint and camera.
// Roots are separately owned native sprites; their copied local position is
// NOT transformed back from source global coordinates by this renderer.
class FieldBushRenderer {
 const encore::upstream::FieldBushData*data_=nullptr;
 encore::ctr::LoadingSpriteSheet sheet_=nullptr;mutable bool needs_bind_=true;
 static bool reject(std::string&e,const char*t){e=t;return false;}
 static bool read(const std::string&p,std::vector<uint8_t>&b,std::string&e){FILE*f=std::fopen(p.c_str(),"rb");if(!f)return reject(e,"DeadBush atlas unavailable");constexpr size_t maximum=16*1024*1024;uint8_t block[8192];bool ok=true;for(;;){auto n=std::fread(block,1,sizeof block,f);if(n>maximum-b.size()){ok=false;break;}b.insert(b.end(),block,block+n);if(n<sizeof block){ok=!std::ferror(f);break;}}if(std::fclose(f))ok=false;if(!ok||b.empty())return reject(e,"DeadBush atlas bounded read rejected");return true;}
 bool draw_sprite(const encore::upstream::FieldBushDescriptor&d,uint32_t frame,bool visible,encore::upstream::Vec2 local_position,const encore::upstream::FieldGeometryTransform&parent,encore::upstream::Vec2 camera,float depth,bool pixel_snap,uint32_t color,std::string&e)const{
  if(!data_||!sheet_||!std::isfinite(depth)||depth<0||depth>1||!d.columns||!d.rows||frame>=uint64_t(d.columns)*d.rows)return reject(e,"DeadBush draw source/frame/depth rejected");
  for(float v:{float(local_position.x),float(local_position.y),float(parent.x.x),float(parent.x.y),float(parent.y.x),float(parent.y.y),float(parent.origin.x),float(parent.origin.y),float(camera.x),float(camera.y)})if(!std::isfinite(v))return reject(e,"DeadBush Canvas transform invalid");if(!visible){e.clear();return true;}
  auto image=encore::ctr::loading_sprite_sheet_get_image(sheet_,0);if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=data_->width()||image.subtex->height!=data_->height())return reject(e,"DeadBush actual source atlas dimensions rejected");
  const auto w=data_->width()/d.columns,h=data_->height()/d.rows;auto sub=*image.subtex;float du=(sub.right-sub.left)/data_->width(),dv=(sub.bottom-sub.top)/data_->height();const float u=(frame%d.columns)*w,v=(frame/d.columns)*h;sub.left=image.subtex->left+u*du;sub.right=image.subtex->left+(u+w)*du;sub.top=image.subtex->top+v*dv;sub.bottom=image.subtex->top+(v+h)*dv;sub.width=uint16_t(w);sub.height=uint16_t(h);
  const float ax=parent.x.x*d.sprite_scale.x,ay=parent.x.y*d.sprite_scale.x,bx=parent.y.x*d.sprite_scale.y,by=parent.y.y*d.sprite_scale.y,sx=std::hypot(ax,ay),sy=std::hypot(bx,by);if(sx==0||sy==0){e.clear();return true;}if(std::abs(ax*bx+ay*by)>1e-5f*sx*sy)return reject(e,"DeadBush unreviewed skew Canvas transform");
  const bool flipped_y=ax*by-ay*bx<0;const float center_x=d.sprite_offset.x+((d.flags&4)?0:w*.5f),center_y=d.sprite_offset.y+((d.flags&4)?0:h*.5f);
  const float px=local_position.x+center_x*d.sprite_scale.x,py=local_position.y+center_y*d.sprite_scale.y;float x=parent.origin.x+parent.x.x*px+parent.y.x*py-camera.x,y=parent.origin.y+parent.x.y*px+parent.y.y*py-camera.y;if(pixel_snap){x=std::floor(x+.5f);y=std::floor(y+.5f);}if(d.flags&8)std::swap(sub.left,sub.right);if(bool(d.flags&16)!=flipped_y)std::swap(sub.top,sub.bottom);image.subtex=&sub;
  if(needs_bind_){C2D_Flush();C3D_TexBind(0,image.tex);needs_bind_=false;}C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,0);if(!C2D_DrawImageAtRotated(image,x,y,depth,std::atan2(ay,ax),&tint,sx,sy))return reject(e,"DeadBush GPU sprite submission failed");e.clear();return true;
 }
public:
 FieldBushRenderer()=default;~FieldBushRenderer(){free();}FieldBushRenderer(const FieldBushRenderer&)=delete;FieldBushRenderer&operator=(const FieldBushRenderer&)=delete;
 bool load(const encore::upstream::FieldBushData&d,const char*prefix,std::string&e){free();if(!d.valid()||!prefix||!*prefix)return reject(e,"DeadBush renderer needs checked source data/root");std::vector<uint8_t>b;if(!read(std::string(prefix)+d.texture_path(),b,e))return false;if(field_bush_renderer_detail::sha256(b.data(),b.size())!=d.texture_hash())return reject(e,"DeadBush atlas SHA mismatch");sheet_=new(std::nothrow)encore::ctr::LoadingSpriteSheetData;if(!sheet_)return reject(e,"DeadBush atlas owner allocation failed");sheet_->metadata=Tex3DS_TextureImport(b.data(),b.size(),&sheet_->texture,nullptr,false);if(!sheet_->metadata){delete sheet_;sheet_=nullptr;return reject(e,"DeadBush genuine tex3ds import failed");}sheet_->source_path=encore::ctr::loading_texture_key((std::string(prefix)+d.texture_path()).c_str());data_=&d;needs_bind_=true;
  if(encore::ctr::loading_sprite_sheet_count(sheet_)!=1){free();return reject(e,"DeadBush atlas image count rejected");}auto image=encore::ctr::loading_sprite_sheet_get_image(sheet_,0);if(!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=d.width()||image.subtex->height!=d.height()||image.subtex->right<=image.subtex->left||image.subtex->top<=image.subtex->bottom){free();return reject(e,"DeadBush unique source atlas dimensions/UV rejected");}for(float v:{image.subtex->left,image.subtex->right,image.subtex->top,image.subtex->bottom})if(!std::isfinite(v)||v<0||v>1){free();return reject(e,"DeadBush atlas UV bounds rejected");}sheet_->texture.border=0;C3D_TexSetFilter(&sheet_->texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&sheet_->texture,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);e.clear();return true;
 }
 void free(){if(sheet_)encore::ctr::loading_sprite_sheet_free(sheet_);sheet_=nullptr;data_=nullptr;needs_bind_=true;}
 bool ready()const{return data_&&sheet_;}
 bool draw(const encore::upstream::FieldBushRuntime&runtime,uint32_t id,const encore::upstream::FieldGeometryTransform&bush_world,encore::upstream::Vec2 camera,float depth,bool ancestor_visible,bool pixel_snap,uint32_t color,std::string&e)const{if(!data_)return reject(e,"DeadBush GPU renderer unbound");auto d=data_->record(id);auto s=runtime.instance(id);if(!d||!s||!s->ready)return reject(e,"DeadBush GPU source instance not Ready");return draw_sprite(*d,s->frame,ancestor_visible&&s->visible&&s->sprite_visible&&!s->deleted,d->sprite_position,bush_world,camera,depth,pixel_snap,color,e);}
 // Dynamic Roots sample comes from actual duplicate/add_child/position Host;
 // copied visibility and texture/grid identity stay separate from dead owner.
 bool draw_roots(uint32_t source_descriptor,uint32_t frame,bool visible,encore::upstream::Vec2 roots_local,const encore::upstream::FieldGeometryTransform&new_parent_world,encore::upstream::Vec2 camera,float depth,bool pixel_snap,uint32_t color,std::string&e)const{if(!data_)return reject(e,"DeadBush Roots GPU renderer unbound");auto d=data_->record(source_descriptor);if(!d)return reject(e,"DeadBush Roots copied descriptor unbound");return draw_sprite(*d,frame,visible,roots_local,new_parent_world,camera,depth,pixel_snap,color,e);}
};
