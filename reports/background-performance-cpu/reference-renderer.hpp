#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

// Presentation mechanisms only. The caller supplies every path, dimension,
// layout, palette/color, shader coefficient, glyph metric and animation sample.
// Resource kind 1 = single-image T3X; kind 2 = checked streaming BPX1 palette.
class BattleRenderer {
public:
    struct Resource {const char* path=nullptr;uint32_t kind=0,width=0,height=0,columns=0,rows=0;};
    struct Glyph {uint32_t codepoint=0,resource=0,u=0,v=0,width=0,height=0;float advance=0,offset_x=0,offset_y=0;};
    struct BackgroundLayer {
        uint32_t resource=0;bool barrel=false;
        // UV normalization extent, not the expanded control/viewport extent.
        // For a tiled source these are the original texture's dimensions.
        float width=0,height=0,opacity=0,effect=0,effect_scale=0;
        float barrel_x=0,barrel_y=0,amplitude_x=0,amplitude_y=0;
        float frequency_x=0,frequency_y=0,speed_x=0,speed_y=0;
        bool repeat=false;
    };
private:
    struct Asset {
        C2D_SpriteSheet sheet=nullptr;FILE* stream=nullptr;
        uint32_t width=0,height=0,columns=0,rows=0,frames=0;
        uint32_t data_offset=0,cached_frame=UINT32_MAX;
        std::vector<uint32_t> palette;
        std::vector<uint8_t> pixels;
    };
    struct UV {float x=0,y=0,cos_x=0,sin_x=0;};
    struct PreparedLayer {BackgroundLayer config;std::vector<UV> uv;};
    std::vector<Asset> assets_;
    std::vector<Glyph> glyphs_;
    std::vector<PreparedLayer> layers_;
    std::vector<uint32_t> surface_;
    std::vector<uint32_t> surface_offsets_;
    C3D_Tex surface_texture_{};
    Tex3DS_SubTexture surface_subtexture_{};
    uint32_t surface_width_=0,surface_height_=0;
    bool texture_ready_=false;

    static uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
    static uint32_t crc_update(uint32_t crc,const uint8_t* data,size_t size){
        for(size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xEDB88320u&uint32_t(-int32_t(crc&1)));}return crc;
    }
    static bool fail(std::string& error,const char* reason,const char* path){error=std::string(reason)+": "+(path?path:"");return false;}
    static bool read_frame(Asset& a,uint32_t frame){
        if(!a.stream||frame>=a.frames)return false;
        if(a.cached_frame==frame)return true;
        const size_t size=size_t(a.width)*a.height;
        if(std::fseek(a.stream,long(a.data_offset+size*frame),SEEK_SET)!=0||
           std::fread(a.pixels.data(),1,size,a.stream)!=size)return false;
        a.cached_frame=frame;return true;
    }
    bool load_indexed(Asset& a,const Resource& r,std::string& error){
        a.stream=std::fopen(r.path,"rb");uint8_t header[36];
        if(!a.stream||std::fread(header,1,sizeof(header),a.stream)!=sizeof(header))return fail(error,"Missing/truncated indexed image",r.path);
        if(std::memcmp(header,"ENCBPIX\0",8)||u32(header+8)!=1)return fail(error,"Unknown indexed image format/version",r.path);
        a.width=u32(header+12);a.height=u32(header+16);a.frames=u32(header+20);
        const auto colors=u32(header+24),bytes=u32(header+28),expected_crc=u32(header+32);
        const uint64_t pixels=uint64_t(a.width)*a.height*a.frames;
        if(!a.width||a.width>1024||!a.height||a.height>1024||!a.frames||a.frames>256||
           !colors||colors>256||pixels>16*1024*1024||bytes!=colors*4+pixels||
           a.width!=r.width||a.height!=r.height||a.frames!=uint64_t(r.columns)*r.rows)
            return fail(error,"Indexed image dimensions differ from validated resource",r.path);
        a.palette.resize(colors);std::vector<uint8_t> palette(colors*4);
        if(std::fread(palette.data(),1,palette.size(),a.stream)!=palette.size())return fail(error,"Truncated indexed palette",r.path);
        uint32_t crc=crc_update(UINT32_MAX,palette.data(),palette.size());
        for(uint32_t i=0;i<colors;++i)a.palette[i]=u32(palette.data()+i*4);
        uint8_t block[4096];size_t remaining=size_t(pixels);
        while(remaining){
            const size_t amount=std::min(remaining,sizeof(block));
            if(std::fread(block,1,amount,a.stream)!=amount)return fail(error,"Truncated indexed pixels",r.path);
            crc=crc_update(crc,block,amount);
            for(size_t i=0;i<amount;++i)if(block[i]>=colors)return fail(error,"Indexed palette reference out of range",r.path);
            remaining-=amount;
        }
        if((crc^UINT32_MAX)!=expected_crc||std::fgetc(a.stream)!=EOF)return fail(error,"Indexed image CRC/length mismatch",r.path);
        a.data_offset=sizeof(header)+colors*4;a.pixels.resize(size_t(a.width)*a.height);
        if(!read_frame(a,0))return fail(error,"Could not read indexed first frame",r.path);
        return true;
    }
    static unsigned morton(unsigned x,unsigned y){
        return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);
    }
    const Glyph* glyph(uint32_t codepoint)const{
        const auto i=std::lower_bound(glyphs_.begin(),glyphs_.end(),codepoint,[](const Glyph& a,uint32_t b){return a.codepoint<b;});
        return i!=glyphs_.end()&&i->codepoint==codepoint?&*i:nullptr;
    }
