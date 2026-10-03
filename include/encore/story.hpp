#pragma once
#include "content.hpp"
namespace encore {
struct StoryHost {
    virtual ~StoryHost()=default;
    virtual bool busy() const=0;
    virtual bool get_flag(uint32_t index) const=0;
    virtual void set_flag(uint32_t index,bool value)=0;
    virtual void say(uint32_t text)=0;
    virtual void teleport(uint32_t map,int x,int y)=0;
    virtual void start_battle(uint32_t enemy)=0;
    virtual void story_error(const char* message)=0;
};
class StoryVM {
    const Program* program_=nullptr;uint32_t pc_=0,wait_=0;
public:
    bool active() const {return program_!=nullptr;}
    uint32_t pc() const {return pc_;}
    uint32_t wait_ticks() const {return wait_;}
    bool start(const Program& p) {if(active())return false;program_=&p;pc_=wait_=0;return true;}
    void cancel() {program_=nullptr;pc_=wait_=0;}
    void tick(StoryHost& host);
};
}
