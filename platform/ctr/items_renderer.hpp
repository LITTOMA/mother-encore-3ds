#pragma once
#include "loading_texture.hpp"
#include "battle_renderer.hpp"
#include "encore/items_menu.hpp"
#include "encore/localization.hpp"
class ItemsRenderer {
 struct Asset {encore::ctr::LoadingSpriteSheet sheet=nullptr;uint32_t width=0,height=0,columns=0,rows=0;};
 const encore::upstream::LocaleSelection*locale_=nullptr;
 encore::upstream::ItemView data_;std::vector<Asset>assets_;
 mutable std::vector<encore::upstream::ItemMenuPose>poses_;
 mutable std::vector<float>xs_,ys_;
 mutable std::vector<uint8_t>visible_;
 mutable std::vector<uint32_t>slots_;
 static uint32_t color(encore::upstream::BattleValue c){return C2D_Color32f(c.x,c.y,c.z,c.w);}
 bool region(uint32_t resource,uint32_t u,uint32_t v,uint32_t w,uint32_t h,float x,float y,float dw,float dh,uint32_t tint)const{
  if(resource>=assets_.size()||!w||!h)return false;const auto&a=assets_[resource];
  if(!a.sheet||u+w>a.width||v+h>a.height)return false;
  const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);auto sub=*image.subtex;
  const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height,left=sub.left,top=sub.top;
  sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;
  C2D_ImageTint t;C2D_PlainImageTint(&t,tint,0);
  return C2D_DrawImageAt({image.tex,&sub},std::floor(x+.5f),std::floor(y+.5f),0,&t,dw/w,dh/h);
 }
 bool ninepatch(uint32_t resource,const uint32_t*patch,float x,float y,float w,float h,uint32_t tint)const{
  if(resource>=assets_.size())return false;const auto&a=assets_[resource];
  const unsigned u[]={0,patch[0],a.width-patch[2],a.width},v[]={0,patch[1],a.height-patch[3],a.height};
  const float xs[]={x,x+patch[0],x+w-patch[2],x+w},ys[]={y,y+patch[1],y+h-patch[3],y+h};
  if(patch[0]+patch[2]>a.width||patch[1]+patch[3]>a.height||w<patch[0]+patch[2]||h<patch[1]+patch[3])return false;
  for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)if(u[c+1]>u[c]&&v[r+1]>v[r])if(!region(resource,u[c],v[r],u[c+1]-u[c],v[r+1]-v[r],xs[c],ys[r],xs[c+1]-xs[c],ys[r+1]-ys[r],tint))return false;
  return true;
 }
 bool draw_node(uint32_t index,const encore::upstream::BattleItemsMenu&menu,const BattleRenderer&font)const{
  using namespace encore::upstream;if(!visible_[index])return true;
  for(uint32_t i=index+1;i<poses_.size();++i)if(poses_[i].source.parent==index&&(poses_[i].source.flags&uint32_t(ItemLayoutFlag::BehindParent)))if(!draw_node(i,menu,font))return false;
  const auto&p=poses_[index];const auto&l=p.source;const auto role=ItemLayoutRole(l.role);const auto kind=ItemDrawKind(l.kind);
  uint32_t resource=l.resource;float x=xs_[index]+p.offset.x,y=ys_[index]+p.offset.y,w=p.rect.z*p.scale.x,h=p.rect.w*p.scale.y;auto tint=p.color;
  std::string label;
  if(role==ItemLayoutRole::ItemLabel){const auto slot=slots_[index];const auto instance=menu.inventory().instance(slot);const auto d=data_.definition(instance.definition);label=std::string(data_.string(d.name));if(locale_){std::string error;if(!locale_->catalog()->bound("item.name/"+std::to_string(instance.definition),label,locale_->code(),label,error))return false;}tint=data_.parameter(menu.inventory().can_use(slot)?ItemParameter::NormalColor:ItemParameter::DisabledColor);}
  else if(role==ItemLayoutRole::Description&&menu.inventory().size()){
   const auto d=data_.definition(menu.inventory().instance(menu.selection()).definition);std::string localized(data_.string(d.description));if(locale_){std::string error;if(!locale_->catalog()->bound("item.description/"+std::to_string(menu.inventory().instance(menu.selection()).definition),localized,locale_->code(),localized,error))return false;}const std::string_view text(localized);size_t begin=0;
   for(uint32_t line=0;line<l.frame&&begin<text.size();++line){const auto at=text.find('\n',begin);begin=at==std::string_view::npos?text.size():at+1;}
   const auto end=text.find('\n',begin);label=std::string(text.substr(begin,end==std::string_view::npos?text.size()-begin:end-begin));
  }else if(role==ItemLayoutRole::ItemIcon&&kind==ItemDrawKind::Sprite&&menu.inventory().size())resource=data_.definition(menu.inventory().instance(menu.selection()).definition).icon;
  bool ok=true;
  if(kind==ItemDrawKind::Text){ok=font.draw_text_clipped(label.c_str(),std::floor(x+.5f),std::floor(y+.5f),x,y,x+w,y+h,role==ItemLayoutRole::ItemLabel?encore::ctr::loading_menu_flavor_color(color(tint)):color(tint));}
  else if(kind==ItemDrawKind::Rectangle)BattleRenderer::draw_rect(x,y,w,h,color(tint));
  else if(kind==ItemDrawKind::NinePatch)ok=ninepatch(resource,l.patch,x,y,w,h,color(tint));
  else if(kind==ItemDrawKind::Sprite){
   if(resource>=assets_.size())return false;const auto&a=assets_[resource];if(p.frame>=a.columns*a.rows)return false;
   const unsigned fw=a.width/a.columns,fh=a.height/a.rows;
   if(l.flags&uint32_t(ItemLayoutFlag::Centered)){x-=w*.5f;y-=h*.5f;}
   ok=region(resource,p.frame%a.columns*fw,p.frame/a.columns*fh,fw,fh,x,y,w,h,color(tint));
  }
  if(!ok)return false;
  for(uint32_t i=index+1;i<poses_.size();++i)if(poses_[i].source.parent==index&&!(poses_[i].source.flags&uint32_t(ItemLayoutFlag::BehindParent)))if(!draw_node(i,menu,font))return false;
  return true;
 }
