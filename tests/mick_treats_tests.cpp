#include "encore/crc32.hpp"
#include "encore/mick_treats.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using namespace encore::upstream;
static int fails=0;
#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"CHECK failed: %s\n",#c);++fails;}}while(0)

static std::vector<uint8_t> read_all(const char* path){
    std::ifstream in(path,std::ios::binary);CHECK(in);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});return bytes;
}

int main(int argc,char** argv){
    if(argc<2){std::fprintf(stderr,"usage: mick_treats_tests <podunk.encmick>\n");return 2;}
    const auto bytes=read_all(argv[1]);std::string error;
    MickData data;CHECK(data.load(bytes.data(),bytes.size(),error));
    const auto view=data.view();CHECK(view);CHECK(view.count(MickSection::Actor)==1);CHECK(view.count(MickSection::Texture)==1);
    const auto a=view.actor();CHECK(view.string(a.item)=="DogTreats");CHECK(view.string(a.require_flag)=="got_dog_treats");
    CHECK(view.string(a.consume_flag)=="gave_treats");CHECK(a.frame==1);CHECK(a.command_count>=1);
    CHECK(a.collision_extents.x==7.5f&&a.collision_extents.y==3.f);CHECK(a.sort_y==8.f);
    CHECK(view.string(a.bark_path)=="Cutscenes/Cutscene Area11");
    CHECK(a.bark_center.x==-24.f&&a.bark_center.y==64.f&&a.bark_extents.x==56.f&&a.bark_extents.y==8.f);
    CHECK(view.count(MickSection::Programmes)>=6);CHECK(view.count(MickSection::Clips)>=28);CHECK(view.count(MickSection::Rng)>=1);
    CHECK(view.programme(a.bark_programme).kind==uint32_t(MickProgramKind::Bark));
    bool named=false,blank=false;
    for(uint32_t i=0;i<view.count(MickSection::Commands);++i){const auto c=view.command(i);if(c.opcode!=uint32_t(MickOpcode::ShowText))continue;
        if(c.b==kMickNone)blank=true;else{CHECK(c.b<view.count(MickSection::Texts));const auto speaker=view.text(c.b);CHECK(!view.string(speaker.en).empty()&&!view.string(speaker.zh).empty());named=true;}}
    CHECK(named&&blank);
    struct Host:MickHost{
        std::string speaker,body;bool scratch=false,thought_done=false,reject_grant=false,reject_item=false;unsigned grants=0;
        bool validate_flag(std::string_view,std::string& e)override{e.clear();return true;}
        bool validate_item(std::string_view name,std::string& e)override{if(reject_item&&name=="KeyBasement"){e="rejected key source";return false;}e.clear();return true;}
        bool flag(std::string_view name,bool& on,std::string& e)override{on=name=="mick_scratch"?scratch:name=="mick_telepathy"?thought_done:false;e.clear();return true;}
        bool set_flag(std::string_view name,bool value,std::string& e)override{if(name=="mick_telepathy")thought_done=value;e.clear();return true;}
        bool grant_key_item(std::string_view name,uint32_t doses,std::string& e)override{if(reject_grant||name!="KeyBasement"||doses!=1){e="key grant rejected";return false;}++grants;e.clear();return true;}
        bool remove_key_item(std::string_view,std::string& e)override{e.clear();return true;}
        bool show_text(std::string_view s,std::string_view b,std::string& e)override{speaker=std::string(s);body=std::string(b);e.clear();return true;}
        bool play_sound(std::string_view,std::string& e)override{e.clear();return true;}
    } host;
    MickRuntime chinese;chinese.set_locale("zh_Hans_CN");
    CHECK(chinese.initialize(view,host,error));
    CHECK(chinese.start_bark(error)&&host.speaker=="米克"&&host.body=="汪汪！");
    CHECK(chinese.advance_text(error)&&host.speaker.empty()&&host.body=="看样子米克有事情和你说。");
    MickRuntime english;CHECK(english.initialize(view,host,error));
    CHECK(english.start_bark(error)&&host.body=="Woof woof!");
    std::string tags="（而且，按住[ui_toggle]后再按下[ui_accept]来使用这个能力的念头更是想都别想。）";
    CHECK(resolve_dialogue_tags(tags,"宁宁","A","B","+",error)&&tags=="（而且，按住B后再按下A来使用这个能力的念头更是想都别想。）");
    std::string select_line="按[ui_select]并让[Ninten]选择";
    CHECK(resolve_dialogue_tags(select_line,"宁宁","A","B","+",error)&&select_line=="按+并让宁宁选择");
    std::string unknown="[ui_cancel]";CHECK(!resolve_dialogue_tags(unknown,"宁宁","A","B","+",error));
    MickRuntime thoughts;CHECK(thoughts.initialize(view,host,error));CHECK(thoughts.start_thought(true,error));const auto food=host.body;CHECK(thoughts.advance_text(error)&&thoughts.state()==MickProgramState::Complete&&host.grants==0);
    host.scratch=true;CHECK(thoughts.start_thought(true,error));CHECK(host.body!=food&&host.grants==0);CHECK(thoughts.advance_text(error));CHECK(host.grants==1&&!host.thought_done&&host.speaker.empty());CHECK(thoughts.advance_text(error));CHECK(thoughts.advance_text(error)&&host.thought_done);CHECK(thoughts.advance_text(error)&&thoughts.state()==MickProgramState::Complete);CHECK(thoughts.start_thought(true,error)&&host.body==food);CHECK(thoughts.advance_text(error)&&host.grants==1);
    CHECK(thoughts.start_thought(false,error)&&host.speaker.empty());CHECK(!thoughts.start_thought(true,error)); // Active dialogue cannot be restarted.
    host.thought_done=false;host.reject_grant=true;MickRuntime failed;CHECK(failed.initialize(view,host,error)&&failed.start_thought(true,error));CHECK(!failed.advance_text(error)&&failed.state()==MickProgramState::Failed&&!host.thought_done&&host.grants==1);
    host.reject_item=true;MickRuntime unknown_key;CHECK(!unknown_key.initialize(view,host,error));host.reject_item=host.reject_grant=false;
    MickData bad;
    auto speaker=bytes;uint32_t commands=0;bool patched=false;
    for(uint32_t i=0;i<9;++i){const uint8_t* d=speaker.data()+64+i*16;const uint32_t kind=uint32_t(d[0])|uint32_t(d[1])<<8|uint32_t(d[2])<<16|uint32_t(d[3])<<24;
        const uint32_t off=uint32_t(d[4])|uint32_t(d[5])<<8|uint32_t(d[6])<<16|uint32_t(d[7])<<24;
        const uint32_t count=uint32_t(d[8])|uint32_t(d[9])<<8|uint32_t(d[10])<<16|uint32_t(d[11])<<24;if(kind!=6)continue;commands=count;
        for(uint32_t n=0;n<count&&!patched;++n){uint8_t* row=speaker.data()+off+n*20;const uint32_t op=uint32_t(row[0])|uint32_t(row[1])<<8|uint32_t(row[2])<<16|uint32_t(row[3])<<24;if(op!=1)continue;row[8]=0xfe;row[9]=row[10]=row[11]=0xff;patched=true;}}
    CHECK(patched&&commands);
    const uint32_t crc=encore::crc32(speaker.data()+32,speaker.size()-32);speaker[16]=uint8_t(crc);speaker[17]=uint8_t(crc>>8);speaker[18]=uint8_t(crc>>16);speaker[19]=uint8_t(crc>>24);
    CHECK(!bad.load(speaker.data(),speaker.size(),error));
    auto truncated=bytes;truncated.pop_back();CHECK(!bad.load(truncated.data(),truncated.size(),error));
    auto magic=bytes;magic[7]='0';CHECK(!bad.load(magic.data(),magic.size(),error));
    auto stride=bytes;stride[64+3*16+12]=0;CHECK(!bad.load(stride.data(),stride.size(),error));
    auto get=[](const std::vector<uint8_t>&b,size_t n){return uint32_t(b[n])|uint32_t(b[n+1])<<8|uint32_t(b[n+2])<<16|uint32_t(b[n+3])<<24;};
    auto mutate=[&](size_t n,uint32_t value){auto b=bytes;for(unsigned k=0;k<4;++k)b[n+k]=uint8_t(value>>(k*8));const auto hash=encore::crc32(b.data()+32,b.size()-32);for(unsigned k=0;k<4;++k)b[16+k]=uint8_t(hash>>(k*8));CHECK(!data.load(b.data(),b.size(),error));CHECK(data.view().count(MickSection::Commands)==view.count(MickSection::Commands));};
    mutate(20,1);mutate(20,3);const auto command_rows=get(bytes,64+5*16+4),programme_rows=get(bytes,64+4*16+4);
    for(uint32_t i=0;i<view.count(MickSection::Commands);++i)if(view.command(i).opcode==uint32_t(MickOpcode::GrantKeyItem)){mutate(command_rows+i*20,99);mutate(command_rows+i*20+4,UINT32_MAX);mutate(command_rows+i*20+8,0);mutate(command_rows+i*20+12,1);}
    mutate(programme_rows+12,5);
    return fails?1:0;
}
