#pragma once
#include "story.hpp"
#include <array>
namespace encore {
enum Button : uint32_t { Up=1,Down=2,Left=4,Right=8,Confirm=16,Cancel=32 };
struct Input {uint32_t held=0,pressed=0;};
enum class Mode : uint8_t { World, Dialogue, Battle, Fault };
struct State {
    uint32_t map=0;int32_t x=0,y=0,hp=30,xp=0;
    uint32_t rng=0x13572468,tick=0;std::array<uint8_t,kMaxFlags> flags{};
};
// These battle rules are test-fixture rules, NOT Mother: Encore rules.
struct BattleState {uint32_t enemy=0;int32_t hp=0;uint32_t turns=0;std::string message;};
class Game final: public StoryHost {
    const Content& content_;State state_;Mode mode_=Mode::World;
    StoryVM vm_;BattleState battle_;uint32_t dialogue_=0;std::string error_;
    uint32_t random();void interact();void battle_action(bool attack);
public:
    explicit Game(const Content& content):content_(content){reset();}
    void reset();void tick(Input input);
    const Content& content() const{return content_;}
    const State& state() const{return state_;}
    const Map& map() const{return content_.maps[state_.map];}
    const BattleState& battle() const{return battle_;}
    const StoryVM& story() const{return vm_;}
    Mode mode() const{return mode_;}
    const std::string& dialogue() const{return content_.text(dialogue_);}
    const std::string& error() const{return error_;}
    bool can_save() const{return mode_==Mode::World&&!vm_.active();}
    bool encode_save(std::vector<uint8_t>& bytes,std::string& error) const;
    bool decode_save(const uint8_t* bytes,size_t size,std::string& error);
    std::string trace_json() const;
    bool busy() const override{return mode_!=Mode::World;}
    bool get_flag(uint32_t i) const override{return i<content_.flag_ids.size()&&state_.flags[i]!=0;}
    void set_flag(uint32_t i,bool v) override {if(i<content_.flag_ids.size())state_.flags[i]=v?1:0;}
    void say(uint32_t text) override{dialogue_=text;mode_=Mode::Dialogue;}
    void teleport(uint32_t map,int x,int y) override{state_.map=map;state_.x=x;state_.y=y;}
    void start_battle(uint32_t enemy) override;
    void story_error(const char* message) override{error_=message;mode_=Mode::Fault;}
};
}
