#include "encore/resource_catalog.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstring>
#include <utility>

namespace encore::upstream { namespace {
constexpr size_t catalog_limit=16384;
constexpr uint32_t resource_limit=16u*1024u*1024u;
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u32(const uint8_t* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
bool battle_role(uint32_t role){return role==uint32_t(ResourceRole::Battle)||role==uint32_t(ResourceRole::EncounterBattle);}
bool round_role(uint32_t role){return role==uint32_t(ResourceRole::Round)||role==uint32_t(ResourceRole::EncounterRound);}
const char* suffix(uint32_t role) {
    switch(static_cast<ResourceRole>(role)) {
    case ResourceRole::Room:return ".encroom";case ResourceRole::Blackbars:return ".encbars";
    case ResourceRole::Battle:case ResourceRole::EncounterBattle:return ".encbattle";
    case ResourceRole::Round:case ResourceRole::EncounterRound:return ".encround";
    case ResourceRole::House:return ".enchouse";case ResourceRole::Items:return ".encitems";
    case ResourceRole::Audio:return ".encaudio";case ResourceRole::Phone:return ".encphone";
    case ResourceRole::Choices:return ".encchoices";case ResourceRole::SaveMenu:return ".encsavemenu";
    case ResourceRole::Session:return ".encsession";case ResourceRole::Settings:return ".encsettings";
    case ResourceRole::Prompts:return ".encprompts";case ResourceRole::Continue:return ".enccontinue";
    case ResourceRole::Restore:return ".encrestore";case ResourceRole::SessionMigration:return ".encmigration";
    case ResourceRole::NewGame:return ".encnewgame";case ResourceRole::Localization:return ".enclocale";
    case ResourceRole::TitleLocale:return ".enctitlelocale";case ResourceRole::SourceFonts:return ".encfont";
    case ResourceRole::Input:return ".encinput";case ResourceRole::LoadingIndicator:return ".encload";
    case ResourceRole::Introduction:return ".encintro";
    case ResourceRole::HouseInspections:return ".encinspect";
    case ResourceRole::DrawerProgram:return ".encdrawer";case ResourceRole::Storage:return ".encstorage";
    case ResourceRole::ItemDetails:return ".encdetails";
    case ResourceRole::FieldEquipment:return ".encfield";
    case ResourceRole::FieldRoom:return ".encroom";case ResourceRole::FieldMap:return ".encmap";
    case ResourceRole::WorldLinks:return ".enclinks";
    }return nullptr;
}
bool canonical(const std::string& path) {
    if(path.empty()||path.size()>256)return false;
    for(unsigned char c:path)if(c<33||c>126||c=='\\'||c==':'||c=='?'||c=='#')return false;
    size_t start=0;
    while(start<=path.size()){
        auto end=path.find('/',start);if(end==path.npos)end=path.size();
        const auto part=path.substr(start,end-start);
        if(part.empty()||part=="."||part=="..")return false;
        if(end==path.size())break;
        start=end+1;
    }return true;
}
struct Reader {
    const uint8_t* p;size_t n;bool ok=true;
    uint32_t integer(){if(n<4){ok=false;return 0;}auto value=u32(p);p+=4;n-=4;return value;}
    std::string path(){auto length=integer();if(!length||length>256||length>n){ok=false;return {};}
        std::string result(reinterpret_cast<const char*>(p),length);p+=length;n-=length;return result;}
};
}
bool ResourceCatalog::load(const uint8_t* p,size_t n,std::string& error) {
    auto fail=[&](const char* message){error=message;return false;};
    if(!p||n<60||n>catalog_limit)return fail("Resource catalog size rejected");
    const auto capability=u32(p+20);
    if(std::memcmp(p,"ENCRSC01",8)||u32(p+8)!=1||u32(p+12)!=n||(capability!=1&&capability!=2)||u32(p+24)||u32(p+28))
        return fail("Resource catalog schema/size/capability/reserved rejected");
    const uint32_t max_role=capability==1?30:33;
    if(~encore::crc32_update(~0u,p+32,n-32)!=u32(p+16))return fail("Resource catalog checksum rejected");
    for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};
        if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("Resource catalog source pin rejected");}
    Reader r{p+52,n-52};const auto count=r.integer(),pairs=r.integer();
    if(count<22||count>128||pairs<1||pairs>32)return fail("Resource catalog role/encounter count rejected");
    ResourceCatalog data;
    bool roots[22]={};
    for(uint32_t i=0;i<count;++i){const auto id=r.integer(),role=r.integer(),size=r.integer(),checksum=r.integer();auto path=r.path();
        if(!r.ok||role<1||role>max_role||((role<=22||role>=25)?id!=role:id<256)||!size||size>resource_limit||!canonical(path))return fail("Resource catalog binding rejected");
        const auto expected=suffix(role);const auto len=std::strlen(expected);
        if(path.size()<=len||path.compare(path.size()-len,len,expected))return fail("Resource catalog binding type rejected");
        for(const auto& prior:data.bindings_)if(prior.id==id||prior.path==path)return fail("Resource catalog duplicate ID/path rejected");
        if(role<=22)roots[role-1]=true;
        data.bindings_.push_back({id,static_cast<ResourceRole>(role),std::move(path),size,checksum});
    }
    for(bool present:roots)if(!present)return fail("Resource catalog missing required role");
    if(capability==2){unsigned field=0;for(const auto& row:data.bindings_)field+=uint32_t(row.role)>=31;
        if(field!=3)return fail("Resource catalog field scene roles incomplete");}
    auto binding=[&](uint32_t id)->const Binding*{for(const auto& row:data.bindings_)if(row.id==id)return &row;return nullptr;};
    for(uint32_t i=0;i<pairs;++i){const auto battle=r.integer(),round=r.integer();
        const auto* b=binding(battle);const auto* v=binding(round);
        if(!r.ok||!b||!v||!battle_role(uint32_t(b->role))||!round_role(uint32_t(v->role)))return fail("Resource catalog encounter type/reference rejected");
        for(const auto& pair:data.encounters_)if(pair.battle==battle||pair.round==round)
            return fail("Resource catalog duplicate encounter rejected");
        data.encounters_.push_back({battle,round});
    }
    for(const auto& row:data.bindings_)if(battle_role(uint32_t(row.role))||round_role(uint32_t(row.role))){bool present=false;
        for(const auto& pair:data.encounters_)if(pair.battle==row.id||pair.round==row.id)present=true;
        if(!present)return fail("Resource catalog unpaired encounter resource rejected");}
    if(!r.ok||r.n)return fail("Resource catalog truncated/trailing payload rejected");
    data.valid_=true;*this=std::move(data);error.clear();return true;
}
bool ResourceCatalog::load_file(const char* path,std::string& error) {
    if(!path){error="Missing resource catalog path";return false;}
    std::vector<uint8_t> bytes;return encore::read_file(path,bytes,catalog_limit,error)&&load(bytes.data(),bytes.size(),error);
}
const std::string& ResourceCatalog::path(ResourceRole role)const {
    static const std::string empty;
    // Singleton identities are admitted by load(). Match their actual role as
    // well as the ID, without a second whitelist that can omit newer roots.
    // Encounter resources use independent IDs and resolve through companions.
    const auto id=uint32_t(role);
    if(valid_)for(const auto& row:bindings_)if(row.id==id&&row.role==role)return row.path;
    return empty;
}
std::string ResourceCatalog::companion_path(std::string_view battle_path)const {
    if(valid_)for(const auto& pair:encounters_){const Binding* battle=nullptr;const Binding* round=nullptr;
        for(const auto& row:bindings_){if(row.id==pair.battle)battle=&row;if(row.id==pair.round)round=&row;}
        if(battle&&round&&battle->path==battle_path)return round->path;}
    return {};
}
bool ResourceCatalog::verify_files(const char* prefix,std::string& error)const {
    if(!valid_||!prefix){error="Invalid resource catalog verification request";return false;}
    std::string root=prefix;if(!root.empty()&&root.back()!='/'&&root.back()!='\\')root+='/';
    for(const auto& binding:bindings_){const auto filename=root+binding.path;FILE* file=std::fopen(filename.c_str(),"rb");
        if(!file){error="Missing catalog-bound resource: "+binding.path;return false;}
        uint8_t bytes[4096];uint32_t count=0,value=~0u;bool good=true;
        while(true){const auto got=std::fread(bytes,1,sizeof(bytes),file);
            if(got>binding.size-count){good=false;break;}count+=uint32_t(got);value=encore::crc32_update(value,bytes,got);
            if(got<sizeof(bytes)){good=!std::ferror(file);break;}}
        const bool closed=std::fclose(file)==0;
        if(!good||!closed||count!=binding.size||~value!=binding.crc){error="Catalog-bound resource size/checksum rejected: "+binding.path;return false;}
    }error.clear();return true;
}
}
