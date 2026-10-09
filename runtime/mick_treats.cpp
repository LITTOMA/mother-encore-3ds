#include "encore/mick_treats.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>

namespace encore::upstream {
namespace {
constexpr size_t header_bytes=64,max_bytes=64u*1024u;
// Bytes, Strings, Texture, Actor=200, Programmes=16, Commands=20, Texts=8, Clips=8, Rng=32
constexpr uint32_t strides[kMickSectionCount]={1,8,12,200,16,20,8,8,32};
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t u16(const uint8_t*p){return uint16_t(p[0]|p[1]<<8);}
float f32(const uint8_t*p){const uint32_t v=u32(p);float f;std::memcpy(&f,&v,4);return f;}
float bits(uint32_t v){float f;std::memcpy(&f,&v,4);return f;}
Vec2 vec(const uint8_t*p){return {f32(p),f32(p+4)};}
bool finite(float v,float limit=100000.f){return std::isfinite(v)&&std::fabs(v)<=limit;}
bool finite(Vec2 v){return finite(v.x)&&finite(v.y);}
bool safe_path(std::string_view p){if(p.empty()||p.size()>256||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;
    size_t a=0;while(a<=p.size()){auto b=p.find('/',a);if(b==p.npos)b=p.size();const auto s=p.substr(a,b-a);if(s.empty()||s=="."||s=="..")return false;
    for(unsigned char ch:s)if(ch<32||ch>126)return false;if(b==p.size())break;a=b+1;}return true;}
bool text_ok(std::string_view s){if(s.empty()||s.size()>512)return false;for(unsigned char c:s)if(c<32&&c!='\n')return false;return true;}
bool ends_with(std::string_view s,std::string_view x){return s.size()>x.size()&&s.compare(s.size()-x.size(),x.size(),x)==0;}
bool chinese(std::string_view code){return code=="zh_Hans_CN"||code.rfind("zh",0)==0;}
bool overlaps_rect(Vec2 cam,Vec2 view,float x,float y,float w,float h){
    return cam.x<x+w&&cam.x+view.x>x&&cam.y<y+h&&cam.y+view.y>y;
}
float ease_out_quart(float t){const float u=1.f-t;return 1.f-u*u*u*u;}
float ease_in_quad(float t){return t*t;}
Vec2 rounded_dir(Vec2 v){
    if(v.x==0&&v.y==0)return {};
    const float len=std::sqrt(v.x*v.x+v.y*v.y);Vec2 n{v.x/len,v.y/len};
    n.x=std::round(n.x);n.y=std::round(n.y);
    if(n.x==0&&n.y==0)return {v.x<0?-1.f:1.f,0};
    return n;
}
}
uint32_t MickView::count(MickSection s)const{const auto k=uint32_t(s);return bytes_&&k>=1&&k<=kMickSectionCount?u32(bytes_+header_bytes+(k-1)*16+8):0;}
const uint8_t*MickView::record(MickSection s,uint32_t i)const{const auto k=uint32_t(s);if(!bytes_||k<1||k>kMickSectionCount||i>=count(s))return nullptr;
    return bytes_+u32(bytes_+header_bytes+(k-1)*16+4)+size_t(i)*strides[k-1];}
std::string_view MickView::string(uint32_t i)const{auto*p=record(MickSection::Strings,i);if(!p)return {};const auto off=u32(p),len=u32(p+4),pool=count(MickSection::Bytes);
    if(!len||off>pool||len>pool-off)return {};return std::string_view(reinterpret_cast<const char*>(record(MickSection::Bytes,0))+off,len);}
MickTexture MickView::texture()const{auto*p=record(MickSection::Texture,0);return p?MickTexture{u32(p),u16(p+4),u16(p+6),u16(p+8),u16(p+10)}:MickTexture{};}
MickActor MickView::actor()const{auto*p=record(MickSection::Actor,0);
    if(!p)return {};
    MickActor a{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),vec(p+24),vec(p+32),vec(p+40),vec(p+48),u32(p+56),u32(p+60),f32(p+64),vec(p+68),vec(p+76),f32(p+84)};
    a.speed=f32(p+88);a.walk_frequency=f32(p+92);a.wander_radius=f32(p+96);
    a.near_offset=vec(p+100);a.near_extents=vec(p+108);a.view_offset=vec(p+116);a.view_radius=f32(p+124);
    a.ray_offset=vec(p+128);a.bark_center=vec(p+136);a.bark_extents=vec(p+144);a.initial_direction=vec(p+152);
    a.rng_lo=u32(p+160);a.rng_hi=u32(p+164);a.bark_programme=u32(p+168);
    a.idle_clip=u32(p+172);a.idle_clip_count=u32(p+176);a.walk_clip=u32(p+180);a.walk_clip_count=u32(p+184);
    a.talk_clip=u32(p+188);a.talk_clip_count=u32(p+192);a.bark_path=u32(p+196);
    return a;}
MickProgramme MickView::programme(uint32_t i)const{auto*p=record(MickSection::Programmes,i);return p?MickProgramme{u32(p),u32(p+4),u32(p+8),u32(p+12)}:MickProgramme{};}
MickCommand MickView::command(uint32_t i)const{auto*p=record(MickSection::Commands,i);return p?MickCommand{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16)}:MickCommand{};}
MickText MickView::text(uint32_t i)const{auto*p=record(MickSection::Texts,i);return p?MickText{u32(p),u32(p+4)}:MickText{};}
MickClip MickView::clip(uint32_t i)const{auto*p=record(MickSection::Clips,i);return p?MickClip{u16(p),u16(p+2),u16(p+4),u16(p+6)}:MickClip{};}
MickRngEvent MickView::rng_event(uint32_t i)const{auto*p=record(MickSection::Rng,i);return p?MickRngEvent{u32(p),f32(p+4),f32(p+8),f32(p+12),f32(p+16),f32(p+20),f32(p+24)}:MickRngEvent{};}

