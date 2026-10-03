#include "encore/music_regions.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=3)return 2;
 MusicRegionData data;MusicRegionController c;MusicRegionContext ctx;std::string e;
 auto require=[&](bool okay){if(!okay){std::cerr<<e<<'\n';std::exit(1);}};
 require(data.load_file(argv[1],e));require(c.initialize(data,8,e));require(c.attach_scene(1,e));ctx.flag=[](std::string_view){return false;};
 std::ofstream out(argv[2]);if(!out)return 2;out<<"[";bool first=true;
 auto snapshot=[&](const char*label){if(!first)out<<',';first=false;out<<"{\"label\":\""<<label<<"\",\"voices\":[";std::vector<MusicRegionVoice>voices;for(const auto&v:c.voices())if(v.allocated)voices.push_back(v);std::sort(voices.begin(),voices.end(),[](auto&a,auto&b){return a.order<b.order;});bool fv=true;for(auto&v:voices){if(!fv)out<<',';fv=false;std::string path;for(auto&t:data.tracks())if(t.id==v.track_id)path=t.source_path;out<<"{\"track\":\""<<path<<"\",\"playing\":"<<(v.playing?"true":"false")<<",\"volume\":"<<v.gain_db<<'}';}out<<"],\"areas\":[";bool fa=true;for(size_t i=0;i<c.regions().size();++i)if(c.regions()[i].registered){if(!fa)out<<',';fa=false;out<<'\"'<<data.regions()[i].source_path.substr(6)<<'\"';}out<<"]}";};
 auto enter=[&](const char*p){require(c.enter(1,p,ctx,e));};auto leave=[&](const char*p){require(c.exit(1,p,ctx,e));};auto idle=[&](){require(c.idle_frame(1,e));};
 enter("Music/MusicArea");snapshot("first_immediate");
 enter("Music/MusicArea3");snapshot("same_song_reuse");
 leave("Music/MusicArea3");enter("Music/MusicArea3");idle();snapshot("same_idle_reentry");
 leave("Music/MusicArea");idle();snapshot("shared_exit");
 ctx.in_cutscene=true;leave("Music/MusicArea3");idle();snapshot("cutscene_exit_ignored");ctx.in_cutscene=false;
 ctx.in_battle=true;enter("Music/MusicArea6");snapshot("battle_enter_ignored");ctx.in_battle=false;
 enter("Music/MusicArea2");snapshot("false_flag_ignored");
 enter("Music/MusicArea6");snapshot("different_song_crossfade");
 leave("Music/MusicArea3");leave("Music/MusicArea6");idle();enter("Music/MusicArea");snapshot("rapid_duplicate_A");
 out<<"]\n";return 0;
}
