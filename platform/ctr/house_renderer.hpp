#pragma once
#include "loading_texture.hpp"
#include "battle_renderer.hpp"
#include "encore/house_presentation.hpp"
#include <cmath>
#include <string>
#include <vector>

// World dialogue has its own source layout, name tag and clipping. Only the
// already validated EBMain font atlas and generic pixel primitives are shared.
class HouseRenderer {
 struct Asset {encore::ctr::LoadingSpriteSheet sheet=nullptr;uint32_t width=0,height=0,columns=0,rows=0;};
 encore::upstream::HouseView content_;std::vector<Asset>assets_;
 C2D_Image region(uint32_t resource,unsigned u,unsigned v,unsigned w,unsigned h,Tex3DS_SubTexture&sub)const{
  if(resource>=assets_.size())return {};
  const auto&a=assets_[resource];
  if(!a.sheet||!w||!h||u>a.width||w>a.width-u||v>a.height||h>a.height-v)return {};
  const auto img=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);sub=*img.subtex;const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height,left=sub.left,top=sub.top;
  sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;return {img.tex,&sub};
 }
 bool tile(uint32_t resource,unsigned u,unsigned v,unsigned w,unsigned h,float x,float y,float dw,float dh)const{
  Tex3DS_SubTexture sub{};const auto img=region(resource,u,v,w,h,sub);if(!img.tex)return false;
  return C2D_DrawImageAt(img,std::floor(x+.5f),std::floor(y+.5f),0,nullptr,dw/w,dh/h);
 }
 bool ninepatch(uint32_t resource,encore::upstream::BattleValue rect,encore::upstream::BattleValue margin)const{
  if(resource>=assets_.size())return false;
  const auto&a=assets_[resource];
  if(margin.x+margin.z>a.width||margin.y+margin.w>a.height||rect.z<margin.x+margin.z||rect.w<margin.y+margin.w)return false;
  const unsigned u[]={0,unsigned(margin.x),a.width-unsigned(margin.z),a.width},v[]={0,unsigned(margin.y),a.height-unsigned(margin.w),a.height};
  const float x[]={rect.x,rect.x+margin.x,rect.x+rect.z-margin.z,rect.x+rect.z},y[]={rect.y,rect.y+margin.y,rect.y+rect.w-margin.w,rect.y+rect.w};
  for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)if(u[col+1]>u[col]&&v[row+1]>v[row])if(!tile(resource,u[col],v[row],u[col+1]-u[col],v[row+1]-v[row],x[col],y[row],x[col+1]-x[col],y[row+1]-y[row]))return false;
  return true;
 }
 bool centered(uint32_t resource,unsigned frame,float x,float y,float rotation=0)const{
  if(resource>=assets_.size())return false;
  const auto&a=assets_[resource];if(!a.columns||!a.rows||frame>=a.columns*a.rows)return false;
  const unsigned w=a.width/a.columns,h=a.height/a.rows;Tex3DS_SubTexture sub{};const auto img=region(resource,frame%a.columns*w,frame/a.columns*h,w,h,sub);if(!img.tex)return false;
  return rotation?C2D_DrawImageAtRotated(img,std::floor(x+.5f),std::floor(y+.5f),0,rotation):C2D_DrawImageAt(img,std::floor(x-w*.5f+.5f),std::floor(y-h*.5f+.5f),0);
 }