bool MickData::load(const uint8_t*p,size_t n,std::string&error){
    auto fail=[&](const char*m){error=m;return false;};
    if(!p||n<header_bytes+kMickSectionCount*16||n>max_bytes)return fail("Mick pack size rejected");
    if(std::memcmp(p,"ENCMIK01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)!=1||u32(p+28)||u32(p+52)!=kMickSectionCount||u32(p+56)||u32(p+60))
        return fail("Mick pack schema/capability/scene/reserved rejected");
    for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("Mick pack source pin rejected");}
    if(encore::crc32(p+32,n-32)!=u32(p+16))return fail("Mick pack checksum rejected");
    size_t cursor=header_bytes+kMickSectionCount*16;
    for(uint32_t i=0;i<kMickSectionCount;++i){const auto*d=p+header_bytes+i*16;const auto off=u32(d+4),count=u32(d+8);
        if(u32(d)!=i+1||u32(d+12)!=strides[i])return fail("Mick pack directory rejected");
        if(!count){if(off)return fail("Mick pack empty section rejected");continue;}
        if(off%4||off<cursor||off>n||count>(n-off)/strides[i])return fail("Mick pack section span rejected");
        for(size_t j=cursor;j<off;++j)if(p[j])return fail("Mick pack padding rejected");
        cursor=size_t(off)+size_t(count)*strides[i];}
    if(cursor!=n)return fail("Mick pack trailing bytes rejected");
    MickView v;v.bytes_=p;v.size_=n;
    using S=MickSection;
    if(v.count(S::Bytes)==0||v.count(S::Strings)==0||v.count(S::Strings)>512||v.count(S::Texture)!=1||v.count(S::Actor)!=1||
       v.count(S::Programmes)==0||v.count(S::Programmes)>16||v.count(S::Commands)==0||v.count(S::Commands)>128||
       v.count(S::Texts)==0||v.count(S::Texts)>64||v.count(S::Clips)==0||v.count(S::Clips)>64||v.count(S::Rng)>512)
        return fail("Mick pack table counts rejected");
    for(uint32_t i=0;i<v.count(S::Strings);++i)if(!text_ok(v.string(i)))return fail("Mick pack string rejected");
    const auto t=v.texture();
    if(!safe_path(v.string(t.path))||!ends_with(v.string(t.path),".t3x")||!t.width||!t.height||!t.columns||!t.rows||
       t.width>1024||t.height>1024||t.width%t.columns||t.height%t.rows)return fail("Mick pack texture rejected");
    const auto a=v.actor();
    const auto frames=uint32_t(t.columns)*uint32_t(t.rows);
    if(v.string(a.path).empty()||v.string(a.bark_path).empty()||a.sprite!=0||a.frame>=frames||!finite(a.position)||!finite(a.sprite_position)||
       !finite(a.interact_center)||!finite(a.interact_extents)||a.interact_extents.x<=0||a.interact_extents.y<=0||
       !finite(a.collision_center)||!finite(a.collision_extents)||a.collision_extents.x<=0||a.collision_extents.y<=0||!finite(a.sort_y)||
       v.string(a.item).empty()||v.string(a.require_flag).empty()||v.string(a.consume_flag).empty()||a.require_flag==a.consume_flag||
       !a.command_count||a.first_command>v.count(S::Commands)||a.command_count>v.count(S::Commands)-a.first_command||
       !finite(a.ray_length)||a.ray_length<=0||a.ray_length>64||!finite(a.speed)||a.speed<=0||a.speed>512||
       !finite(a.walk_frequency)||a.walk_frequency<=0||a.walk_frequency>30||!finite(a.wander_radius)||a.wander_radius<=0||a.wander_radius>512||
       !finite(a.near_offset)||!finite(a.near_extents)||a.near_extents.x<=0||a.near_extents.y<=0||!finite(a.view_offset)||
       !finite(a.view_radius)||a.view_radius<=0||a.view_radius>512||!finite(a.ray_offset)||!finite(a.bark_center)||
       !finite(a.bark_extents)||a.bark_extents.x<=0||a.bark_extents.y<=0||!finite(a.initial_direction)||
       (a.initial_direction.x==0&&a.initial_direction.y==0)||a.bark_programme>=v.count(S::Programmes)||
       a.idle_clip_count==0||a.walk_clip_count==0||a.talk_clip_count==0||
       a.idle_clip>v.count(S::Clips)||a.idle_clip_count>v.count(S::Clips)-a.idle_clip||
       a.walk_clip>v.count(S::Clips)||a.walk_clip_count>v.count(S::Clips)-a.walk_clip||
       a.talk_clip>v.count(S::Clips)||a.talk_clip_count>v.count(S::Clips)-a.talk_clip)
        return fail("Mick pack actor rejected");
    for(uint32_t i=0;i<v.count(S::Texts);++i){const auto tx=v.text(i);if(tx.en>=v.count(S::Strings)||tx.zh>=v.count(S::Strings))return fail("Mick pack text rejected");}
    auto clip_ok=[&](uint32_t first,uint32_t count,uint16_t anim){for(uint32_t i=0;i<count;++i){const auto c=v.clip(first+i);if(c.anim!=anim||c.direction>3||c.frame>=frames||!c.milliseconds||c.milliseconds>10000)return false;}return true;};
    if(!clip_ok(a.idle_clip,a.idle_clip_count,0)||!clip_ok(a.walk_clip,a.walk_clip_count,1)||!clip_ok(a.talk_clip,a.talk_clip_count,2))return fail("Mick pack clip rejected");
    bool saw_bark=false,saw_talk=false;
    for(uint32_t i=0;i<v.count(S::Programmes);++i){
        const auto g=v.programme(i);
        if((g.kind!=uint32_t(MickProgramKind::Talk)&&g.kind!=uint32_t(MickProgramKind::Bark))||!g.count||g.first>v.count(S::Commands)||g.count>v.count(S::Commands)-g.first)
            return fail("Mick pack programme rejected");
        if(g.kind==uint32_t(MickProgramKind::Talk))saw_talk=true;else saw_bark=true;
        if(g.flag&&v.string(g.flag).empty())return fail("Mick pack programme flag rejected");
        bool ended=false;
        for(uint32_t pc=0;pc<g.count;++pc){const auto c=v.command(g.first+pc);
            switch(MickOpcode(c.opcode)){
            case MickOpcode::ShowText:if(c.a>=v.count(S::Texts)||c.b||c.c||c.d)return fail("Mick pack ShowText rejected");break;
            case MickOpcode::AwaitText:case MickOpcode::End:if(c.a||c.b||c.c||c.d)return fail("Mick pack gate rejected");if(c.opcode==uint32_t(MickOpcode::End))ended=true;break;
            case MickOpcode::RemoveKeyItem:if(c.a!=a.item||c.b||c.c||c.d)return fail("Mick pack RemoveKeyItem rejected");break;
            case MickOpcode::SetFlag:if(!c.a||c.a>=v.count(S::Strings)||v.string(c.a).empty()||c.b>1||c.c||c.d)return fail("Mick pack SetFlag rejected");break;
            case MickOpcode::Choice:if(c.a>=v.count(S::Texts)||c.c>=v.count(S::Texts)||c.b>=g.count||c.d>=g.count)return fail("Mick pack Choice rejected");break;
            case MickOpcode::PlaySound:if(!c.a||c.a>=v.count(S::Strings)||v.string(c.a).empty()||c.b||c.c||c.d)return fail("Mick pack PlaySound rejected");break;
            case MickOpcode::JumpActor:if(!finite(bits(c.a))||bits(c.a)<=0||bits(c.a)>64||!finite(bits(c.b))||bits(c.b)<=0||bits(c.b)>5||c.c==0||c.c>8||c.d)return fail("Mick pack JumpActor rejected");break;
            case MickOpcode::TurnActor:if(!finite(bits(c.a))||!finite(bits(c.b))||(bits(c.a)==0&&bits(c.b)==0)||!finite(bits(c.c))||bits(c.c)<=0||bits(c.c)>5||c.d)return fail("Mick pack TurnActor rejected");break;
            case MickOpcode::MovePlayer:if(!finite(bits(c.a))||!finite(bits(c.b))||(bits(c.a)==0&&bits(c.b)==0)||!finite(bits(c.c))||bits(c.c)<=0||bits(c.c)>512||c.d)return fail("Mick pack MovePlayer rejected");break;
            case MickOpcode::Wait:if(!finite(bits(c.a))||bits(c.a)<=0||bits(c.a)>30||c.b||c.c||c.d)return fail("Mick pack Wait rejected");break;
            default:return fail("Mick pack opcode rejected");}}
        if(!ended||v.command(g.first+g.count-1).opcode!=uint32_t(MickOpcode::End))return fail("Mick pack programme must End");
    }
    if(!saw_talk||!saw_bark||v.programme(a.bark_programme).kind!=uint32_t(MickProgramKind::Bark))return fail("Mick pack talk/bark programme missing");
    for(uint32_t i=0;i<v.count(S::Rng);++i){const auto e=v.rng_event(i);
        if(e.kind<uint32_t(MickRngKind::Randi)||e.kind>uint32_t(MickRngKind::ArmWander)||!finite(e.x)||!finite(e.y)||!finite(e.w)||!finite(e.h)||e.w<=0||e.h<=0||!finite(e.a)||!finite(e.b))
            return fail("Mick pack RNG event rejected");
        if(e.kind==uint32_t(MickRngKind::RandRange)&&(e.a==e.b))return fail("Mick pack RNG range rejected");}
    error.clear();bytes_.assign(p,p+n);return true;
}
bool MickData::load_file(const char*path,std::string&error){
    std::vector<uint8_t> bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);
}

