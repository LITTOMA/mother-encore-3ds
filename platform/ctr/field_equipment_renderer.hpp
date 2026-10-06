#pragma once
#include "battle_renderer.hpp"
#include "item_details_renderer.hpp"
#include "loading_texture.hpp"
#include "encore/field_equipment_menu.hpp"
#include "encore/localization.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

// Borrows checked data and fonts owned by the application. Admission, free and
// destruction must occur outside GPU frames and before graphics shutdown.
class FieldEquipmentRenderer {
    using View=encore::upstream::FieldEquipmentView;
    using Menu=encore::upstream::FieldEquipmentMenu;
    using Phase=encore::upstream::FieldEquipmentPhase;
    using Role=encore::upstream::FieldLayoutRole;
    using Parameter=encore::upstream::FieldParameter;
    using Binding=encore::upstream::FieldBinding;
    using ClipRole=encore::upstream::FieldClipRole;
    using Layout=encore::upstream::ItemLayout;
    using Kind=encore::upstream::ItemDrawKind;
    static constexpr uint32_t none=encore::upstream::item_no_index;
    struct Asset {encore::ctr::LoadingSpriteSheet sheet=nullptr;uint32_t width=0,height=0,columns=0,rows=0;};
    View data_;
    encore::upstream::ItemView items_;
    const encore::upstream::LocaleSelection* locale_=nullptr;
    const ItemDetailsRenderer* details_=nullptr;
    std::vector<Asset> assets_;
    std::vector<Layout> layouts_;
    mutable std::string error_;
    mutable float clip_left_=0,clip_top_=0,clip_right_=0,clip_bottom_=0;
    bool fail(const char* message)const{error_=message;return false;}
    static float pixel(float value){return std::floor(value+.5f);}
    static uint32_t color(encore::upstream::BattleValue value){return encore::ctr::loading_menu_flavor_color(C2D_Color32f(value.x,value.y,value.z,value.w));}
    static bool plain(std::string_view text){
        size_t cursor=0;uint32_t cp=0;
        while(cursor<text.size())if(!encore::utf8_next(text,cursor,cp)||cp=='['||cp==']'||(cp<32&&cp!='\n'))return false;
        return true;
    }
    static bool acquire(Asset& asset,const encore::upstream::BattleResource& r,std::string_view path,const char* root,std::string& error){
        if(r.kind!=1||!r.width||!r.height||!r.columns||!r.rows||r.width%r.columns||r.height%r.rows){error="Field equipment texture schema rejected";return false;}
        asset.width=r.width;asset.height=r.height;asset.columns=r.columns;asset.rows=r.rows;
        const auto full=std::string(root)+std::string(path);asset.sheet=encore::ctr::loading_sprite_sheet_acquire(full.c_str(),&error);
        if(!asset.sheet)return false;
        if(encore::ctr::loading_sprite_sheet_count(asset.sheet)!=1){error="Field equipment atlas subtexture count rejected";return false;}
        const auto image=encore::ctr::loading_sprite_sheet_get_image(asset.sheet,0);
        if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=r.width||image.subtex->height!=r.height){error="Field equipment texture dimensions rejected";return false;}
        C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);return true;
    }
    bool region(const Asset& a,uint32_t u,uint32_t v,uint32_t w,uint32_t h,float x,float y,float width,float height,uint32_t tint)const{
        if(!a.sheet||!w||!h||u>a.width||v>a.height||w>a.width-u||h>a.height-v||width<=0||height<=0)return false;
        x=pixel(x);y=pixel(y);
        const float x0=std::max(x,clip_left_),y0=std::max(y,clip_top_),x1=std::min(x+width,clip_right_),y1=std::min(y+height,clip_bottom_);
        if(x1<=x0||y1<=y0)return true;
        const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);auto sub=*image.subtex;
        const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height,left=sub.left,top=sub.top;
        sub.left=left+(u+(x0-x)*w/width)*du;sub.right=left+(u+(x1-x)*w/width)*du;
        sub.top=top+(v+(y0-y)*h/height)*dv;sub.bottom=top+(v+(y1-y)*h/height)*dv;
        sub.width=uint16_t(pixel(x1-x0));sub.height=uint16_t(pixel(y1-y0));
        if(!sub.width||!sub.height)return true;
        C2D_ImageTint paint;C2D_PlainImageTint(&paint,tint,0);
        return C2D_DrawImageAt({image.tex,&sub},pixel(x0),pixel(y0),0,&paint,1,1);
    }
    bool sprite(const Asset& a,const Layout& l,float dx,float dy)const{
        if(!a.columns||!a.rows||uint64_t(l.frame)>=uint64_t(a.columns)*a.rows)return false;
        const auto w=a.width/a.columns,h=a.height/a.rows;
        float x=dx+l.rect.x+(l.rect.z-w)/2,y=dy+l.rect.y+(l.rect.w-h)/2;
        if(l.flags&uint32_t(encore::upstream::ItemLayoutFlag::Centered)){x=dx+l.rect.x-w/2.f;y=dy+l.rect.y-h/2.f;}
        return region(a,l.frame%a.columns*w,l.frame/a.columns*h,w,h,x,y,w,h,color(l.color));
    }
    bool ninepatch(const Asset& a,const Layout& l,float dx,float dy)const{
        const auto* p=l.patch;
        if(p[0]>a.width||p[2]>a.width-p[0]||p[1]>a.height||p[3]>a.height-p[1]||l.rect.z<p[0]+p[2]||l.rect.w<p[1]+p[3])return false;
        const uint32_t us[]={0,p[0],a.width-p[2],a.width},vs[]={0,p[1],a.height-p[3],a.height};
        const float x=dx+l.rect.x,y=dy+l.rect.y,xs[]={x,x+p[0],x+l.rect.z-p[2],x+l.rect.z},ys[]={y,y+p[1],y+l.rect.w-p[3],y+l.rect.w};
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)if(us[col+1]>us[col]&&vs[row+1]>vs[row])
            if(!region(a,us[col],vs[row],us[col+1]-us[col],vs[row+1]-vs[row],xs[col],ys[row],xs[col+1]-xs[col],ys[row+1]-ys[row],color(l.color)))return false;
        return true;
    }
    bool art(const Layout& l,float dx,float dy)const{
        switch(Kind(l.kind)){
            case Kind::Container:return true;
            case Kind::Rectangle:{const float x0=std::max(pixel(dx+l.rect.x),clip_left_),y0=std::max(pixel(dy+l.rect.y),clip_top_),x1=std::min(pixel(dx+l.rect.x)+l.rect.z,clip_right_),y1=std::min(pixel(dy+l.rect.y)+l.rect.w,clip_bottom_);if(x1>x0&&y1>y0)BattleRenderer::draw_rect(x0,y0,x1-x0,y1-y0,color(l.color));return true;}
            case Kind::NinePatch:if(l.resource>=assets_.size())return fail("Field equipment panel resource missing");return ninepatch(assets_[l.resource],l,dx,dy)||fail("Field equipment panel rejected");
            case Kind::Sprite:if(l.resource>=assets_.size())return fail("Field equipment sprite resource missing");return sprite(assets_[l.resource],l,dx,dy)||fail("Field equipment sprite rejected");
            default:return fail("Unknown field equipment draw operation");
        }
    }
    bool text(const BattleRenderer& font,const Layout& l,std::string_view label,float dx,float dy,bool number=false,bool centered=false,bool right_aligned=false)const{
        if(!plain(label))return fail("Field equipment text controls rejected");
        const float left=dx+l.rect.x,top=dy+l.rect.y,right=left+l.rect.z,bottom=top+l.rect.w;
        const float x0=std::max(left,clip_left_),y0=std::max(top,clip_top_),x1=std::min(right,clip_right_),y1=std::min(bottom,clip_bottom_);
        if(x1<=x0||y1<=y0)return true;
        const auto role=Role(l.role);const auto tint=role==Role::CashLabel||role==Role::CashValue||role==Role::PauseCash?C2D_Color32f(l.color.x,l.color.y,l.color.z,l.color.w):color(l.color);
        const std::string value(label);
        if(!number){const float measured=font.text_width(value.c_str());return font.draw_text_clipped(value.c_str(),pixel(left+(centered?(l.rect.z-measured)/2:right_aligned?l.rect.z-measured:0)),pixel(top),x0,y0,x1,y1,tint)||fail("Field equipment source glyph unavailable");}
        const float spacing=data_.parameter(Parameter::NumberSpacing);size_t cursor=0;uint32_t cp=0;float measured=0;
        while(cursor<label.size()){
            const auto start=cursor;if(!encore::utf8_next(label,cursor,cp)||cp=='\n')return fail("Field equipment number UTF-8 rejected");
            const std::string glyph(label.substr(start,cursor-start));measured+=font.text_width(glyph.c_str());if(cp!=' '&&cursor<label.size())measured+=spacing;
        }
        float x=left+(centered?(l.rect.z-measured)/2:right_aligned?l.rect.z-measured:0);cursor=0;
        while(cursor<label.size()){
            const auto start=cursor;if(!encore::utf8_next(label,cursor,cp))return false;const std::string glyph(label.substr(start,cursor-start));
            if(!font.draw_text_clipped(glyph.c_str(),pixel(x),pixel(top),x0,y0,x1,y1,tint))return fail("Field equipment number glyph unavailable");
            x+=font.text_width(glyph.c_str());if(cp!=' '&&cursor<label.size())x+=spacing;
        }
        return true;
    }
    bool item_name(uint32_t definition,std::string& label)const{
        const auto item=items_.definition(definition);if(!item.id)return fail("Field equipment item definition missing");
        label=std::string(items_.string(item.name));
        if(locale_){if(!locale_->catalog())return fail("Field equipment locale catalog missing");if(!locale_->catalog()->bound("item.name/"+std::to_string(definition),label,locale_->code(),label,error_))return false;}
        return plain(label)||fail("Field equipment item name controls rejected");
    }
    const Layout* layout(Role role)const{
        const auto found=std::find_if(layouts_.begin(),layouts_.end(),[role](const Layout& l){return l.role==uint32_t(role);});
        return found==layouts_.end()?nullptr:&*found;
    }
