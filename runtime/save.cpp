#include "encore/game.hpp"
#include <algorithm>
#include <cstring>
namespace encore {
namespace {
void u32(std::vector<uint8_t>& b,uint32_t n){for(int i=0;i<4;++i)b.push_back(uint8_t(n>>(8*i)));}
uint32_t get(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
}
bool Game::encode_save(std::vector<uint8_t>& bytes,std::string& error) const{
    if(!can_save()){error="Save only at an idle world checkpoint";return false;}
    std::vector<uint8_t> b{'E','N','C','S','A','V','0','1'};
    u32(b,1);u32(b,content_.family);u32(b,content_.rules);u32(b,map().id);
    u32(b,uint32_t(state_.x));u32(b,uint32_t(state_.y));u32(b,uint32_t(state_.hp));u32(b,uint32_t(state_.xp));
    u32(b,state_.rng);u32(b,state_.tick);u32(b,uint32_t(content_.flag_ids.size()));
    for(size_t i=0;i<content_.flag_ids.size();++i){u32(b,content_.flag_ids[i]);u32(b,state_.flags[i]);}
    u32(b,crc32(b.data(),b.size()));bytes=std::move(b);error.clear();return true;
}
bool Game::decode_save(const uint8_t* b,size_t size,std::string& error){
    auto fail=[&](const char* s){error=s;return false;};
    if(!can_save())return fail("Load only at an idle world checkpoint");
    if(!b||size<56||size>56+kMaxFlags*8||std::memcmp(b,"ENCSAV01",8)!=0)return fail("Invalid save format/size");
    if(get(b+8)!=1||get(b+12)!=content_.family||get(b+16)!=content_.rules)return fail("Save requires unsupported migration");
    uint32_t nf=get(b+48);if(nf>kMaxFlags||size!=56+size_t(nf)*8)return fail("Invalid save flag table");
    if(crc32(b,size-4)!=get(b+size-4))return fail("Save checksum mismatch (try .bak)");
    State s;const auto map_id=get(b+20);bool found=false;
    for(size_t i=0;i<content_.maps.size();++i)if(content_.maps[i].id==map_id){s.map=uint32_t(i);found=true;break;}
    if(!found)return fail("Saved map was removed; explicit migration is required");
    const auto x=get(b+24),y=get(b+28),hp=get(b+32),xp=get(b+36);
    if(x>1024||y>1024||hp==0||hp>30||xp>999999||content_.maps[s.map].blocked(int(x),int(y)))return fail("Save state out of range/blocked");
    s.x=int32_t(x);s.y=int32_t(y);s.hp=int32_t(hp);s.xp=int32_t(xp);s.rng=get(b+40);s.tick=get(b+44);
    if(s.rng==0)return fail("Invalid RNG state");
    std::vector<uint32_t> seen;
    for(uint32_t i=0;i<nf;++i){uint32_t id=get(b+52+i*8),value=get(b+56+i*8);
        if(value>1||std::find(seen.begin(),seen.end(),id)!=seen.end())return fail("Duplicate/invalid save flag");
        seen.push_back(id);
        auto it=std::find(content_.flag_ids.begin(),content_.flag_ids.end(),id);
        if(it==content_.flag_ids.end())return fail("Saved flag was removed; explicit migration is required");
        s.flags[size_t(it-content_.flag_ids.begin())]=uint8_t(value);
    }
    // New known flags default to false; stable IDs survive table reordering.
    state_=s;mode_=Mode::World;vm_.cancel();battle_={};error_.clear();error.clear();return true;
}
}