bool MickRuntime::fail(std::string&e,const char*m){e=m;state_=MickProgramState::Failed;return false;}
bool MickRuntime::initialize(MickView view,MickHost&host,std::string&e){
    if(!view){e="Mick runtime requires a checked pack";return false;}
    const auto a=view.actor();
    if(!host.validate_flag(view.string(a.require_flag),e)||!host.validate_flag(view.string(a.consume_flag),e)||!host.validate_item(view.string(a.item),e))return false;
    for(uint32_t i=0;i<view.count(MickSection::Programmes);++i){const auto g=view.programme(i);if(g.flag&&!host.validate_flag(view.string(g.flag),e))return false;
        for(uint32_t pc=0;pc<g.count;++pc){const auto c=view.command(g.first+pc);if(c.opcode==uint32_t(MickOpcode::SetFlag)&&!host.validate_flag(view.string(c.a),e))return false;}}
    *this=MickRuntime{};view_=view;host_=&host;ready_=true;
    spawn_=position_=a.position;facing_=a.initial_direction;destination_=position_;
    rng_.set_state((uint64_t(a.rng_hi)<<32)|a.rng_lo);
    rng_done_.assign(view.count(MickSection::Rng),0);
    e.clear();return true;
}
std::string_view MickRuntime::localized(const MickText&t,std::string_view code)const{return view_.string(chinese(code)?t.zh:t.en);}
std::string_view MickRuntime::choice_text(uint32_t i)const{
    if(state_!=MickProgramState::WaitingChoice||i>1)return {};
    return localized(view_.text(choice_text_[i]),locale_);
}
bool MickRuntime::continues()const{
    if(state_!=MickProgramState::WaitingText&&state_!=MickProgramState::WaitingChoice)return false;
    return pc_<programme_count_&&view_.command(programme_first_+pc_).opcode!=uint32_t(MickOpcode::End);
}
bool MickRuntime::select(MickProgramKind kind,std::string&e){
    const auto n=view_.count(MickSection::Programmes);int found=-1;
    for(uint32_t i=0;i<n;++i){const auto g=view_.programme(i);if(g.kind!=uint32_t(kind))continue;
        if(!g.flag){found=int(i);continue;}
        bool on=false;if(!host_->flag(view_.string(g.flag),on,e))return false;if(on)found=int(i);}
    if(found<0)return fail(e,"Mick programme is not available");
    const auto g=view_.programme(uint32_t(found));
    programme_first_=g.first;programme_count_=g.count;pc_=0;state_=MickProgramState::Idle;return true;
}
bool MickRuntime::start_talk(std::string&e){
    if(!ready_||!host_||state_==MickProgramState::WaitingText||state_==MickProgramState::WaitingChoice||state_==MickProgramState::WaitingMotion)return fail(e,"Mick programme start rejected");
    if(!select(MickProgramKind::Talk,e))return false;
    return step(e);
}
void MickRuntime::face_toward(Vec2 target){const Vec2 aim{target.x-position_.x,target.y-position_.y};if(aim.x||aim.y)facing_=aim;}
bool MickRuntime::start_bark(std::string&e){
    if(!ready_||!host_||state_==MickProgramState::WaitingText||state_==MickProgramState::WaitingChoice||state_==MickProgramState::WaitingMotion)return fail(e,"Mick bark start rejected");
    const auto g=view_.programme(view_.actor().bark_programme);
    programme_first_=g.first;programme_count_=g.count;pc_=0;state_=MickProgramState::Idle;return step(e);
}
bool MickRuntime::advance_text(std::string&e){
    if(state_!=MickProgramState::WaitingText)return fail(e,"Mick text acknowledgement out of order");
    state_=MickProgramState::Idle;return step(e);
}
bool MickRuntime::move_choice(int delta){if(state_!=MickProgramState::WaitingChoice||!delta)return false;choice_index_=delta>0?1u:0u;return true;}
bool MickRuntime::confirm_choice(bool cancel,std::string&e){
    if(state_!=MickProgramState::WaitingChoice)return fail(e,"Mick choice acknowledgement out of order");
    // Reviewed woof cancel target is option 1. B selects it; A selects the cursor.
    pc_=choice_pc_[cancel?1u:choice_index_];state_=MickProgramState::Idle;return step(e);
}
bool MickRuntime::step(std::string&e){
    auto rejected=[&]{state_=MickProgramState::Failed;if(e.empty())e="Mick programme host effect rejected";return false;};
    for(uint32_t guard=0;guard<=programme_count_;++guard){
        if(pc_>=programme_count_)return fail(e,"Mick programme ran past its span");
        const auto c=view_.command(programme_first_+pc_);
        switch(MickOpcode(c.opcode)){
        case MickOpcode::ShowText:{
            anim_=2;const auto body=localized(view_.text(c.a),locale_);
            if(!host_->show_text(body,e))return rejected();++pc_;break;}
        case MickOpcode::AwaitText:++pc_;state_=MickProgramState::WaitingText;e.clear();return true;
        case MickOpcode::RemoveKeyItem:if(!host_->remove_key_item(view_.string(c.a),e))return rejected();++pc_;break;
        case MickOpcode::SetFlag:if(!host_->set_flag(view_.string(c.a),c.b!=0,e))return rejected();++pc_;break;
        case MickOpcode::PlaySound:if(!host_->play_sound(view_.string(c.a),e))return rejected();++pc_;break;
        case MickOpcode::Choice:
            choice_text_[0]=c.a;choice_pc_[0]=c.b;choice_text_[1]=c.c;choice_pc_[1]=c.d;choice_index_=0;
            state_=MickProgramState::WaitingChoice;e.clear();return true;
        case MickOpcode::JumpActor:jump_height_=bits(c.a);jump_length_=bits(c.b);jump_left_=int(c.c);jump_time_=0;++pc_;break;
        case MickOpcode::TurnActor:turn_target_={bits(c.a),bits(c.b)};turn_interval_=bits(c.c);turn_wait_=0;turn_left_=8;++pc_;break;
        case MickOpcode::MovePlayer:{
            move_target_={bits(c.a),bits(c.b)};move_speed_=bits(c.c);move_left_=std::sqrt(move_target_.x*move_target_.x+move_target_.y*move_target_.y);
            const float len=move_left_;shove_dir_=len?Vec2{move_target_.x/len,move_target_.y/len}:Vec2{};
            move_target_=shove_dir_;++pc_;break;}
        case MickOpcode::Wait:wait_left_=bits(c.a);++pc_;state_=MickProgramState::WaitingMotion;e.clear();return true;
        case MickOpcode::End:state_=MickProgramState::Complete;anim_=0;e.clear();return true;
        default:return fail(e,"Mick programme opcode rejected");}
    }
    return fail(e,"Mick programme did not reach a gate");
}
uint32_t MickRuntime::direction_index()const{
    const float ax=std::fabs(facing_.x),ay=std::fabs(facing_.y);
    if(ax>ay)return facing_.x<0?1u:2u;
    return facing_.y<0?3u:0u;
}
uint32_t MickRuntime::sheet_frame()const{
    const auto a=view_.actor();const uint32_t dir=direction_index();
    const uint32_t first=anim_==1?a.walk_clip:anim_==2?a.talk_clip:a.idle_clip;
    const uint32_t count=anim_==1?a.walk_clip_count:anim_==2?a.talk_clip_count:a.idle_clip_count;
    uint32_t begin=first,n=0;float total=0;
    for(uint32_t i=0;i<count;++i){const auto c=view_.clip(first+i);if(c.direction!=dir)continue;if(!n)begin=first+i;++n;total+=c.milliseconds*0.001f;}
    if(!n)return a.frame;
    if(anim_==0)return view_.clip(begin).frame;
    float t=total>0?std::fmod(anim_time_,total):0;
    for(uint32_t i=0;i<n;++i){const auto c=view_.clip(begin+i);const float d=c.milliseconds*0.001f;if(t<=d||i+1==n)return c.frame;t-=d;}
    return view_.clip(begin).frame;
}
Vec2 MickRuntime::sprite_center()const{const auto a=view_.actor();return {position_.x+(a.sprite_position.x-a.position.x),position_.y+(a.sprite_position.y-a.position.y)-jump_lift_};}
Vec2 MickRuntime::collision_center()const{const auto a=view_.actor();return {position_.x+(a.collision_center.x-a.position.x),position_.y+(a.collision_center.y-a.position.y)};}
Vec2 MickRuntime::interact_center()const{const auto a=view_.actor();return {position_.x+(a.interact_center.x-a.position.x),position_.y+(a.interact_center.y-a.position.y)};}
void MickRuntime::consume_rng(Vec2 camera,Vec2 view){
    const auto n=view_.count(MickSection::Rng);
    for(uint32_t i=0;i<n;++i){if(rng_done_[i])continue;const auto e=view_.rng_event(i);
        if(!overlaps_rect(camera,view,e.x,e.y,e.w,e.h))continue;
        rng_done_[i]=1;
        if(e.kind==uint32_t(MickRngKind::Randi))rng_.randi();
        else if(e.kind==uint32_t(MickRngKind::Randf))rng_.randf();
        else if(e.kind==uint32_t(MickRngKind::RandRange))rng_.rand_range(e.a,e.b);
        else if(e.kind==uint32_t(MickRngKind::ArmWander)&&!wander_armed_){
            const auto a=view_.actor();wander_left_=float(rng_.rand_range(0.1,a.walk_frequency));wander_armed_=true;}}}
