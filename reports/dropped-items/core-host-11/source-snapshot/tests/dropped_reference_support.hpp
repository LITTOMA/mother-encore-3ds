#pragma once
#include "encore/dropped_item_actor.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>

using namespace encore::upstream;
namespace {
size_t checks=0;std::string current;int frame=-1;
void check(bool ok,int line){++checks;if(!ok){std::cerr<<"Dropped source failure "<<current<<":"<<frame<<" at "<<line<<"\n";std::exit(1);}}
#define CHECK(value) check(bool(value),__LINE__)
uint32_t bits(float v){uint32_t result;std::memcpy(&result,&v,4);return result;}
float real(uint32_t v){float result;std::memcpy(&result,&v,4);return result;}
void same(float value,uint32_t expected,const char*field){++checks;if(bits(value)!=expected){std::cerr<<std::setprecision(12)<<"Dropped source float "<<current<<":"<<frame<<" "<<field<<" actual="<<value<<" expected="<<real(expected)<<" bits="<<bits(value)<<"/"<<expected<<"\n";std::exit(1);}}
struct Reader {
 std::vector<uint8_t>bytes;size_t at=0;
 uint32_t word(){CHECK(at+4<=bytes.size());uint32_t v=0;for(unsigned n=0;n<4;++n)v|=uint32_t(bytes[at++])<<(8*n);return v;}
 int32_t integer(){return int32_t(word());}
 std::string text(){auto n=word();CHECK(n<=4096&&n<=bytes.size()-at);std::string value(reinterpret_cast<const char*>(bytes.data()+at),n);at+=n;CHECK(value.find('\0')==std::string::npos);return value;}
 bool boolean(){auto v=word();CHECK(v<=1);return v!=0;}
};
struct Row {
 int frame=0;uint32_t idle=0,delta=0,ledger=0,keys=0;
 bool alive=false,flag_present=false,flag=false,global=false,prompt_visible=false;
 std::vector<std::string>dialogues;std::vector<uint32_t>party;
 std::string item;uint32_t doses=0,uid=0;bool equipped=false;
 std::array<uint32_t,4>transform{};
 bool visible=false,sprite_visible=false,timer_paused=false,timer_stopped=false,queued=false;
 uint32_t rotation=0,sparkle=0,timer=0,animation=0,speed=0,time=0;
};
Row row(Reader&r){
 Row v;v.frame=r.integer();v.idle=r.word();v.delta=r.word();v.alive=r.boolean();v.flag_present=r.boolean();v.flag=r.boolean();v.ledger=r.word();
 auto n=r.word();CHECK(n<=16);while(n--)v.dialogues.push_back(r.text());
 n=r.word();CHECK(n<=32);while(n--)v.party.push_back(r.word());v.keys=r.word();v.global=r.boolean();
 if(v.global){v.item=r.text();v.doses=r.word();v.equipped=r.boolean();v.uid=r.word();}
 v.prompt_visible=r.boolean();
 if(v.alive){for(auto&bits:v.transform)bits=r.word();v.visible=r.boolean();v.sprite_visible=r.boolean();v.rotation=r.word();v.sparkle=r.word();v.timer_paused=r.boolean();v.timer=r.word();v.timer_stopped=r.boolean();v.animation=r.word();CHECK(v.animation<=2);v.speed=r.word();v.time=r.word();v.queued=r.boolean();}
 return v;
}
}
