// Manual SDK helper/admission checks. Does not initialize graphics, submit GPU
// work, or claim a rendered/emulator/hardware result.
#include "platform/ctr/basement_actor_renderer.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Basement renderer manual failure %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static std::string hex(const std::array<uint8_t,32>&bytes){const char*d="0123456789abcdef";std::string s;for(auto b:bytes){s+=d[b>>4];s+=d[b&15];}return s;}
int main(){
 using namespace basement_actor_renderer_detail;
 CHECK(hex(sha256(nullptr,0))=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 const char*abc="abc";CHECK(hex(sha256(reinterpret_cast<const uint8_t*>(abc),std::strlen(abc)))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 const char*longer="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";CHECK(hex(sha256(reinterpret_cast<const uint8_t*>(longer),std::strlen(longer)))=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
 encore::upstream::BasementActorResource r;r.width=512;r.height=448;r.columns=8;r.rows=7;r.position={0,11};r.offset={0,-15};
 // The actual 448px content occupies 448/512 of the padded hardware page.
 // Derive cell UVs from the decoder's actual content bounds, never 1/rows.
 Tex3DS_SubTexture page{512,448,0,1,1,.125f},cell{};
 CHECK(frame_region(r,52,page,cell));CHECK(cell.width==64&&cell.height==64&&cell.left==.5f&&cell.right==.625f&&cell.top==.25f&&cell.bottom==.125f);
 CHECK(!frame_region(r,56,page,cell));auto invalid=page;invalid.width=511;CHECK(!frame_region(r,0,invalid,cell));invalid=page;invalid.left=NAN;CHECK(!frame_region(r,0,invalid,cell));invalid=page;invalid.bottom=invalid.top;CHECK(!frame_region(r,0,invalid,cell));
 encore::upstream::Vec2 pose;CHECK(origin(r,{128,1040},{0,900},pose));CHECK(pose.x==96&&pose.y==104);CHECK(!origin(r,{NAN,1040},{0,900},pose));CHECK(!origin(r,{128,1040},{INFINITY,900},pose));
 // Present width120 occupies120/128 of its t3x page: no rescale/padding drift.
 r.width=120;r.height=30;r.columns=5;r.rows=1;r.position={};r.offset={};page={120,30,0,1,.9375f,.0625f};CHECK(frame_region(r,4,page,cell));CHECK(cell.width==24&&cell.height==30&&cell.left==.75f&&cell.right==.9375f);CHECK(origin(r,{128,1016},{0,900},pose)&&pose.x==116&&pose.y==101);
 BasementActorRenderer renderer;std::string error;CHECK(!renderer.ready());CHECK(!renderer.draw(999,0,{},{},0,error));CHECK(!renderer.draw(1,0,{},{},NAN,error));encore::upstream::BasementActorData data;CHECK(!renderer.load(data,"romfs:/",error)&&!renderer.ready());renderer.free();renderer.free();
 std::puts("Manual basement renderer SHA/grid/offset/negative admission checks passed; GPU not exercised");
}
