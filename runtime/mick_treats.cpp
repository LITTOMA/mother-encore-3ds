#include "encore/mick_treats.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>

namespace encore::upstream {
namespace {
constexpr size_t header_bytes=64,max_bytes=32u*1024u;
// Bytes=1, Strings=8, Texture=12, Actor=68, Commands=20, Texts=8
constexpr uint32_t strides[kMickSectionCount]={1,8,12,68,20,8};
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t u16(const uint8_t*p){return uint16_t(p[0]|p[1]<<8);}
float f32(const uint8_t*p){const uint32_t v=u32(p);float f;std::memcpy(&f,&v,4);return f;}
Vec2 vec(const uint8_t*p){return {f32(p),f32(p+4)};}
bool finite(float v,float limit=100000.f){return std::isfinite(v)&&std::fabs(v)<=limit;}
bool finite(Vec2 v){return finite(v.x)&&finite(v.y);}
bool safe_path(std::string_view p){if(p.empty()||p.size()>256||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;
    size_t a=0;while(a<=p.size()){auto b=p.find('/',a);if(b==p.npos)b=p.size();const auto s=p.substr(a,b-a);if(s.empty()||s=="."||s=="..")return false;
    for(unsigned char ch:s)if(ch<32||ch>126)return false;if(b==p.size())break;a=b+1;}return true;}
bool text_ok(std::string_view s){if(s.empty()||s.size()>512)return false;for(unsigned char c:s)if(c<32&&c!='\n')return false;return true;}
bool ends_with(std::string_view s,std::string_view x){return s.size()>x.size()&&s.compare(s.size()-x.size(),x.size(),x)==0;}
bool chinese(std::string_view code){return code=="zh_Hans_CN"||code.rfind("zh",0)==0;}
}
uint32_t MickView::count(MickSection s)const{const auto k=uint32_t(s);return bytes_&&k>=1&&k<=kMickSectionCount?u32(bytes_+header_bytes+(k-1)*16+8):0;}
const uint8_t*MickView::record(MickSection s,uint32_t i)const{const auto k=uint32_t(s);if(!bytes_||k<1||k>kMickSectionCount||i>=count(s))return nullptr;
    return bytes_+u32(bytes_+header_bytes+(k-1)*16+4)+size_t(i)*strides[k-1];}
std::string_view MickView::string(uint32_t i)const{auto*p=record(MickSection::Strings,i);if(!p)return {};const auto off=u32(p),len=u32(p+4),pool=count(MickSection::Bytes);
    if(!len||off>pool||len>pool-off)return {};return std::string_view(reinterpret_cast<const char*>(record(MickSection::Bytes,0))+off,len);}
MickTexture MickView::texture()const{auto*p=record(MickSection::Texture,0);return p?MickTexture{u32(p),u16(p+4),u16(p+6),u16(p+8),u16(p+10)}:MickTexture{};}
MickActor MickView::actor()const{auto*p=record(MickSection::Actor,0);
    return p?MickActor{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),vec(p+24),vec(p+32),vec(p+40),vec(p+48),u32(p+56),u32(p+60),f32(p+64)}:MickActor{};}
MickCommand MickView::command(uint32_t i)const{auto*p=record(MickSection::Commands,i);return p?MickCommand{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16)}:MickCommand{};}
MickText MickView::text(uint32_t i)const{auto*p=record(MickSection::Texts,i);return p?MickText{u32(p),u32(p+4)}:MickText{};}

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
    if(v.count(S::Bytes)==0||v.count(S::Strings)==0||v.count(S::Strings)>256||v.count(S::Texture)!=1||v.count(S::Actor)!=1||
       v.count(S::Commands)==0||v.count(S::Commands)>64||v.count(S::Texts)==0||v.count(S::Texts)>16)return fail("Mick pack table counts rejected");
    for(uint32_t i=0;i<v.count(S::Strings);++i)if(!text_ok(v.string(i)))return fail("Mick pack string rejected");
    const auto t=v.texture();
    if(!safe_path(v.string(t.path))||!ends_with(v.string(t.path),".t3x")||!t.width||!t.height||!t.columns||!t.rows||
       t.width>1024||t.height>1024||t.width%t.columns||t.height%t.rows)return fail("Mick pack texture rejected");
    const auto a=v.actor();
    if(v.string(a.path).empty()||a.sprite!=0||a.frame>=uint32_t(t.columns)*uint32_t(t.rows)||!finite(a.position)||!finite(a.sprite_position)||
       !finite(a.interact_center)||!finite(a.interact_extents)||a.interact_extents.x<=0||a.interact_extents.y<=0||
       v.string(a.item).empty()||v.string(a.require_flag).empty()||v.string(a.consume_flag).empty()||
       a.require_flag==a.consume_flag||!a.command_count||a.first_command>v.count(S::Commands)||
       a.command_count>v.count(S::Commands)-a.first_command||!finite(a.ray_length)||a.ray_length<=0||a.ray_length>64)
        return fail("Mick pack actor rejected");
    for(uint32_t i=0;i<v.count(S::Texts);++i){const auto tx=v.text(i);if(tx.en>=v.count(S::Strings)||tx.zh>=v.count(S::Strings))return fail("Mick pack text rejected");}
    bool ended=false;
    for(uint32_t pc=0;pc<a.command_count;++pc){const auto c=v.command(a.first_command+pc);
        if(c.d||ended)return fail("Mick pack command rejected");
        switch(MickOpcode(c.opcode)){
        case MickOpcode::ShowText:if(c.a>=v.count(S::Texts)||c.b||c.c)return fail("Mick pack ShowText rejected");break;
        case MickOpcode::AwaitText:if(c.a||c.b||c.c)return fail("Mick pack AwaitText rejected");break;
        case MickOpcode::RemoveKeyItem:if(c.a!=a.item||c.b||c.c)return fail("Mick pack RemoveKeyItem rejected");break;
        case MickOpcode::SetFlag:if(c.a!=a.consume_flag||c.b!=1||c.c)return fail("Mick pack SetFlag rejected");break;
        case MickOpcode::End:if(c.a||c.b||c.c)return fail("Mick pack End rejected");ended=true;break;
        default:return fail("Mick pack opcode rejected");}}
    if(!ended)return fail("Mick pack programme must End");
    error.clear();bytes_.assign(p,p+n);return true;
}
bool MickData::load_file(const char*path,std::string&error){
    std::vector<uint8_t> bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);
}

