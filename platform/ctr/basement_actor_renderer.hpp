#pragma once
#include "loading_texture.hpp"
#include "encore/basement_actor_assets.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace basement_actor_renderer_detail {
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
inline bool frame_region(const encore::upstream::BasementActorResource&r,uint32_t frame,const Tex3DS_SubTexture&image,Tex3DS_SubTexture&out){
 if(!r.width||!r.height||!r.columns||!r.rows||r.width%r.columns||r.height%r.rows||frame>=uint64_t(r.columns)*r.rows||image.width!=r.width||image.height!=r.height||Tex3DS_SubTextureRotated(&image))return false;
 for(float v:{image.left,image.right,image.top,image.bottom})if(!std::isfinite(v)||v<0||v>1)return false;
 if(image.right<=image.left||image.top<=image.bottom)return false;
 const uint32_t w=r.width/r.columns,h=r.height/r.rows,x=frame%r.columns*w,y=frame/r.columns*h;
 const float du=(image.right-image.left)/r.width,dv=(image.bottom-image.top)/r.height;
 out=image;out.width=uint16_t(w);out.height=uint16_t(h);
 out.left=image.left+x*du;out.right=image.left+(x+w)*du;out.top=image.top+y*dv;out.bottom=image.top+(y+h)*dv;
 return true;
}
inline bool origin(const encore::upstream::BasementActorResource&r,encore::upstream::Vec2 world,encore::upstream::Vec2 camera,encore::upstream::Vec2&out){
 if(!r.columns||!r.rows)return false;
 for(float v:{world.x,world.y,camera.x,camera.y,r.position.x,r.position.y,r.offset.x,r.offset.y})if(!std::isfinite(v))return false;
 out={world.x+r.position.x+r.offset.x-camera.x-float(r.width/r.columns)*.5f,world.y+r.position.y+r.offset.y-camera.y-float(r.height/r.rows)*.5f};
 if(!std::isfinite(out.x)||!std::isfinite(out.y))return false;
 out.x=std::floor(out.x+.5f);out.y=std::floor(out.y+.5f);return true;
}
}

