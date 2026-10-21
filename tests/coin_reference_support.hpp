#pragma once
#include "encore/coins.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
using namespace encore::upstream;

namespace coin_reference {
inline size_t checks=0;inline std::string error,label;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"Coin reference "<<__LINE__<<" "<<label<<": "<<#x<<" ("<<error<<")\n";std::exit(1);}}while(0)
template<class T>T read(std::istream&in){T v{};in>>v;CHECK(bool(in));return v;}
inline float real(uint32_t bits){float v;std::memcpy(&v,&bits,4);return v;}
inline uint32_t bits(float v){uint32_t b;std::memcpy(&b,&v,4);return b;}
inline uint64_t bits(double v){uint64_t b;std::memcpy(&b,&v,8);return b;}
inline Vec2 point(std::istream&in){return {read<float>(in),read<float>(in)};}
inline Vec2 unit(Vec2 a,Vec2 b){const float x=b.x-a.x,y=b.y-a.y,d=std::sqrt(x*x+y*y);return {y/d,-x/d};}
inline CircleSolid solid(std::istream&in){
 CircleSolid s;s.kind=CircleSolidKind(read<unsigned>(in));const auto count=read<unsigned>(in);CHECK(count>=2&&count<=64);for(unsigned n=0;n<count;++n)s.points.push_back(point(in));s.minimum=s.maximum=s.points[0];for(auto p:s.points){s.minimum.x=std::min(s.minimum.x,p.x);s.minimum.y=std::min(s.minimum.y,p.y);s.maximum.x=std::max(s.maximum.x,p.x);s.maximum.y=std::max(s.maximum.y,p.y);}
 if(s.kind==CircleSolidKind::Rectangle){s.center={(s.minimum.x+s.maximum.x)/2,(s.minimum.y+s.maximum.y)/2};s.extents={(s.maximum.x-s.minimum.x)/2,(s.maximum.y-s.minimum.y)/2};s.points.clear();}
 else if(s.kind==CircleSolidKind::Segment)s.normals.push_back(unit(s.points[0],s.points[1]));
 else{double area=0;for(size_t n=0;n<s.points.size();++n){const auto a=s.points[n],b=s.points[(n+1)%s.points.size()];area+=double(a.x)*b.y-double(a.y)*b.x;}if(area<0)std::reverse(s.points.begin(),s.points.end());for(size_t n=0;n<s.points.size();++n)s.normals.push_back(unit(s.points[n],s.points[(n+1)%s.points.size()]));}
 CHECK(CircleMotionSolver::validate(s));return s;
}
inline void point(std::istream&in,Vec2 actual,float tolerance=0){const auto x=read<uint32_t>(in),y=read<uint32_t>(in);const Vec2 expected{real(x),real(y)};if(actual.x!=expected.x||actual.y!=expected.y){std::cerr.precision(10);std::cerr<<label<<" vector actual "<<actual.x<<' '<<actual.y<<" expected "<<expected.x<<' '<<expected.y<<'\n';}CHECK(std::abs(actual.x-expected.x)<=tolerance&&std::abs(actual.y-expected.y)<=tolerance);if(tolerance)CHECK(std::round(actual.x)==std::round(expected.x)&&std::round(actual.y)==std::round(expected.y));}
inline CoinServices services(const CoinsData&d,SessionSnapshot&session,SourceRandom&random){CoinServices s;s.epoch=71;s.session=&session;s.random=&random;s.hud_ready=true;s.owners.insert(17);s.sounds={d.info().drop_audio,d.info().collect_audio};s.bodies={{1,CoinBodyKind::PartyObject},{2,CoinBodyKind::Other}};s.areas={{3,0}};return s;}
inline void state(std::istream&in,const CoinActor&a,float tolerance){const auto&s=a.snapshot();point(in,s.position,tolerance);CHECK(bits(s.speed)==read<uint64_t>(in));point(in,s.direction);CHECK(bits(s.air_time)==read<uint64_t>(in));CHECK(bits(s.air_time)==read<uint64_t>(in));CHECK(s.grounded==bool(read<unsigned>(in)));CHECK(s.physics==bool(read<unsigned>(in)));CHECK(s.body_disabled==bool(read<unsigned>(in)));CHECK(s.pickup_disabled==bool(read<unsigned>(in)));point(in,s.offset);CHECK(s.frame==read<uint32_t>(in));CHECK(s.sparkle==read<uint32_t>(in));const auto glow=read<uint64_t>(in);if(bits(s.glow)!=glow)std::cerr<<label<<" glow actualbits "<<bits(s.glow)<<" expectedbits "<<glow<<'\n';CHECK(bits(s.glow)==glow);CHECK(s.animation==read<uint32_t>(in));CHECK(s.playing==bool(read<unsigned>(in)));CHECK(bits(s.animation_time)==read<uint32_t>(in));CHECK(s.queued==bool(read<unsigned>(in)));}
struct Contact{unsigned frame=0,kind=0;uint64_t identity=0;uint32_t layer=0;};
std::vector<uint64_t>fingerprint(const CoinActor&a){const auto&s=a.snapshot();return{bits(s.position.x),bits(s.position.y),bits(s.direction.x),bits(s.direction.y),bits(s.offset.x),bits(s.offset.y),bits(s.speed),bits(s.air_time),bits(s.glow),s.frame,s.sparkle,s.animation,bits(s.animation_time),s.grounded,s.physics,s.body_disabled,s.pickup_disabled,s.playing,s.queued,s.removed};}
struct RejectedSource:CircleSolidSource{bool collect(Vec2,Vec2,std::vector<CircleSolid>&)const override{return false;}};
}
