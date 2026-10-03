#include "room_fixture.hpp"
#include "encore/opening_trigger.hpp"
#include <cstdio>
#include <limits>
static bool overlap(encore::upstream::Vec2 p){return encore::upstream::first_trigger_overlap(encore_test::room(),0,p);}
int main(){
    if(overlap({520,397})||overlap({433,397})||overlap({432.01f,397})||overlap({432,397}))return 1;
    if(!overlap({431.99f,397})||!overlap({431,397}))return 2;
    if(overlap({420,350})||overlap({420,430})||overlap({std::numeric_limits<float>::quiet_NaN(),397}))return 3;
    std::puts("First-entry lamp trigger matches native edge samples; general exit hysteresis outside scope");
}
