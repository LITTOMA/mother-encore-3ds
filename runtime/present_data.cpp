#include "encore/present_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <set>

namespace encore::upstream {
namespace {
constexpr size_t header_bytes=64,max_bytes=64u*1024u;
constexpr uint32_t strides[kPresentSectionCount]={1,8,12,24,8,8,8,4,28,52,12,20,8,8};
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
bool printable(std::string_view s){if(s.empty())return false;for(unsigned char c:s)if(c<32||c>126)return false;return true;}
bool ends_with(std::string_view s,std::string_view x){return s.size()>x.size()&&s.compare(s.size()-x.size(),x.size(),x)==0;}
}
uint32_t PresentView::count(PresentSection s)const{const auto k=uint32_t(s);return bytes_&&k>=1&&k<=kPresentSectionCount?u32(bytes_+header_bytes+(k-1)*16+8):0;}
const uint8_t*PresentView::record(PresentSection s,uint32_t i)const{const auto k=uint32_t(s);if(!bytes_||k<1||k>kPresentSectionCount||i>=count(s))return nullptr;
    return bytes_+u32(bytes_+header_bytes+(k-1)*16+4)+size_t(i)*strides[k-1];}
std::string_view PresentView::string(uint32_t i)const{auto*p=record(PresentSection::Strings,i);if(!p)return {};const auto off=u32(p),len=u32(p+4),pool=count(PresentSection::Bytes);
    if(!len||off>pool||len>pool-off)return {};return std::string_view(reinterpret_cast<const char*>(record(PresentSection::Bytes,0))+off,len);}
PresentTexture PresentView::texture(uint32_t i)const{auto*p=record(PresentSection::Textures,i);return p?PresentTexture{u32(p),u16(p+4),u16(p+6),u16(p+8),u16(p+10)}:PresentTexture{};}
PresentClip PresentView::clip(uint32_t i)const{auto*p=record(PresentSection::Clips,i);return p?PresentClip{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),f32(p+20)}:PresentClip{};}
PresentFrameKey PresentView::frame_key(uint32_t i)const{auto*p=record(PresentSection::FrameKeys,i);return p?PresentFrameKey{f32(p),u32(p+4)}:PresentFrameKey{};}
PresentAudioKey PresentView::audio_key(uint32_t i)const{auto*p=record(PresentSection::AudioKeys,i);return p?PresentAudioKey{f32(p),u32(p+4)}:PresentAudioKey{};}
PresentRegion PresentView::region(uint32_t i)const{auto*p=record(PresentSection::SparkleFrames,i);return p?PresentRegion{u16(p),u16(p+2),u16(p+4),u16(p+6)}:PresentRegion{};}
uint32_t PresentView::order(uint32_t i)const{auto*p=record(PresentSection::SparkleOrder,i);return p?u32(p):kPresentNone;}
PresentSparkles PresentView::sparkles()const{auto*p=record(PresentSection::Sparkles,0);return p?PresentSparkles{u32(p),u32(p+4),u32(p+8),f32(p+12),vec(p+16),f32(p+24)}:PresentSparkles{};}
PresentObject PresentView::object(uint32_t i)const{auto*p=record(PresentSection::Objects,i);
    return p?PresentObject{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),vec(p+20),vec(p+28),vec(p+36),u32(p+44),u32(p+48)}:PresentObject{};}
PresentTemplate PresentView::item_template(uint32_t i)const{auto*p=record(PresentSection::Templates,i);return p?PresentTemplate{u32(p),u32(p+4),u32(p+8)}:PresentTemplate{};}
PresentCommand PresentView::command(uint32_t i)const{auto*p=record(PresentSection::Commands,i);return p?PresentCommand{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16)}:PresentCommand{};}
PresentProgram PresentView::program(uint32_t i)const{auto*p=record(PresentSection::Programs,i);return p?PresentProgram{u32(p),u32(p+4)}:PresentProgram{};}
PresentText PresentView::text(uint32_t i)const{auto*p=record(PresentSection::Texts,i);return p?PresentText{u32(p),u32(p+4)}:PresentText{};}

