#pragma once
#include <citro2d.h>
#include "encore/movement.hpp"
class OpeningActorRenderer {
public:
 template<class T>void draw_actor(const T&,float,float)const{draw_test::calls.push_back({9001,0,0,{}});}
 template<class T>void draw_player(encore::upstream::Vec2,const T&,float,float)const{draw_test::calls.push_back({9000,0,0,{}});}
};
