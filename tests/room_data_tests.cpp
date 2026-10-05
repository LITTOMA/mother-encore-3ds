#include "encore/room_data.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool ok,const char* label) { ++checks;if(!ok){++failures;std::fprintf(stderr,"FAIL: %s\n",label);} }
uint32_t get32(const std::vector<uint8_t>& b,size_t o) {return uint32_t(b[o])|(uint32_t(b[o+1])<<8)|(uint32_t(b[o+2])<<16)|(uint32_t(b[o+3])<<24);}
void put16(std::vector<uint8_t>& b,size_t o,uint16_t v) {b[o]=uint8_t(v);b[o+1]=uint8_t(v>>8);}
void put32(std::vector<uint8_t>& b,size_t o,uint32_t v) {for(unsigned i=0;i<4;++i)b[o+i]=uint8_t(v>>(8*i));}
void putfloat(std::vector<uint8_t>& b,size_t o,float v) {uint32_t n;std::memcpy(&n,&v,4);put32(b,o,n);}
void putdouble(std::vector<uint8_t>& b,size_t o,double v) {uint64_t n;std::memcpy(&n,&v,8);put32(b,o,uint32_t(n));put32(b,o+4,uint32_t(n>>32));}
size_t dir(RoomSection s) {return kRoomHeaderBytes+(static_cast<uint16_t>(s)-1)*kRoomDirectoryEntryBytes;}
size_t row(const std::vector<uint8_t>& b,RoomSection s,uint32_t i=0) {const auto d=dir(s);return get32(b,d+4)+size_t(i)*get32(b,d+12);}
void repair(std::vector<uint8_t>& b) {put32(b,52,0);put32(b,52,encore::crc32(b.data(),b.size()));}
}
int main(int argc,char** argv) {
    if(argc!=2) {std::fprintf(stderr,"Usage: encore_room_data_tests room.encroom\n");return 2;}
    std::vector<uint8_t> base;std::string error;
    if(!encore::read_file(argv[1],base,kMaxRoomBytes,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 2;}
    RoomData data;
    check(data.empty()&&!data.view().valid(),"new owner has no default content");
    check(data.load(base.data(),base.size(),error),"baseline room validates");
    if(data.empty()){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    const auto v=data.view();const auto* original=v.bytes();const auto original_id=v.scene().stable_id;
    check(v.byte_size()==base.size()&&v.bytes()!=base.data(),"owner has one independent byte buffer");
    check(v.string(0).empty()&&v.string_data(0)[0]==0,"empty string is NUL terminated");
    check(v.scene().stable_id==get32(base,40),"scene identity decoded from header/table");
    check(v.actor_instance_count()>0&&v.resource_count()>0&&v.flag_count()>0,"valid pack has required runtime references");
    for(uint32_t i=0;i<v.string_count();++i)check(std::strlen(v.string_data(i))==v.string(i).size(),"borrowed string boundaries");
    for(uint32_t i=0;i<v.clip_count();++i) {
        FrameClip f{};const auto c=v.clip(i);
        if(c.flags&16) {f.length=7;check(!v.frame_clip(i,f)&&f.length==7,"legacy sampler rejects delayed clip transactionally");continue;}
        check(v.frame_clip(i,f)&&f.length==c.length&&f.count==c.key_count&&f.loop==c.loop(),"clip stack decoding");
        for(uint32_t j=0;j<c.key_count;++j)check(f.keys[j].time==v.key(c.first_key+j).time&&f.keys[j].frame==v.key(c.first_key+j).frame,"key stack decoding");
    }
    check(v.rule_f32(RoomRuleKey::WalkSpeed)>0&&v.rule_f64(RoomRuleKey::CameraReturnSeconds)>0,"rules decoded with exact scalar types");
    check(v.rule_u32(RoomRuleKey::WalkSpeed)==0,"wrong scalar getter is empty");
    const auto shaker=v.binding(1);check(shaker.kind==uint16_t(RoomBindingKind::PeriodicCameraShake)&&shaker.value==4&&shaker.duration==5,"source periodic shake binding");
    check(v.resource(shaker.target_index).kind==2&&v.string(v.resource(shaker.target_index).path_string)=="res://Audio/Sound effects/M3/PK_Thunder_a_b_y_O_hit.wav","source room shake sound reference");
    check(v.rule_f64(RoomRuleKey::RoomShakeWaitSeconds)==4&&v.rule_f64(RoomRuleKey::RoomShakeWaitMarginSeconds)==.5&&v.rule_f64(RoomRuleKey::RoomShakeLengthSeconds)==.8&&v.rule_f64(RoomRuleKey::RoomShakeDirectionX)==1&&v.rule_f64(RoomRuleKey::RoomShakeDirectionY)==0,"source periodic shake f64 rules");

    check(v.vec2(0).x==v.vertex(0).x,"vertex getter alias");
    check(v.resource(v.resource_count()).stable_id==0&&v.string(v.string_count()).empty(),"invalid accessor indexes are safe");
    FrameClip untouched{};untouched.length=7;
    check(!v.frame_clip(v.clip_count(),untouched)&&untouched.length==7,"failed clip lookup preserves result");
    auto rejected=[&](std::vector<uint8_t> b,const char* label,bool crc=true) {
        if(crc)repair(b);
        if(data.load(b.data(),b.size(),error)){check(false,label);std::exit(1);}
        check(!error.empty(),"rejection provides diagnostic");
        check(data.view().bytes()==original&&data.view().scene().stable_id==original_id,"failed load preserves prior owner and view");
    };
    auto bad32=[&](size_t off,uint32_t value,const char* label){auto b=base;put32(b,off,value);rejected(std::move(b),label);};
    auto bad16=[&](size_t off,uint16_t value,const char* label){auto b=base;put16(b,off,value);rejected(std::move(b),label);};
    auto bad8=[&](size_t off,uint8_t value,const char* label){auto b=base;b[off]=value;rejected(std::move(b),label);};
    check(!data.load(nullptr,0,error)&&data.view().bytes()==original,"null load is transactional");
    check(!data.load(base.data(),kMaxRoomBytes+1,error)&&data.view().bytes()==original,"excessive size rejected before read");
    check(!data.load_file(nullptr,error)&&data.view().bytes()==original,"null path is transactional");
    check(!data.load_file("nonexistent-room-negative-test.encroom",error)&&data.view().bytes()==original,"missing file has no fallback");
    for(size_t n=0;n<base.size();++n)check(!data.load(base.data(),n,error),"every truncated prefix rejected");
    bad8(0,'X',"bad magic");bad16(8,2,"unknown major");bad16(10,1,"unknown minor");
    for(auto off:{12u,16u,20u,24u,28u,32u,36u,40u,44u,108u,112u})bad32(off,0,"unsupported/invalid header field");
    bad32(32,1,"old room rules rejected");bad32(36,1,"old room capabilities rejected");
    bad32(32,2,"previous room rules rejected");bad32(36,2,"previous room capabilities rejected");
    const auto shake=row(base,RoomSection::Binding,1);
    bad16(shake+4,10,"unknown room binding kind");bad32(shake+8,kRoomNoIndex,"missing shake sound");bad32(shake+8,0,"shake sound cannot be texture");
    for(auto off:{shake+16,shake+24}){auto b=base;putdouble(b,off,0);rejected(std::move(b),"shake magnitude/delay must be positive");}
    for(uint32_t i=20;i<25;++i){auto b=base;putdouble(b,row(base,RoomSection::Rule,i)+8,i==21?-1:i==23?2:i==24?2:0);rejected(std::move(b),"invalid periodic shake rule value");}
    bad16(48,25,"missing required section");bad16(50,20,"bad directory stride");bad32(116,1,"unknown header flag");bad8(120,1,"header reserved byte");
    {auto b=base;b.back()^=1;rejected(std::move(b),"CRC corruption",false);}
    for(uint32_t i=0;i<kRoomSectionCount;++i) {
        const size_t d=kRoomHeaderBytes+i*kRoomDirectoryEntryBytes;
        bad16(d,0,"unknown section type");bad16(d+2,2,"unknown section version");
        bad32(d+12,get32(base,d+12)+1,"section stride mismatch");bad32(d+20,0,"section required flag missing");
        bad32(d+8,0xffffffffu,"section count overflow");bad32(d+16,0xffffffffu,"section byte count overflow");
        if(get32(base,d+8)) {
            bad32(d+4,128,"section overlaps directory");bad32(d+4,get32(base,d+4)+1,"unaligned section");bad32(d+4,0xfffffff8u,"section outside file");
        }
    }
    {auto b=base;std::swap_ranges(b.begin()+128,b.begin()+152,b.begin()+152);rejected(std::move(b),"reordered directory");}
    bad32(dir(RoomSection::Resource)+4,get32(base,dir(RoomSection::StringRef)+4),"overlapping sections");
    {auto b=base;b.push_back(0);put32(b,16,uint32_t(b.size()));rejected(std::move(b),"unexpected trailing byte");}
    for(uint32_t i=1;i<kRoomSectionCount;++i) {
        const auto d=kRoomHeaderBytes+i*kRoomDirectoryEntryBytes,pd=d-kRoomDirectoryEntryBytes;
        const auto end=get32(base,pd+4)+get32(base,pd+16),next=get32(base,d+4);
        if(next>end&&end>kRoomHeaderBytes+kRoomSectionCount*kRoomDirectoryEntryBytes) {bad8(end,1,"nonzero alignment padding");break;}
    }
    const auto strings=row(base,RoomSection::StringRef),pool=row(base,RoomSection::StringBytes);
    bad32(strings,0,"string outside pool");bad32(strings+4,1,"string zero not empty");bad32(strings+8+4,0xffffffffu,"string length overflow");
    bad8(pool,0xff,"invalid UTF-8");
    {auto b=base;const auto at=get32(base,strings+8);b[at]=0;rejected(std::move(b),"embedded NUL");}
    {auto b=base;const auto at=get32(base,strings+8);b[at]=0xc0;b[at+1]=0x80;rejected(std::move(b),"overlong UTF-8");}
    const auto resource=row(base,RoomSection::Resource),clip=row(base,RoomSection::Clip),profile=row(base,RoomSection::ActorProfile),instance=row(base,RoomSection::ActorInstance),scene=row(base,RoomSection::Scene);
    bad32(resource,0,"zero stable identity");
    if(v.resource_count()>1)bad32(resource+56,v.resource(0).stable_id,"duplicate stable identity");
    bad32(resource+4,kRoomNoIndex,"resource path outside string table");bad16(resource+16,5,"unknown resource kind");bad16(resource+18,1,"unknown resource flags");bad32(resource+20,1,"resource reserved data");
    bad16(resource+12,0,"zero texture grid");
    {auto b=base;const auto path=get32(b,strings+size_t(v.resource(0).path_string)*8);b[path]='/';rejected(std::move(b),"absolute resource path");}
    {auto b=base;putfloat(b,row(base,RoomSection::Vertex),std::numeric_limits<float>::quiet_NaN());rejected(std::move(b),"NaN vertex");}
    bad32(row(base,RoomSection::Polygon)+8,kRoomNoIndex,"polygon vertex range");bad16(row(base,RoomSection::Polygon)+12,2,"degenerate polygon count");bad16(row(base,RoomSection::Polygon)+14,2,"unknown polygon flags");
    {auto b=base;putfloat(b,row(base,RoomSection::Polygon)+16,123456);rejected(std::move(b),"polygon bounds mismatch");}
    bad32(row(base,RoomSection::BodyRule),0,"zero body ID");bad8(row(base,RoomSection::BodyRule)+8,2,"invalid body boolean");bad32(row(base,RoomSection::BodyRule)+12,1,"body reserved fields");
    bad32(row(base,RoomSection::Overlay)+4,kRoomNoIndex,"overlay resource reference");bad16(row(base,RoomSection::Overlay)+24,65535,"overlay rectangle outside resource");
    bad16(row(base,RoomSection::Overlay)+28,2,"unknown overlay pass flag");
    bad32(row(base,RoomSection::MapDraw)+20,1,"unknown map flags");
    bad16(clip+12,9,"clip key capacity");bad8(clip+14,32,"unknown clip flag");bad8(clip+15,3,"unknown visibility");bad16(clip+18,2,"unknown clip channel");bad32(clip+8,kRoomNoIndex,"clip key range overflow");
    bad16(row(base,RoomSection::Key)+4,65535,"key outside atlas");bad16(row(base,RoomSection::Key)+6,1,"key reserved fields");
    {auto b=base;putfloat(b,row(base,RoomSection::Key),1);rejected(std::move(b),"clip first key not zero");}
    bad16(profile+4,4,"unknown execution profile");bad16(profile+6,2,"unknown profile flag");bad32(profile+8,kRoomNoIndex,"missing primary resource");bad16(profile+30,1,"profile reserved fields");bad16(profile+70,1,"profile direction reserved fields");bad16(profile+68,7,"directional profile incomplete");
    bad32(instance+4,kRoomNoIndex,"actor profile reference");bad16(instance+8,3,"unsupported actor binding");bad16(instance+10,2,"unknown actor instance flags");bad32(instance+36,1,"actor reserved fields");
    bad32(row(base,RoomSection::CameraArea)+4,1,"unknown camera flags");
    {auto b=base;putfloat(b,row(base,RoomSection::CameraArea)+16,-1);rejected(std::move(b),"negative camera extents");}
    bad8(row(base,RoomSection::Flag)+8,2,"invalid flag boolean");bad8(row(base,RoomSection::Flag)+9,2,"unknown flag definition flag");bad32(row(base,RoomSection::Flag)+12,1,"flag reserved fields");
    bad32(row(base,RoomSection::InitialFlag),kRoomNoIndex,"initial flag reference");bad8(row(base,RoomSection::InitialFlag)+5,1,"initial flag reserved fields");
    bad32(row(base,RoomSection::Condition),kRoomNoIndex,"condition flag reference");bad8(row(base,RoomSection::Condition)+5,1,"unknown condition domain");bad16(row(base,RoomSection::Condition)+6,1,"condition reserved fields");
    bad32(row(base,RoomSection::Trigger)+20,kRoomNoIndex,"trigger program reference");bad32(row(base,RoomSection::Trigger)+28,1,"trigger reserved fields");
    const auto program=row(base,RoomSection::Program),command=row(base,RoomSection::Command),binding=row(base,RoomSection::Binding),battle=row(base,RoomSection::Battle),rule=row(base,RoomSection::Rule);
    bad32(program+4,kRoomNoIndex,"program command range");bad32(program+12,0,"zero program phrases");
    bad32(program+16,v.string_count(),"program source path index");
    bad32(program+16,0,"program source path empty");
    if(v.program_count()>1)bad32(row(base,RoomSection::Program,1)+16,v.program(0).source_path_string,"duplicate program source path");
    bad16(command,44,"unknown opcode");bad32(command+12,1,"command flags");bad32(command+40,0,"unexpected auxiliary command field");bad32(command+44,1,"command reserved field");bad32(command+4,kRoomNoIndex,"command phrase out of program");
    for(uint32_t i=0;i<v.command_count();++i) {
        const auto c=v.command(i);const auto off=command+size_t(i)*48;
        if(c.actor_index==kRoomNoActor&&c.opcode!=5) {bad16(off+2,0,"unused command actor");break;}
    }
    {auto b=base;putdouble(b,command+24,1);rejected(std::move(b),"unused command value");}
    {auto b=base;putdouble(b,command+32,std::numeric_limits<double>::infinity());rejected(std::move(b),"infinite command duration");}
    bad16(binding+4,10,"unknown binding kind");bad32(binding+12,0,"unexpected binding auxiliary");bad32(battle+20,16,"unknown battle flags");bad32(battle+24,kRoomNoIndex,"battle cutscene string reference");bad32(battle+28,kRoomNoIndex,"battle pack resource reference");bad32(battle+28,0,"battle pack cannot be texture");
    bad16(rule,28,"unknown rule key");bad16(rule+2,3,"wrong rule type");bad32(rule+4,1,"rule reserved fields");bad32(rule+12,1,"unused f32 rule payload");
    {auto b=base;putfloat(b,rule+8,0);rejected(std::move(b),"zero divisor/rule");}
    bad32(row(base,RoomSection::Experience),1,"experience level one is nonzero");bad32(row(base,RoomSection::Experience)+4,0,"nonascending experience");
    bad32(scene+16,kRoomNoIndex,"scene player reference");bad16(scene+48,4,"unknown scene motion");bad32(scene+68,kRoomNoIndex,"scene camera reference");bad32(scene+72,2,"unsupported rule profile");bad32(scene+76,1,"unknown scene flags");
    bad8(row(base,RoomSection::AnimationBinding)+2,6,"unknown animation motion");bad8(row(base,RoomSection::AnimationBinding)+3,8,"unknown animation direction");bad32(row(base,RoomSection::AnimationBinding)+4,kRoomNoIndex,"animation clip reference");
    // Every opcode seen in the actual program rejects unknown flags and
    // fields that its schema does not use; no name-based runtime dispatch.
    bool opcode_seen[44]{};
    for(uint32_t i=0;i<v.command_count();++i) {
        const auto c=v.command(i);check(c.opcode<44,"supported opcode fits coverage table");if(c.opcode>=44)continue;if(opcode_seen[c.opcode])continue;opcode_seen[c.opcode]=true;
        const auto off=command+size_t(i)*48;
        bad32(off+12,0x80000000u,"each opcode rejects unknown flags");
        bad32(off+40,0,"each opcode rejects unused or invalid auxiliary fields");
        if(c.target_index==kRoomNoIndex)bad32(off+8,0,"opcode rejects unused target");
        else bad32(off+8,kRoomNoIndex,"opcode rejects missing required target");
        if(c.actor_index!=kRoomNoActor&&c.opcode!=5)bad16(off+2,kRoomNoActor,"opcode rejects missing required actor");
        if(c.opcode==27) {auto b=base;putfloat(b,off+16,0);putfloat(b,off+20,0);rejected(std::move(b),"direction command rejects zero vector");}
        if(c.opcode==29)bad32(off+8,v.movement_path_count(),"move path index outside table");
        if(c.opcode==31||c.opcode==37) {auto b=base;putdouble(b,off+24,2);rejected(std::move(b),"set flag rejects nonboolean value");}
        if(c.opcode==32)bad32(off+8,0,"dialogue stable identity cannot be zero");
    }
    for(unsigned i=0;i<42;++i)check(opcode_seen[i],"actual program covers every supported opcode validation path");
    // A forward branch must not carry a live phrase timer, even when a later
    // textual wait would balance the linear command stream. Such a branch can
    // skip the consuming wait at runtime.
    bool branch_timer_checked=false;
    for(uint32_t i=0;i<v.program_count();++i) {
        const auto p=v.program(i);
        for(uint32_t j=1;j<p.command_count;++j) {
            if(v.command(p.first_command+j).opcode!=36||v.command(p.first_command+j-1).opcode!=33)continue;
            uint32_t later=j+1;
            while(later<p.command_count&&v.command(p.first_command+later).opcode!=33)++later;
            if(later==p.command_count)continue;
            auto b=base;const auto start=row(base,RoomSection::Command,p.first_command+j-1);
            put16(b,start,3);put32(b,start+12,0);putdouble(b,start+32,1);
            put32(b,row(base,RoomSection::Command,p.first_command+later)+12,1);
            rejected(std::move(b),"branch rejects unresolved timer even with later consuming wait");
            branch_timer_checked=true;break;
        }
        if(branch_timer_checked)break;
    }
    check(branch_timer_checked,"source branch fixture exercises timer-transfer rejection");
    check(v.movement_path_count()>0&&v.movement_path_entry_count()>0,"source movement paths present");
    check(v.movement_path(v.movement_path_count()).stable_id==0&&v.movement_path_entry(v.movement_path_entry_count()).duration==0,"movement accessor bounds");
    const auto path=row(base,RoomSection::MovementPath),entry=row(base,RoomSection::MovementPathEntry);
    bad32(path,0,"zero path stable identity");bad32(path+4,kRoomNoIndex,"path entry span overflow");bad16(path+8,0,"empty movement path");bad16(path+8,17,"movement path entry capacity");
    bad16(path+10,16,"unknown movement path flag");bad16(path+12,3,"unsupported path animation motion");bad16(path+14,1,"path padding");
    {auto b=base;putdouble(b,path+16,0);rejected(std::move(b),"path nonpositive speed");}
    {auto b=base;putdouble(b,path+16,std::numeric_limits<double>::quiet_NaN());rejected(std::move(b),"path NaN speed");}
    bad16(entry,2,"unknown path entry kind");bad16(entry+2,1,"path entry u16 padding");bad32(entry+4,1,"path entry u32 padding");
    {auto b=base;putfloat(b,entry+8,std::numeric_limits<float>::infinity());rejected(std::move(b),"path nonfinite vector");}
    for(uint32_t i=0;i<v.movement_path_entry_count();++i) {
        const auto e=v.movement_path_entry(i);const auto off=row(base,RoomSection::MovementPathEntry,i);
        auto b=base;
        if(e.kind==0) {putdouble(b,off+16,1);rejected(std::move(b),"move entry unused duration");}
        else {putdouble(b,off+16,0);rejected(std::move(b),"wait entry requires positive duration");b=base;putfloat(b,off+8,1);rejected(std::move(b),"wait entry unused vector");}
    }
    if(v.movement_path_count()>1)bad32(row(base,RoomSection::MovementPath,1)+4,v.movement_path(0).first_entry,"path entry ownership overlap");
    for(uint32_t i=0;i<v.binding_count();++i)if(v.binding(i).kind==uint16_t(RoomBindingKind::StopRoomShaker)) {
        const auto off=row(base,RoomSection::Binding,i);
        bad32(off+8,kRoomNoIndex,"stop shaker requires target binding");bad32(off+8,i,"stop shaker cannot target itself");
        auto b=base;putdouble(b,off+24,1);rejected(std::move(b),"stop shaker duration unused");
    }
    for(uint32_t i=0;i<v.resource_count();++i)if(v.resource(i).kind==uint16_t(RoomResourceKind::CheckedBattlePack)) {
        const auto off=row(base,RoomSection::Resource,i);
        bad16(off+8,1,"battle resource dimensions unused");bad16(off+12,1,"battle resource grid unused");
        auto b=base;std::fill(b.begin()+off+24,b.begin()+off+56,0);rejected(std::move(b),"battle resource needs fingerprint");
        b=base;const auto string_offset=get32(b,strings+size_t(v.resource(i).path_string)*8);b[string_offset]='/';rejected(std::move(b),"battle resource rejects absolute path");
    }
    for(uint32_t i=0;i<v.battle_count();++i)if(!v.string(v.battle(i).win_cutscene_string).empty()) {
        auto b=base;const auto off=get32(b,strings+size_t(v.battle(i).win_cutscene_string)*8);b[off]='/';rejected(std::move(b),"battle continuation rejects absolute path");
    }
    bool delayed_clip=false;
    for(uint32_t i=0;i<v.clip_count();++i)if(v.clip(i).flags&16) {
        delayed_clip=true;const auto off=row(base,RoomSection::Clip,i);
        bad8(off+14,v.clip(i).flags&~16u,"delayed first key requires preserve flag");
        auto b=base;putfloat(b,row(base,RoomSection::Key,v.clip(i).first_key),0);rejected(std::move(b),"preserve flag requires delayed first key");
    }
    check(delayed_clip,"source delayed first-key clips present");
    bool npc_profile=false;
    for(uint32_t i=0;i<v.actor_profile_count();++i)if(v.actor_profile(i).execution_kind==3) {
        npc_profile=true;const auto off=row(base,RoomSection::ActorProfile,i);const auto a=v.actor_profile(i);
        check(a.direction_count==4&&a.animation_binding_count==12,"NPC cardinal idle walk talk coverage");
        bad16(off+68,8,"NPC direction count rejected");bad16(off+24,11,"NPC incomplete animation coverage");
        bad8(row(base,RoomSection::AnimationBinding,a.animation_binding_first)+3,4,"NPC noncardinal animation rejected");
    }
    check(npc_profile,"source directional NPC present");
    for(auto section:{RoomSection::Overlay,RoomSection::MapDraw}) {
        auto b=base;const auto d=dir(section);const auto start=get32(b,d+4),size=get32(b,d+16);
        std::fill(b.begin()+start,b.begin()+start+size,0);put32(b,d+4,0);put32(b,d+8,0);put32(b,d+16,0);repair(b);
        RoomData empty;check(empty.load(b.data(),b.size(),error)&&empty.view().count(section)==0,"valid optional empty table");
        put32(b,d+4,start);rejected(std::move(b),"noncanonical empty section offset");
    }
    // CRC-correct arbitrary mutations exercise parser paths under sanitizers.
    uint32_t random=0x938ba721;
    for(unsigned i=0;i<256;++i) {
        random^=random<<13;random^=random>>17;random^=random<<5;
        auto b=base;b[random%b.size()]^=uint8_t(1u<<(random%8));repair(b);
        RoomData fuzz;const bool accepted=fuzz.load(b.data(),b.size(),error);
        check(!accepted||fuzz.view().valid(),"CRC-correct mutation has safe outcome");
    }
    // Same compiled loader accepts and reports content changes from the blob.
    auto changed=base;const float new_x=v.scene().spawn.x+3.25f;putfloat(changed,scene+32,new_x);
    const float old_sort=v.overlay(0).sort_y;putfloat(changed,row(base,RoomSection::Overlay)+16,old_sort+2.5f);repair(changed);
    check(data.load(changed.data(),changed.size(),error),"valid data-only changed room loads");
    check(data.view().scene().spawn.x==new_x&&data.view().overlay(0).sort_y==old_sort+2.5f,"changed content decoded without engine fallback");
    changed[scene+32]^=255;
    check(data.view().scene().spawn.x==new_x,"owner buffer is immutable through source mutation");
    check(data.load(data.view().bytes(),data.byte_size(),error),"aliased successful reload remains safe");
    check(data.load_file(argv[1],error),"file loading transaction restores requested external room");
    std::printf("RoomData: %u checks, %u failures; pack=%zu bytes, views=%zu bytes, owner=%zu bytes\n",checks,failures,base.size(),sizeof(RoomView),sizeof(RoomData));
    return failures?1:0;
}