bool PresentData::load(const uint8_t*p,size_t n,std::string&error){
    auto fail=[&](const char*m){error=m;return false;};
    if(!p||n<header_bytes+kPresentSectionCount*16||n>max_bytes)return fail("Present pack size rejected");
    if(std::memcmp(p,"ENCPRS01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)!=1||u32(p+28)||u32(p+52)!=kPresentSectionCount||u32(p+56)||u32(p+60))
        return fail("Present pack schema/capability/scene/reserved rejected");
    for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("Present pack source pin rejected");}
    if(encore::crc32(p+32,n-32)!=u32(p+16))return fail("Present pack checksum rejected");
    size_t cursor=header_bytes+kPresentSectionCount*16;
    for(uint32_t i=0;i<kPresentSectionCount;++i){const auto*d=p+header_bytes+i*16;const auto off=u32(d+4),count=u32(d+8);
        if(u32(d)!=i+1||u32(d+12)!=strides[i])return fail("Present pack directory rejected");
        if(!count){if(off)return fail("Present pack empty section rejected");continue;}
        if(off%4||off<cursor||off>n||count>(n-off)/strides[i])return fail("Present pack section span rejected");
        for(size_t j=cursor;j<off;++j)if(p[j])return fail("Present pack padding rejected");
        cursor=size_t(off)+size_t(count)*strides[i];}
    if(cursor!=n)return fail("Present pack trailing bytes rejected");
    PresentView v;v.bytes_=p;v.size_=n;
    using S=PresentSection;
    const auto strings=v.count(S::Strings),textures=v.count(S::Textures),clips=v.count(S::Clips),fkeys=v.count(S::FrameKeys),akeys=v.count(S::AudioKeys);
    const auto regions=v.count(S::SparkleFrames),orders=v.count(S::SparkleOrder),objects=v.count(S::Objects),templates=v.count(S::Templates);
    const auto commands=v.count(S::Commands),programs=v.count(S::Programs),texts=v.count(S::Texts);
    if(!v.count(S::Bytes)||!strings||strings>1024||textures!=4||clips!=2||v.count(S::Sparkles)!=1||!objects||objects>32||programs>objects||commands>256||
       templates>16||texts>64||!regions||regions>64||!orders||orders>256)return fail("Present pack table counts rejected");
    for(uint32_t i=0;i<strings;++i)if(!printable(v.string(i)))return fail("Present pack string rejected");
    auto str=[&](uint32_t i){return i<strings;};
    for(uint32_t i=0;i<textures;++i){const auto t=v.texture(i);
        if(!str(t.path)||!safe_path(v.string(t.path))||!ends_with(v.string(t.path),".t3x")||!t.width||!t.height||!t.columns||!t.rows||t.width>1024||t.height>1024||t.width%t.columns||t.height%t.rows)
            return fail("Present pack texture rejected");}
    const auto sp=v.sparkles();
    if(sp.texture>=textures||v.texture(sp.texture).columns!=1||v.texture(sp.texture).rows!=1||sp.first_order!=0||sp.order_count!=orders||!finite(sp.fps)||sp.fps<=0||sp.fps>120||!finite(sp.offset)||!finite(sp.initial_range)||sp.initial_range<1||sp.initial_range>4096)
        return fail("Present pack sparkles rejected");
    const auto sheet=v.texture(sp.texture);
    for(uint32_t i=0;i<regions;++i){const auto r=v.region(i);if(!r.w||!r.h||r.x+r.w>sheet.width||r.y+r.h>sheet.height)return fail("Present pack sparkle region rejected");}
    for(uint32_t i=0;i<orders;++i)if(v.order(i)>=regions)return fail("Present pack sparkle order rejected");
    uint32_t roles=0;uint32_t box_frames=kPresentNone;
    for(uint32_t i=0;i<textures;++i)if(i!=sp.texture){const auto t=v.texture(i);if(t.rows!=1)return fail("Present pack box sheet rejected");if(box_frames==kPresentNone)box_frames=t.columns;else if(box_frames!=t.columns)return fail("Present pack box frame counts disagree");}
    for(uint32_t i=0;i<clips;++i){const auto c=v.clip(i);
        if((c.role!=1&&c.role!=2)||(roles&(1u<<c.role))||!finite(c.length)||c.length<=0||c.length>60||!c.frame_key_count||c.first_frame_key>fkeys||c.frame_key_count>fkeys-c.first_frame_key||c.first_audio_key>akeys||c.audio_key_count>akeys-c.first_audio_key)
            return fail("Present pack clip rejected");
        roles|=1u<<c.role;float last=-1;
        for(uint32_t k=0;k<c.frame_key_count;++k){const auto key=v.frame_key(c.first_frame_key+k);if(!finite(key.time)||key.time<0||key.time>c.length||key.time<last||key.frame>=box_frames)return fail("Present pack frame key rejected");last=key.time;}
        last=-1;for(uint32_t k=0;k<c.audio_key_count;++k){const auto key=v.audio_key(c.first_audio_key+k);if(!finite(key.time)||key.time<0||key.time>c.length||key.time<last||key.playing>1)return fail("Present pack audio key rejected");last=key.time;}}
    for(uint32_t i=0;i<templates;++i){const auto t=v.item_template(i);if(!str(t.item)||v.string(t.item).find('/')!=std::string_view::npos||!t.doses||t.doses>65535||t.key_item>1)return fail("Present pack item template rejected");}
    std::set<uint32_t>ids;
    for(uint32_t i=0;i<texts;++i){const auto t=v.text(i);if(!t.dialogue_id||!ids.insert(t.dialogue_id).second||!str(t.source_path)||!safe_path(v.string(t.source_path))||!ends_with(v.string(t.source_path),".yaml"))return fail("Present pack text rejected");}
    std::vector<uint32_t>program_owner(programs,kPresentNone);std::set<std::string_view>paths;
    for(uint32_t i=0;i<objects;++i){const auto o=v.object(i);
        if(!str(o.path)||!paths.insert(v.string(o.path)).second||o.texture>=textures||o.texture==sp.texture||(o.policy!=1&&o.policy!=2)||!str(o.flag)||(o.item!=kPresentNone&&!str(o.item))||
           !finite(o.position)||!finite(o.interact_center)||!finite(o.interact_extents)||o.interact_extents.x<=0||o.interact_extents.y<=0||!str(o.sound)||!safe_path(v.string(o.sound))||v.string(o.sound).rfind("Audio/",0)!=0)
            return fail("Present pack object rejected");
        if(o.program!=kPresentNone){if(o.program>=programs||program_owner[o.program]!=kPresentNone||o.policy!=uint32_t(PresentPolicy::Flag)||o.item==kPresentNone)return fail("Present pack object programme binding rejected");program_owner[o.program]=i;}}
    for(uint32_t i=0;i<programs;++i){const auto pr=v.program(i);if(program_owner[i]==kPresentNone||!pr.count||pr.first>commands||pr.count>commands-pr.first)return fail("Present pack programme span rejected");
        // Every command must terminate and keep ShowText/AwaitText discipline on all paths.
        std::vector<std::vector<uint32_t>>edges(pr.count);
        for(uint32_t pc=0;pc<pr.count;++pc){const auto c=v.command(pr.first+pc);auto&e=edges[pc];
            if(c.d)return fail("Present pack command reserved operand rejected");
            switch(PresentOpcode(c.opcode)){
            case PresentOpcode::ShowText:if(c.a>=texts||c.b||c.c)return fail("Present pack text command rejected");break;
            case PresentOpcode::AwaitText:case PresentOpcode::End:if(c.a||c.b||c.c)return fail("Present pack gate/end command rejected");break;
            case PresentOpcode::BranchFlag:if(!str(c.a)||c.b>1||c.c>=pr.count)return fail("Present pack flag branch rejected");e.push_back(c.c);break;
            case PresentOpcode::GrantItem:if(c.a>=templates||c.b||c.c)return fail("Present pack grant rejected");break;
            case PresentOpcode::PlaySound:if(!str(c.a)||!safe_path(v.string(c.a))||v.string(c.a).rfind("Audio/",0)!=0||c.b||c.c)return fail("Present pack sound command rejected");break;
            case PresentOpcode::SetFlag:if(!str(c.a)||c.b>1||c.c)return fail("Present pack flag write rejected");break;
            case PresentOpcode::Jump:if(c.a>=pr.count||c.b||c.c)return fail("Present pack jump rejected");e.push_back(c.a);break;
            case PresentOpcode::PlayClip:if(c.a>=clips||c.b||c.c)return fail("Present pack clip command rejected");break;
            default:return fail("Present pack unknown opcode rejected");}
            if(c.opcode!=uint32_t(PresentOpcode::Jump)&&c.opcode!=uint32_t(PresentOpcode::End)){if(pc+1>=pr.count)return fail("Present pack programme falls through");e.push_back(pc+1);}}
        std::vector<uint32_t>degree(pr.count);for(auto&e:edges)for(auto t:e)++degree[t];
        std::vector<uint32_t>ready;for(uint32_t pc=0;pc<pr.count;++pc)if(!degree[pc])ready.push_back(pc);uint32_t processed=0;
        while(!ready.empty()){const auto pc=ready.back();ready.pop_back();++processed;for(auto t:edges[pc])if(!--degree[t])ready.push_back(t);}
        if(processed!=pr.count)return fail("Present pack cyclic programme rejected");
        std::set<std::pair<uint32_t,bool>>seen;std::vector<std::pair<uint32_t,bool>>queue{{0,false}};
        while(!queue.empty()){auto[pc,pending]=queue.back();queue.pop_back();if(!seen.insert({pc,pending}).second)continue;const auto op=PresentOpcode(v.command(pr.first+pc).opcode);
            if(op==PresentOpcode::ShowText){if(pending)return fail("Present pack unacknowledged text replaced");pending=true;}
            else if(op==PresentOpcode::AwaitText){if(!pending)return fail("Present pack gate without text");pending=false;}
            else if(op==PresentOpcode::End&&pending)return fail("Present pack unacknowledged termination");
            for(auto t:edges[pc])queue.push_back({t,pending});}
        bool ends=false;for(uint32_t pc=0;pc<pr.count;++pc)ends|=v.command(pr.first+pc).opcode==uint32_t(PresentOpcode::End);if(!ends)return fail("Present pack programme lacks End");
        if(v.command(pr.first).opcode!=uint32_t(PresentOpcode::BranchFlag)||v.string(v.command(pr.first).a)!=v.string(v.object(program_owner[i]).flag))return fail("Present pack programme must test its own flag first");}
    bytes_.assign(p,p+n);error.clear();return true;
}
bool PresentData::load_file(const char*path,std::string&error){
    if(!path){error="Missing present pack path";return false;}
    std::vector<uint8_t> bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);
}
}