public:
 void set_locale(const encore::upstream::LocaleSelection*value){locale_=value;}
 bool load(encore::upstream::ItemView data,const char*root,std::string&error){
  free();if(!data.valid()||!root){error="Missing checked Items assets";return false;}data_=data;assets_.resize(data.count(encore::upstream::ItemSection::Resources));
  for(uint32_t i=0;i<assets_.size();++i){const auto r=data.resource(i);auto&a=assets_[i];a.width=r.width;a.height=r.height;a.columns=r.columns;a.rows=r.rows;
   const auto path=std::string(root)+std::string(data.string(r.path));a.sheet=encore::ctr::loading_sprite_sheet_load(path.c_str());
   if(!a.sheet||encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){error="Items texture unavailable: "+path;free();return false;}
   const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);if(!image.tex||!image.subtex||image.subtex->width!=a.width||image.subtex->height!=a.height||Tex3DS_SubTextureRotated(image.subtex)){error="Items texture metadata mismatch";free();return false;}C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
  }
  const auto n=data.count(encore::upstream::ItemSection::Layouts);poses_.resize(n);xs_.resize(n);ys_.resize(n);visible_.resize(n);slots_.resize(n,encore::upstream::item_no_index);return true;
 }
 void free(){for(auto&a:assets_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);assets_.clear();data_={};poses_.clear();xs_.clear();ys_.clear();visible_.clear();slots_.clear();}
 bool draw(const encore::upstream::BattleItemsMenu&menu,const BattleRenderer&font,float width,float height,float left,float top)const{
  using namespace encore::upstream;if(!menu.visible())return true;if(!data_.valid())return false;
  const auto native=data_.parameter(ItemParameter::SourceViewport),grid=data_.parameter(ItemParameter::GridShape);
  const auto first=menu.row_offset()*uint32_t(grid.x);const auto size=menu.inventory().size();
  for(uint32_t i=0;i<poses_.size();++i){auto&p=poses_[i];p=menu.pose(i);const auto&l=p.source;
   const bool root=l.parent==item_no_index;xs_[i]=p.rect.x+(root?left+(width-native.x)*l.anchor.x:xs_[l.parent]);ys_[i]=p.rect.y+(root?top+(height-native.y)*l.anchor.y:ys_[l.parent]);visible_[i]=p.visible&&(root||visible_[l.parent]);
   slots_[i]=root?item_no_index:slots_[l.parent];
   if(l.role==uint32_t(ItemLayoutRole::ItemLabel)){slots_[i]=first+l.frame;visible_[i]&=slots_[i]<size;}
   if(l.role==uint32_t(ItemLayoutRole::Equipped))visible_[i]&=slots_[i]<size&&menu.inventory().instance(slots_[i]).equipped!=0;
   if(l.role==uint32_t(ItemLayoutRole::Scrollbar))visible_[i]&=size>uint32_t(grid.x*grid.y);
   if(l.role==uint32_t(ItemLayoutRole::InfoPanel))visible_[i]&=size>0;
  }
  for(uint32_t i=0;i<poses_.size();++i)if(poses_[i].source.parent==item_no_index)if(!draw_node(i,menu,font))return false;
  return true;
 }
};
