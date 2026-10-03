#pragma once
#include "loading_texture.hpp"
#include "battle_renderer.hpp"
#include "encore/battle_action_presentation.hpp"

// Additive glow, source rotation, original atlas frames and bitmap damage text.
// This owns only action textures;
// The established background path is untouched.
class RoundRenderer {
 struct Asset {encore::ctr::LoadingSpriteSheet sheet=nullptr;
 uint32_t width=0,height=0,columns=0,rows=0;
 };
 
 std::vector<Asset>assets_;
 
 encore::upstream::RoundView content_;
 
 C2D_Image region(uint32_t resource,uint32_t u,uint32_t v,uint32_t w,uint32_t h,Tex3DS_SubTexture&sub)const{
  if(resource>=assets_.size())return {};
 const auto&a=assets_[resource];
 if(!a.sheet||u>a.width||w>a.width-u||v>a.height||h>a.height-v)return {};
 
  const auto img=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
 sub=*img.subtex;
 const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height,left=sub.left,top=sub.top;
 
  sub.left=left+u*du;
 sub.right=left+(u+w)*du;
 sub.top=top+v*dv;
 sub.bottom=top+(v+h)*dv;
 sub.width=w;
 sub.height=h;
 return {img.tex,&sub};
 
 }
 static uint32_t color(encore::upstream::BattleValue c){return C2D_Color32f(c.x,c.y,c.z,c.w);
 }
 bool draw_region(uint32_t r,uint32_t u,uint32_t v,uint32_t w,uint32_t h,float x,float y,float dw,float dh,uint32_t tint,float blend=0)const{
  Tex3DS_SubTexture sub{};
 const auto img=region(r,u,v,w,h,sub);
 if(!img.tex)return false;
 C2D_ImageTint t;
 C2D_PlainImageTint(&t,tint,blend);
 return C2D_DrawImageAt(img,x,y,0,&t,dw/w,dh/h);
 
 }
 bool ninepatch(const encore::upstream::BattleActionPose&p,float x,float y)const{
  using namespace encore::upstream;
 if(p.resource>=assets_.size())return false;
 const auto&a=assets_[p.resource];
 const auto m=content_.parameter(RoundParameter::DialogueMargins);
 
  const unsigned u[]={0,unsigned(m.x),a.width-unsigned(m.z),a.width},v[]={0,unsigned(m.y),a.height-unsigned(m.w),a.height};
 
  const float xs[]={x,x+m.x,x+p.rect.z-m.z,x+p.rect.z},ys[]={y,y+m.y,y+p.rect.w-m.w,y+p.rect.w};
 
  if(m.x+m.z>a.width||m.y+m.w>a.height||p.rect.z<m.x+m.z||p.rect.w<m.y+m.w)return false;
 
  for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)if(u[col+1]>u[col]&&v[row+1]>v[row])
   if(!draw_region(p.resource,u[col],v[row],u[col+1]-u[col],v[row+1]-v[row],xs[col],ys[row],xs[col+1]-xs[col],ys[row+1]-ys[row],color(p.color)))return false;
 
  return true;
 
 }
 bool number(const encore::upstream::BattleActionPose&p,float x,float y)const{
  using namespace encore::upstream;
 const auto g=content_.parameter(RoundParameter::DamageGlyphGrid);
 const auto shadow=content_.parameter(RoundParameter::DamageShadow);
 
  if(p.resource>=assets_.size()||g.y<=0||g.z<=0||g.w<=0)return false;
 const float width=p.text.size()*g.w*p.scale.x;
 
  x+=(p.rect.z-width)/2;
 const auto col=BattleValue{p.color.x,p.color.y,p.color.z,p.color.w*p.modulate.w};
 
  for(const auto ch:p.text){const auto code=static_cast<unsigned char>(ch);
 if(code<g.x)return false;
 const unsigned u=unsigned(code-g.x)*unsigned(g.y);
 
   if(!draw_region(p.resource,u,0,unsigned(g.y),unsigned(g.z),x+shadow.x,y+shadow.y,g.y*p.scale.x,g.z*p.scale.y,C2D_Color32f(0,0,0,col.w),1))return false;
 
   if(!draw_region(p.resource,u,0,unsigned(g.y),unsigned(g.z),x,y,g.y*p.scale.x,g.z*p.scale.y,color(col),1))return false;
 x+=g.w*p.scale.x;
 
  }return true;
 
 }
