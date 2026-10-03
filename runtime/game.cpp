#include "encore/game.hpp"
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
namespace encore {
void Game::reset(){state_={};state_.map=content_.start_map;state_.x=content_.spawn_x;state_.y=content_.spawn_y;mode_=Mode::World;vm_.cancel();battle_={};error_.clear();}
uint32_t Game::random(){uint32_t x=state_.rng;x^=x<<13;x^=x>>17;x^=x<<5;return state_.rng=x;}
void Game::interact(){
    const Object* nearest=nullptr;int distance=25*25;
    for(const auto& o:map().objects){int dx=o.x-state_.x,dy=o.y-state_.y,d=dx*dx+dy*dy;if(d<distance){distance=d;nearest=&o;}}
    if(nearest)vm_.start(content_.programs[nearest->program]);
}
void Game::start_battle(uint32_t enemy){battle_={};battle_.enemy=enemy;battle_.hp=content_.enemies[enemy].hp;battle_.message="A: attack   B: leave test battle";mode_=Mode::Battle;}
void Game::battle_action(bool attack){
    if(!attack){mode_=Mode::World;return;}
    const auto& e=content_.enemies[battle_.enemy];
    const int damage=std::max<int32_t>(1,7+int(random()%4)-e.defense);battle_.hp=std::max<int32_t>(0,battle_.hp-damage);++battle_.turns;
    if(battle_.hp==0){state_.xp=std::min<int32_t>(999999,state_.xp+e.reward);mode_=Mode::World;return;}
    const int hit=std::max<int32_t>(1,e.attack+int(random()%3)-2);state_.hp=std::max<int32_t>(0,state_.hp-hit);
    char text[96];std::snprintf(text,sizeof(text),"You hit %d. Training target hits %d.",damage,hit);battle_.message=text;
    if(state_.hp==0){state_.hp=30;teleport(content_.start_map,content_.spawn_x,content_.spawn_y);mode_=Mode::World;}
}
void Game::tick(Input in){
    ++state_.tick;
    if(mode_==Mode::Fault)return;
    if(mode_==Mode::Dialogue){if(in.pressed&(Confirm|Cancel))mode_=Mode::World;else return;}
    else if(mode_==Mode::Battle){if(in.pressed&Confirm)battle_action(true);else if(in.pressed&Cancel)battle_action(false);else return;}
    else if(!vm_.active()){
        const int dx=((in.held&Right)?2:0)-((in.held&Left)?2:0);
        const int dy=((in.held&Down)?2:0)-((in.held&Up)?2:0);
        // Intentionally axis-separated, point-foot collision for the M0 fixture.
        if(!map().blocked(state_.x+dx,state_.y))state_.x+=dx;
        if(!map().blocked(state_.x,state_.y+dy))state_.y+=dy;
        if(in.pressed&Confirm)interact();
    }
    vm_.tick(*this);
}
std::string Game::trace_json() const{
    char b[384];uint32_t flags=0;for(size_t i=0;i<content_.flag_ids.size();++i)flags|=uint32_t(state_.flags[i])<<i;
    std::snprintf(b,sizeof(b),"{\"tick\":%" PRIu32 ",\"map_id\":%" PRIu32 ",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"hp\":%" PRId32 ",\"xp\":%" PRId32 ",\"mode\":%u,\"flags\":%" PRIu32 ",\"rng\":%" PRIu32 ",\"vm_pc\":%" PRIu32 ",\"vm_wait\":%" PRIu32 ",\"vm_active\":%s,\"enemy_hp\":%" PRId32 "}",
        state_.tick,map().id,state_.x,state_.y,state_.hp,state_.xp,unsigned(mode_),flags,state_.rng,vm_.pc(),vm_.wait_ticks(),vm_.active()?"true":"false",battle_.hp);
    return b;
}
}
