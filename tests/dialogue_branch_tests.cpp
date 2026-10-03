#include "encore/dialogue.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

using namespace encore::upstream;
namespace {
unsigned checks=0,failures=0;
void check(bool value,const char* label) {
    ++checks;
    if(!value){++failures;std::fprintf(stderr,"FAIL: %s\n",label);}
}
void require(bool value,const char* label) { check(value,label);if(!value)std::exit(1); }
void put16(std::vector<uint8_t>& bytes,size_t at,uint16_t value) {
    bytes[at]=uint8_t(value);bytes[at+1]=uint8_t(value>>8);
}
void put32(std::vector<uint8_t>& bytes,size_t at,uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes[at+i]=uint8_t(value>>(8*i));
}
void putdouble(std::vector<uint8_t>& bytes,size_t at,double value) {
    uint64_t bits;std::memcpy(&bits,&value,sizeof(bits));
    put32(bytes,at,uint32_t(bits));put32(bytes,at+4,uint32_t(bits>>32));
}
using Kind=DialogueActionKind;
RoomCommand command(Kind kind,uint32_t phrase=0,uint32_t target=kRoomNoIndex,
                    uint32_t auxiliary=kRoomNoIndex,double value=0,double duration=0) {
    RoomCommand out;out.opcode=uint16_t(kind);out.phrase=phrase;out.target_index=target;
    out.auxiliary_index=auxiliary;out.value=value;out.duration=duration;return out;
}
RoomCommand done(uint32_t phrase) { return command(Kind::DialogueDone,phrase,kRoomNoIndex,kRoomNoIndex,0,.5); }
// In-memory synthetic instructions exercise mechanism only. No generated pack
// is patched on disk and every runnable fixture passes the real checked loader.
struct Fixtures {
    std::vector<uint8_t> base;
    RoomData source;
    uint32_t program_index=kRoomNoIndex,leader_string=kRoomNoIndex;
    RoomProgram program;
    size_t command_offset=0;
    explicit Fixtures(const char* path) {
        std::string error;
        const bool read=encore::read_file(path,base,kMaxRoomBytes,error);
        require(read,error.c_str());
        const bool loaded=source.load(base.data(),base.size(),error);
        require(loaded,error.c_str());
        const auto room=source.view();
        // Nonzero first_command detects accidentally absolute branch PCs.
        for(uint32_t i=0;i<room.program_count();++i) {
            const auto p=room.program(i);
            if(p.first_command&&p.command_count>=40&&p.phrase_count>=5){program_index=i;program=p;break;}
        }
        require(program_index!=kRoomNoIndex,"fixture has a nonzero-origin program with sufficient space");
        command_offset=room.section_offset(RoomSection::Command)+size_t(program.first_command)*48;
        for(uint32_t i=0;i<room.string_count();++i) {
            const auto s=room.string(i);
            if(s.empty()||s.size()>64||s.front()<'a'||s.front()>'z')continue;
            const bool identifier=std::all_of(s.begin(),s.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';});
            if(identifier){leader_string=i;break;}
        }
        require(leader_string!=kRoomNoIndex&&room.flag_count(),"fixture has checked condition references");
    }
    std::vector<uint8_t> bytes(const std::vector<RoomCommand>& commands) const {
        require(commands.size()<program.command_count,"synthetic program fits existing command ownership");
        auto out=base;
        // Retain the checked source version. The untouched appended records
        // may require newer rules; changing just the header is not a legacy fixture.
        for(uint32_t i=0;i<program.command_count;++i) {
            auto c=command(Kind::YieldIdle,program.phrase_count-1);
            if(i<commands.size())c=commands[i];
            if(i+1==program.command_count)c=done(program.phrase_count-1);
            const size_t at=command_offset+size_t(i)*48;
            std::fill(out.begin()+at,out.begin()+at+48,0);
            put16(out,at,c.opcode);put16(out,at+2,c.actor_index);
            put32(out,at+4,c.phrase);put32(out,at+8,c.target_index);put32(out,at+12,c.flags);
            putdouble(out,at+24,c.value);putdouble(out,at+32,c.duration);put32(out,at+40,c.auxiliary_index);
        }
        repair(out);return out;
    }
    static void repair(std::vector<uint8_t>& bytes) {
        put32(bytes,52,0);put32(bytes,52,encore::crc32(bytes.data(),bytes.size()));
    }
    RoomData load(const std::vector<RoomCommand>& commands) const {
        const auto payload=bytes(commands);RoomData result;std::string error;
        const bool loaded=result.load(payload.data(),payload.size(),error);
        require(loaded,error.c_str());return result;
    }
    bool rejects(std::vector<uint8_t> payload) const {
        repair(payload);RoomData data;std::string error;
        return !data.load(payload.data(),payload.size(),error)&&!error.empty();
    }
};
struct LogSink : DialogueSink {
    std::vector<DialogueAction> actions;
    Kind reject=Kind::Jump;
    bool apply(const DialogueAction& action) override {
        if(action.kind==reject)return false;
        actions.push_back(action);return true;
    }
};
struct Sink final : LogSink {
    bool flag=false,query_error=false;
    uint32_t leader=kRoomNoIndex;
    std::vector<DialogueAction> queries;
    bool branch_condition(const DialogueAction& action,bool& matched) override {
        queries.push_back(action);
        if(query_error)return false;
        if(action.kind==Kind::BranchFlag){matched=flag==(action.value!=0);return true;}
        if(action.kind==Kind::BranchLeader){matched=leader==action.target_index;return true;}
        return false;
    }
};
std::vector<RoomCommand> conditional(Kind kind,uint32_t target,double value=0) {
    return {command(kind,0,target,3,value),command(Kind::BeginCutscene,1),done(1),
            command(Kind::BeginCutscene,2),done(2)};
}
std::vector<RoomCommand> choice_program() {
    auto show=command(Kind::ShowDialogue,0,1);show.flags=1;
    return {command(Kind::BeginCutscene),show,command(Kind::AwaitChoices,0,0),
            command(Kind::OpenSave,1),command(Kind::AwaitSubmenu,1),
            command(Kind::BranchFlag,1,0,8,1),command(Kind::Jump,1,10),
            command(Kind::YieldIdle,1),command(Kind::BeginCutscene,2),done(2),
            command(Kind::BeginCutscene,3),done(3)};
}
}

int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"Usage: encore_dialogue_branch_tests room.encroom\n");return 2;}
    Fixtures fixture(argv[1]);
    static_assert(uint16_t(Kind::Jump)==36&&uint16_t(Kind::BranchFlag)==37&&uint16_t(Kind::BranchLeader)==38&&
                  uint16_t(Kind::AwaitChoices)==39&&uint16_t(Kind::OpenSave)==40&&uint16_t(Kind::AwaitSubmenu)==41,
                  "Checked branch opcode identities are stable");
    for(const auto kind:{Kind::Jump,Kind::BranchFlag,Kind::BranchLeader,Kind::AwaitChoices,Kind::OpenSave,Kind::AwaitSubmenu})
        check(std::strcmp(dialogue_action_name(kind),"Unknown")!=0,"new opcodes have diagnostic names");
    for(bool actual:{false,true})for(bool expected:{false,true}) {
        auto data=fixture.load(conditional(Kind::BranchFlag,0,expected?1:0));
        DialoguePlayer player;Sink sink;sink.flag=actual;
        check(player.start(data.view(),fixture.program_index,sink,17)&&player.status()==DialogueStatus::Completed,"flag branch reaches a terminal source path");
        check(sink.queries.size()==1&&sink.actions.size()==2,"flag query is separate from applied actions");
        check(sink.actions[0].phrase==(actual==expected?2u:1u),"flag true and false comparisons select correct path");
        check(player.phrase()==sink.actions[0].phrase&&!player.active(),"early source terminator stops before other branches");
    }
    for(bool matched:{false,true}) {
        auto data=fixture.load(conditional(Kind::BranchLeader,fixture.leader_string));
        DialoguePlayer player;Sink sink;sink.leader=matched?fixture.leader_string:kRoomNoIndex;
        check(player.start(data.view(),fixture.program_index,sink,17)&&player.status()==DialogueStatus::Completed,"leader branch completes");
        check(sink.actions.size()==2&&sink.actions[0].phrase==(matched?2u:1u),"leader identity match controls branch");
        check(sink.queries.size()==1&&sink.queries[0].target_index==fixture.leader_string,"leader query preserves checked string reference");
    }
    for(const auto kind:{Kind::BranchFlag,Kind::BranchLeader}) {
        auto data=fixture.load(conditional(kind,kind==Kind::BranchFlag?0:fixture.leader_string));
        DialoguePlayer player;Sink sink;sink.query_error=true;
        check(!player.start(data.view(),fixture.program_index,sink,17)&&player.status()==DialogueStatus::Error,"query error fails closed");
        check(sink.actions.empty()&&!player.active()&&std::strlen(player.error()),"query error never falls through as a false condition");
        DialoguePlayer unsupported;LogSink old_sink;
        check(!unsupported.start(data.view(),fixture.program_index,old_sink,17)&&unsupported.status()==DialogueStatus::Error&&old_sink.actions.empty(),"existing sink default rejects conditional programs");
    }
    {
        auto data=fixture.load({command(Kind::Jump,0,3),command(Kind::BeginCutscene,1),done(1),command(Kind::BeginCutscene,2),done(2)});
        DialoguePlayer player;Sink sink;
        check(player.start(data.view(),fixture.program_index,sink,17)&&player.status()==DialogueStatus::Completed,"jump uses program-relative PC");
        check(sink.actions.size()==2&&sink.actions[0].phrase==2&&sink.queries.empty(),"jump skips alternative without applying an action or query");
    }
    for(bool saved:{false,true}) {
        auto data=fixture.load(choice_program());DialoguePlayer player;Sink sink;
        require(player.start(data.view(),fixture.program_index,sink,17),"choice program starts");
        check(player.status()==DialogueStatus::AwaitChoices&&player.active()&&!player.input_allowed(),"choices suspend with ordinary dialogue input disabled");
        check(sink.actions.size()==3&&sink.actions.back().kind==Kind::AwaitChoices&&sink.actions.back().target_index==0,"choice group prepared exactly once");
        const auto count=sink.actions.size();
        check(!player.dialogue_finished(sink)&&!player.dialogue_finished(sink,true)&&!player.actor_ready(0,17,sink)&&!player.submenu_closed(17,sink),"text actor and submenu callbacks cannot resume choices");
        check(!player.choices_selected(3,16,sink)&&!player.choices_selected(fixture.program.command_count,17,sink)&&!player.choices_selected(kRoomNoIndex,17,sink),"stale and invalid choice targets rejected");
        for(unsigned i=0;i<12;++i)require(player.idle_begin(sink)&&player.idle_process(.25,sink),"choices allow ordinary idle phase progression");
        check(player.status()==DialogueStatus::AwaitChoices&&sink.actions.size()==count,"no choice continuation occurs before selected callback");
        require(player.choices_selected(3,17,sink),"selected Record target resumes");
        check(player.status()==DialogueStatus::AwaitSubmenu&&sink.actions.size()==count+1&&sink.actions.back().kind==Kind::OpenSave,"save request applies once then suspends before flag query");
        check(sink.queries.empty(),"save result flag is not read before submenu close");
        check(!player.choices_selected(10,17,sink)&&!player.submenu_closed(16,sink)&&!player.dialogue_finished(sink)&&!player.actor_ready(0,17,sink),"submenu rejects unrelated or stale callbacks");
        for(unsigned i=0;i<12;++i)require(player.idle_begin(sink)&&player.idle_process(.25,sink),"submenu allows idle phase progression");
        check(player.status()==DialogueStatus::AwaitSubmenu&&sink.actions.size()==count+1&&sink.queries.empty(),"hidden text and elapsed time cannot resume submenu");
        sink.flag=saved;require(player.submenu_closed(17,sink),"explicit submenu close resumes");
        check(player.status()==DialogueStatus::Completed&&sink.queries.size()==1&&player.phrase()==(saved?2u:3u),"post-close branch reads current saved flag");
        const auto terminal=sink.actions.size();
        check(!player.submenu_closed(17,sink)&&!player.choices_selected(3,17,sink)&&player.idle_begin(sink)&&player.idle_process(.25,sink)&&sink.actions.size()==terminal,"terminal callbacks cannot replay actions");
    }
    {
        auto data=fixture.load(choice_program());DialoguePlayer player;Sink sink;
        require(player.start(data.view(),fixture.program_index,sink,18),"cancel-target choice starts");
        require(player.choices_selected(10,18,sink),"Nothing/cancel target resumes directly");
        check(player.status()==DialogueStatus::Completed&&player.phrase()==3&&sink.actions.size()==5&&sink.queries.empty(),"non-save choice skips save request and flag mutation");
    }
    for(bool submenu:{false,true}) {
        auto data=fixture.load(choice_program());DialoguePlayer player;Sink sink;
        require(player.start(data.view(),fixture.program_index,sink,19),"cancellation starts");
        if(submenu)require(player.choices_selected(3,19,sink),"cancellation opens submenu");
        player.cancel();const auto count=sink.actions.size();
        check(player.status()==DialogueStatus::Cancelled&&!player.active()&&!player.choices_selected(3,19,sink)&&!player.submenu_closed(19,sink),"cancel invalidates every menu continuation");
        check(player.idle_begin(sink)&&player.idle_process(.25,sink)&&sink.actions.size()==count,"cancelled menu has no delayed actions");
    }
    for(const auto rejected:{Kind::AwaitChoices,Kind::OpenSave}) {
        auto data=fixture.load(choice_program());DialoguePlayer player;Sink sink;sink.reject=rejected;
        const bool started=player.start(data.view(),fixture.program_index,sink,20);
        if(rejected==Kind::AwaitChoices)check(!started&&player.status()==DialogueStatus::Error,"choice preparation rejection fails closed");
        else check(started&&!player.choices_selected(3,20,sink)&&player.status()==DialogueStatus::Error,"save request rejection never reaches submenu wait");
        check(!player.choices_selected(10,20,sink)&&!player.submenu_closed(20,sink),"rejected menu cannot resume");
    }
    {
        // Existing zero-duration completion markers are preserved in linear
        // programs; they do not end execution until the final positive ending.
        auto data=fixture.load({command(Kind::DialogueDone),command(Kind::BeginCutscene,1),command(Kind::AwaitDialogue,1)});
        DialoguePlayer player;Sink sink;
        check(player.start(data.view(),fixture.program_index,sink,21)&&player.status()==DialogueStatus::AwaitDialogue&&sink.actions.size()==2,"legacy nonterminal DialogueDone marker still continues");
    }
    {
        std::vector<RoomCommand> commands(33,command(Kind::BeginCutscene));
        auto data=fixture.load(commands);DialoguePlayer player;Sink sink;
        check(!player.start(data.view(),fixture.program_index,sink,22)&&player.status()==DialogueStatus::Error&&sink.actions.size()==32,"one resume is bounded at exactly 32 instructions");
        check(std::strstr(player.error(),"instruction limit")!=nullptr,"instruction budget failure is diagnosed");
    }
    {
        auto bad=fixture.bytes({command(Kind::Jump,0,3),command(Kind::BeginCutscene,1),done(1),done(2)});
        put32(bad,fixture.command_offset+8,0);
        check(fixture.rejects(bad),"backward branch loop rejected by checked loader");
        put32(bad,fixture.command_offset+8,fixture.program.command_count);
        check(fixture.rejects(bad),"out-of-program jump rejected by checked loader");
        bad=fixture.bytes(conditional(Kind::BranchFlag,0,1));
        put32(bad,fixture.command_offset+40,fixture.program.command_count);
        check(fixture.rejects(bad),"out-of-program conditional target rejected by checked loader");
        bad=fixture.bytes({command(Kind::BeginCutscene)});
        put16(bad,fixture.command_offset+size_t(fixture.program.command_count-1)*48,uint16_t(Kind::BeginCutscene));
        putdouble(bad,fixture.command_offset+size_t(fixture.program.command_count-1)*48+32,0);
        check(fixture.rejects(bad),"program with no terminator rejected by checked loader");
    }
    std::printf("Dialogue branch scheduler: %u checks, %s\n",checks,failures?"FAILED":"passed");
    return failures?1:0;
}