public:
    FieldEquipmentRenderer()=default;
    FieldEquipmentRenderer(const FieldEquipmentRenderer&)=delete;
    FieldEquipmentRenderer& operator=(const FieldEquipmentRenderer&)=delete;
    ~FieldEquipmentRenderer(){free();}
    void set_locale(const encore::upstream::LocaleSelection* locale){locale_=locale;}
    void set_details(const ItemDetailsRenderer* details){details_=details;}
    bool ready()const{return data_.valid();}
    View content()const{return data_;}
    bool borrowed_rect(Role role,encore::upstream::BattleValue& out)const{const auto* item=layout(role);if(!ready()||!item)return fail("Borrowed field art binding missing");out=item->rect;return true;}
    // Borrow already admitted source art. The application keeps this renderer
    // alive while the submenu uses it; no second texture owner is introduced.
    bool draw_borrowed_art(Role role,encore::upstream::BattleValue rect,uint32_t frame,float left,float top,float width,float height,const encore::upstream::BattleValue* checked_patch=nullptr)const{
        if(!ready()||width<=0||height<=0)return fail("Borrowed field art is unavailable");
        const auto* source=layout(role);if(!source)return fail("Borrowed field art role missing");
        auto item=*source;item.rect=rect;item.frame=frame;
        if(checked_patch){const float values[]={checked_patch->x,checked_patch->y,checked_patch->z,checked_patch->w};for(size_t i=0;i<4;++i){if(!std::isfinite(values[i])||values[i]<0||values[i]>UINT32_MAX||std::floor(values[i])!=values[i])return fail("Borrowed field patch rejected");item.patch[i]=uint32_t(values[i]);}}
        clip_left_=left;clip_top_=top;clip_right_=left+width;clip_bottom_=top+height;
        return art(item,0,0);
    }
    const std::string& error()const{return error_;}
    bool load(View data,encore::upstream::ItemView items,const char* root,std::string& error){
        error.clear();if(!data.valid()||!items.valid()||!root||!*root||!data.bind_items(items,error)){if(error.empty())error="Field equipment requires checked content";return false;}
        if(!data.verify_resources(root,error))return false;
        FieldEquipmentRenderer candidate;candidate.data_=data;candidate.items_=items;
        candidate.assets_.resize(data.count(encore::upstream::FieldSection::Resources));
        for(uint32_t i=0;i<candidate.assets_.size();++i){const auto r=data.resource(i);if(!acquire(candidate.assets_[i],r.image,data.string(r.image.path),root,error))return false;}
        for(uint32_t i=0;i<data.count(encore::upstream::FieldSection::Layouts);++i){const auto l=data.layout(i);
            if(l.parent!=none||l.role<uint32_t(Role::PausePanel)||l.role>uint32_t(Role::ItemsActionPanel)||l.kind<uint32_t(Kind::Container)||l.kind>uint32_t(Kind::Text)||(l.flags&~(uint32_t(encore::upstream::ItemLayoutFlag::Centered)|uint32_t(encore::upstream::ItemLayoutFlag::Visible)))){error="Unsupported field equipment layout operation";return false;}
            candidate.layouts_.push_back(l);
        }
        C3D_FrameSync();std::swap(data_,candidate.data_);std::swap(items_,candidate.items_);assets_.swap(candidate.assets_);layouts_.swap(candidate.layouts_);error_.clear();return true;
    }
    void free(){
        if(!assets_.empty())C3D_FrameSync();for(auto& asset:assets_)if(asset.sheet)encore::ctr::loading_sprite_sheet_free(asset.sheet);
        assets_.clear();layouts_.clear();data_={};items_={};error_.clear();
    }
    bool draw(const Menu& menu,const BattleRenderer& main_font,const BattleRenderer& number_font,float width,float height,float left=0,float top=0)const;
};

