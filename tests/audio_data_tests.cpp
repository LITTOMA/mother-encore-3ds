#include "encore/audio_data.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok){std::cerr<<"FAIL: "<<why<<"\n";std::exit(1);}}
void put32(std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(i*8));}
void fix_crc(std::vector<uint8_t>& b){put32(b,16,0);put32(b,16,audio_crc32(b.data(),b.size()));}
std::vector<uint8_t> read_file(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char** argv){
    check(argc==3,"provide bank path and asset root");std::string error;AudioBank bank;
    const auto bytes=read_file(argv[1]);check(bank.load(bytes.data(),bytes.size(),error),error.c_str());
    check(bank.count()==21,"external slice bank assets");check(std::abs(bank.silence_db()+80)<0.001,"source silence threshold");
    check(bank.asset(12).stable_id==36&&!bank.asset(12).loops()&&bank.asset(13).stable_id==37&&!bank.asset(13).loops()&&bank.asset(14).stable_id==1201&&!bank.asset(14).loops(),"phone ring, hangup and Adult voice appended without renumbering");
    check(bank.asset(15).stable_id==1202&&bank.asset(15).source_path=="res://Audio/Music/Mother Earth.mp3"&&bank.asset(15).sample_rate==44100&&bank.asset(15).channels==2,"source title track appended without quality reduction");
    check(bank.asset(8).stable_id==30&&!bank.asset(8).loops(),"source post-win SMAAAASH one-shot appended");
    check(bank.asset(3).stable_id==1002&&!bank.asset(3).loops(),"source Doll boss encounter one-shot");
    const auto first=bank.asset(0);check(first.stable_id==21&&first.loops()&&first.loop_start==281446,"source import loop point");
    check(bank.asset(99).stable_id==0,"missing asset empty");AudioAsset a;check(!bank.find(0,a),"missing stable identity");
    for(size_t n=0;n<bytes.size();++n){AudioBank candidate;check(!candidate.load(bytes.data(),n,error),"all truncations rejected");}
    for(size_t i=0;i<bytes.size();++i){auto changed=bytes;changed[i]^=0x80;check(!bank.load(changed.data(),changed.size(),error),"all one-bit corruptions rejected");check(bank.count()==21,"failed load preserves prior owner");}
    auto reject=[&](size_t offset,uint32_t value,const char* why){auto changed=bytes;put32(changed,offset,value);fix_crc(changed);check(!bank.load(changed.data(),changed.size(),error),why);};
    reject(8,2,"version rejected");reject(20,65,"count rejected");reject(24,95,"stride rejected");reject(28,0,"strings offset rejected");reject(44,1,"header reserved rejected");
    reject(64,0,"zero stable ID rejected");reject(64+96,21,"duplicate stable ID rejected");reject(64+36,0,"invalid rate rejected");reject(64+40,3,"channels rejected");reject(64+40,2u|(2u<<16),"unknown loop flags rejected");reject(64+44,0,"zero frames rejected");reject(64+48,0xffffffffu,"out of range loop rejected");reject(64+52,1,"PCM length mismatch rejected");reject(64+60,0,"string ref rejected");reject(64+64,0xffffffffu,"string overflow rejected");reject(64+80,1,"asset reserved rejected");reject(36,0x7fc00000,"NaN master rejected");reject(40,0,"nonsilent threshold rejected");reject(64+76,0x7f800000,"infinite asset gain rejected");
    {auto changed=bytes;std::fill(changed.begin()+68,changed.begin()+100,0);fix_crc(changed);check(!bank.load(changed.data(),changed.size(),error),"zero source digest rejected");}
    {auto changed=bytes;const size_t strings=64+bank.count()*96;changed[strings]='/';fix_crc(changed);check(!bank.load(changed.data(),changed.size(),error),"unsafe PCM path rejected");}
    {auto changed=bytes;changed.back()=1;fix_crc(changed);check(!bank.load(changed.data(),changed.size(),error),"unterminated final source path rejected");}
    AudioFrameCursor cursor;uint32_t start=0;
    check(cursor.reset(10,3,true),"loop cursor reset");check(cursor.take(8,start)==8&&start==0,"intro first chunk");check(cursor.take(8,start)==2&&start==8,"intro endpoint exact");check(cursor.take(8,start)==7&&start==3,"repeat begins at loop offset");check(cursor.take(2,start)==2&&start==3,"loop rewrap");
    check(!cursor.reset(0,0,false)&&!cursor.reset(10,10,true)&&!cursor.reset(10,1,false),"invalid cursors rejected");
    check(cursor.reset(3,0,false),"oneshot reset");check(cursor.take(7,start)==3&&start==0,"oneshot partial");check(cursor.take(7,start)==0&&start==3,"oneshot EOF");
    AudioFade fade_in;fade_in.reset(-80);check(fade_in.start(0,2,true),"start source quartic-out fade-in");check(fade_in.advance(1)&&std::abs(fade_in.db()+5)<.00001,"fade-in quartic-out halfway is minus5db");check(fade_in.advance(1)&&fade_in.db()==0&&!fade_in.active(),"fade-in reaches target without stopping state");
    AudioFade fade;fade.reset(0);check(fade.start(-80,2),"start quartic fade");check(fade.advance(1)&&std::abs(fade.db()+5)<0.00001,"quartic halfway is -5 dB");check(fade.advance(1)&&fade.db()==-80&&!fade.active(),"fade reaches silence");check(!fade.advance(-1)&&!fade.start(-80,-1),"negative fade rejected");check(fade.start(0,0)&&fade.db()==0&&!fade.active(),"immediate fade");check(!fade.advance(INFINITY)&&!fade.start(NAN,1),"nonfinite fade rejected");
    check(std::abs(audio_linear_gain(-20)-0.1)<0.00001&&audio_linear_gain(NAN)==0,"dB conversion");
    const auto temporary=std::filesystem::temp_directory_path()/std::filesystem::path("encore-audio-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".pcm");
    const std::vector<uint8_t> pcm={1,0,2,0,3,0,4,0,5,0,6,0};
    {std::ofstream f(temporary,std::ios::binary);f.write(reinterpret_cast<const char*>(pcm.data()),pcm.size());}
    AudioAsset fixture;fixture.frames=3;fixture.channels=2;fixture.pcm_bytes=12;fixture.pcm_crc=audio_crc32(pcm.data(),pcm.size());fixture.flags=1;fixture.loop_start=1;
    AudioPcmStream stream;check(stream.open(fixture,temporary.string().c_str(),error),error.c_str());int16_t out[16]{};uint32_t got=0;
    check(stream.read(out,7,got,error)&&got==7,"loop fills buffer across EOF");
    const int16_t expected[]={1,2,3,4,5,6,3,4,5,6,3,4,5,6};check(!std::memcmp(out,expected,sizeof(expected)),"stereo interleaving and loop position");
    check(stream.rewind()&&stream.read(out,1,got,error)&&out[0]==1&&out[1]==2,"retrigger restarts intro");
    auto bad=fixture;bad.pcm_crc^=1;check(!stream.open(bad,temporary.string().c_str(),error)&&stream.is_open(),"failed open preserves prior stream");
    check(!stream.read(nullptr,1,got,error),"null buffer rejected");check(!stream.read(out,0,got,error),"zero capacity rejected");stream.close();check(!stream.read(out,1,got,error),"closed stream rejected");
    fixture.flags=0;fixture.loop_start=0;check(stream.open(fixture,temporary.string().c_str(),error),"oneshot open");check(stream.read(out,7,got,error)&&got==3,"oneshot returns exact final frames");check(stream.read(out,7,got,error)&&got==0,"oneshot no fabricated tail");stream.close();std::filesystem::remove(temporary);
    for(uint32_t i=0;i<bank.count();++i){const auto asset=bank.asset(i);const std::string path=std::string(argv[2])+std::string(asset.pcm_path);check(stream.open(asset,path.c_str(),error),error.c_str());check(stream.read(out,7,got,error)&&got==7,"real source PCM stream");stream.close();}
    std::cout<<"Audio data: "<<checks<<" checks passed; no platform/hardware audibility claim\n";
}