public:
 HouseRenderer()=default;HouseRenderer(const HouseRenderer&)=delete;HouseRenderer&operator=(const HouseRenderer&)=delete;
 bool load(encore::upstream::HouseView content,const char*prefix,std::string&error){
  free();if(!content.valid()||!prefix){error="House renderer needs checked resources";return false;}content_=content;assets_.resize(content.count(encore::upstream::HouseSection::Resources));
  for(uint32_t i=0;i<assets_.size();++i){const auto r=content.resource(i);auto&a=assets_[i];a.width=r.width;a.height=r.height;a.columns=r.columns;a.rows=r.rows;
   const std::string path=std::string(prefix)+std::string(content.string(r.path));a.sheet=encore::ctr::loading_sprite_sheet_load(path.c_str());
   if(!a.sheet||encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){error="Missing house texture: "+path;free();return false;}
   const auto img=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);if(!img.tex||!img.subtex||img.subtex->width!=r.width||img.subtex->height!=r.height||Tex3DS_SubTextureRotated(img.subtex)){error="House texture differs from validated dimensions: "+path;free();return false;}C3D_TexSetFilter(img.tex,GPU_NEAREST,GPU_NEAREST);
  }return true;
 }
 void free(){for(auto&a:assets_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);assets_.clear();content_={};}
 // The scene transition retires House world art while the same checked UI
 // atlas remains owned by the continued dialogue renderer.
 void free_world(){
  using namespace encore::upstream;
  for(uint32_t i=0;i<assets_.size();++i){
   bool retained=false;
   for(uint32_t c=0;c<content_.count(HouseSection::Clips);++c){
    const auto clip=content_.clip(c);
    if(clip.resource==i&&clip.role>=uint32_t(HouseClipRole::DialogueOpen)&&clip.role<=uint32_t(HouseClipRole::Cursor))retained=true;
   }
   if(!retained&&assets_[i].sheet){encore::ctr::loading_sprite_sheet_free(assets_[i].sheet);assets_[i].sheet=nullptr;}
  }
 }
 bool draw_npc(const encore::upstream::HouseNpcPose&p,float cx,float cy)const{
  if(!p.visible)return true;
  if(p.shadow_resource!=encore::upstream::house_no_index&&!centered(p.shadow_resource,0,p.position.x+p.shadow_offset.x-cx,p.position.y+p.shadow_offset.y-cy))return false;
  return centered(p.primary_resource,p.frame,p.position.x+p.sprite_offset.x-cx,p.position.y+p.sprite_offset.y-cy);
 }
 bool draw_door(const encore::upstream::HouseDoorPose&p,float cx,float cy)const{
  return !p.visible||centered(p.resource,0,p.position.x+p.offset.x-cx,p.position.y+p.offset.y-cy);
 }
 bool draw_dialogue(const encore::upstream::WorldDialoguePose&p,const BattleRenderer&font,float expand_x=0,float expand_y=0,float origin_x=0,float origin_y=0)const{
  using namespace encore::upstream;if(!p.visible)return true;const float ox=origin_x+p.anchor.x*expand_x,oy=origin_y+p.anchor.y*expand_y;auto box=p.box,name=p.name;box.x+=ox;box.y+=oy;name.x+=ox;name.y+=oy;
  if(p.name_visible){
  if(!ninepatch(p.name_resource,name,content_.parameter(HouseParameter::NameMargins)))return false;
  const auto fm=content_.parameter(HouseParameter::FontMetrics);const float tx=name.x+p.name_label.x,ty=name.y+p.name_label.y+(p.name_label.w-fm.x)*.5f;
  if(!font.draw_text_clipped(p.speaker.c_str(),tx,ty,name.x,name.y,name.x+name.z-content_.parameter(HouseParameter::NameMargins).z,name.y+name.w,C2D_Color32(255,255,255,255)))return false;
  }
  if(!ninepatch(p.box_resource,box,content_.parameter(HouseParameter::DialogueMargins)))return false;
  if(p.text_visible){const float left=p.clip.x+ox,top=p.clip.y+oy,right=left+p.clip.z,bottom=top+p.clip.w;
   for(const auto&line:p.lines){
    if(!line.colors.empty()&&line.colors.size()!=line.text.size())return false;
    float text_x=left+p.text_layout.x;
    for(size_t begin=0;begin<line.text.size();){const uint32_t color=line.colors.empty()?0xffffffffu:line.colors[begin];size_t end=begin+1;while(end<line.text.size()&&(line.colors.empty()||line.colors[end]==color))++end;const auto run=line.text.substr(begin,end-begin);
     if(!font.draw_text_clipped(run.c_str(),text_x,top+p.text_layout.y+line.y,left,top,right,bottom,color))return false;
     text_x+=font.text_width(run.c_str());begin=end;
    }
    if(line.bullet&&!font.draw_text_clipped(p.bullet.c_str(),left+p.bullet_layout.x+p.bullet_layout.z-font.text_width(p.bullet.c_str()),top+p.bullet_layout.y+line.y,left,top,right,bottom,C2D_Color32(255,255,255,255)))return false;
   }
  }
  return !p.cursor_visible||centered(p.cursor_resource,p.cursor_frame,p.cursor.x+ox,p.cursor.y+oy,p.cursor_rotation);
 }
};