inline bool FieldEquipmentRenderer::draw(const Menu& menu,const BattleRenderer& main_font,const BattleRenderer& number_font,float width,float height,float left,float top)const{
    if(!menu.visible())return true;error_.clear();
    if(!data_.valid()||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return fail("Field equipment presentation unavailable");
    const auto& state=menu.snapshot();
    if(state.owner!=data_.binding(Binding::Owner)||!state.inventory.valid())return fail("Unsupported field equipment owner");
    const float dx=left+(width-data_.parameter(Parameter::ReferenceWidth))/2;
    const float dy=top+(height-data_.parameter(Parameter::ReferenceHeight))/2;
    // Source menus slide off their own canvas; the extra native screen area
    // must not reveal a panel that the source already moved out of view.
    clip_left_=pixel(dx);clip_top_=pixel(dy);clip_right_=pixel(dx+data_.parameter(Parameter::ReferenceWidth));clip_bottom_=pixel(dy+data_.parameter(Parameter::ReferenceHeight));
    const auto phase=menu.phase();
    const float pause_y=dy+menu.animation_value(phase==Phase::PauseClosing?ClipRole::PauseClose:ClipRole::PauseOpen);
    const float equip_y=dy+menu.animation_value(phase==Phase::EquipClosing?ClipRole::EquipClose:ClipRole::EquipOpen);
    const float description_y=dy+menu.animation_value(ClipRole::DescriptionOpen);
    const bool chinese=menu.chinese();
    const uint32_t columns=uint32_t(data_.parameter(Parameter::PauseColumns));
    const uint32_t rows=uint32_t(data_.parameter(Parameter::ListRows));
    if(!columns||!rows)return fail("Field equipment layout bounds rejected");
    const bool selecting=phase==Phase::Candidates;
    const auto* selected=menu.selected_item();
    auto portrait_visible=[&](Role role){
        if(!selected)return false;
        if(role==Role::PortraitEquipped)return selected->equipped!=0;
        encore::upstream::FieldEquipment equipment;bool suitable=false;
        for(uint32_t i=0;i<data_.count(encore::upstream::FieldSection::Equipment);++i){const auto e=data_.equipment(i);if(e.definition==selected->definition){equipment=e;suitable=true;break;}}
        if(role==Role::PortraitSuitable)return suitable;
        if(!suitable||selected->equipped)return false;
        double difference=0;const auto* previous=menu.equipped_item(equipment.slot);
        for(size_t stat=0;stat<equipment.boosts.size();++stat){
            int32_t prior=0;if(previous)for(uint32_t i=0;i<data_.count(encore::upstream::FieldSection::Equipment);++i){const auto e=data_.equipment(i);if(e.definition==previous->definition){prior=e.boosts[stat];break;}}
            difference+=(equipment.boosts[stat]-prior)*data_.parameter(Parameter(uint32_t(Parameter::BoostWeight0)+stat));
        }
        return role==Role::PortraitBetter?difference>0:role==Role::PortraitLower&&difference<0;
    };
    for(const auto& source:layouts_){
        const auto role=Role(source.role);auto l=source;
        const bool pause=role==Role::PausePanel||role==Role::PauseInside||role==Role::PauseTitle||role==Role::PauseCommand||role==Role::PauseCash||role==Role::PauseCursor||role==Role::CashLabel||role==Role::CashValue||role==Role::CashCents;
        if(!pause&&!menu.equipment_visible())continue;
        if(role==Role::BoostEmpty||role==Role::BoostBetter||role==Role::BoostLower||role==Role::Cursor)continue;
        const float y=pause?pause_y:equip_y;
        switch(role){
            case Role::ItemsTargetPanel:case Role::ItemsActionPanel:break;
            case Role::PauseTitle:case Role::EquipTitle:
                if(!text(main_font,l,data_.binding(role==Role::PauseTitle?Binding::PauseTitle:Binding::EquipTitle,chinese),dx,y))return false;break;
            case Role::PauseCommand:
                for(uint32_t i=0;i<data_.count(encore::upstream::FieldSection::Commands);++i){
                    l=source;l.rect.x+=(i%columns)*data_.parameter(Parameter::PauseColumnPitch);l.rect.y+=(i/columns)*data_.parameter(Parameter::PauseRowPitch);
                    const auto command=data_.command(i);if(!text(main_font,l,data_.string(chinese?command.zh:command.en),dx,y))return false;
                }break;
            case Role::CashLabel:
                if(!text(main_font,l,data_.binding(Binding::CashPattern,chinese),dx,y))return false;break;
            case Role::CashValue:
                // CashBoxPause uses EBMain, unlike the Equip stats numbers.
                if(!text(main_font,l,std::to_string(state.cash),dx,y,false,false,true))return false;break;
            case Role::PauseCash:
                if(!text(main_font,l,data_.binding(Binding::CashRight,chinese),dx,y))return false;break;
            case Role::Owner:
                if(!text(main_font,l,state.nickname,dx,y))return false;break;
            case Role::LevelLabel:
                if(!text(main_font,l,data_.binding(Binding::Level,chinese),dx,y))return false;break;
            case Role::LevelValue:
                if(!text(number_font,l,std::to_string(state.level),dx,y,true))return false;break;
            case Role::SlotLabel:
                for(uint32_t i=0;i<data_.count(encore::upstream::FieldSection::Slots);++i){l=source;l.rect.y+=i*data_.parameter(Parameter::SlotPitch);const auto slot=data_.slot(i);if(!text(main_font,l,data_.string(chinese?slot.zh:slot.en),dx,y))return false;}break;
            case Role::SlotItem:
                for(uint32_t i=0;i<data_.count(encore::upstream::FieldSection::Slots);++i){
                    l=source;l.rect.y+=i*data_.parameter(Parameter::SlotPitch);std::string label;const auto* item=menu.equipped_item(i);
                    if(item){if(!item_name(item->definition,label))return false;}else label=std::string(data_.binding(Binding::Empty,chinese));
                    if(!text(main_font,l,label,dx,y))return false;
                }break;
            case Role::ListPanel:if(selecting&&!art(l,dx,y))return false;break;
            case Role::ListItem:
                if(selecting)for(uint32_t i=menu.candidate_offset();i<menu.candidate_count()&&i-menu.candidate_offset()<rows;++i){
                    l=source;l.rect.y+=(i-menu.candidate_offset())*data_.parameter(Parameter::ListPitch);const auto candidate=menu.candidate(i);std::string label;
                    if(candidate.none)label=std::string(data_.binding(Binding::None,chinese));else if(!item_name(candidate.item.definition,label))return false;
                    if(!text(main_font,l,label,dx,y))return false;
                }break;
            case Role::StatLabel:case Role::StatValue:case Role::StatProjected:case Role::StatIcon:
                for(size_t i=0;i<state.stats.size();++i){
                    l=source;l.rect.y+=i*data_.parameter(Parameter::StatPitch);
                    if(role==Role::StatLabel){if(!text(main_font,l,data_.binding(Binding(uint32_t(Binding::StatMaxHP)+i),chinese),dx,y))return false;}
                    else if(role==Role::StatValue){if(!text(number_font,l,std::to_string(state.stats[i]),dx,y,true))return false;}
                    else if(role==Role::StatProjected){if(state.stats[i]!=menu.preview_stats()[i]&&!text(number_font,l,std::to_string(menu.preview_stats()[i]),dx,y,true,false,true))return false;}
                    else {const auto* icon=layout(state.stats[i]==menu.preview_stats()[i]?Role::BoostEmpty:menu.preview_stats()[i]>state.stats[i]?Role::BoostBetter:Role::BoostLower);if(!icon)return fail("Field equipment boost binding missing");l.kind=uint32_t(Kind::Sprite);l.resource=icon->resource;l.frame=icon->frame;if(!art(l,dx,y))return false;}
                }break;
            case Role::PauseCursor:
                if(phase==Phase::Pause&&!menu.items_suspended()){l.rect.x+=menu.cursor_column()*data_.parameter(Parameter::PauseColumnPitch);l.rect.y+=menu.cursor_row()*data_.parameter(Parameter::PauseRowPitch);l.frame=menu.cursor_frame();if(!art(l,dx,y))return false;}break;
            case Role::SlotCursor:
                if(phase==Phase::Slots){l.rect.y+=menu.cursor_row()*data_.parameter(Parameter::SlotPitch);l.frame=menu.cursor_frame();if(!art(l,dx,y))return false;}break;
            case Role::CandidateCursor:
                if(selecting){l.rect.y+=menu.cursor_row()*data_.parameter(Parameter::ListPitch);l.frame=menu.cursor_frame();if(!art(l,dx,y))return false;}break;
            case Role::DescriptionPanel:
                if(menu.description_visible()&&!art(l,dx,description_y))return false;break;
            case Role::DescriptionText:
                if(menu.description_visible()){const auto* described=menu.description_item();if(!described)break;if(!details_||!details_->ready())return fail("Field equipment description consumer unavailable");const float text_y=pixel(description_y+l.rect.y),text_height=std::min(l.rect.w,clip_bottom_-text_y);if(text_height>0&&!details_->draw(*described,main_font,dx+l.rect.x,text_y,l.rect.z,text_height))return fail(details_->error().c_str());}break;
            case Role::PortraitEquipped:case Role::PortraitSuitable:case Role::PortraitBetter:case Role::PortraitLower:
                if(portrait_visible(role)&&!art(l,dx,y))return false;break;
            case Role::PausePanel:case Role::PauseInside:case Role::EquipmentPanel:case Role::StatsPanel:case Role::Portrait:case Role::SlotPanel:case Role::CashCents:
                if(!art(l,dx,y))return false;break;
            default:return fail("Unknown field equipment layout role");
        }
    }
    return true;
}
