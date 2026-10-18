#include "encore/music_player_choice.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {namespace {
bool reject(std::string&e,const char*why){e=why;return false;}
bool gain(float v){return std::isfinite(v)&&v>=-120&&v<=24;}
bool seconds(double v){return std::isfinite(v)&&v>=0&&v<=60;}
bool stream(std::string_view value,bool empty=false){
    if(value.empty())return empty;
    if(!value.data()||value.size()>1024||value.substr(0,6)!="res://")return false;
    const auto path=value.substr(6);if(path.empty()||path.front()=='/'||path.back()=='/')return false;
    size_t at=0;
    while(at<path.size()){
        const auto end=path.find('/',at);const auto part=path.substr(at,end==path.npos?path.size()-at:end-at);
        if(part.empty()||part=="."||part=="..")return false;
        for(unsigned char c:part)if(c<32||c>126||c=='\\'||c==':')return false;
        if(end==path.npos)break;at=end+1;
    }
    return true;
}
}
bool valid_music_stream_uri(std::string_view value){return stream(value);}
bool choose_music_loop(const std::vector<MusicPlayerSnapshot>&players,const MusicLoopRequest&request,MusicLoopChoice&result,std::string&e){
    if(players.empty()||players.size()>256||!stream(request.stream)||
       !stream(request.first_registered_stream)||!gain(request.gain_db)||!gain(request.silence_db)||
       request.silence_db>-20||!seconds(request.fadein_seconds)||!seconds(request.fade_to_seconds))
        return reject(e,"Music loop requires checked source players and tuning");
    for(size_t i=0;i<players.size();++i){
        const auto&p=players[i];
        if(p.identity==MusicLoopChoice::no_player||!stream(p.stream,true)||(p.playing&&p.stream.empty())||!gain(p.gain_db))
            return reject(e,"Music player source snapshot is invalid");
        for(size_t j=0;j<i;++j)if(players[j].identity==p.identity)return reject(e,"Music player node identities are duplicated");
    }
    const auto&latest=players.back();const MusicPlayerSnapshot*selected=&latest;bool same=false;
    if(!request.requester_is_first){
        same=request.first_registered_stream==request.stream;
        // The source searches every global node in child order, even stopped
        // players. It only performs this search when another changer is first.
        for(const auto&p:players)if(p.stream==request.stream){same=true;selected=&p;break;}
    }else same=latest.stream==request.stream;
    MusicLoopChoice next;next.same_song=same;next.target_gain_db=request.gain_db;
    if(!same&&latest.playing){
        if(players.size()==256)return reject(e,"Music player node capacity exhausted");
        next.append_player=true;next.restart_stream=true;next.volume=MusicLoopVolume::FadeIn;
        next.initial_gain_db=request.silence_db;next.seconds=request.fadein_seconds;next.curve=MusicRegionCurve::QuartOut;
    }else{
        next.player=selected->identity;next.initial_gain_db=selected->gain_db;
        if(selected->playing&&selected->gain_db>request.silence_db){
            if(selected->gain_db!=request.gain_db){next.volume=MusicLoopVolume::FadeTo;next.seconds=request.fade_to_seconds;}
            // Equal property values do not cancel a previously active tween.
        }else{
            next.restart_stream=true;next.volume=MusicLoopVolume::Set;next.initial_gain_db=request.gain_db;
        }
    }
    result=next;e.clear();return true;
}
bool sample_music_tween(float from,float to,double duration,double elapsed,MusicRegionCurve curve,float&result,std::string&e){
    if(!gain(from)||!gain(to)||!seconds(duration)||!std::isfinite(elapsed)||elapsed<0||
       (curve!=MusicRegionCurve::Linear&&curve!=MusicRegionCurve::QuartIn&&curve!=MusicRegionCurve::QuartOut))
        return reject(e,"Music tween source parameters are invalid");
    double t=duration>0?std::min(1.0,elapsed/duration):1.0;
    if(curve==MusicRegionCurve::QuartIn)t=t*t*t*t;
    else if(curve==MusicRegionCurve::QuartOut){const double remaining=1-t;t=1-remaining*remaining*remaining*remaining;}
    const float next=float(double(from)+(double(to)-double(from))*t);
    if(!gain(next))return reject(e,"Music tween result is outside the admitted volume domain");
    result=next;e.clear();return true;
}
}
