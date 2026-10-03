#include "encore/story.hpp"
namespace encore {
void StoryVM::tick(StoryHost& h) {
    if(!active()||h.busy())return;
    if(wait_){--wait_;return;}
    // A malformed loop must never freeze a frame. Synchronous work has a strict budget.
    for(unsigned budget=0;budget<64;++budget) {
        if(pc_>=program_->code.size()){cancel();h.story_error("Story fell off end without END");return;}
        const auto in=program_->code[pc_++];
        switch(in.op){
        case Op::End:cancel();return;
        case Op::Say:h.say(uint32_t(in.a));return;
        case Op::SetFlag:h.set_flag(uint32_t(in.a),in.b!=0);break;
        case Op::IfFlag:if(h.get_flag(uint32_t(in.a))==(in.b!=0))pc_=uint32_t(in.c);break;
        case Op::Jump:pc_=uint32_t(in.a);break;
        case Op::Wait:wait_=uint32_t(in.a);return;
        case Op::Teleport:h.teleport(uint32_t(in.a),in.b,in.c);return;
        case Op::Battle:h.start_battle(uint32_t(in.a));return;
        default:cancel();h.story_error("Unimplemented opcode");return;
        }
        if(h.busy())return;
    }
    cancel();h.story_error("Story exceeded 64 synchronous instructions");
}
}