bool MickRuntime::fail(std::string&e,const char*m){e=m;state_=MickProgramState::Failed;return false;}
bool MickRuntime::initialize(MickView view,MickHost&host,std::string&e){
    if(!view){e="Mick runtime requires a checked pack";return false;}
    const auto a=view.actor();
    if(!host.validate_flag(view.string(a.require_flag),e)||!host.validate_flag(view.string(a.consume_flag),e)||
       !host.validate_item(view.string(a.item),e))return false;
    *this=MickRuntime{};view_=view;host_=&host;ready_=true;e.clear();return true;
}
bool MickRuntime::available(std::string&e)const{
    if(!ready_||!host_){e="Mick runtime is not initialized";return false;}
    const auto a=view_.actor();bool got=false,gave=false;
    if(!host_->flag(view_.string(a.require_flag),got,e)||!host_->flag(view_.string(a.consume_flag),gave,e))return false;
    e.clear();return got&&!gave;
}
std::string_view MickRuntime::localized(const MickText&t,std::string_view code)const{
    return view_.string(chinese(code)?t.zh:t.en);
}
bool MickRuntime::continues()const{
    if(state_!=MickProgramState::WaitingText)return false;
    const auto a=view_.actor();
    return pc_<a.command_count&&view_.command(a.first_command+pc_).opcode!=uint32_t(MickOpcode::End);
}
bool MickRuntime::start(std::string&e){
    if(!ready_||!host_||state_==MickProgramState::WaitingText)return fail(e,"Mick programme start rejected");
    std::string why;if(!available(why))return fail(e,why.empty()?"Mick treats dialogue unavailable":why.c_str());
    pc_=0;state_=MickProgramState::Idle;return step(e);
}
bool MickRuntime::advance_text(std::string&e){
    if(state_!=MickProgramState::WaitingText)return fail(e,"Mick text acknowledgement out of order");
    state_=MickProgramState::Idle;return step(e);
}
bool MickRuntime::step(std::string&e){
    const auto a=view_.actor();
    auto rejected=[&]{state_=MickProgramState::Failed;if(e.empty())e="Mick programme host effect rejected";return false;};
    for(uint32_t guard=0;guard<=a.command_count;++guard){
        if(pc_>=a.command_count)return fail(e,"Mick programme ran past its span");
        const auto c=view_.command(a.first_command+pc_);
        switch(MickOpcode(c.opcode)){
        case MickOpcode::ShowText:{
            const auto body=localized(view_.text(c.a),locale_);
            if(!host_->show_text(body,e))return rejected();++pc_;break;}
        case MickOpcode::AwaitText:++pc_;state_=MickProgramState::WaitingText;e.clear();return true;
        case MickOpcode::RemoveKeyItem:if(!host_->remove_key_item(view_.string(c.a),e))return rejected();++pc_;break;
        case MickOpcode::SetFlag:if(!host_->set_flag(view_.string(c.a),c.b!=0,e))return rejected();++pc_;break;
        case MickOpcode::End:state_=MickProgramState::Complete;e.clear();return true;
        default:return fail(e,"Mick programme opcode rejected");}
    }
    return fail(e,"Mick programme did not reach a gate");
}
}