// GPU sprite presentation only. The existing Room/House consumers choose the
// stable resource and frame and own visibility, source animation time and audio.
class BasementActorRenderer {
 struct Asset {encore::upstream::BasementActorResource resource;encore::ctr::LoadingSpriteSheet sheet=nullptr;std::vector<uint32_t>frames;mutable bool needs_bind=true;};
 std::vector<Asset>assets_;
 static bool reject(std::string&e,const std::string&m){e=m;return false;}
 static bool read(const std::string&path,std::vector<uint8_t>&bytes,std::string&e){
  FILE*f=std::fopen(path.c_str(),"rb");if(!f)return reject(e,"Basement sprite texture unavailable: "+path);
  constexpr size_t maximum=16*1024*1024;uint8_t block[8192];bool ok=true;
  for(;;){const auto n=std::fread(block,1,sizeof block,f);if(n>maximum-bytes.size()){ok=false;break;}bytes.insert(bytes.end(),block,block+n);if(n<sizeof block){ok=!std::ferror(f);break;}}
  if(std::fclose(f))ok=false;if(!ok||bytes.empty())return reject(e,"Basement sprite texture read/size rejected: "+path);return true;
 }
public:
 BasementActorRenderer()=default;~BasementActorRenderer(){free();}
 BasementActorRenderer(const BasementActorRenderer&)=delete;BasementActorRenderer&operator=(const BasementActorRenderer&)=delete;
 // Load/free must run at the host's GPU-idle admission boundary.
 bool load(const encore::upstream::BasementActorData&data,const char*prefix,std::string&error){
  free();if(!data.valid()||!prefix||!*prefix||data.resources().empty())return reject(error,"Basement sprite renderer requires checked data and resource root");
  for(const auto&r:data.resources()){
   if(r.width>1024||r.height>1024){free();return reject(error,"Basement sprite texture exceeds GPU extent");}
   const auto path=std::string(prefix)+r.path;std::vector<uint8_t>bytes;
   if(!read(path,bytes,error)){free();return false;}
   if(basement_actor_renderer_detail::sha256(bytes.data(),bytes.size())!=r.sha256){free();return reject(error,"Basement sprite texture SHA mismatch: "+path);}
   auto*sheet=new(std::nothrow)encore::ctr::LoadingSpriteSheetData;
   if(!sheet){free();return reject(error,"Basement sprite texture owner allocation failed");}
   // Import the exact verified bytes. Do not acquire a resident image whose
   // source path alone cannot prove its converted-file fingerprint.
   sheet->metadata=Tex3DS_TextureImport(bytes.data(),bytes.size(),&sheet->texture,nullptr,false);
   if(!sheet->metadata){delete sheet;free();return reject(error,"Basement sprite tex3ds decode/allocation failed: "+path);}
   sheet->source_path=encore::ctr::loading_texture_key(path.c_str());
   assets_.push_back({r,sheet,{}});auto&asset=assets_.back();
   for(const auto&animation:data.animations())if(animation.resource_id==r.id)for(const auto&key:animation.keys)asset.frames.push_back(key.frame);
   std::sort(asset.frames.begin(),asset.frames.end());asset.frames.erase(std::unique(asset.frames.begin(),asset.frames.end()),asset.frames.end());
   if(asset.frames.empty()){free();return reject(error,"Basement sprite has no admitted source animation frames");}
   if(encore::ctr::loading_sprite_sheet_count(sheet)!=1){free();return reject(error,"Basement sprite requires exactly one source atlas image: "+path);}
   const auto image=encore::ctr::loading_sprite_sheet_get_image(sheet,0);Tex3DS_SubTexture frame{};
   if(!image.tex||!image.subtex||!basement_actor_renderer_detail::frame_region(r,0,*image.subtex,frame)){free();return reject(error,"Basement sprite unique atlas dimensions/UV rejected: "+path);}
   sheet->texture.border=0;C3D_TexSetFilter(&sheet->texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&sheet->texture,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);
  }
  error.clear();return true;
 }
 void free(){for(auto&a:assets_)encore::ctr::loading_sprite_sheet_free(a.sheet);assets_.clear();}
 bool ready()const{return!assets_.empty();}
 // camera is the viewport's top-left world coordinate, matching House/Field
 // consumers. Source child position and Sprite offset are applied exactly once.
 bool draw(uint32_t resource_id,uint32_t frame,encore::upstream::Vec2 world_position,encore::upstream::Vec2 camera,float depth,std::string&error)const{
  if(!std::isfinite(depth)||depth<0||depth>1)return reject(error,"Basement sprite draw depth rejected");
  const auto at=std::find_if(assets_.begin(),assets_.end(),[&](const auto&a){return a.resource.id==resource_id;});
  if(at==assets_.end())return reject(error,"Basement sprite draw resource not admitted");
  if(!std::binary_search(at->frames.begin(),at->frames.end(),frame))return reject(error,"Basement sprite frame is not source-owned; atlas padding rejected");
  auto image=encore::ctr::loading_sprite_sheet_get_image(at->sheet,0);Tex3DS_SubTexture sub{};encore::upstream::Vec2 origin;
  if(!image.tex||!image.subtex||!basement_actor_renderer_detail::frame_region(at->resource,frame,*image.subtex,sub)||!basement_actor_renderer_detail::origin(at->resource,world_position,camera,origin))return reject(error,"Basement sprite draw frame/transform rejected");
  image.subtex=&sub;
  // A new owner may reuse a freed C3D_Tex address still remembered by C2D.
  // Rebind once per admitted allocation; steady frames remain batchable.
  if(at->needs_bind){C2D_Flush();C3D_TexBind(0,image.tex);at->needs_bind=false;}
  if(!C2D_DrawImageAt(image,origin.x,origin.y,depth,nullptr,1,1))return reject(error,"Basement sprite GPU draw submission failed");
  error.clear();return true;
 }
};