void MickRuntime::pick_destination(){
    const auto a=view_.actor();
    for(int guard=0;guard<32;++guard){
        const bool horizontal=(rng_.randi()%2)==1;
        float x=position_.x,y=position_.y;const float reach=a.wander_radius*0.5f;
        if(horizontal)x=float(rng_.rand_range(spawn_.x-reach,spawn_.x+reach));
        else y=float(rng_.rand_range(spawn_.y-reach,spawn_.y+reach));
        x=std::round(x);y=std::round(y);
        if(std::fabs(x-position_.x)>8.f||std::fabs(y-position_.y)>8.f){destination_={x,y};probing_=true;return;}
    }
    destination_=position_;probing_=false;
}
void MickRuntime::tick_wander(float dt,Vec2,bool paused,MickWorld& world){
    const auto a=view_.actor();const bool busy=state_==MickProgramState::WaitingText||state_==MickProgramState::WaitingChoice||state_==MickProgramState::WaitingMotion||state_==MickProgramState::Failed;
    const bool near=world.player_near({position_.x+a.near_offset.x,position_.y+a.near_offset.y},a.near_extents);
    const bool view=world.player_in_view({position_.x+a.view_offset.x,position_.y+a.view_offset.y},a.view_radius);
    if(view)looking_=true;else looking_=false;
    if(looking_&&!busy){const Vec2 aim{0,0};(void)aim;}
    if(paused||busy||!wander_armed_){if(!busy)anim_=0;return;}
    if(wander_left_>=0){wander_left_-=dt;if(wander_left_<=0){if(!looking_&&!near)pick_destination();else destination_=position_;wander_left_=float(rng_.rand_range(a.walk_frequency-0.5,a.walk_frequency+0.5));}}
    if(near||looking_){destination_=position_;anim_=0;return;}
    if(probing_){bool clear=false;if(!world.probe(position_,destination_,clear)){destination_=position_;probing_=false;return;}if(clear){}else destination_=position_;probing_=false;}
    const float dx=destination_.x-position_.x,dy=destination_.y-position_.y;
    const float difference=std::max(std::ceil(std::fabs(a.speed*dt)),1.f);
    if(std::fabs(dx)<=difference&&std::fabs(dy)<=difference){anim_=0;position_.x=std::round(position_.x);position_.y=std::round(position_.y);return;}
    const float len=std::sqrt(dx*dx+dy*dy);if(len==0){anim_=0;return;}
    facing_={dx/len,dy/len};anim_=1;anim_time_+=dt;
    Vec2 out{};if(!world.slide(position_,{facing_.x*a.speed,facing_.y*a.speed},out))return;
    const float moved=std::sqrt((out.x-position_.x)*(out.x-position_.x)+(out.y-position_.y)*(out.y-position_.y));
    position_=out;
    if(moved<0.5f){anim_=0;position_.x=std::round(position_.x);position_.y=std::round(position_.y);destination_=position_;}
}
void MickRuntime::tick_motion(float dt){
    if(jump_left_>0){
        if(jump_gap_>0){jump_gap_-=dt;jump_lift_=0;}
        else{
            jump_time_+=dt;const float up=jump_length_*0.6f,down=jump_length_*0.4f;
            if(jump_time_<up)jump_lift_=jump_height_*ease_out_quart(up?jump_time_/up:1.f);
            else if(jump_time_<jump_length_)jump_lift_=jump_height_*(1.f-ease_in_quad(down?(jump_time_-up)/down:1.f));
            else{jump_lift_=0;jump_time_=0;if(--jump_left_>0)jump_gap_=0.1f;}
        }
    }
    if(turn_left_>0){
        const Vec2 want=rounded_dir(turn_target_),have=rounded_dir(facing_);
        if(have.x==want.x&&have.y==want.y){facing_=want;turn_left_=0;}
        else if((turn_wait_-=dt)<=0){
            const float cross=have.x*want.y-have.y*want.x;const float sign=cross<0?-1.f:1.f;
            const float rad=sign*0.78539816339f;const float c=std::cos(rad),s=std::sin(rad);
            facing_=rounded_dir({have.x*c-have.y*s,have.x*s+have.y*c});turn_wait_=turn_interval_;if(--turn_left_==0)facing_=want;}
    }
}
bool MickRuntime::simulate(float dt,Vec2 player,Vec2 camera,Vec2 view,bool paused,MickWorld& world,std::string& e){
    if(!ready_)return true;
    if(!(dt>0)||dt>0.1f)return fail(e,"Mick step time rejected");
    consume_rng(camera,view);
    const auto actor=view_.actor();
    const bool seen=world.player_in_view({position_.x+actor.view_offset.x,position_.y+actor.view_offset.y},actor.view_radius);
    const bool talking=state_==MickProgramState::WaitingText||state_==MickProgramState::WaitingChoice||state_==MickProgramState::WaitingMotion;
    if(seen&&!talking){const Vec2 aim{player.x-position_.x,player.y-position_.y};if(aim.x||aim.y)facing_=aim;}
    tick_motion(dt);
    if(move_left_>0){
        const float step=std::min(move_left_,move_speed_*dt);move_left_-=step;
        Vec2 dir=rounded_dir(shove_dir_);if(!dir.x&&!dir.y)dir={0,-1};
        if(!world.shove_player({player.x+shove_dir_.x*step,player.y+shove_dir_.y*step},dir,true))return fail(e,"Mick bark shove rejected");
        if(move_left_<0)move_left_=0;
    }
    if(state_==MickProgramState::WaitingMotion){
        if(wait_left_>0)wait_left_-=dt;
        if(move_left_<=0&&wait_left_<=0){move_left_=wait_left_=0;state_=MickProgramState::Idle;if(!step(e))return false;}
    }
    if(state_!=MickProgramState::WaitingText&&state_!=MickProgramState::WaitingChoice&&state_!=MickProgramState::WaitingMotion&&state_!=MickProgramState::Failed)
        tick_wander(dt,player,paused,world);
    if(state_==MickProgramState::Failed)return false;
    e.clear();return true;
}
}
