#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace encore {
bool Map::blocked(int x,int y) const {
    if(x<0||y<0||x>=int(width)*16||y>=int(height)*16) return true;
    return tiles[size_t(y/16)*width+size_t(x/16)]!=0;
}
namespace {
struct Reader {
    const uint8_t* p; size_t n,pos=0; bool ok=true;
    uint8_t u8() { if(pos>=n) {ok=false;return 0;} return p[pos++]; }
    uint16_t u16() { uint16_t a=u8(); return uint16_t(a|(uint16_t(u8())<<8)); }
    uint32_t u32() { uint32_t a=u16(); return a|(uint32_t(u16())<<16); }
    int32_t i32() { uint32_t v=u32(); int32_t x; std::memcpy(&x,&v,4); return x; }
    int16_t i16() { uint16_t v=u16(); int16_t x; std::memcpy(&x,&v,2); return x; }
    std::string str() {
        size_t len=u16();
        if(len>1024||pos>n||len>n-pos) {ok=false;return {};}
        std::string s(reinterpret_cast<const char*>(p+pos),len);pos+=len;
        if(s.find('\0')!=std::string::npos) ok=false;
        return s;
    }
};
bool unique_ids(const std::vector<uint32_t>& ids) {
    auto c=ids;std::sort(c.begin(),c.end());return std::adjacent_find(c.begin(),c.end())==c.end();
}
}
bool Content::load(const uint8_t* bytes,size_t size,std::string& error) {
    auto fail=[&](const char* s){error=s;return false;};
    if(!bytes||size<64||size>kMaxPackBytes||std::memcmp(bytes,"ENCPAK01",8)!=0) return fail("Invalid pack magic/size");
    Reader r{bytes,size};r.pos=8;
    const uint32_t version=r.u32(),rule=r.u32(),payload=r.u32(),checksum=r.u32(),caps=r.u32();
    Content t;t.family=r.u32();t.rules=rule;t.payload_crc=checksum;
    const auto ns=r.u32(),nm=r.u32(),np=r.u32(),ne=r.u32(),nf=r.u32();
    t.start_map=r.u32();t.spawn_x=r.i32();t.spawn_y=r.i32();
    if(version!=kPackVersion||rule!=kRulesVersion||(caps&~kCapabilities)!=0||t.family!=kContentFamily)
        return fail("Incompatible pack format, rules, capability or content family");
    if(payload!=size-64||crc32(bytes+64,payload)!=checksum) return fail("Pack length/checksum mismatch");
    if(ns==0||ns>1024||nm==0||nm>64||np==0||np>512||ne>128||nf>kMaxFlags||t.start_map>=nm)
        return fail("Pack count exceeds schema limits");
    for(uint32_t i=0;i<ns&&r.ok;++i) t.strings.push_back(r.str());
    std::vector<uint32_t> map_ids,object_ids,program_ids,enemy_ids;
    for(uint32_t i=0;i<nm&&r.ok;++i) {
        Map m;m.id=r.u32();m.title=r.u32();m.width=r.u16();m.height=r.u16();
        auto no=r.u16();if(r.u16()!=0) return fail("Non-zero map reserved field");
        if(!m.id||m.title>=ns||m.width<3||m.height<3||m.width>64||m.height>64||no>128) return fail("Invalid map definition");
        map_ids.push_back(m.id);
        for(size_t j=0;j<size_t(m.width)*m.height;++j) {auto v=r.u8();if(v>2) return fail("Unknown tile kind");m.tiles.push_back(v);}
        for(uint32_t j=0;j<no;++j) {
            Object o;o.id=r.u32();auto kind=r.u8();
            if(r.u8()!=0||r.u8()!=0||r.u8()!=0) return fail("Non-zero object reserved field");
            o.kind=ObjectKind(kind);o.x=r.i16();o.y=r.i16();o.program=r.u32();o.name=r.u32();
            if(!o.id||kind>3||o.program>=np||o.name>=ns||m.blocked(o.x,o.y)) return fail("Invalid object binding or position");
            object_ids.push_back(o.id);m.objects.push_back(o);
        }
        t.maps.push_back(std::move(m));
    }
    for(uint32_t i=0;i<np&&r.ok;++i) {
        Program p;p.id=r.u32();auto count=r.u32();
        if(!p.id||count==0||count>2048) return fail("Invalid program size/ID");
        program_ids.push_back(p.id);
        for(uint32_t j=0;j<count;++j) {
            Instruction ins;auto op=r.u8();
            if(r.u8()!=0||r.u8()!=0||r.u8()!=0) return fail("Non-zero instruction reserved field");
            ins.op=Op(op);ins.a=r.i32();ins.b=r.i32();ins.c=r.i32();
            if(op>7) return fail("Unsupported story opcode");
            p.code.push_back(ins);
        }
        t.programs.push_back(std::move(p));
    }
    for(uint32_t i=0;i<ne&&r.ok;++i) {
        Enemy e;e.id=r.u32();e.name=r.u32();e.hp=r.i32();e.attack=r.i32();e.defense=r.i32();e.reward=r.i32();
        if(!e.id||e.name>=ns||e.hp<=0||e.hp>9999||e.attack<0||e.attack>999||e.defense<0||e.defense>999||e.reward<0||e.reward>9999)
            return fail("Invalid enemy data");
        enemy_ids.push_back(e.id);t.enemies.push_back(e);
    }
    for(uint32_t i=0;i<nf&&r.ok;++i) {auto id=r.u32();if(!id) return fail("Invalid flag ID");t.flag_ids.push_back(id);}
    if(!r.ok||r.pos!=size) return fail("Truncated pack or unexpected trailing bytes");
    if(!unique_ids(map_ids)||!unique_ids(object_ids)||!unique_ids(program_ids)||!unique_ids(enemy_ids)||!unique_ids(t.flag_ids))
        return fail("Duplicate stable ID within a namespace");
    auto index=[](int32_t v,size_t n){return v>=0&&size_t(v)<n;};
    for(const auto& p:t.programs) for(const auto& in:p.code) {
        bool valid=true;
        switch(in.op) {
        case Op::End:break;
        case Op::Say:valid=index(in.a,t.strings.size());break;
        case Op::SetFlag:valid=index(in.a,nf)&&(in.b==0||in.b==1);break;
        case Op::IfFlag:valid=index(in.a,nf)&&(in.b==0||in.b==1)&&index(in.c,p.code.size());break;
        case Op::Jump:valid=index(in.a,p.code.size());break;
        case Op::Wait:valid=in.a>=0&&in.a<=36000;break;
        case Op::Teleport:valid=index(in.a,nm)&&!t.maps[size_t(in.a)].blocked(in.b,in.c);break;
        case Op::Battle:valid=index(in.a,ne);break;
        }
        if(!valid) return fail("Invalid story operand/reference");
    }
    if(t.maps[t.start_map].blocked(t.spawn_x,t.spawn_y)) return fail("Spawn is outside walkable map");
    *this=std::move(t);error.clear();return true;
}
bool Content::load_file(const char* path,std::string& error) {
    std::vector<uint8_t> b;return read_file(path,b,kMaxPackBytes,error)&&load(b.data(),b.size(),error);
}
}
