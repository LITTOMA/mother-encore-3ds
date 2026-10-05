#pragma once
#include "battle_renderer.hpp"
#include "loading_texture.hpp"
#include "encore/item_details.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

// Borrows admitted content. Texture admission and destruction happen outside
// GPU frames; a failed admission preserves the previous live presentation.
class ItemDetailsRenderer {
    using View=encore::upstream::ItemDetailsView;
    using Kind=encore::upstream::ItemDetailsTokenKind;
    struct Asset {encore::ctr::LoadingSpriteSheet sheet=nullptr;uint32_t width=0,height=0;};
    View data_;
    std::vector<Asset> assets_;
    std::string locale_,nickname_;
    mutable std::string error_;
    static float pixel(float value){return std::floor(value+.5f);}
    bool fail(const char* message)const{error_=message;return false;}
    static uint32_t color(uint32_t rgba){return C2D_Color32(rgba&255,(rgba>>8)&255,(rgba>>16)&255,(rgba>>24)&255);}
    static bool image(const Asset& asset,float x,float y,float left,float top,float right,float bottom,uint32_t tint){
        if(!asset.sheet)return false;
        const float gx=pixel(x),gy=pixel(y);
        const int x0=std::max(0,int(std::ceil(left-gx))),y0=std::max(0,int(std::ceil(top-gy)));
        const int x1=std::min(int(asset.width),int(std::floor(right-gx))),y1=std::min(int(asset.height),int(std::floor(bottom-gy)));
        if(x1<=x0||y1<=y0)return true;
        const auto source=encore::ctr::loading_sprite_sheet_get_image(asset.sheet,0);auto sub=*source.subtex;
        const float du=(sub.right-sub.left)/asset.width,dv=(sub.bottom-sub.top)/asset.height;
        const float u=sub.left,v=sub.top;
        sub.left=u+x0*du;sub.right=u+x1*du;sub.top=v+y0*dv;sub.bottom=v+y1*dv;
        sub.width=x1-x0;sub.height=y1-y0;
        C2D_ImageTint paint;C2D_PlainImageTint(&paint,tint,0);
        return C2D_DrawImageAt({source.tex,&sub},gx+x0,gy+y0,0,&paint,1,1);
    }
public:
    ItemDetailsRenderer()=default;
    ItemDetailsRenderer(const ItemDetailsRenderer&)=delete;
    ItemDetailsRenderer& operator=(const ItemDetailsRenderer&)=delete;
    ~ItemDetailsRenderer(){free();}
    bool ready()const{return data_.valid();}
    const std::string& error()const{return error_;}
    void set_locale(std::string_view locale){locale_=std::string(locale);}
    void set_nickname(std::string_view nickname){nickname_=std::string(nickname);}
    bool load(View data,encore::upstream::ItemView items,const char* root,std::string& error){
        error.clear();
        if(!data.valid()||!items.valid()||!root||!*root||!data.bind_items(items,error)){
            if(error.empty())error="Item details require checked content";return false;
        }
        if(!data.verify_resources(root,error))return false;
        ItemDetailsRenderer candidate;candidate.data_=data;
        candidate.assets_.resize(data.count(encore::upstream::ItemDetailsSection::Resources));
        for(uint32_t i=0;i<candidate.assets_.size();++i){
            const auto r=data.resource(i);auto& a=candidate.assets_[i];
            if(r.kind!=1||!r.width||!r.height||r.columns!=1||r.rows!=1){error="Item details image schema rejected";return false;}
            a.width=r.width;a.height=r.height;const auto path=std::string(root)+std::string(data.string(r.path));
            a.sheet=encore::ctr::loading_sprite_sheet_acquire(path.c_str(),&error);
            if(!a.sheet)return false;
            const auto texture=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
            if(encore::ctr::loading_sprite_sheet_count(a.sheet)!=1||!texture.tex||!texture.subtex||Tex3DS_SubTextureRotated(texture.subtex)||
               texture.subtex->width!=a.width||texture.subtex->height!=a.height){error="Item details image dimensions rejected";return false;}
            C3D_TexSetFilter(texture.tex,GPU_NEAREST,GPU_NEAREST);
        }
        C3D_FrameSync();std::swap(data_,candidate.data_);assets_.swap(candidate.assets_);error_.clear();return true;
    }
    void free(){
        if(!assets_.empty())C3D_FrameSync();
        for(auto& asset:assets_)if(asset.sheet)encore::ctr::loading_sprite_sheet_free(asset.sheet);
        assets_.clear();data_={};error_.clear();
    }
    bool draw(encore::upstream::ItemInstance instance,const BattleRenderer& font,float x,float y,float width,float height)const{
        error_.clear();
        if(!data_.valid())return fail("Item details renderer unavailable");
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)
            return fail("Item details rectangle rejected");
        encore::upstream::ItemDetailsComposition runs;
        const auto measure=[&font](std::string_view value){const std::string text(value);return font.text_width(text.c_str());};
        if(!data_.compose(instance.definition,instance.doses,nickname_,locale_,width,measure,runs,error_))return false;
        const float left=pixel(x),top=pixel(y),right=left+width,bottom=top+height;
        for(const auto& atom:runs.atoms){
            if(atom.kind==Kind::Text){
                if(!font.draw_text_clipped(atom.text.c_str(),pixel(left+atom.x),pixel(top+atom.y),left,top,right,bottom,color(atom.color)))
                    return fail("Item details source glyph unavailable");
            }else if(atom.kind==Kind::InlineImage){
                if(atom.resource>=assets_.size()||atom.width!=assets_[atom.resource].width||atom.height!=assets_[atom.resource].height)
                    return fail("Item details inline image reference rejected");
                if(!image(assets_[atom.resource],left+atom.x,top+atom.y,left,top,right,bottom,color(atom.color)))
                    return fail("Item details inline image draw rejected");
            }else return fail("Unknown item details draw atom");
        }
        return true;
    }
};
