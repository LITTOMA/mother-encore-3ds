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
    auto truncated=bytes;truncated.pop_back();MickData bad;CHECK(!bad.load(truncated.data(),truncated.size(),error));
    auto magic=bytes;magic[7]='0';CHECK(!bad.load(magic.data(),magic.size(),error));
    auto stride=bytes;stride[64+3*16+12]=0;CHECK(!bad.load(stride.data(),stride.size(),error));
    return fails?1:0;
}
