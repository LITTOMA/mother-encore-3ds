#pragma once
#include "loading_texture.hpp"
#include "opening_renderer.hpp"
#include "encore/field_data.hpp"
#include "encore/field_scene.hpp"
#include "encore/load_progress.hpp"
#include <3ds.h>
#include <citro2d.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// Draws a checked ENCMAP01 field in its compiled canvas order. All geometry,
// pictures and draw order come from the resource; see docs/PODUNK_FIELD.md.
class FieldRenderer {
public:
    bool load(encore::upstream::FieldMapView map,const std::string& locale,std::string& error){
        free();if(!map){error="Field map is not loaded";return false;}
        map_=map;using encore::upstream::FieldMapSection;
        const auto atlases=map.count(FieldMapSection::Atlases);
        for(uint32_t i=0;i<atlases;++i){
            encore::report_load_progress(encore::LoadPhase::Texture,i,atlases);
            const auto atlas=map.atlas(i);const std::string path="romfs:/"+std::string(map.text(atlas.path));
            std::string why;auto sheet=encore::ctr::loading_sprite_sheet_load(path.c_str(),&why);
            if(!sheet||encore::ctr::loading_sprite_sheet_count(sheet)!=1){if(sheet)encore::ctr::loading_sprite_sheet_free(sheet);error="Field atlas missing or invalid: "+path;free();return false;}
            const auto image=encore::ctr::loading_sprite_sheet_get_image(sheet,0);
            if(!image.tex||!image.subtex||image.subtex->width!=atlas.width||image.subtex->height!=atlas.height||Tex3DS_SubTextureRotated(image.subtex)){
                encore::ctr::loading_sprite_sheet_free(sheet);error="Field atlas layout differs from checked metadata: "+path;free();return false;}
            C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
            sheets_.push_back(sheet);
        }
        encore::report_load_progress(encore::LoadPhase::Texture,atlases,atlases);
        const auto tiles=map.count(FieldMapSection::Tiles),sprites=map.count(FieldMapSection::Sprites);
        tile_pictures_.resize(tiles);for(uint32_t i=0;i<tiles;++i)tile_pictures_[i]=map.localized_picture(map.tile(i).picture,locale);
        sprite_pictures_.resize(sprites);for(uint32_t i=0;i<sprites;++i)sprite_pictures_[i]=map.localized_picture(map.sprite(i).picture,locale);
        entries_.reserve(4096);deferred_.reserve(64);
        return true;
    }
    void free(){for(auto sheet:sheets_)encore::ctr::loading_sprite_sheet_free(sheet);sheets_.clear();tile_pictures_.clear();sprite_pictures_.clear();entries_.clear();deferred_.clear();map_={};}
    bool ready()const{return map_.valid()&&!sheets_.empty();}
    // camera: world top-left of the view; offset: screen position of the view.
    // Returns false if the scene and renderer disagree; the frame is then skipped.
    bool draw(const encore::upstream::FieldScene& scene,const OpeningActorRenderer& actors,float camera_x,float camera_y,float width,float height,float offset_x,float offset_y,double time){
        using namespace encore::upstream;
        if(!ready()||scene.map().bytes_identity()!=map_.bytes_identity())return false;
        time_=time;cx_=camera_x;cy_=camera_y;w_=width;h_=height;ox_=offset_x;oy_=offset_y;deferred_.clear();
        for(uint32_t g=0;g<map_.count(FieldMapSection::Groups);++g){
            const auto group=map_.group(g);
            if(group.kind==uint32_t(FieldGroupKind::Static)){
                for(uint32_t k=0;k<group.count;++k){const auto index=group.first+k;if(!scene.item_active(index))continue;const auto item=map_.item(index);
                    if(item.kind==uint16_t(FieldItemKind::LayerCells))draw_layer(scene,item.first);
                    else draw_sprites(scene,item.first,item.count);}
                continue;
            }
            entries_.clear();
            for(uint32_t k=0;k<group.count;++k){const auto index=group.first+k;if(!scene.item_active(index))continue;const auto item=map_.item(index);
                switch(FieldItemKind(item.kind)){
                case FieldItemKind::SortedLayerCells:collect_cells(scene,item);break;
                case FieldItemKind::SortedLayer:if(scene.layer_active(item.first))entries_.push_back({item.sort_y,item.order,1,item.first,0});break;
                // Grass alone contributes ~1000 y-sort items; skip ones wholly off-screen
                // before the per-frame sort (Godot only sorts currently visible canvas items).
                case FieldItemKind::Sprites:if(sprites_visible(scene,item.first,item.count))entries_.push_back({item.sort_y,item.order,2,item.first,item.count});break;
                case FieldItemKind::Player:entries_.push_back({scene.world.player().position.y,item.order,3,0,0});break;
                default:break;}
            }
            // VisualServer ItemPtrSort: approximately equal Y falls back to collection order.
            std::stable_sort(entries_.begin(),entries_.end(),[](const Entry&a,const Entry&b){
                const float tolerance=std::max(0.00001f,std::fabs(a.y)*0.00001f);
                if(std::fabs(a.y-b.y)<tolerance)return a.order<b.order;return a.y<b.y;});
            for(const auto& e:entries_){
                if(e.kind==0)draw_cell(e.first,e.second,true);
                else if(e.kind==1)draw_layer(scene,e.first);
                else if(e.kind==2)draw_sprites(scene,e.first,e.second);
                else actors.draw_player(scene.world.player().position,scene.world.animation(),cx_-ox_,cy_-oy_);
            }
        }
        // Godot draws positive z_index canvas items after every z=0 item of the layer.
        std::stable_sort(deferred_.begin(),deferred_.end(),[](const Deferred&a,const Deferred&b){return a.z<b.z;});
        for(const auto& d:deferred_)draw_tile(d.tile,d.x,d.y);
        return true;
    }
private:
    struct Entry {float y;uint32_t order;uint32_t kind,first,second;};
    struct Deferred {int32_t z;uint32_t tile;float x,y;};
    bool visible(float x,float y,float w,float h)const{return x+w>cx_&&x<cx_+w_&&y+h>cy_&&y<cy_+h_;}
    uint32_t frame_of(uint32_t picture)const{
        const auto p=map_.picture(picture);if(p.frame_count<=1||p.fps<=0)return p.first_frame;
        return p.first_frame+uint32_t(std::fmod(std::floor(time_*p.fps),double(p.frame_count)));
    }
    void blit(uint32_t frame_index,float x,float y,bool flip_h,bool flip_v)const{
        const auto f=map_.frame(frame_index);if(f.atlas>=sheets_.size())return;const auto atlas=map_.atlas(f.atlas);
        const auto image=encore::ctr::loading_sprite_sheet_get_image(sheets_[f.atlas],0);auto sub=*image.subtex;
        const float du=(sub.right-sub.left)/float(atlas.width),dv=(sub.bottom-sub.top)/float(atlas.height);
        const float left=sub.left,top=sub.top;
        float l=left+f.x*du,r=left+(f.x+f.w)*du,t=top+f.y*dv,b=top+(f.y+f.h)*dv;
        if(flip_h)std::swap(l,r);if(flip_v)std::swap(t,b);
        sub.left=l;sub.right=r;sub.top=t;sub.bottom=b;sub.width=f.w;sub.height=f.h;
        C2D_DrawImageAt({image.tex,&sub},std::floor(x-cx_+ox_+0.5f),std::floor(y-cy_+oy_+0.5f),0);
    }
    void draw_tile(uint32_t tile_index,float x,float y)const{
        const auto tile=map_.tile(tile_index);
        blit(frame_of(tile_pictures_[tile_index]),x,y,tile.flags&uint32_t(encore::upstream::FieldFlip::Horizontal),tile.flags&uint32_t(encore::upstream::FieldFlip::Vertical));
    }
    void draw_cell(uint32_t layer_index,uint32_t cell_index,bool sorted){
        const auto layer=map_.layer(layer_index);const auto cell=map_.cell(cell_index);const auto tile=map_.tile(cell.tile);
        const float x=layer.position.x+float(cell.x)*16.f+tile.offset.x,y=layer.position.y+float(cell.y)*16.f+tile.offset.y;
        if(!visible(x,y,tile.w,tile.h))return;
        if(tile.z>0){deferred_.push_back({tile.z,cell.tile,x,y});return;}
        (void)sorted;draw_tile(cell.tile,x,y);
    }
    void draw_layer(const encore::upstream::FieldScene& scene,uint32_t layer_index){
        if(!scene.layer_active(layer_index))return;
        const auto layer=map_.layer(layer_index);if(layer.flags&uint32_t(encore::upstream::FieldLayerFlag::Hidden))return;
        // Tiles may extend above/left of their cell by their texture offset.
        constexpr float reach=160.f;
        for(uint32_t c=0;c<layer.chunk_count;++c){
            const auto chunk=map_.chunk(layer.chunk_first+c);
            const float x=layer.position.x+float(chunk.cx)*256.f,y=layer.position.y+float(chunk.cy)*256.f;
            if(x+256.f+reach<cx_||x-reach>cx_+w_||y+256.f+reach<cy_||y-reach>cy_+h_)continue;
            for(uint32_t k=0;k<chunk.count;++k)draw_cell(layer_index,chunk.first+k,false);
        }
    }
    void collect_cells(const encore::upstream::FieldScene& scene,const encore::upstream::FieldItem& item){
        if(!scene.layer_active(item.first))return;
        const auto layer=map_.layer(item.first);if(layer.flags&uint32_t(encore::upstream::FieldLayerFlag::Hidden))return;
        constexpr float reach=160.f;
        for(uint32_t c=0;c<layer.chunk_count;++c){
            const auto chunk=map_.chunk(layer.chunk_first+c);
            const float x=layer.position.x+float(chunk.cx)*256.f,y=layer.position.y+float(chunk.cy)*256.f;
            if(x+256.f+reach<cx_||x-reach>cx_+w_||y+256.f+reach<cy_||y-reach>cy_+h_)continue;
            for(uint32_t k=0;k<chunk.count;++k){
                const auto cell=map_.cell(chunk.first+k);const auto tile=map_.tile(cell.tile);
                const float tx=layer.position.x+float(cell.x)*16.f+tile.offset.x,ty=layer.position.y+float(cell.y)*16.f+tile.offset.y;
                if(!visible(tx,ty,tile.w,tile.h))continue;
                // Quadrant canvas item origin: cell top-left, plus one cell for bottom-left origin.
                entries_.push_back({layer.position.y+float(cell.y)*16.f+layer.origin_y,item.order+cell.order,0,item.first,chunk.first+k});
            }
        }
    }
    bool sprites_visible(const encore::upstream::FieldScene& scene,uint32_t first,uint32_t count)const{
        for(uint32_t i=first;i<first+count;++i){
            if(!scene.sprite_visible(i))continue;
            const auto s=map_.sprite(i);if(visible(s.position.x,s.position.y,s.w,s.h))return true;
        }
        return false;
    }
    void draw_sprites(const encore::upstream::FieldScene& scene,uint32_t first,uint32_t count){
        for(uint32_t i=first;i<first+count;++i){
            if(!scene.sprite_visible(i))continue;
            const auto s=map_.sprite(i);if(!visible(s.position.x,s.position.y,s.w,s.h))continue;
            blit(frame_of(sprite_pictures_[i]),s.position.x,s.position.y,s.flags&1,s.flags&2);
        }
    }
    encore::upstream::FieldMapView map_;
    std::vector<encore::ctr::LoadingSpriteSheet> sheets_;
    std::vector<uint32_t> tile_pictures_,sprite_pictures_;
    std::vector<Entry> entries_;
    std::vector<Deferred> deferred_;
    double time_=0;float cx_=0,cy_=0,w_=400,h_=240,ox_=0,oy_=0;
};