public:
    BattleRenderer()=default;
    BattleRenderer(const BattleRenderer&)=delete;BattleRenderer& operator=(const BattleRenderer&)=delete;
    bool load(const std::vector<Resource>& resources,uint32_t width,uint32_t height,std::string& error){
        free();if(resources.empty()||resources.size()>1024||!width||width>1024||!height||height>1024){error="Invalid presentation resource/surface bounds";return false;}
        assets_.resize(resources.size());
        for(size_t i=0;i<resources.size();++i){
            auto& a=assets_[i];const auto& r=resources[i];
            if(!r.path||!r.width||!r.height||!r.columns||!r.rows){error="Incomplete presentation resource";free();return false;}
            a.width=r.width;a.height=r.height;a.columns=r.columns;a.rows=r.rows;
            if(r.kind==2){if(!load_indexed(a,r,error)){free();return false;}}
            else if(r.kind==1){
                if(r.width>1024||r.height>1024||r.width%r.columns||r.height%r.rows){error="Invalid texture grid";free();return false;}
                a.sheet=C2D_SpriteSheetLoad(r.path);
                if(!a.sheet||C2D_SpriteSheetCount(a.sheet)!=1){fail(error,"Missing/invalid presentation texture",r.path);free();return false;}
                const auto img=C2D_SpriteSheetGetImage(a.sheet,0);
                if(!img.tex||!img.subtex||img.subtex->width!=r.width||img.subtex->height!=r.height||Tex3DS_SubTextureRotated(img.subtex)){
                    fail(error,"Texture differs from validated dimensions",r.path);free();return false;
                }
                C3D_TexSetFilter(img.tex,GPU_NEAREST,GPU_NEAREST);
            }else{error="Unknown presentation resource kind";free();return false;}
        }
        surface_width_=width;surface_height_=height;surface_.resize(size_t(width)*height);
        unsigned tw=8,th=8;while(tw<width)tw<<=1;while(th<height)th<<=1;
        if(!C3D_TexInit(&surface_texture_,tw,th,GPU_RGBA8)){error="Presentation surface allocation failed";free();return false;}
        texture_ready_=true;std::memset(surface_texture_.data,0,surface_texture_.size);
        surface_offsets_.resize(surface_.size());
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)surface_offsets_[size_t(y)*width+x]=((y/8)*(tw/8)+x/8)*64+morton(x,y);
        C3D_TexSetFilter(&surface_texture_,GPU_NEAREST,GPU_NEAREST);
        C3D_TexSetWrap(&surface_texture_,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
        surface_subtexture_={u16(width),u16(height),0,1,float(width)/tw,1-float(height)/th};
        return true;
    }
    void free(){
        for(auto& a:assets_){if(a.sheet)C2D_SpriteSheetFree(a.sheet);if(a.stream)std::fclose(a.stream);}
        assets_.clear();glyphs_.clear();layers_.clear();surface_.clear();surface_offsets_.clear();
        if(texture_ready_)C3D_TexDelete(&surface_texture_);
        texture_ready_=false;surface_width_=surface_height_=0;surface_texture_={};surface_subtexture_={};
    }
    bool set_glyphs(std::vector<Glyph> glyphs,std::string& error){
        std::sort(glyphs.begin(),glyphs.end(),[](const Glyph& a,const Glyph& b){return a.codepoint<b.codepoint;});
        for(size_t i=0;i<glyphs.size();++i){
            const auto& g=glyphs[i];
            if(g.resource>=assets_.size()||!assets_[g.resource].sheet||
               g.u>assets_[g.resource].width||g.width>assets_[g.resource].width-g.u||
               g.v>assets_[g.resource].height||g.height>assets_[g.resource].height-g.v||
               !std::isfinite(g.advance)||!std::isfinite(g.offset_x)||!std::isfinite(g.offset_y)||
               g.advance<0||(i&&glyphs[i-1].codepoint==g.codepoint)){
                error="Invalid external glyph metadata";return false;
            }
        }
        glyphs_=std::move(glyphs);return true;
    }
    bool set_background(const std::vector<BackgroundLayer>& layers,std::string& error){
        layers_.clear();
        if(layers.empty()||layers.size()>8){error="Invalid background layer count";return false;}
        for(const auto& layer:layers){
            const float values[]={layer.width,layer.height,layer.opacity,layer.effect,layer.effect_scale,
                layer.barrel_x,layer.barrel_y,layer.amplitude_x,layer.amplitude_y,
                layer.frequency_x,layer.frequency_y,layer.speed_x,layer.speed_y};
            for(float value:values)if(!std::isfinite(value)){error="Non-finite background coefficient";layers_.clear();return false;}
            if(layer.resource>=assets_.size()||!assets_[layer.resource].stream||layer.width<=0||layer.height<=0||
               !std::isfinite(layer.width)||!std::isfinite(layer.height)||layer.opacity<0||layer.opacity>1){error="Invalid background layer";layers_.clear();return false;}
            if(!read_frame(assets_[layer.resource],0)){error="Background source cannot be read";layers_.clear();return false;}
            PreparedLayer prepared;prepared.config=layer;prepared.uv.resize(surface_.size());
            for(unsigned y=0;y<surface_height_;++y)for(unsigned x=0;x<surface_width_;++x){
                auto& uv=prepared.uv[size_t(y)*surface_width_+x];
                uv.x=(x+0.5f)/layer.width;uv.y=(y+0.5f)/layer.height;
                if(layer.barrel){
                    const float px=2*uv.x-layer.barrel_x,py=2*uv.y-layer.barrel_y;
                    const float d=std::sqrt(px*px+py*py),z2=1+d*d*layer.effect;
                    if(z2<0||!std::isfinite(z2)){error="Background distortion has invalid square root";layers_.clear();return false;}
                    // The source shader deliberately uses 3.14159, not a higher precision pi.
                    const float radius=std::atan2(d,std::sqrt(z2))/3.14159f*layer.effect_scale;
                    const float phi=std::atan2(py,px);
                    uv.x=radius*std::cos(phi)+0.5f;uv.y=radius*std::sin(phi)+0.5f;
                }
                uv.cos_x=std::cos(layer.frequency_x*uv.y);uv.sin_x=std::sin(layer.frequency_x*uv.y);
                if(!std::isfinite(uv.x)||!std::isfinite(uv.y)||!std::isfinite(uv.cos_x)||!std::isfinite(uv.sin_x)){
                    error="Invalid background sampling coordinate";layers_.clear();return false;
                }
            }
            layers_.push_back(std::move(prepared));
        }
        return true;
    }
    // TIME is supplied by the owner and is the shader clock, not necessarily the
    // battle animation clock. UV divisors and repeat/clamp are external data.
    bool compose_background(float time,uint32_t clear_color){
        if(!std::isfinite(time)||layers_.empty())return false;
        std::fill(surface_.begin(),surface_.end(),clear_color);
        for(const auto& prepared:layers_){
            const auto& layer=prepared.config;auto& a=assets_[layer.resource];
            if(!read_frame(a,0))return false;
            if(!std::isfinite(time*layer.speed_x)||!std::isfinite(time*layer.speed_y))return false;
            const float c=std::cos(time*layer.speed_x),s=std::sin(time*layer.speed_x);
            for(size_t i=0;i<surface_.size();++i){
                const auto& uv=prepared.uv[i];float u=uv.x,v=uv.y;
                if(layer.frequency_x!=0&&layer.amplitude_x!=0)u+=layer.amplitude_x*(uv.cos_x*c-uv.sin_x*s);
                if(layer.frequency_y!=0&&layer.amplitude_y!=0)v+=layer.amplitude_y*std::cos(layer.frequency_y*u+time*layer.speed_y);
                if(!std::isfinite(u)||!std::isfinite(v))return false;
                if(layer.repeat){
                    if(u<0||u>=1)u-=std::floor(u);
                    if(v<0||v>=1)v-=std::floor(v);
                }
                // Clamp before conversion, so truncation equals nearest-sample
                // floor without a per-pixel libm call on the ARM11.
                const auto sx=std::min(uint32_t(std::clamp(u,0.0f,1.0f)*a.width),a.width-1);
                const auto sy=std::min(uint32_t(std::clamp(v,0.0f,1.0f)*a.height),a.height-1);
                const auto color=a.palette[a.pixels[size_t(sy)*a.width+sx]],base=surface_[i];
                if(layer.opacity==1&&(color>>24)==255){surface_[i]=color;continue;}
                // Exact rounded average for opaque half-opacity layers. This is
                // algebraically identical to the generic float blend below.
                if(layer.opacity==0.5f&&(color>>24)==255&&(base>>24)==255){
                    const uint32_t difference=color^base;
                    surface_[i]=(color&base)+((difference&0xfefefefeu)>>1)+(difference&0x01010101u);continue;
                }
                const float alpha=float(color>>24)/255*layer.opacity;uint32_t mixed=0;
                for(unsigned shift=0;shift<24;shift+=8){
                    const float value=float((color>>shift)&255)*alpha+float((base>>shift)&255)*(1-alpha);
                    mixed|=uint32_t(value+0.5f)<<shift;
                }
                mixed|=uint32_t(255*alpha+float(base>>24)*(1-alpha)+0.5f)<<24;
                surface_[i]=mixed;
            }
        }
        return true;
    }
    // The current surface is SCREEN_TEXTURE. This writes shader output only;
    // draw_surface's blend choice must match the audited source GPU backend.
    // In particular, a backend that treats blend_disabled as MIX still alpha-
    // composites these output pixels over the world drawn beneath the surface.
    // Offsets place a source-resolution mask within an expanded canvas. Pixels
    // outside that mask repeat its nearest border pixel, with no resampling.
    // The owner supplies this explicit display adaptation; (0,0) on the source
    // canvas preserves the reference path byte-for-byte.
    bool compose_transition(uint32_t resource,uint32_t frame,uint32_t old_color,uint32_t new_color,
                            int32_t offset_x=0,int32_t offset_y=0){
        if(resource>=assets_.size())return false;
        auto& a=assets_[resource];
        if(a.width>surface_width_||a.height>surface_height_||!read_frame(a,frame))return false;
        for(uint32_t y=0;y<surface_height_;++y)for(uint32_t x=0;x<surface_width_;++x){
            const auto sx=uint32_t(std::clamp(int64_t(x)-offset_x,int64_t(0),int64_t(a.width)-1));
            const auto sy=uint32_t(std::clamp(int64_t(y)-offset_y,int64_t(0),int64_t(a.height)-1));
            const auto pixel=a.palette[a.pixels[size_t(sy)*a.width+sx]];
            const size_t i=size_t(y)*surface_width_+x;
            if(pixel==old_color)surface_[i]=new_color;
            else if((pixel>>24)!=255)surface_[i]=pixel;
        }
        return true;
    }
    void upload_surface(){
        if(!texture_ready_)return;
        auto* words=static_cast<uint32_t*>(surface_texture_.data);
        for(size_t i=0;i<surface_.size();++i){const auto c=surface_[i];
            words[surface_offsets_[i]]=((c&0xffu)<<24)|((c&0xff00u)<<8)|((c>>8)&0xff00u)|(c>>24);
        }
        C3D_TexFlush(&surface_texture_);
    }
    void draw_surface(float x,float y,float width,float height,bool overwrite=true){
        if(!texture_ready_)return;
        if(overwrite){C2D_Flush();C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);}
        C2D_DrawImageAt({&surface_texture_,&surface_subtexture_},x,y,0,nullptr,width/surface_width_,height/surface_height_);
        if(overwrite){C2D_Flush();
            // C2D_Prepare does not restore global blend state. Restore ordinary
            // source-alpha composition explicitly before sprites and both UIs.
            C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
            C3D_AlphaTest(true,GPU_GREATER,0);C2D_Prepare();}
    }
    bool draw_region(uint32_t resource,uint32_t u,uint32_t v,uint32_t w,uint32_t h,
                     float x,float y,float width,float height,uint32_t color,float color_blend=0,float depth=0)const{
        if(resource>=assets_.size()||!w||!h||width<=0||height<=0)return false;
        const auto& a=assets_[resource];if(!a.sheet||u>a.width||w>a.width-u||v>a.height||h>a.height-v)return false;
        const auto img=C2D_SpriteSheetGetImage(a.sheet,0);auto sub=*img.subtex;
        const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height,left=sub.left,top=sub.top;
        sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;
        C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,color_blend);
        return C2D_DrawImageAt({img.tex,&sub},x,y,depth,&tint,width/w,height/h);
    }
    bool draw_sprite(uint32_t resource,uint32_t frame,float x,float y,float width,float height,
                     uint32_t color,float color_blend=0,float depth=0)const{
        if(resource>=assets_.size())return false;
        const auto& a=assets_[resource];
        if(!a.columns||!a.rows||frame>=uint64_t(a.columns)*a.rows)return false;
        const auto w=a.width/a.columns,h=a.height/a.rows;
        return draw_region(resource,frame%a.columns*w,frame/a.columns*h,w,h,x,y,width,height,color,color_blend,depth);
    }
    bool draw_ninepatch(uint32_t resource,float x,float y,float width,float height,const uint32_t margins[4],
                        uint32_t color,float scale_x=1,float scale_y=1,float depth=0)const{
        if(resource>=assets_.size()||!assets_[resource].sheet)return false;
        const auto& a=assets_[resource];
        const unsigned l=margins[0],t=margins[1],r=margins[2],b=margins[3];
        if(l+r>a.width||t+b>a.height||width<(l+r)*scale_x||height<(t+b)*scale_y)return false;
        const unsigned us[]={0,l,a.width-r,a.width},vs[]={0,t,a.height-b,a.height};
        const float xs[]={x,x+l*scale_x,x+width-r*scale_x,x+width},ys[]={y,y+t*scale_y,y+height-b*scale_y,y+height};
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)
            if(us[col+1]>us[col]&&vs[row+1]>vs[row])draw_region(resource,us[col],vs[row],us[col+1]-us[col],vs[row+1]-vs[row],xs[col],ys[row],xs[col+1]-xs[col],ys[row+1]-ys[row],color,0,depth);
        return true;
    }
    bool draw_tiled(uint32_t resource,float x,float y,float width,float height,uint32_t color,
                    float scale_x=1,float scale_y=1,float depth=0)const{
        if(resource>=assets_.size()||!assets_[resource].sheet||width<=0||height<=0||scale_x<=0||scale_y<=0)return false;
        const auto& a=assets_[resource];
        const unsigned w=a.width/a.columns,h=a.height/a.rows;
        for(float dy=0;dy<height;dy+=h*scale_y)for(float dx=0;dx<width;dx+=w*scale_x){
            const float dw=std::min(w*scale_x,width-dx),dh=std::min(h*scale_y,height-dy);
            const unsigned sw=unsigned(std::ceil(dw/scale_x)),sh=unsigned(std::ceil(dh/scale_y));
            if(!draw_region(resource,0,0,sw,sh,x+dx,y+dy,dw,dh,color,0,depth))return false;
        }
        return true;
    }
    float text_width(const char* text)const{
        if(!text)return 0;
        float width=0;for(const auto* p=reinterpret_cast<const unsigned char*>(text);*p;++p){const auto* g=glyph(*p);if(g)width+=g->advance;}return width;
    }
    // This reviewed glyph domain is ASCII. Unsupported bytes fail instead of
    // silently substituting a system font or inventing fallback metrics.
    bool draw_text(const char* text,float x,float y,float scale_x,float scale_y,uint32_t color,float depth=0)const{
        if(!text)return false;
        for(const auto* p=reinterpret_cast<const unsigned char*>(text);*p;++p){
            const auto* g=glyph(*p);if(!g||!g->advance)return false;
            if(g->width&&g->height&&!draw_region(g->resource,g->u,g->v,g->width,g->height,x+g->offset_x*scale_x,y+g->offset_y*scale_y,
               g->width*scale_x,g->height*scale_y,color,1,depth))return false;
            x+=g->advance*scale_x;
        }
        return true;
    }
    static void draw_rect(float x,float y,float width,float height,uint32_t color,float depth=0){C2D_DrawRectSolid(x,y,depth,width,height,color);}
};
