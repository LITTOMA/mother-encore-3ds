#include "encore/field_data.hpp"
#include "encore/field_scene.hpp"
#include "encore/world_links.hpp"
#include "encore/room_data.hpp"
#include "encore/collision.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include "fixtures/segment_v0362.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<"Field data check "<<checks<<": "<<why<<'\n';std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(i*8));}
void put16(std::vector<uint8_t>&b,size_t p,uint16_t n){b[p]=uint8_t(n);b[p+1]=uint8_t(n>>8);}
void put_float(std::vector<uint8_t>&b,size_t p,float value){uint32_t bits;std::memcpy(&bits,&value,4);put(b,p,bits);}
void rehash(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
size_t section(const std::vector<uint8_t>&b,uint32_t kind){return get(b,64+(kind-1)*16+4);}
uint32_t count(const std::vector<uint8_t>&b,uint32_t kind){return get(b,64+(kind-1)*16+8);}

// Minimal spatial source: one horizontal segment wall at y=0 from x=-32..32.
struct WallSource final:MotionObstacleSource {
    Vec2 points[2]{{-32,0},{32,0}};Vec2 normals[2]{{0,1},{0,0}};
    bool collect(Vec2,Vec2,std::vector<MotionObstacle>&out)const override{out.push_back({points,normals,2,{},{-32,0},{32,0},true});return true;}
};
}
int main(int argc,char**argv){
    check(argc==5,"podunk.encmap, world.enclinks, podunk.encroom and opening.encintro paths required");
    std::string error;
    std::vector<uint8_t> map_bytes,link_bytes;
    check(encore::read_file(argv[1],map_bytes,8u*1024u*1024u,error),error.c_str());
    check(encore::read_file(argv[2],link_bytes,64*1024,error),error.c_str());

    // ENCMAP01 admission and transactional rejection.
    FieldMapData map;check(map.load(map_bytes.data(),map_bytes.size(),error),error.c_str());
    const auto view=map.view();const auto layers=view.count(FieldMapSection::Layers),cells=view.count(FieldMapSection::Cells);
    check(view.scene_id()!=0&&layers>0&&cells>0&&view.count(FieldMapSection::Groups)>0,"checked field map content");
    check(view.count(static_cast<FieldMapSection>(0))==0&&view.count(static_cast<FieldMapSection>(99))==0&&view.text({UINT32_MAX,1}).empty(),"unknown lookups are empty");
    auto reject=[&](std::vector<uint8_t>b,const char*why,bool fix=true){if(fix)rehash(b);check(!map.load(b.data(),b.size(),error),why);
        check(map.view().count(FieldMapSection::Layers)==layers&&map.view().count(FieldMapSection::Cells)==cells,"failed map admission preserves previous bytes");};
    for(size_t size:{size_t(0),size_t(63),size_t(64+21*16-1),map_bytes.size()-1})check(!map.load(map_bytes.data(),size,error),"truncated map rejected");
    check(!map.load(nullptr,map_bytes.size(),error),"null map rejected");
    {auto b=map_bytes;b[0]^=1;reject(b,"unknown map magic");}
    for(size_t field:{size_t(8),size_t(20),size_t(28),size_t(52),size_t(56),size_t(60)}){auto b=map_bytes;put(b,field,get(b,field)+1);reject(b,"unknown map version/capability/reserved/section count");}
    {auto b=map_bytes;put(b,24,0);reject(b,"zero scene identity rejected");}
    {auto b=map_bytes;b[40]^=1;reject(b,"wrong source pin rejected");}
    {auto b=map_bytes;b.back()^=1;reject(b,"map CRC mismatch",false);}
    {auto b=map_bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b,"trailing map bytes rejected");}
    {auto b=map_bytes;put(b,64+12,2);reject(b,"wrong section stride rejected");}
    {auto b=map_bytes;put(b,64+16*1,get(b,64+16*1)+1);reject(b,"reordered section kind rejected");}
    {auto b=map_bytes;put(b,64+16*1+4,get(b,64+16*1+4)+2);reject(b,"misaligned section rejected");}
    const size_t atlases=section(map_bytes,2),tiles=section(map_bytes,6),chunks=section(map_bytes,11),cell_rows=section(map_bytes,12),items=section(map_bytes,14),groups=section(map_bytes,15);
    {auto b=map_bytes;const auto len=get(b,atlases+4);b[section(b,1)+get(b,atlases)+len-1]^=1;reject(b,"non-t3x atlas path rejected");}
    {auto b=map_bytes;put16(b,atlases+8,1000);reject(b,"non power-of-two atlas rejected");}
    {auto b=map_bytes;put(b,tiles,count(b,4));reject(b,"tile picture outside table rejected");}
    {auto b=map_bytes;put_float(b,tiles+8,NAN);reject(b,"nonfinite tile offset rejected");}
    {auto b=map_bytes;put16(b,cell_rows+4,uint16_t(count(b,6)));reject(b,"cell tile outside table rejected");}
    {auto b=map_bytes;put16(b,cell_rows,uint16_t(int16_t(get(b,chunks)&0xffff)*16+40));reject(b,"cell outside its chunk rejected");}
    if(get(map_bytes,chunks+8)>=2){auto b=map_bytes;const auto x0=get(b,cell_rows)&0xffff,y0=get(b,cell_rows)>>16;put16(b,cell_rows+8,uint16_t(x0));put16(b,cell_rows+10,uint16_t(y0));reject(b,"duplicate/unordered cell rejected");}
    {auto b=map_bytes;put(b,chunks+4,1);reject(b,"unowned cells rejected");}
    {auto b=map_bytes;put16(b,items,9);reject(b,"unknown draw item kind rejected");}
    {auto b=map_bytes;put(b,groups,7);reject(b,"unknown draw group kind rejected");}
    {auto b=map_bytes;put(b,groups+8,get(b,groups+8)+1);reject(b,"draw group overrun rejected");}
    {   // Exactly one player entry, inside a y-sort group.
        auto b=map_bytes;bool changed=false;
        for(uint32_t i=0;i<count(b,14);++i)if((get(b,items+i*32)&0xffff)==4){put16(b,items+i*32,3);put(b,items+i*32+8,1);changed=true;break;}
        check(changed,"player draw item present");reject(b,"missing player draw item rejected");
    }

    // ENCLNK01 admission and rejection.
    WorldLinksData links;check(links.load(link_bytes.data(),link_bytes.size(),error),error.c_str());
    check(links.scene_count()>=2&&links.route_count()>=2,"checked world links content");
    const auto route_count=links.route_count();
    auto reject_links=[&](std::vector<uint8_t>b,const char*why,bool fix=true){if(fix)rehash(b);check(!links.load(b.data(),b.size(),error),why);check(links.route_count()==route_count,"failed links admission preserves previous bytes");};
    {auto b=link_bytes;b[0]^=1;reject_links(b,"unknown links magic");}
    {auto b=link_bytes;put(b,24,1);reject_links(b,"links scene field must stay reserved");}
    {auto b=link_bytes;b.back()^=1;reject_links(b,"links CRC mismatch",false);}
    const size_t routes=section(link_bytes,3),scenes=section(link_bytes,2);
    {auto b=link_bytes;put(b,routes+8,999);reject_links(b,"route to an unknown scene rejected");}
    {auto b=link_bytes;put(b,routes+8,get(b,routes+4));reject_links(b,"route into its own scene rejected");}
    {auto b=link_bytes;put_float(b,routes+28,0.5f);reject_links(b,"noncardinal route direction rejected");}
    {auto b=link_bytes;put16(b,routes+52,3);reject_links(b,"unported transition kind rejected");}
    {auto b=link_bytes;put_float(b,routes+56,0);reject_links(b,"zero fade speed rejected");}
    {auto b=link_bytes;put(b,routes+80,get(b,routes+80)^1);reject_links(b,"music fade flag must match its duration");}
    {auto b=link_bytes;put(b,routes+84,get(b,routes));reject_links(b,"duplicate route identity rejected");}
    {auto b=link_bytes;put(b,scenes+24,get(b,scenes));reject_links(b,"duplicate scene identity rejected");}
    {auto b=link_bytes;put(b,scenes+12,0);reject_links(b,"scene without room role rejected");}

    // Segment obstacles: a wall stops downward motion and still allows sliding.
    {
        StaticMotionSolver solver;
        const ConvexPolygon actor{{{-7,3},{6,3},{6,9},{-7,9}}};
        check(solver.configure(actor,{},0.08f),"solver configured");
        WallSource wall;solver.attach_source(&wall);
        SlideResult r;Vec2 p{0,-20};size_t frame=0;
        auto compare=[&]{
            const auto&reference=segment_reference::frames.at(frame++);
            check(std::abs(p.x-reference.position.x)<0.0001f&&std::abs(p.y-reference.position.y)<0.0001f,"segment position matches official Godot recovery, including its nonzero overlap");
            check(std::abs(r.velocity.x-reference.velocity.x)<0.0001f&&std::abs(r.velocity.y-reference.velocity.y)<0.0001f,"segment velocity matches official Godot frame");
        };
        for(int i=0;i<60;++i){check(solver.slide(p,{0,60},r),"slide against segment");p=r.position;compare();}
        for(int i=0;i<10;++i){check(solver.slide(p,{60,60},r),"diagonal slide along segment");p=r.position;compare();}
        solver.attach_source(nullptr);
        for(int i=0;i<30;++i){check(solver.slide(p,{0,60},r),"free slide");p=r.position;compare();}
        check(frame==segment_reference::frames.size(),"all 100 official segment frames checked");
    }

    // Live scene: spawn from the House route, walk, stop at a boundary, return home.
    {
        RoomData room;check(room.load_file(argv[3],error),error.c_str());
        IntroductionData fades;check(fades.load_file(argv[4],error),error.c_str());
        const auto house_route=links.find_route_from(1,"Doors/Podunk");check(house_route!=WorldLinksData::kNotFound,"House front door route");
        const auto route=links.route(house_route);
        auto scene=std::make_unique<FieldScene>();
        std::vector<bool> flags(room.view().flag_count(),false);
        check(!scene->prepare(room.view(),view,links,std::vector<bool>(1,false),route.destination,route.direction,{400,240},error),"flag table size mismatch rejected");
        scene=std::make_unique<FieldScene>();
        check(scene->prepare(room.view(),view,links,flags,route.destination,route.direction,{400,240},error),error.c_str());
        check(scene->world.house_paused()&&scene->phase()==FieldPhase::Transition,"field waits for the door fade-out");
        check(scene->finish_transition()&&scene->phase()==FieldPhase::Walking,"unpause at the fade-out boundary");
        auto step=[&](int x,int y){WalkInput in;in.x=int8_t(x);in.y=int8_t(y);
            check(scene->before_physics(in),"before physics");check(scene->world.advance(in),"field movement");check(scene->after_physics(),"after physics");
            scene->world.idle_frame(1.0/60);check(scene->idle_frame(1.0/60,false),"field idle");};
        const auto start=scene->world.player().position;
        for(int i=0;i<30;++i)step(0,1);
        check(scene->world.player().position.y>start.y+20,"player leaves the House door");
        int frames=0;while(scene->phase()==FieldPhase::Walking&&frames++<600)step(0,1);
        check(scene->phase()==FieldPhase::Unsupported&&!scene->error().empty(),"explicit development boundary stops the player");
        const auto stopped=scene->world.player().position;
        check(scene->idle_frame(1.0/60,true)&&scene->phase()==FieldPhase::Walking&&scene->world.player().position.y<stopped.y,"B returns to the last safe point");
        frames=0;uint32_t requested=WorldLinksData::kNotFound;
        while(requested==WorldLinksData::kNotFound&&frames++<600){const float dx=scene->world.player().position.x-route.destination.x;step(std::fabs(dx)>1?(dx<0?1:-1):0,std::fabs(dx)>1?0:-1);requested=scene->take_route();}
        check(requested!=WorldLinksData::kNotFound&&links.route(requested).to==1,"walking into the House door requests the return route");
        SceneDoorTransition door;check(door.begin(links,requested,fades,error),error.c_str());
        check(!door.swapped(error),"swap outside its boundary rejected");
        door.cancel();check(door.begin(links,requested,fades,error),error.c_str());
        frames=0;while(door.phase()==SceneDoorPhase::FadeIn&&frames++<600)check(door.idle_frame(1.0/60,error),"fade-in");
        check(door.phase()==SceneDoorPhase::SwapRequested&&door.swapped(error),"swap at the end of the fade-in");
        bool unpause=false;frames=0;
        while(door.phase()!=SceneDoorPhase::Done&&frames++<600){check(door.idle_frame(1.0/60,error),"fade-out");for(const auto&e:door.take_events())unpause|=e.kind==SceneDoorEventKind::Unpause;}
        check(unpause&&door.phase()==SceneDoorPhase::Done,"fade-out unpauses before it completes");
        check(!door.begin(links,99,fades,error),"unknown route index rejected");
    }
    // Pinned CamareaRect Control margins place its trigger far from the bridge.
    // Godot 3.6.2 source-script oracle: center (2390,1308), limits 1952/1064/2472/1520.
    {
        check(view.count(FieldMapSection::Cameras)==1,"one audited Podunk camera area");
        const auto camera=view.camera(0);
        check(camera.center.x==2390&&camera.center.y==1308&&camera.extents.x==98&&camera.extents.y==188,"camera trigger includes ReferenceRect parent translation");
        check(camera.has_rect&&camera.left==1952&&camera.top==1064&&camera.width==520&&camera.height==456,"original ReferenceRect camera limits retained");
        RoomData room;check(room.load_file(argv[3],error),error.c_str());
        std::vector<bool> flags(room.view().flag_count(),false);
        for(const Vec2 viewport:{Vec2{400,240},Vec2{320,180}}){
            auto scene=std::make_unique<FieldScene>();
            check(scene->prepare(room.view(),view,links,flags,{314,279},{1,0},viewport,error),error.c_str());
            check(scene->finish_transition()&&scene->after_physics(),"camera bridge fixture initialized");
            auto follows=[&]{const auto p=scene->world.player().position;const auto o=scene->camera_origin(p,viewport);
                check(o.x==std::floor(p.x-viewport.x*.5f)&&o.y==std::floor(p.y-viewport.y*.5f),"bridge camera follows player without distant limits");};
            follows();
            for(int direction:{1,-1})for(int i=0;i<48;++i){
                WalkInput input;input.x=int8_t(direction);
                check(scene->before_physics(input)&&scene->world.advance(input)&&scene->after_physics(),"normal movement across old camera trigger");
                scene->world.idle_frame(1.0/60);check(scene->idle_frame(1.0/60,false),"camera idle");
                check(scene->phase()==FieldPhase::Walking,"bridge remains walkable");follows();
            }
            check(scene->world.warp_same_scene({2390,1308},{1,0})&&scene->after_physics(),"enter actual distant camera area");
            const auto limited=scene->camera_origin({2390,1308},viewport);
            check(limited.x==2472-viewport.x&&limited.y==std::floor(1308-viewport.y*.5f),"actual area still applies original limits");
            check(scene->world.warp_same_scene({351,274},{1,0})&&scene->after_physics(),"leave actual camera area");follows();
        }
    }
    // Telepathy follows the source facing ray; adjacency alone cannot select Mick.
    {
        RoomData room;check(room.load_file(argv[3],error),error.c_str());MickData data;
        std::string mick_path(argv[1]);const auto suffix=mick_path.rfind(".encmap");check(suffix!=std::string::npos,"map suffix");mick_path.replace(suffix,7,".encmick");check(data.load_file(mick_path.c_str(),error),error.c_str());
        struct Host:MickHost {
            bool grant_key_item(std::string_view,uint32_t,std::string&e)override{e="Ray-only test host cannot grant keys";return false;}
            std::string body;bool validate_flag(std::string_view,std::string&)override{return true;}bool validate_item(std::string_view,std::string&)override{return true;}
            bool flag(std::string_view,bool&v,std::string&)override{v=false;return true;}bool set_flag(std::string_view,bool,std::string&)override{return true;}bool remove_key_item(std::string_view,std::string&)override{return true;}
            bool show_text(std::string_view,std::string_view v,std::string&)override{body=std::string(v);return true;}bool play_sound(std::string_view,std::string&)override{return true;}
        }host;
        const auto actor=data.view().actor();const Vec2 position{actor.position.x,actor.position.y+14};std::vector<bool>flags(room.view().flag_count(),false);
        auto scene=std::make_unique<FieldScene>();check(scene->prepare(room.view(),view,links,flags,position,{0,-1},{400,240},error),error.c_str());check(scene->finish_transition(),scene->error().c_str());check(!scene->try_telepathy({0,6},16,error),"unbound thoughts rejected");check(scene->bind_mick(data.view(),host,error),error.c_str());
        check(!scene->try_telepathy({NAN,6},16,error)&&scene->phase()==FieldPhase::Walking&&!scene->world.house_paused(),"invalid ray fails without pause");
        check(scene->try_telepathy({0,6},16,error),error.c_str());const auto thoughts=host.body;check(scene->phase()==FieldPhase::MickTalk&&scene->world.house_paused(),"thought dialogue owns pause");check(!scene->try_telepathy({0,6},16,error),"cannot restart active thought dialogue");check(scene->mick().advance_text(error)&&scene->finish_talk(),"thought completion resumes world");
        check(scene->world.set_house_direction({0,1}),"face away");check(scene->try_telepathy({0,6},16,error),error.c_str());check(host.body!=thoughts,"facing away uses source no-problem, not proximity thoughts");check(scene->mick().advance_text(error)&&scene->finish_talk(),"empty-ray dialogue resumes world");
    }
    std::cout<<"Field data/scene checks: "<<checks<<'\n';
    return 0;
}
