#pragma once
#include <citro2d.h>
namespace encore::ctr {
struct PropTestSheet {C3D_Tex tex;Tex3DS_SubTexture sub;};
using LoadingSpriteSheet=PropTestSheet*;
inline LoadingSpriteSheet loading_sprite_sheet_load(const char*path,std::string*error=nullptr){
 auto i=draw_test::specs.find(path);if(i==draw_test::specs.end()||i->second.missing){if(error)*error="Explicit texture stub missing";return nullptr;}
 const auto s=i->second;auto*v=new PropTestSheet;v->tex.id=s.id;v->sub.width=s.width;v->sub.height=s.height;v->sub.rotated=s.rotated;
 // The fixture models a padded power-of-two root image's true subtexture.
 unsigned w=8,h=8;while(w<s.width)w*=2;while(h<s.height)h*=2;
 v->sub.right=float(s.width)/w;v->sub.bottom=1-float(s.height)/h;++draw_test::live;return v;
}
inline void loading_sprite_sheet_free(LoadingSpriteSheet p){if(p){delete p;--draw_test::live;}}
inline unsigned loading_sprite_sheet_count(LoadingSpriteSheet p){return p?1:0;}
inline C2D_Image loading_sprite_sheet_get_image(LoadingSpriteSheet p,unsigned){return {&p->tex,&p->sub};}
}
