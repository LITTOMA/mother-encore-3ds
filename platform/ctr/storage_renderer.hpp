#pragma once
#include "battle_renderer.hpp"
#include "loading_texture.hpp"
#include "encore/storage_menu.hpp"
#include "encore/localization.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

// The renderer borrows checked content and the application's admitted fonts.
// load/free/destruction belong outside an active GPU frame and before gfxExit.
// All source coordinates are retained at 1:1; the viewport adds translation.
class StorageRenderer {
    using View=encore::upstream::StorageView;
    using Items=encore::upstream::ItemView;
    using Menu=encore::upstream::StorageMenu;
    using Layout=encore::upstream::ItemLayout;
    using Role=encore::upstream::StorageLayoutRole;
    using Kind=encore::upstream::ItemDrawKind;
    using Parameter=encore::upstream::StorageParameter;
    using Binding=encore::upstream::StorageBinding;
    using Phase=encore::upstream::StorageMenuPhase;
    static constexpr uint32_t none=encore::upstream::item_no_index;
    struct Asset {
        encore::ctr::LoadingSpriteSheet sheet=nullptr;
        uint32_t width=0,height=0,columns=0,rows=0;
    };
    View data_;Items items_;
    const encore::upstream::LocaleSelection* locale_=nullptr;
    std::vector<Asset> assets_,icons_;
    std::vector<Layout> layouts_;
    mutable std::string error_;
    static float pixel(float x){return std::floor(x+.5f);}
    static uint32_t color(encore::upstream::BattleValue c){
        return encore::ctr::loading_menu_flavor_color(C2D_Color32f(c.x,c.y,c.z,c.w));
    }
    bool fail(const char* message)const{error_=message;return false;}
    static bool acquire(Asset& a,const encore::upstream::BattleResource& r,
                        std::string_view path,const char* root,std::string& error){
        if(r.kind!=1||!r.width||!r.height||!r.columns||!r.rows||
           r.width%r.columns||r.height%r.rows){error="Storage texture schema rejected";return false;}
        a.width=r.width;a.height=r.height;a.columns=r.columns;a.rows=r.rows;
        const auto full=std::string(root)+std::string(path);
        a.sheet=encore::ctr::loading_sprite_sheet_acquire(full.c_str(),&error);
        if(!a.sheet)return false;
        if(encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){error="Storage atlas must have one subtexture";return false;}
        const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
        if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||
           image.subtex->width!=a.width||image.subtex->height!=a.height){error="Storage texture dimensions rejected";return false;}
        C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);return true;
    }
    static bool region(const Asset& a,uint32_t u,uint32_t v,uint32_t w,uint32_t h,
                       float x,float y,float width,float height,uint32_t tint){
        if(!a.sheet||!w||!h||u>a.width||v>a.height||w>a.width-u||h>a.height-v||width<=0||height<=0)return false;
        const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
        auto sub=*image.subtex;
        const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height;
        const float left=sub.left,top=sub.top;
        sub.left=left+u*du;sub.right=left+(u+w)*du;
        sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;
        C2D_ImageTint t;C2D_PlainImageTint(&t,tint,0);
        return C2D_DrawImageAt({image.tex,&sub},pixel(x),pixel(y),0,&t,width/w,height/h);
    }
    static bool sprite(const Asset& a,uint32_t frame,const Layout& l,float dx,float dy){
        if(!a.columns||!a.rows||uint64_t(frame)>=uint64_t(a.columns)*a.rows)return false;
        const auto w=a.width/a.columns,h=a.height/a.rows;
        // The source TextureRect keeps the original image centered in its box.
        // Do not enlarge a 19-pixel portrait to fill its 21-pixel control.
        float x=dx+l.rect.x+(l.rect.z-w)/2,y=dy+l.rect.y+(l.rect.w-h)/2;
        if(l.flags&uint32_t(encore::upstream::ItemLayoutFlag::Centered)){
            x=dx+l.rect.x-w/2.f;y=dy+l.rect.y-h/2.f;
        }
        return region(a,frame%a.columns*w,frame/a.columns*h,w,h,x,y,w,h,color(l.color));
    }
    static bool ninepatch(const Asset& a,const Layout& l,float dx,float dy){
        const auto* p=l.patch;
        if(p[0]>a.width||p[2]>a.width-p[0]||p[1]>a.height||p[3]>a.height-p[1]||
           l.rect.z<p[0]+p[2]||l.rect.w<p[1]+p[3])return false;
        const uint32_t u[]={0,p[0],a.width-p[2],a.width},v[]={0,p[1],a.height-p[3],a.height};
        const float x=dx+l.rect.x,y=dy+l.rect.y;
        const float xs[]={x,x+p[0],x+l.rect.z-p[2],x+l.rect.z},ys[]={y,y+p[1],y+l.rect.w-p[3],y+l.rect.w};
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col){
            if(u[col+1]==u[col]||v[row+1]==v[row])continue;
            if(!region(a,u[col],v[row],u[col+1]-u[col],v[row+1]-v[row],xs[col],ys[row],
                       xs[col+1]-xs[col],ys[row+1]-ys[row],color(l.color)))return false;
        }
        return true;
    }
    bool art(const Layout& l,float dx,float dy)const{
        const auto kind=Kind(l.kind);
        if(kind==Kind::Container)return true;
        if(kind==Kind::Rectangle){BattleRenderer::draw_rect(pixel(dx+l.rect.x),pixel(dy+l.rect.y),l.rect.z,l.rect.w,color(l.color));return true;}
        if(l.resource>=assets_.size())return fail("Storage layout resource unavailable");
        if(kind==Kind::NinePatch)return ninepatch(assets_[l.resource],l,dx,dy)||fail("Storage nine-patch rejected");
        if(kind==Kind::Sprite)return sprite(assets_[l.resource],l.frame,l,dx,dy)||fail("Storage sprite rejected");
        return fail("Unknown Storage draw operation");
    }
    bool pane(const Layout& l,bool& stored)const{
        if(l.parent>=layouts_.size())return fail("Storage pane owner missing");
        const auto role=Role(layouts_[l.parent].role);
        if(role!=Role::OnHandList&&role!=Role::StoredList)return fail("Storage pane owner rejected");
        stored=role==Role::StoredList;return true;
    }
    static bool plain(std::string_view text){
        size_t cursor=0;uint32_t cp=0;
        while(cursor<text.size()){
            if(!encore::utf8_next(text,cursor,cp)||cp=='['||cp==']'||(cp<32&&cp!='\n'))return false;
        }
        return true;
    }
    bool item_text(uint32_t id,bool description,std::string& out)const{
        const auto d=items_.definition(id);
        if(!d.id)return fail("Storage item definition unavailable");
        out=std::string(items_.string(description?d.description:d.name));
        if(locale_){
            if(!locale_->catalog())return fail("Storage locale catalog missing");
            if(!locale_->catalog()->bound((description?"item.description/":"item.name/")+std::to_string(id),
                out,locale_->code(),out,error_))return false;
        }
        return plain(out)||fail("Unsupported Storage text controls");
    }
    bool text(const BattleRenderer& font,const Layout& l,std::string_view value,float dx,float dy,
              bool wrap=false,bool centered=false)const{
        if(!plain(value))return fail("Unsupported Storage text controls");
        const float x=dx+l.rect.x,y=dy+l.rect.y,w=l.rect.z,h=l.rect.w;
        if(!wrap){
            const std::string label(value);const float origin=centered?x+(w-font.text_width(label.c_str()))/2:x;
            return font.draw_text_clipped(label.c_str(),pixel(origin),pixel(y),x,y,x+w,y+h,color(l.color))||fail("Storage glyph unavailable");
        }
        // Bounded plain UTF-8 wrapping only. The checked control determines the
        // maximum lines; original rich tags remain gated by ItemDefinitionFlag.
        const float pitch=font.text_height(data_.parameter(Parameter::TextLineHeight));
        if(!std::isfinite(pitch)||pitch<=0)return fail("Storage font height rejected");
        const auto limit=uint32_t(std::floor(h/pitch));if(!limit)return fail("Storage text control too small");
        size_t begin=0;
        for(uint32_t row=0;row<limit&&begin<value.size();++row){
            size_t end=begin,last_space=value.npos;uint32_t cp=0;bool newline=false;
            while(end<value.size()){
                const auto previous=end;if(!encore::utf8_next(value,end,cp))return fail("Storage UTF-8 rejected");
                if(cp=='\n'){end=previous;newline=true;break;}
                if(cp==' ')last_space=previous;
                const std::string probe(value.substr(begin,end-begin));
                if(font.text_width(probe.c_str())>w){end=previous;break;}
            }
            if(newline&&end==begin){++begin;continue;}
            if(end==begin){size_t cursor=end;if(!encore::utf8_next(value,cursor,cp))return false;end=cursor;}
            else if(!newline&&end<value.size()&&last_space!=value.npos&&last_space>begin)end=last_space;
            const std::string line(value.substr(begin,end-begin));
            if(!font.draw_text_clipped(line.c_str(),pixel(x),pixel(y+row*pitch),x,y,x+w,y+h,color(l.color)))return fail("Storage glyph unavailable");
            begin=end;while(begin<value.size()&&value[begin]==' ')++begin;
            if(begin<value.size()&&value[begin]=='\n')++begin;
        }
        return true;
    }
    bool selected(const Menu& menu,uint32_t& definition)const{
        if(menu.phase()==Phase::AskEquip||menu.phase()==Phase::AskUnequip){definition=menu.prompt_definition();return true;}
        const auto count=menu.storage_panel()?menu.storage().size():menu.inventory().size();
        if(!count)return false;if(menu.selection()>=count)return false;
        definition=(menu.storage_panel()?menu.storage().instance(menu.selection()):menu.inventory().instance(menu.selection())).definition;return true;
    }
    double equipment_score(uint32_t definition)const{
        for(uint32_t i=0;i<data_.count(encore::upstream::StorageSection::Equipment);++i){
            const auto e=data_.equipment(i);if(e.definition!=definition)continue;
            double score=0;
            for(uint32_t stat=0;stat<e.boosts.size();++stat)
                score+=e.boosts[stat]*data_.parameter(Parameter(uint32_t(Parameter::BoostWeight0)+stat));
            return score;
        }
        return 0;
    }
    bool portrait_visible(Role role,const Menu& menu)const{
        // Source portraits use the list's current item, including after a
        // withdrawal; the prompt separately keeps the transferred item's icon.
        const auto count=menu.storage_panel()?menu.storage().size():menu.inventory().size();
        if(!count||menu.selection()>=count)return false;
        const auto instance=menu.storage_panel()?menu.storage().instance(menu.selection()):menu.inventory().instance(menu.selection());
        const auto d=items_.definition(instance.definition);
        const bool full=menu.storage_panel()&&!menu.inventory().has_space();
        const bool equipped=!menu.storage_panel()&&instance.equipped;
        if(role==Role::PortraitFull)return full;
        if(role==Role::PortraitEquipped)return equipped;
        bool suitable=false;
        if(d.flags&uint32_t(encore::upstream::ItemDefinitionFlag::Equipment))
            for(uint32_t i=0;i<data_.count(encore::upstream::StorageSection::Equipment);++i)
                suitable|=data_.equipment(i).definition==instance.definition;
        if(role==Role::PortraitSuitable)return suitable&&!equipped&&!full;
        if(!suitable||equipped)return false;
        double previous=0;
        for(uint32_t i=0;i<menu.inventory().size();++i){const auto e=menu.inventory().instance(i);
            if(e.equipped&&items_.definition(e.definition).equipment_slot==d.equipment_slot)previous+=equipment_score(e.definition);
        }
        const double difference=equipment_score(instance.definition)-previous;
        return role==Role::PortraitBetter?difference>0:role==Role::PortraitLower&&difference<0;
    }
    bool prompt(const Menu& menu,std::string& out)const{
        if(menu.phase()==Phase::Warning){out=menu.prompt();return plain(out)||fail("Storage warning controls rejected");}
        auto b=menu.phase()==Phase::AskUnequip?Binding::UnequipEn:Binding::EquipEn;
        if(menu.chinese())b=Binding(uint32_t(b)+1);out=std::string(data_.binding(b));
        const std::string token="{item}";const auto at=out.find(token);
        if(at!=out.npos){std::string name;if(!item_text(menu.prompt_definition(),false,name))return false;
            out.replace(at,token.size(),name);if(out.find(token)!=out.npos)return fail("Repeated Storage item substitution");}
        return plain(out)||fail("Storage prompt controls rejected");
    }
    bool counter(const Menu& menu,std::string& out)const{
        out=std::string(data_.binding(Binding::CounterPattern));
        for(auto number:{menu.storage().size(),menu.storage().capacity()}){
            const auto at=out.find("%s");if(at==out.npos)return fail("Storage counter format rejected");out.replace(at,2,std::to_string(number));
        }
        return (out.find('%')==out.npos&&plain(out))||fail("Storage counter format rejected");
    }
    bool counter_text(const BattleRenderer& font,const Layout& l,std::string_view value,float dx,float dy)const{
        // The Introduction font catalog preserves original raw advances. Godot
        // adds extra_spacing_char only to a non-space followed by another char.
        // The spacing itself is checked source data, not a font substitution.
        const float spacing=data_.parameter(Parameter::CounterCharacterSpacing);
        size_t cursor=0;uint32_t cp=0;float measured=0;
        while(cursor<value.size()){
            const auto begin=cursor;if(!encore::utf8_next(value,cursor,cp)||cp=='\n')return fail("Storage counter UTF-8 rejected");
            const std::string glyph(value.substr(begin,cursor-begin));measured+=font.text_width(glyph.c_str());
            if(cp!=' '&&cursor<value.size())measured+=spacing;
        }
        const float left=dx+l.rect.x,top=dy+l.rect.y,right=left+l.rect.z,bottom=top+l.rect.w;
        float x=pixel(left+(l.rect.z-measured)/2);cursor=0;
        while(cursor<value.size()){
            const auto begin=cursor;if(!encore::utf8_next(value,cursor,cp))return false;
            const std::string glyph(value.substr(begin,cursor-begin));
            if(!font.draw_text_clipped(glyph.c_str(),pixel(x),pixel(top),left,top,right,bottom,color(l.color)))return fail("Storage counter glyph unavailable");
            x+=font.text_width(glyph.c_str());if(cp!=' '&&cursor<value.size())x+=spacing;
        }
        return true;
    }