public:
 RoundRenderer()=default;
 RoundRenderer(const RoundRenderer&)=delete;
 RoundRenderer&operator=(const RoundRenderer&)=delete;
 
 size_t cpu_bytes()const{return assets_.capacity()*sizeof(Asset);}
 void swap(RoundRenderer& other){assets_.swap(other.assets_);std::swap(content_,other.content_);}
 bool load(encore::upstream::RoundView content,const char*prefix,std::string&error,bool resident_only=false){
  auto previous=std::move(assets_);
 struct Retired {std::vector<Asset>& assets;~Retired(){for(auto& a:assets)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);}} retired{previous};
 content_=content;
 if(!content.valid()||!prefix){error="Round renderer requires checked content";
 return false;
 }assets_.resize(content.count(encore::upstream::RoundSection::Resources));
 
  for(uint32_t i=0;i<assets_.size();++i){const auto r=content.resource(i);
 auto&a=assets_[i];
 if(r.kind!=1||!r.width||!r.height||r.width>1024||r.height>1024||!r.columns||!r.rows||r.width%r.columns||r.height%r.rows){error="Invalid action texture grid";
 free();
 return false;
 }
   const std::string path=std::string(prefix)+std::string(content.string(r.path));
 if(resident_only&&!encore::ctr::loading_sprite_sheet_resident(path.c_str())){error="Round prewarm resource not resident: "+path;free();return false;}
 a.sheet=encore::ctr::loading_sprite_sheet_acquire(path.c_str());
 if(!a.sheet||encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){error="Missing action texture: "+path;
 free();
 return false;
 }
   const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
 if(!image.tex||!image.subtex||image.subtex->width!=r.width||image.subtex->height!=r.height||Tex3DS_SubTextureRotated(image.subtex)){error="Action texture dimensions differ: "+path;
 free();
 return false;
 }
   a.width=r.width;
 a.height=r.height;
 a.columns=r.columns;
 a.rows=r.rows;
 C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
 
  }return true;
 
 }
 void free(){for(auto&a:assets_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);
 assets_.clear();
 content_={};
 }
 bool draw(const encore::upstream::BattleActionPose&p,const BattleRenderer&font,float expand_x=0,float expand_y=0)const{
  using namespace encore::upstream;
 if(!p.visible)return true;
 
  float x=p.rect.x+p.offset.x+p.anchor.x*expand_x,y=p.rect.y+p.offset.y+p.anchor.y*expand_y;
 
  const auto role=RoundMediaRole(p.role);
 
  if(role==RoundMediaRole::BossFlash){
 // The source shader measures distance in normalized UV, so its circle is an
 // ellipse in source pixels. Expanded view retains source pixel dimensions.
 if(p.radius<=0||p.color.w<=0||p.rect.w<=0)return true;
 const float cx=p.offset.x+expand_x/2,cy=p.offset.y+expand_y/2;
 const float ry=p.radius/2,rx=p.radius*p.rect.z/(2*p.rect.w);
 const int first=std::max(0,int(std::ceil(cy-ry-.5f))),last=std::min(int(p.rect.w+expand_y)-1,int(std::floor(cy+ry-.5f)));
 for(int row=first;row<=last;++row){const float dy=(row+.5f-cy)/ry;const float half=rx*std::sqrt(std::max(0.f,1-dy*dy));
  const float left=std::max(0.f,std::ceil(cx-half-.5f)),right=std::min(p.rect.z+expand_x,std::ceil(cx+half-.5f));
  if(right>left)BattleRenderer::draw_rect(left,float(row),right-left,1,color(p.color));
 }return true;
 }
 if(role==RoundMediaRole::BackgroundDim||role==RoundMediaRole::TransitionRect){
 // Extend the top curtain upward and bottom curtain downward at1:1 scale.
 if(role==RoundMediaRole::TransitionRect&&p.anchor.y==0)y-=expand_y;
 const auto c=BattleValue{p.color.x*p.modulate.x,p.color.y*p.modulate.y,p.color.z*p.modulate.z,p.color.w*p.modulate.w};
 BattleRenderer::draw_rect(x,y,p.rect.z+expand_x,p.rect.w+expand_y,color(c));
 return true;
 }
  if(role==RoundMediaRole::Dialogue){if(p.resource!=round_no_index)return ninepatch(p,x,y);
 if(p.centered)x+=(p.rect.z-font.text_width(p.text.c_str())*p.scale.x)/2;
 // Source project enables GPU pixel snap; match the established text path.
 return font.draw_text(p.text.c_str(),std::floor(x+.5f),std::floor(y+.5f),p.scale.x,p.scale.y,color(p.color));
 }
  if(role==RoundMediaRole::FlyingNumber)return number(p,x,y);
 
  if(p.resource>=assets_.size())return false;
 const auto&a=assets_[p.resource];
 if(p.frame>=a.columns*a.rows)return false;
 const unsigned w=a.width/a.columns,h=a.height/a.rows;
 Tex3DS_SubTexture sub{};
 const auto img=region(p.resource,p.frame%a.columns*w,p.frame/a.columns*h,w,h,sub);
 if(!img.tex)return false;
 
  const float dw=p.rect.z*p.scale.x,dh=p.rect.w*p.scale.y;
 if(dw<=0||dh<=0)return true;
 
  // Actor rectangles are top-left based;
// Effect nodes are source-centered.
  const bool actor=role==RoundMediaRole::PartySprite||role==RoundMediaRole::EnemySprite;
 
  if(actor||!p.centered){x+=p.rect.z/2;
 y+=p.rect.w/2;
 }
  const float radians=p.rotation*std::acos(-1.f)/180.f;
 C2D_ImageTint tint;
 
  auto pass=[&](BattleValue col,float blend){C2D_PlainImageTint(&tint,color(col),blend);
 return C2D_DrawImageAtRotated(img,x,y,0,radians,&tint,dw/w,dh/h);
 };
 
  const float alpha=p.color.w*p.modulate.w;
 
  if(!pass({p.color.x,p.color.y,p.color.z,alpha},0))return false;
 
  if(p.glow_modifier>0){C2D_Flush();
 C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE,GPU_ZERO,GPU_ONE);
 pass({p.glow_color.x,p.glow_color.y,p.glow_color.z,p.glow_modifier*alpha},1);
 C2D_Flush();
 C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
 C2D_Prepare();
 }
  if(p.flash_modifier>0&&!pass({p.flash_color.x,p.flash_color.y,p.flash_color.z,p.flash_modifier*alpha},1))return false;
 
  return true;
 
 }
};
 
