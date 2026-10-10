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
        std::string speaker,body;
        bool validate_flag(std::string_view,std::string& e)override{e.clear();return true;}
        bool validate_item(std::string_view,std::string& e)override{e.clear();return true;}
        bool flag(std::string_view,bool& on,std::string& e)override{on=false;e.clear();return true;}
        bool set_flag(std::string_view,bool,std::string& e)override{e.clear();return true;}
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
    return fails?1:0;
}