public:
    StorageRenderer()=default;
    StorageRenderer(const StorageRenderer&)=delete;
    StorageRenderer& operator=(const StorageRenderer&)=delete;
    ~StorageRenderer(){free();}
    void set_locale(const encore::upstream::LocaleSelection* locale){locale_=locale;}
    bool ready()const{return data_.valid();}
    const std::string& error()const{return error_;}
    bool load(View data,Items items,const char* root,std::string& error){
        error.clear();
        if(!data.valid()||!items.valid()||!root||!*root||!data.bind_items(items,error)){if(error.empty())error="Storage requires checked content";return false;}
        StorageRenderer candidate;candidate.data_=data;candidate.items_=items;
        candidate.assets_.resize(data.count(encore::upstream::StorageSection::Resources));
        for(uint32_t i=0;i<candidate.assets_.size();++i){const auto r=data.resource(i);
            if(!acquire(candidate.assets_[i],r,data.string(r.path),root,error))return false;}
        candidate.icons_.resize(items.count(encore::upstream::ItemSection::Resources));
        for(uint32_t i=0;i<items.count(encore::upstream::ItemSection::Definitions);++i){
            const auto rindex=items.definition(i).icon;if(rindex==none)continue;
            if(rindex>=candidate.icons_.size()){error="Storage item icon reference rejected";return false;}
            if(candidate.icons_[rindex].sheet)continue;const auto r=items.resource(rindex);
            if(!acquire(candidate.icons_[rindex],r,items.string(r.path),root,error))return false;
        }
        for(uint32_t i=0;i<data.count(encore::upstream::StorageSection::Layouts);++i){const auto l=data.layout(i);
            if(l.role<uint32_t(Role::Container)||l.role>uint32_t(Role::QuestionCursor)||l.kind<uint32_t(Kind::Container)||l.kind>uint32_t(Kind::Text)||
               (l.flags&~uint32_t(encore::upstream::ItemLayoutFlag::Centered))){error="Unsupported Storage layout operation";return false;}
            const auto role=Role(l.role);const auto kind=Kind(l.kind);bool supported=false;
            switch(role){
                case Role::Title:case Role::Counter:case Role::ItemLabel:case Role::Prompt:
                case Role::Yes:case Role::No:case Role::DescriptionText:supported=kind==Kind::Text;break;
                case Role::Panel:case Role::OnHandList:case Role::StoredList:case Role::Description:
                case Role::IconFrame:case Role::Scrollbar:case Role::ScrollThumb:supported=kind==Kind::NinePatch;break;
                case Role::Portrait:case Role::Cursor:case Role::QuestionCursor:case Role::Equipped:
                case Role::PortraitEquipped:case Role::PortraitSuitable:case Role::PortraitBetter:
                case Role::PortraitLower:case Role::PortraitFull:case Role::Separator:supported=kind==Kind::Sprite;break;
                case Role::Highlight:supported=kind==Kind::Rectangle;break;
                case Role::Container:case Role::ItemIcon:supported=kind==Kind::Container;break;
            }
            if(!supported){error="Storage layout role/draw-kind mismatch";return false;}
            candidate.layouts_.push_back(l);
        }
        C3D_FrameSync();std::swap(data_,candidate.data_);std::swap(items_,candidate.items_);
        assets_.swap(candidate.assets_);icons_.swap(candidate.icons_);layouts_.swap(candidate.layouts_);
        error_.clear();error.clear();return true;
    }
    void free(){
        if(!assets_.empty()||!icons_.empty())C3D_FrameSync();
        for(auto& a:assets_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);
        for(auto& a:icons_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);
        assets_.clear();icons_.clear();layouts_.clear();data_={};items_={};error_.clear();
    }
    // counter_font must use the separately admitted source BottleRocket face.
    // main_font uses the source EBMain face for the selected locale.
    bool draw(const Menu& menu,const BattleRenderer& main_font,const BattleRenderer& counter_font,
              float width,float height,float left=0,float top=0)const{
        if(!menu.active())return true;error_.clear();
        if(!data_.valid()||!data_.same_content(menu.content()))return fail("Storage renderer content owner mismatch");
        if(!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(left)||!std::isfinite(top)||width<=0||height<=0)return fail("Storage viewport rejected");
        if(menu.chinese()&&(!locale_||!locale_->catalog()))return fail("Chinese Storage item binding unavailable");
        const float dx=left+(width-data_.parameter(Parameter::ReferenceWidth))/2,
                    dy=top+(height-data_.parameter(Parameter::ReferenceHeight))/2;
        const auto rows=uint32_t(data_.parameter(Parameter::Rows));const float pitch=data_.parameter(Parameter::RowPitch);
        const bool asking=menu.phase()==Phase::AskUnequip||menu.phase()==Phase::AskEquip;
        const bool warning=menu.phase()==Phase::Warning;uint32_t definition=none;const bool has_item=selected(menu,definition);
        for(const auto& source:layouts_){auto l=source;const auto role=Role(l.role);
            if(role==Role::ItemLabel||role==Role::Equipped){
                bool stored=false;if(!pane(l,stored))return false;
                const auto first=menu.panel_row_offset(stored),count=stored?menu.storage().size():menu.inventory().size();
                for(uint32_t row=0;row<rows&&first+row<count;++row){
                    const auto instance=stored?menu.storage().instance(first+row):menu.inventory().instance(first+row);
                    auto cell=l;cell.rect.y+=row*pitch;
                    if(role==Role::Equipped){if(instance.equipped&&!art(cell,dx,dy))return false;}
                    else{
                        // A label's normal StyleBox draws behind its glyphs.
                        // The IR retains this separate source-derived style.
                        if(!asking&&stored==menu.storage_panel()&&first+row==menu.panel_selection(stored)){
                            auto highlight=std::find_if(layouts_.begin(),layouts_.end(),[&](const Layout& r){return r.role==uint32_t(Role::Highlight)&&r.parent==l.parent;});
                            if(highlight==layouts_.end())return fail("Storage highlight style missing");
                            auto background=*highlight;background.rect.y+=row*pitch;if(!art(background,dx,dy))return false;
                        }
                        std::string label;if(!item_text(instance.definition,false,label)||!text(main_font,cell,label,dx,dy))return false;
                    }
                }
                continue;
            }
            if(role==Role::Highlight)continue;
            if(role==Role::PortraitEquipped||role==Role::PortraitSuitable||role==Role::PortraitBetter||role==Role::PortraitLower||role==Role::PortraitFull){
                if(portrait_visible(role,menu)&&!art(l,dx,dy))return false;continue;
            }
            if(role==Role::Cursor){
                bool stored=false;if(!pane(l,stored))return false;
                const auto count=stored?menu.storage().size():menu.inventory().size();
                if(asking||!count||stored!=menu.storage_panel())continue;
                const auto select=menu.panel_selection(stored),first=menu.panel_row_offset(stored);
                if(select<first||select-first>=rows)return fail("Storage visible selection rejected");l.rect.y+=menu.cursor_row(stored)*pitch;l.frame=menu.cursor_frame();
                if(!art(l,dx,dy))return false;continue;
            }
            if(role==Role::QuestionCursor){
                if(!asking)continue;l.rect.y+=menu.question_cursor_row()*pitch;l.frame=menu.cursor_frame();
                if(!art(l,dx,dy))return false;continue;
            }
            if(role==Role::Scrollbar||role==Role::ScrollThumb){
                bool stored=false;if(!pane(l,stored))return false;const auto count=stored?menu.storage().size():menu.inventory().size();
                if(count<=rows)continue;
                if(role==Role::ScrollThumb){
                    auto track=std::find_if(layouts_.begin(),layouts_.end(),[&](const Layout& r){return r.role==uint32_t(Role::Scrollbar)&&r.parent==l.parent;});
                    if(track==layouts_.end())return fail("Storage scroll track missing");
                    l.rect.y=track->rect.y+track->rect.w*menu.panel_row_offset(stored)/count;l.rect.w=track->rect.w*rows/count;
                }
                if(!art(l,dx,dy))return false;continue;
            }
            if(role==Role::Description||role==Role::IconFrame||role==Role::ItemIcon){
                if(!has_item&&!warning)continue;
                if(role==Role::ItemIcon){
                    if(!has_item)continue;const auto icon=items_.definition(definition).icon;
                    if(icon==none)continue;if(icon>=icons_.size()||!icons_[icon].sheet)return fail("Storage item icon unavailable");
                    if(!sprite(icons_[icon],0,l,dx,dy))return fail("Storage item icon rejected");
                }else if(!art(l,dx,dy))return false;
                continue;
            }
            if(role==Role::DescriptionText){
                if(asking||warning||!has_item)continue;
                if(items_.definition(definition).flags&uint32_t(encore::upstream::ItemDefinitionFlag::RichDescription))continue;
                std::string description;if(!item_text(definition,true,description)||!text(main_font,l,description,dx,dy,true))return false;continue;
            }
            if(role==Role::Prompt){if(!asking&&!warning)continue;std::string label;if(!prompt(menu,label)||!text(main_font,l,label,dx,dy,true))return false;continue;}
            if(role==Role::Yes||role==Role::No){
                if(!asking)continue;const auto binding=role==Role::Yes?(menu.chinese()?Binding::YesZh:Binding::YesEn):(menu.chinese()?Binding::NoZh:Binding::NoEn);
                if(!text(main_font,l,data_.binding(binding),dx,dy))return false;continue;
            }
            if(role==Role::Title){if(!text(main_font,l,data_.binding(menu.chinese()?Binding::TitleZh:Binding::TitleEn),dx,dy,false,true))return false;continue;}
            if(role==Role::Counter){std::string label;if(!counter(menu,label)||!counter_text(counter_font,l,label,dx,dy))return false;continue;}
            if(role==Role::Separator){
                if(l.resource>=assets_.size())return fail("Storage separator missing");const auto& a=assets_[l.resource];
                for(uint32_t y=0;y<uint32_t(l.rect.w);y+=a.height){const auto h=std::min(a.height,uint32_t(l.rect.w)-y);
                    if(!region(a,0,0,a.width,h,dx+l.rect.x,dy+l.rect.y+y,a.width,h,color(l.color)))return fail("Storage separator rejected");}
                continue;
            }
            if(role==Role::Container||role==Role::Panel||role==Role::OnHandList||role==Role::StoredList||role==Role::Portrait){if(!art(l,dx,dy))return false;continue;}
            return fail("Unknown Storage layout role");
        }
        return true;
    }
};
