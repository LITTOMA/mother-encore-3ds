#pragma once
#include "loading_texture.hpp"
#include "encore/load_progress.hpp"
#include <3ds.h>
#include <citro2d.h>
#include "encore/animation.hpp"
#include "encore/actor_actions.hpp"
#include "encore/room_data.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// Generic presentation over validated external room records. The owner of the
// RoomView must outlive this renderer; no game asset paths/layouts are embedded.
class OpeningActorRenderer {
    struct Atlas { encore::ctr::LoadingSpriteSheet sheet=nullptr; unsigned width=0,height=0,columns=0,rows=0; };
    encore::upstream::RoomView room_;
    std::vector<Atlas> atlases_;
    static void region(const Atlas& atlas,unsigned u,unsigned v,unsigned w,unsigned h,float x,float y){
        if(!atlas.sheet||u+w>atlas.width||v+h>atlas.height)return;
        const auto image=encore::ctr::loading_sprite_sheet_get_image(atlas.sheet,0);auto sub=*image.subtex;
        const float du=(sub.right-sub.left)/float(atlas.width),dv=(sub.bottom-sub.top)/float(atlas.height);
        const float left=sub.left,top=sub.top;
        sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;
        sub.width=w;sub.height=h;
        C2D_DrawImageAt({image.tex,&sub},std::floor(x+0.5f),std::floor(y+0.5f),0);
    }
    void centered(uint32_t resource,unsigned frame,float x,float y)const{
        if(resource>=atlases_.size())return;
        const auto& atlas=atlases_[resource];
        if(!atlas.sheet||!atlas.columns||!atlas.rows||frame>=atlas.columns*atlas.rows)return;
        const unsigned w=atlas.width/atlas.columns,h=atlas.height/atlas.rows;
        region(atlas,frame%atlas.columns*w,frame/atlas.columns*h,w,h,x-w*0.5f,y-h*0.5f);
    }
    void pose(uint32_t profile_index,encore::upstream::Vec2 position,encore::upstream::Vec2 sprite_position,
              encore::upstream::Vec2 sprite_offset,encore::upstream::Vec2 emote_position,
              unsigned frame,unsigned emote_frame,bool shadow,bool emote,float cx,float cy)const{
        const auto profile=room_.actor_profile(profile_index);
        if(shadow)centered(profile.shadow_resource,0,position.x+profile.shadow_offset.x-cx,position.y+profile.shadow_offset.y-cy);
        centered(profile.primary_resource,frame,position.x+sprite_position.x+sprite_offset.x-cx,position.y+sprite_position.y+sprite_offset.y-cy);
        if(emote)centered(profile.emote_resource,emote_frame,position.x+sprite_position.x+emote_position.x-cx,position.y+sprite_position.y+emote_position.y-cy);
    }
public:
    bool load(const encore::upstream::RoomView& room,std::string& error){
        free();if(!room){error="Room data is not loaded";return false;}room_=room;
        atlases_.resize(room.resource_count());
        for(uint32_t index=0;index<room.resource_count();++index){
            const auto resource=room.resource(index);if(resource.kind!=1)continue;
            encore::report_load_progress(encore::LoadPhase::Texture,index,room.resource_count());
            auto& atlas=atlases_[index];
            atlas.width=resource.width;atlas.height=resource.height;atlas.columns=resource.columns;atlas.rows=resource.rows;
            const std::string path="romfs:/"+std::string(room.string(resource.path_string));
            atlas.sheet=encore::ctr::loading_sprite_sheet_load(path.c_str());
            if(!atlas.sheet||encore::ctr::loading_sprite_sheet_count(atlas.sheet)!=1){error="Room texture missing or invalid: "+path;free();return false;}
            const auto image=encore::ctr::loading_sprite_sheet_get_image(atlas.sheet,0);
            if(!image.tex||!image.subtex||image.subtex->width!=atlas.width||image.subtex->height!=atlas.height||Tex3DS_SubTextureRotated(image.subtex)){
                error="Room texture layout differs from validated metadata: "+path;free();return false;
            }
            C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
        }
        encore::report_load_progress(encore::LoadPhase::Texture,room.resource_count(),room.resource_count());
        return true;
    }
    void free(){for(auto& atlas:atlases_)if(atlas.sheet)encore::ctr::loading_sprite_sheet_free(atlas.sheet);atlases_.clear();room_={};}
    void draw_map(uint32_t index,float cx,float cy)const{
        const auto draw=room_.map_draw(index);
        if(draw.resource_index<atlases_.size())region(atlases_[draw.resource_index],0,0,draw.w,draw.h,draw.x-cx,draw.y-cy);
    }
    void draw_overlay(uint32_t index,float cx,float cy)const{
        const auto draw=room_.overlay(index);
        if(draw.resource_index<atlases_.size())region(atlases_[draw.resource_index],draw.u,draw.v,draw.w,draw.h,draw.x-cx,draw.y-cy);
    }
    void draw_player(encore::upstream::Vec2 position,encore::upstream::AnimationSample sample,float cx,float cy)const{
        if(!sample.main_visible)return;
        const auto instance=room_.actor_instance(room_.scene().player_instance_index);const auto profile=room_.actor_profile(instance.profile_index);
        pose(instance.profile_index,position,profile.sprite_position,profile.sprite_offset,profile.emote_offset,
             sample.frame,profile.emote_initial_frame,(profile.flags&1)!=0,false,cx,cy);
    }
    void draw_actor(const encore::upstream::ActorActionState& actor,float cx,float cy)const{
        if(!actor.initialized)return;
        pose(actor.profile_index,actor.position,actor.sprite_position,actor.sprite_offset,actor.emote_position,
             actor.frame,actor.emote_frame,actor.shadow_visible,true,cx,cy);
    }
};
inline void opening_camera(const encore::upstream::RoomView& room,encore::upstream::Vec2 player,
                           float viewport_width,float viewport_height,float& x,float& y){
    const float hw=viewport_width*0.5f,hh=viewport_height*0.5f;
    x=std::floor(player.x-hw);y=std::floor(player.y-hh);
    for(uint32_t i=0;i<room.camera_area_count();++i){
        const auto area=room.camera_area(i);
        if(std::abs(player.x-area.center.x)>area.extents.x||std::abs(player.y-area.center.y)>area.extents.y)continue;
        const float ex=std::max(hw,area.extents.x),ey=std::max(hh,area.extents.y);
        x=std::floor(std::clamp(player.x-hw,area.center.x-ex,area.center.x+ex-viewport_width));
        y=std::floor(std::clamp(player.y-hh,area.center.y-ey,area.center.y+ey-viewport_height));
        return;
    }
}
