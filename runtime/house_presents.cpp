#include "encore/house_presents.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {
bool PresentRuntime::fail(std::string&e,const char*m){e=m;state_=PresentProgramState::Failed;return false;}
bool PresentRuntime::initialize(PresentView view,PresentHost&host,std::string&e){
    // Built as a candidate: a rejected binding keeps the current state.
    if(!view){e="Present runtime requires a checked pack";return false;}
    for(uint32_t i=0;i<view.count(PresentSection::Texts);++i){const auto t=view.text(i);if(!host.validate_text(t.dialogue_id,view.string(t.source_path),e))return false;}
    for(uint32_t i=0;i<view.count(PresentSection::Templates);++i){const auto t=view.item_template(i);if(!host.validate_item(t,view.string(t.item),e))return false;}
    for(uint32_t i=0;i<view.count(PresentSection::Commands);++i){const auto c=view.command(i);
        if((c.opcode==uint32_t(PresentOpcode::BranchFlag)||c.opcode==uint32_t(PresentOpcode::SetFlag))&&!host.validate_flag(view.string(c.a),e))return false;
        if(c.opcode==uint32_t(PresentOpcode::PlaySound)&&!host.validate_sound(view.string(c.a),e))return false;}
    const auto sp=view.sparkles();
    std::vector<Object>objects(view.count(PresentSection::Objects));
    for(uint32_t i=0;i<objects.size();++i){const auto o=view.object(i);auto&s=objects[i];
        if(!host.validate_sound(view.string(o.sound),e))return false;
        bool opened=false;
        if(o.policy==uint32_t(PresentPolicy::Flag)){if(!host.validate_flag(view.string(o.flag),e)||!host.flag(view.string(o.flag),opened,e))return false;}
        // Present._ready: an opened box shows its last frame and stops/hides Sparkles.
        s.frame=opened?4:0;s.sparkles_visible=s.sparkles_playing=!opened;
        s.sparkle_frame=std::min<uint32_t>(22,sp.order_count-1);s.sparkle_timeout=float(1.0/double(sp.fps));}
    *this=PresentRuntime{};objects_=std::move(objects);view_=view;host_=&host;e.clear();return true;
}
bool PresentRuntime::scene_ready(SourceRandom&random,std::string&e){
    if(!view_||ready_)return fail(e,"Present scene ready out of order");
    const auto sp=view_.sparkles();
    for(auto&s:objects_){
        // AnimatedSprite.set_frame clamps to the last frame and resets the timeout.
        const auto value=int64_t(random.rand_range(0.0,double(sp.initial_range)));
        s.sparkle_frame=uint32_t(std::clamp<int64_t>(value,0,int64_t(sp.order_count)-1));
        if(s.sparkles_playing)s.sparkle_timeout=float(1.0/double(sp.fps));
    }
    ready_=true;e.clear();return true;
}
void PresentRuntime::play_clip(uint32_t object,uint32_t clip){auto&s=objects_[object];s.clip=clip;s.clip_time=0;s.clip_started=false;}
bool PresentRuntime::advance(double delta,std::string&e){
    if(!view_)return fail(e,"Present runtime is not initialized");
    if(!std::isfinite(delta)||delta<0||delta>1)return fail(e,"Present idle time rejected");
    const auto sp=view_.sparkles();const float step=float(delta);const float duration=float(1.0/double(sp.fps));
    for(uint32_t i=0;i<objects_.size();++i){auto&s=objects_[i];
        // Godot 3.6 AnimatedSprite NOTIFICATION_INTERNAL_PROCESS, looping forward.
        if(s.sparkles_playing){float remaining=step;
            while(remaining>0){if(s.sparkle_timeout<=0){s.sparkle_timeout=duration;s.sparkle_frame=s.sparkle_frame>=sp.order_count-1?0:s.sparkle_frame+1;}
                const float take=std::min(s.sparkle_timeout,remaining);remaining-=take;s.sparkle_timeout-=take;}}
        if(s.clip==kPresentNone)continue;
        const auto clip=view_.clip(s.clip);const float from=s.clip_time;float to=float(s.clip_time+step);
        const bool end=to>=clip.length;if(end)to=clip.length;
        // Discrete value tracks: keys in [from, to); the first process also takes time 0.
        auto in_range=[&](float t){return (t>=from&&t<to)||(!s.clip_started&&t==0)||(end&&t==clip.length&&t>from);};
        for(uint32_t k=0;k<clip.frame_key_count;++k){const auto key=view_.frame_key(clip.first_frame_key+k);if(in_range(key.time))s.frame=key.frame;}
        for(uint32_t k=0;k<clip.audio_key_count;++k){const auto key=view_.audio_key(clip.first_audio_key+k);
            if(in_range(key.time))audio_.push_back({key.playing?PresentAudioKind::Play:PresentAudioKind::Stop,i,view_.string(view_.object(i).sound)});}
        s.clip_started=true;s.clip_time=to;if(end)s.clip=kPresentNone;
    }
    e.clear();return true;
}
PresentPose PresentRuntime::pose(uint32_t i)const{
    if(!view_||i>=objects_.size())return {};
    const auto o=view_.object(i);const auto&s=objects_[i];const auto sp=view_.sparkles();
    PresentPose p;p.texture=o.texture;p.frame=s.frame;p.position=o.position;p.sparkles=s.sparkles_visible;p.sparkle_texture=sp.texture;
    p.sparkle_region=view_.region(view_.order(sp.first_order+s.sparkle_frame));p.sparkle_position={o.position.x+sp.offset.x,o.position.y+sp.offset.y};
    return p;
}
bool PresentRuntime::start(uint32_t object,std::string&e){
    if(!view_||!host_||!ready_||state_==PresentProgramState::WaitingText||!supported(object))return fail(e,"Present programme start rejected");
    active_=object;pc_=0;state_=PresentProgramState::Idle;return step(e);
}
bool PresentRuntime::continues()const{
    if(state_!=PresentProgramState::WaitingText)return false;
    const auto pr=view_.program(view_.object(active_).program);
    return pc_<pr.count&&view_.command(pr.first+pc_).opcode!=uint32_t(PresentOpcode::End);
}
bool PresentRuntime::advance_text(std::string&e){
    if(state_!=PresentProgramState::WaitingText)return fail(e,"Present text acknowledgement out of order");
    state_=PresentProgramState::Idle;return step(e);
}
bool PresentRuntime::step(std::string&e){
    const auto o=view_.object(active_);const auto pr=view_.program(o.program);
    auto rejected=[&]{state_=PresentProgramState::Failed;if(e.empty())e="Present programme host effect rejected";return false;};
    for(uint32_t guard=0;guard<=pr.count;++guard){
        if(pc_>=pr.count)return fail(e,"Present programme ran past its span");
        const auto c=view_.command(pr.first+pc_);
        switch(PresentOpcode(c.opcode)){
        case PresentOpcode::ShowText:{const auto t=view_.text(c.a);if(!host_->show_text(t.dialogue_id,e))return rejected();++pc_;break;}
        case PresentOpcode::AwaitText:++pc_;state_=PresentProgramState::WaitingText;e.clear();return true;
        case PresentOpcode::BranchFlag:{bool value=false;if(!host_->flag(view_.string(c.a),value,e))return rejected();pc_=value==(c.b!=0)?c.c:pc_+1;break;}
        case PresentOpcode::GrantItem:{const auto t=view_.item_template(c.a);if(!host_->grant_item(t,view_.string(t.item),e))return rejected();++pc_;break;}
        case PresentOpcode::PlaySound:if(!host_->play_sound(view_.string(c.a),e))return rejected();++pc_;break;
        case PresentOpcode::SetFlag:{const auto name=view_.string(c.a);if(!host_->set_flag(name,c.b!=0,e))return rejected();
            // ItemHolder._update_state after _set_flag_status: an opened box stops/hides Sparkles.
            if(name==view_.string(o.flag)&&c.b){auto&s=objects_[active_];s.sparkles_visible=s.sparkles_playing=false;}
            ++pc_;break;}
        case PresentOpcode::Jump:pc_=c.a;break;
        case PresentOpcode::PlayClip:play_clip(active_,c.a);++pc_;break;
        case PresentOpcode::End:state_=PresentProgramState::Complete;e.clear();return true;
        default:return fail(e,"Present programme opcode rejected");}
    }
    return fail(e,"Present programme did not reach a gate");
}
}
