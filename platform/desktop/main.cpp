#include "encore/game.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
using namespace encore;
static void show(const Game& g){
    std::cout<<"\nENCORE NATIVE / ENGINE SANDBOX (not game content)\n";
    const auto& m=g.map();std::cout<<g.content().text(m.title)<<"\n";
    for(int y=0;y<m.height;++y){for(int x=0;x<m.width;++x){char c=m.tiles[size_t(y)*m.width+x]? '#':'.';
        for(const auto& o:m.objects)if(o.x/16==x&&o.y/16==y)c="GDTS"[unsigned(o.kind)];
        if(g.state().x/16==x&&g.state().y/16==y)c='@';std::cout<<c;}std::cout<<'\n';}
    std::cout<<"HP "<<g.state().hp<<"/30  XP "<<g.state().xp<<"  tick "<<g.state().tick<<"\n";
    if(g.mode()==Mode::Dialogue)std::cout<<"DIALOGUE: "<<g.dialogue()<<"\n";
    if(g.mode()==Mode::Battle)std::cout<<"BATTLE: "<<g.content().text(g.content().enemies[g.battle().enemy].name)<<" HP "<<g.battle().hp<<"\n"<<g.battle().message<<"\n";
    if(g.mode()==Mode::Fault)std::cout<<"FAULT: "<<g.error()<<"\n";
    std::cout<<"wasd move | e interact/attack | b close/flee | k save | l load | . wait | q quit\n";
}
static void command(Game& g,char c){
    Input i{};int ticks=1;
    switch(c){case 'w':i.held=Up;ticks=8;break;case 's':i.held=Down;ticks=8;break;case 'a':i.held=Left;ticks=8;break;case 'd':i.held=Right;ticks=8;break;case 'e':i.pressed=Confirm;break;case 'b':i.pressed=Cancel;break;default:break;}
    for(int n=0;n<ticks;++n){g.tick(i);i.pressed=0;}
}
int main(int argc,char** argv){
    std::string pack="romfs/sandbox.encpak",trace,script,save="encore-sandbox.sav";bool smoke=false;
    for(int i=1;i<argc;++i){std::string a=argv[i];
        if((a=="--pack"||a=="--trace"||a=="--script"||a=="--save")&&i+1<argc){auto v=std::string(argv[++i]);if(a=="--pack")pack=v;else if(a=="--trace")trace=v;else if(a=="--save")save=v;else script=v;}
        else if(a=="--smoke")smoke=true;
        else {std::cerr<<"Usage: encore_host [--pack file] [--script keys] [--trace file] [--save file] [--smoke]\n";return 2;}}
    Content content;std::string error;if(!content.load_file(pack.c_str(),error)){std::cerr<<error<<"\n";return 1;}
    Game game(content);std::ofstream log;if(!trace.empty()){log.open(trace);if(!log){std::cerr<<"Cannot create trace\n";return 1;}}
    if(smoke)script="deee..dddddddddeeeeee..";
    auto emit=[&](){if(log)log<<game.trace_json()<<'\n';};emit();
    if(!script.empty()){
        for(char c:script){command(game,c);emit();if(game.mode()==Mode::Fault){std::cerr<<game.error()<<'\n';return 1;}}
        if(smoke&&(!game.can_save()||!game.get_flag(0)||game.state().xp!=10)){std::cerr<<"Smoke fixture did not complete its gameplay loop\n";return 1;}
        std::cout<<game.trace_json()<<'\n';return 0;
    }
    show(game);std::string line;
    while(std::getline(std::cin,line)){
        for(char c:line){if(c=='q')return 0;
            if(c=='k'){std::vector<uint8_t>b;if(game.encode_save(b,error)&&write_file_atomic(save.c_str(),b,error))std::cout<<"Saved.\n";else std::cout<<error<<'\n';}
            else if(c=='l'){std::vector<uint8_t>b;if(read_file(save.c_str(),b,1024,error)&&game.decode_save(b.data(),b.size(),error))std::cout<<"Loaded.\n";else std::cout<<error<<'\n';}
            else command(game,c);emit();
        }show(game);
    }return 0;
}
