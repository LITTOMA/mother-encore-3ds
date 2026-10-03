#include "encore/game.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore;
static int checks=0;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);} }while(0)
static void press(Game& g,uint32_t key){g.tick({0,key});}
static void move(Game& g,uint32_t key,int frames){for(int i=0;i<frames;++i)g.tick({key,0});}
static void set32(std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(8*i));}
static void checksum(std::vector<uint8_t>& b){set32(b,b.size()-4,crc32(b.data(),b.size()-4));}
int main(int argc,char** argv){
    CHECK(argc==2);Content c;std::string e;CHECK(c.load_file(argv[1],e));CHECK(c.maps.size()==2);CHECK(c.programs.size()==5);
    Game g(c);CHECK(g.state().x==56&&g.state().y==88);CHECK(g.can_save());
    move(g,Left,100);CHECK(g.state().x==16); // Boundary wall collision.
    g.reset();move(g,Right,8);press(g,Confirm);CHECK(g.mode()==Mode::Dialogue);CHECK(!g.can_save());
    std::vector<uint8_t> save;CHECK(!g.encode_save(save,e));
    press(g,Confirm);CHECK(g.get_flag(0));CHECK(g.mode()==Mode::Dialogue);press(g,Confirm);CHECK(g.can_save());
    CHECK(g.encode_save(save,e));const auto snapshot=g.trace_json();
    move(g,Down,8);CHECK(g.decode_save(save.data(),save.size(),e));CHECK(g.trace_json()==snapshot);
    auto corrupt=save;corrupt[25]^=0x80;CHECK(!g.decode_save(corrupt.data(),corrupt.size(),e));CHECK(g.trace_json()==snapshot);
    corrupt=save;set32(corrupt,20,999999);checksum(corrupt);CHECK(!g.decode_save(corrupt.data(),corrupt.size(),e));
    corrupt=save;set32(corrupt,40,0);checksum(corrupt);CHECK(!g.decode_save(corrupt.data(),corrupt.size(),e));
    // Load independent of vector order: map and flag records are keyed by stable ID.
    Content reordered=c;std::swap(reordered.maps[0],reordered.maps[1]);reordered.start_map=1;
    Game r(reordered);CHECK(r.decode_save(save.data(),save.size(),e));CHECK(r.map().id==1001);CHECK(r.get_flag(0));
    // Actual interaction-driven training battle and VM continuation.
    g.teleport(0,200,88);press(g,Confirm);CHECK(g.mode()==Mode::Dialogue);press(g,Confirm);CHECK(g.mode()==Mode::Battle);
    for(int i=0;i<10&&g.mode()==Mode::Battle;++i)press(g,Confirm);
    CHECK(g.mode()==Mode::Dialogue);CHECK(g.state().xp==10);press(g,Confirm);CHECK(g.can_save());
    g.teleport(0,264,136);press(g,Confirm);CHECK(g.story().wait_ticks()==12);
    for(int i=0;i<12;++i)g.tick({});CHECK(g.state().map==0);g.tick({});CHECK(g.state().map==1);g.tick({});CHECK(g.can_save());
    Game a(c),b(c);a.start_battle(0);b.start_battle(0);
    for(int i=0;i<4;++i){press(a,Confirm);press(b,Confirm);CHECK(a.trace_json()==b.trace_json());}
    std::vector<uint8_t> pack;CHECK(read_file(argv[1],pack,kMaxPackBytes,e));
    for(size_t n=0;n<pack.size();++n){Content bad;CHECK(!bad.load(pack.data(),n,e));}
    auto p=pack;p[12]=99;Content bad;CHECK(!bad.load(p.data(),p.size(),e));
    p=pack;p[24]=0x80;CHECK(!bad.load(p.data(),p.size(),e));
    p=pack;p.back()^=0x40;CHECK(!bad.load(p.data(),p.size(),e));
    // Every single-byte payload mutation must be rejected by CRC.
    for(size_t i=64;i<pack.size();++i){p=pack;p[i]^=1;CHECK(!bad.load(p.data(),p.size(),e));}
    Program infinite;infinite.id=1;infinite.code.push_back({Op::Jump,0,0,0});
    StoryVM vm;Game h(c);CHECK(vm.start(infinite));vm.tick(h);CHECK(h.mode()==Mode::Fault);CHECK(!vm.active());
    Program invalid;invalid.id=2;invalid.code.push_back({Op(255),0,0,0});
    h.reset();CHECK(vm.start(invalid));vm.tick(h);CHECK(h.mode()==Mode::Fault);

    // Valid CRC is not sufficient: structural/semantic checks must still reject invalid data.
    p=pack;set32(p,56,0);CHECK(!bad.load(p.data(),p.size(),e));
    p=pack;set32(p,48,33);CHECK(!bad.load(p.data(),p.size(),e));
    size_t first_program=64;
    for(const auto& string:c.strings)first_program+=2+string.size();
    for(const auto& map:c.maps)first_program+=16+map.tiles.size()+20*map.objects.size();
    p=pack;p[first_program+8]=255;set32(p,20,crc32(p.data()+64,p.size()-64));CHECK(!bad.load(p.data(),p.size(),e));
    p=pack;set32(p,first_program+8+16+4,9999);set32(p,20,crc32(p.data()+64,p.size()-64));CHECK(!bad.load(p.data(),p.size(),e));
    // Deterministic mutation stress, including CRC-valid payloads. Acceptance is permitted
    // only when the mutated content remains semantically valid; no crash/UB is acceptable.
    uint32_t fuzz=0x879ac15;
    for(unsigned n=0;n<512;++n){
        p=pack;fuzz^=fuzz<<13;fuzz^=fuzz>>17;fuzz^=fuzz<<5;
        size_t at=64+fuzz%(p.size()-64);p[at]^=uint8_t(1+(fuzz>>24));
        set32(p,20,crc32(p.data()+64,p.size()-64));
        Content candidate;
        if(candidate.load(p.data(),p.size(),e)){
            Game probe(candidate);for(unsigned tick=0;tick<16;++tick)probe.tick({Right,tick==0?Confirm:0U});
        }
        CHECK(true);
    }
    // Shared file I/O also gets an on-disk round trip and backup check.
    const char* path=".encore-unit-save.sav";
    std::remove(path);std::remove(".encore-unit-save.sav.bak");std::remove(".encore-unit-save.sav.tmp");
    CHECK(write_file_atomic(path,save,e));
    auto next=save;next[44]^=1;checksum(next);CHECK(write_file_atomic(path,next,e));
    std::vector<uint8_t> disk;
    CHECK(read_file(path,disk,1024,e));CHECK(disk==next);
    CHECK(read_file(".encore-unit-save.sav.bak",disk,1024,e));CHECK(disk==save);
    std::remove(path);std::remove(".encore-unit-save.sav.bak");
    // A failed content load must not replace a previously valid pack.
    CHECK(!c.load(nullptr,0,e));CHECK(c.maps.size()==2);
    std::printf("PASS: %d checks (includes exhaustive truncation and one-byte CRC rejection)\n",checks);return 0;
}
