#include "encore/native_input.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace encore::upstream {
namespace {
constexpr size_t header_size=32,payload_size=56,file_size=header_size+payload_size;
constexpr double pi=3.14159265358979323846,sector_width=pi/4;
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
float f32(const uint8_t* p){const uint32_t u=u32(p);float f;std::memcpy(&f,&u,4);return f;}
uint32_t checksum(const uint8_t* p,size_t size){
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<size;++i){crc^=(i>=16&&i<20)?0:p[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
    return crc^0xffffffffu;
}
bool tuning_valid(const NativeInputTuning& t){
    for(float v:{t.circle_nominal_radius,t.circle_activate,t.circle_release,t.angular_hysteresis_degrees,
                  t.touch_activate,t.touch_release,t.tap_max_travel,t.tap_max_seconds,t.base_radius,t.thumb_radius})
        if(!std::isfinite(v))return false;
    return t.circle_release>0&&t.circle_release<t.circle_activate&&t.circle_activate<=t.circle_nominal_radius&&t.circle_nominal_radius<=32767&&
        t.angular_hysteresis_degrees>=0&&t.angular_hysteresis_degrees<22.5f&&
        t.touch_release>0&&t.touch_release<t.touch_activate&&t.touch_activate<=120&&
        t.tap_max_travel>0&&t.tap_max_travel<=t.touch_activate&&t.tap_max_seconds>0&&t.tap_max_seconds<=2&&
        t.thumb_radius>0&&t.thumb_radius<t.base_radius&&t.base_radius<=120&&
        t.screen_width==320&&t.screen_height==240&&
        (t.base_rgba>>24)>0&&(t.base_rgba>>24)<128&&(t.thumb_rgba>>24)>0&&(t.thumb_rgba>>24)<128;
}
bool held(NativeDpad p){return p.left||p.right||p.up||p.down;}
NativeInputDirection direction(int sector){
    if(sector<0)return {};
    return {(sector==0||sector==1||sector==7)?1:((sector>=3&&sector<=5)?-1:0),
            (sector>=1&&sector<=3)?1:((sector>=5&&sector<=7)?-1:0)};
}
int quantize(double x,double y,int previous,const NativeInputTuning& t){
    double angle=std::atan2(y,x);if(angle<0)angle+=2*pi;
    if(previous>=0){
        double distance=std::fabs(angle-previous*sector_width);distance=std::min(distance,2*pi-distance);
        if(distance<=sector_width/2+t.angular_hysteresis_degrees*pi/180)return previous;
    }
    // Half-open sectors: an exact +22.5 degree boundary selects sector 1.
    return int(std::floor((angle+sector_width/2)/sector_width))%8;
}
int radial_sector(double x,double y,int previous,double activate,double release,const NativeInputTuning& t){
    const double radius=std::hypot(x,y);
    if(radius<=release||(previous<0&&radius<activate))return -1;
    return quantize(x,y,previous,t);
}
}
bool NativeInputData::load(const uint8_t* bytes,size_t size,std::string& error){
    auto fail=[&](const char* message){error=message;return false;};
    if(!bytes||size!=file_size)return fail("Invalid native input size");
    if(std::memcmp(bytes,"ENCINP01",8)||u32(bytes+8)!=1)return fail("Unsupported native input version");
    if(u32(bytes+12)!=file_size||u32(bytes+20)!=payload_size||u32(bytes+24)||u32(bytes+28))return fail("Invalid native input header");
    if(checksum(bytes,size)!=u32(bytes+16))return fail("Native input checksum mismatch");
    const uint8_t* p=bytes+header_size;
    NativeInputTuning t;
    t.circle_nominal_radius=f32(p);t.circle_activate=f32(p+4);t.circle_release=f32(p+8);t.angular_hysteresis_degrees=f32(p+12);
    t.touch_activate=f32(p+16);t.touch_release=f32(p+20);t.tap_max_travel=f32(p+24);t.tap_max_seconds=f32(p+28);
    t.base_radius=f32(p+32);t.thumb_radius=f32(p+36);
    t.screen_width=u32(p+40);t.screen_height=u32(p+44);t.base_rgba=u32(p+48);t.thumb_rgba=u32(p+52);
    if(!tuning_valid(t))return fail("Invalid native input tuning");
    tuning_=t;valid_=true;error.clear();return true;
}
bool NativeInputData::load_file(const char* path,std::string& error){
    FILE* file=path?std::fopen(path,"rb"):nullptr;if(!file){error="Cannot open native input resource";return false;}
    uint8_t bytes[file_size+1];const size_t size=std::fread(bytes,1,sizeof(bytes),file);
    const bool failed=std::ferror(file)!=0;const int closed=std::fclose(file);
    if(failed||closed){error="Cannot read native input resource";return false;}
    return load(bytes,size,error);
}
bool NativeInputAdapter::configure(const NativeInputData& data,std::string& error){
    if(!data.valid()){error="Native input resource not loaded";return false;}
    tuning_=data.tuning();configured_=true;reset();error.clear();return true;
}
void NativeInputAdapter::reset(){
    context_=0;context_seen_=false;pad_blocked_=dpad_blocked_=touch_blocked_=false;
    touch_active_=touch_dragged_=touch_timed_out_=false;pad_sector_=touch_sector_=-1;
    origin_x_=origin_y_=0;touch_seconds_=touch_max_distance_=0;
}
NativeInputResult NativeInputAdapter::sample(uint32_t context,int16_t circle_x,int16_t circle_y,
        NativeDpad dpad,bool touch_down,int touch_x,int touch_y,double real_delta){
    NativeInputResult result;
    if(!configured_)return result;
    const double pad_radius=std::hypot(double(circle_x),double(circle_y));
    const bool dpad_held=held(dpad);
    if(!context_seen_||context!=context_||!context||!std::isfinite(real_delta)||real_delta<0){
        context_=context;context_seen_=true;
        pad_blocked_=pad_radius>tuning_.circle_release;dpad_blocked_=dpad_held;touch_blocked_=touch_down;
        pad_sector_=touch_sector_=-1;touch_active_=touch_dragged_=touch_timed_out_=false;
        touch_seconds_=touch_max_distance_=0;
        return result;
    }
    if(pad_radius<=tuning_.circle_release)pad_blocked_=false;
    if(!dpad_held)dpad_blocked_=false;
    if(!touch_down)touch_blocked_=false;
    if(!pad_blocked_)pad_sector_=radial_sector(double(circle_x),-double(circle_y),pad_sector_,tuning_.circle_activate,tuning_.circle_release,tuning_);
    if(touch_down&&(touch_x<0||touch_y<0||uint32_t(touch_x)>=tuning_.screen_width||uint32_t(touch_y)>=tuning_.screen_height)){
        // Invalid driver coordinates cancel, rather than move the logical origin.
        touch_blocked_=true;touch_active_=false;touch_sector_=-1;
    }
    if(touch_active_){
        // Wall time, never scaled simulation time or fixed-step catch-up debt.
        touch_seconds_=std::min(double(tuning_.tap_max_seconds),touch_seconds_+real_delta);
        if(touch_seconds_>=tuning_.tap_max_seconds)touch_timed_out_=true;
    }
    if(touch_down&&!touch_blocked_){
        if(!touch_active_){
            touch_active_=true;touch_dragged_=touch_timed_out_=false;
            origin_x_=touch_x;origin_y_=touch_y;touch_seconds_=touch_max_distance_=0;touch_sector_=-1;
        }
        const double dx=double(touch_x)-origin_x_,dy=double(touch_y)-origin_y_,radius=std::hypot(dx,dy);
        touch_max_distance_=std::max(touch_max_distance_,radius);
        if(radius>=tuning_.touch_activate)touch_dragged_=true;
        if(touch_dragged_)touch_sector_=radial_sector(dx,dy,touch_sector_,tuning_.touch_activate,tuning_.touch_release,tuning_);
        result.touch_direction=direction(touch_sector_);
        result.gesture.active=true;result.gesture.dragged=touch_dragged_;
        result.gesture.origin_x=float(origin_x_);result.gesture.origin_y=float(origin_y_);
        const double travel=tuning_.base_radius-tuning_.thumb_radius;
        const double scale=radius>travel?travel/radius:1;
        // This clamp affects display only; edge-origin gestures retain full input.
        result.gesture.thumb_x=float(std::clamp(origin_x_+dx*scale,double(tuning_.thumb_radius),double(tuning_.screen_width-1)-tuning_.thumb_radius));
        result.gesture.thumb_y=float(std::clamp(origin_y_+dy*scale,double(tuning_.thumb_radius),double(tuning_.screen_height-1)-tuning_.thumb_radius));
    }else if(touch_active_){
        result.confirm_pulse=!touch_dragged_&&!touch_timed_out_&&touch_max_distance_<tuning_.tap_max_travel;
        touch_active_=false;touch_sector_=-1;
    }
    // Any D-pad button owns the device, even when opposing bits cancel. A drag
    // owns input until lift, including returning to center, so another device
    // cannot unexpectedly start movement underneath an active virtual stick.
    if(dpad_held&&!dpad_blocked_){
        result.source=NativeInputSource::Dpad;result.direction={int(dpad.right)-int(dpad.left),int(dpad.down)-int(dpad.up)};
    }else if(touch_active_&&touch_dragged_){
        result.source=NativeInputSource::Touch;result.direction=result.touch_direction;
    }else if(pad_sector_>=0&&!pad_blocked_){result.source=NativeInputSource::Pad;result.direction=direction(pad_sector_);}
    return result;
}
}
