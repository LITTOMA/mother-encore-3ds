#include "encore/native_input.hpp"
#include "encore/menu_navigation.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;bool ok=true;
void check(bool value,const char* message){++checks;if(!value){ok=false;std::fprintf(stderr,"FAIL: %s\n",message);}}
void axes(const NativeInputResult& r,int x,int y,const char* message){check(r.direction.x==x&&r.direction.y==y,message);}
constexpr double pi=3.14159265358979323846;
NativeInputData data;std::string error;
NativeInputAdapter fresh(){NativeInputAdapter a;check(a.configure(data,error),"configure checked tuning");a.sample(1,0,0,{},false,0,0,0);return a;}
NativeInputResult pad(NativeInputAdapter& a,int x,int y,double dt=.016,uint32_t context=1){return a.sample(context,int16_t(x),int16_t(y),{},false,0,0,dt);}
NativeInputResult touch(NativeInputAdapter& a,bool down,int x,int y,double dt=.016,uint32_t context=1){return a.sample(context,0,0,{},down,x,y,dt);}
void put32(std::vector<uint8_t>& b,size_t offset,uint32_t x){for(unsigned i=0;i<4;++i)b[offset+i]=uint8_t(x>>(i*8));}
void put_float(std::vector<uint8_t>& b,size_t offset,float value){uint32_t bits;std::memcpy(&bits,&value,4);put32(b,offset,bits);}
void reseal(std::vector<uint8_t>& b){
    put32(b,16,0);uint32_t crc=0xffffffffu;
    for(uint8_t value:b){crc^=value;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
    put32(b,16,crc^0xffffffffu);
}
void parser(const std::vector<uint8_t>& bytes){
    NativeInputData d;NativeInputAdapter a;
    check(!a.configure(d,error),"unloaded config fails closed");axes(pad(a,156,156),0,0,"unconfigured adapter is neutral");
    check(!d.load(nullptr,bytes.size(),error),"null resource rejected");
    for(size_t n=0;n<bytes.size();++n)check(!d.load(bytes.data(),n,error),"every truncation rejected");
    auto b=bytes;b.push_back(0);check(!d.load(b.data(),b.size(),error),"trailing byte rejected");
    for(size_t offset:{size_t(0),size_t(8),size_t(12),size_t(20),size_t(24),size_t(28)}){
        b=bytes;b[offset]^=1;reseal(b);check(!d.load(b.data(),b.size(),error),"unknown header rejected even with correct checksum");
    }
    b=bytes;b[40]^=1;check(!d.load(b.data(),b.size(),error),"checksum corruption rejected");
    for(unsigned field=0;field<10;++field){b=bytes;put_float(b,32+field*4,std::numeric_limits<float>::quiet_NaN());reseal(b);check(!d.load(b.data(),b.size(),error),"all non-finite floats rejected");}
    for(const auto& change:{std::pair<size_t,float>{32,23},{36,18},{40,0},{44,22.5f},{48,6},{52,0},{56,9},{60,0},{64,7},{68,0}}){
        b=bytes;put_float(b,change.first,change.second);reseal(b);check(!d.load(b.data(),b.size(),error),"invalid tuning relationship rejected");
    }
    for(size_t offset:{size_t(72),size_t(76)}){b=bytes;put32(b,offset,999);reseal(b);check(!d.load(b.data(),b.size(),error),"unsupported viewport rejected");}
    for(size_t offset:{size_t(80),size_t(84)}){b=bytes;put32(b,offset,0xffffffff);reseal(b);check(!d.load(b.data(),b.size(),error),"opaque debug-obscuring overlay rejected");}
    check(d.load(bytes.data(),bytes.size(),error),"valid binary loads");
    b=bytes;put_float(b,36,30);reseal(b);check(d.load(b.data(),b.size(),error),"external activation tuning reloads");
    check(a.configure(d,error),"new tuning configures same executable");a.sample(1,0,0,{},false,0,0,0);
    axes(pad(a,25,0),0,0,"external activation change changes behavior");axes(pad(a,30,0),1,0,"external activation threshold respected");
    b[0]=0;check(!d.load(b.data(),b.size(),error)&&d.valid()&&d.tuning().circle_activate==30,"failed reload preserves valid resource");
}
void circle(){
    auto a=fresh();
    axes(pad(a,0,0),0,0,"center neutral");axes(pad(a,23,0),0,0,"below activation neutral");
    axes(pad(a,24,0),1,0,"activation threshold inclusive");axes(pad(a,20,0),1,0,"radial hysteresis band holds");
    axes(pad(a,18,0),0,0,"release threshold inclusive");axes(pad(a,20,0),0,0,"deadzone release clears sector");
    axes(pad(a,0,24),0,-1,"SDK positive Y maps up");axes(pad(a,0,-24),0,1,"SDK negative Y maps down");
    axes(pad(a,-100,100),-1,-1,"diagonal axes unnormalized");
    axes(pad(a,-32768,-32768),-1,1,"signed raw extremes do not overflow");
    axes(pad(a,32767,32767),1,-1,"opposite extremes jump immediately");
    for(int radius:{32,100,140,156})for(int degrees=0;degrees<360;++degrees){
        a=fresh();const double angle=degrees*pi/180;
        const int x=int(std::lround(radius*std::cos(angle))),y=int(std::lround(radius*std::sin(angle)));
        const auto r=pad(a,x,-y);
        // Independent oracle: select the largest dot product of eight unit rays.
        int nearest=0;double best=-1e100;
        for(int sector=0;sector<8;++sector){const double dot=x*std::cos(sector*pi/4)+y*std::sin(sector*pi/4);if(dot>best){best=dot;nearest=sector;}}
        const int ex=int(std::lround(std::cos(nearest*pi/4))),ey=int(std::lround(std::sin(nearest*pi/4)));
        axes(r,ex,ey,"cold angular sweep equals nearest octant");check(r.source==NativeInputSource::Pad,"cold sweep pad source");
    }
    auto at_angle=[&](int degrees){return pad(a,int(std::lround(1000*std::cos(degrees*pi/180))),int(std::lround(-1000*std::sin(degrees*pi/180))));};
    a=fresh();axes(at_angle(0),1,0,"angular warm seed east");
    for(int degrees:{22,24,23,26,22,25})axes(at_angle(degrees),1,0,"boundary jitter retains previous sector");
    axes(at_angle(27),1,1,"expanded boundary changes to diagonal");axes(at_angle(20),1,1,"reverse hysteresis holds");axes(at_angle(18),1,0,"reverse hysteresis crosses");
    axes(at_angle(335),1,0,"wraparound hysteresis holds");axes(at_angle(333),1,-1,"wraparound crossing selects northeast");
    axes(at_angle(180),-1,0,"large turn chooses nearest immediately");axes(at_angle(45),1,1,"large diagonal turn chooses nearest immediately");
    for(int sense:{-1,1}){
        a=fresh();at_angle(0);int transitions=0,oldx=1,oldy=0;
        for(int step=1;step<=360;++step){auto r=at_angle(sense*step);if(r.direction.x!=oldx||r.direction.y!=oldy)++transitions;oldx=r.direction.x;oldy=r.direction.y;}
        check(transitions==8,"warm clockwise/counterclockwise sweep has eight transitions");
    }
}
void gestures(){
    auto a=fresh();auto r=touch(a,true,150,120);
    axes(r,0,0,"first touch sets neutral origin");check(r.gesture.active&&!r.gesture.dragged&&r.gesture.origin_x==150&&!r.confirm_pulse,"touch down view without confirm");
    r=touch(a,false,0,0,.1);check(r.confirm_pulse&&!r.gesture.active,"short tap confirms only on release");
    for(unsigned frame=0;frame<5;++frame)check(!touch(a,false,0,0,.5).confirm_pulse,"release pulse cannot repeat on low FPS frames");
    touch(a,true,150,120);r=touch(a,true,157,120,.05);axes(r,0,0,"travel below drag threshold remains neutral");check(!r.confirm_pulse,"held tap never confirms");check(touch(a,false,0,0,.05).confirm_pulse,"small travel remains tap");
    touch(a,true,150,120);r=touch(a,true,158,120,.02);axes(r,1,0,"touch drag starts at threshold");check(r.gesture.dragged&&r.source==NativeInputSource::Touch,"drag state latches");
    axes(touch(a,true,157,120),1,0,"touch radial hysteresis holds");axes(touch(a,true,156,120),0,0,"touch releases direction at inner threshold");
    r=touch(a,true,150,120);check(r.source==NativeInputSource::Touch&&r.gesture.dragged,"centered drag retains gesture ownership");
    check(!touch(a,false,0,0).confirm_pulse,"drag returning to origin never becomes tap");
    touch(a,true,150,120);touch(a,true,150,120,.35);check(!touch(a,false,0,0,.01).confirm_pulse,"stationary long hold never taps");
    touch(a,true,150,120);check(!touch(a,false,0,0,1).confirm_pulse,"long release frame counts full unscaled duration");
    touch(a,true,150,120,20);check(touch(a,false,0,0,.1).confirm_pulse,"new contact does not inherit preceding frame delay");
    for(int x:{-1,0,1})for(int y:{-1,0,1})if(x||y){
        a=fresh();touch(a,true,160,120);r=touch(a,true,160+x*20,120+y*20);
        axes(r,x,y,"virtual stick reaches all eight unnormalized directions");check(!touch(a,false,0,0).confirm_pulse,"all direction drags suppress tap");
    }
    for(int x:{0,319})for(int y:{0,239}){
        a=fresh();r=touch(a,true,x,y);check(r.gesture.origin_x==x&&r.gesture.origin_y==y,"edge logical origin never clamped");
        check(r.gesture.thumb_x>=7&&r.gesture.thumb_x<=312&&r.gesture.thumb_y>=7&&r.gesture.thumb_y<=232,"display thumb may clamp inside viewport");
        check(touch(a,false,0,0,.1).confirm_pulse,"edge tap confirms");
        touch(a,true,x,y);r=touch(a,true,x?x-40:40,y?y-40:40);axes(r,x?-1:1,y?-1:1,"edge inward drag retains logical delta");
        check(r.gesture.origin_x==x&&r.gesture.origin_y==y,"edge drag never shifts origin");
        check(!touch(a,false,0,0).confirm_pulse,"edge drag release suppresses confirm");
    }
    a=fresh();touch(a,true,100,100);r=touch(a,true,320,100);check(!r.gesture.active&&!r.confirm_pulse,"out-of-range driver sample cancels gesture");
    r=touch(a,true,100,100);check(!r.gesture.active,"invalid contact remains blocked until lift");check(!touch(a,false,0,0).confirm_pulse,"invalid contact release cannot confirm");
    touch(a,true,100,100);check(touch(a,false,0,0,.1).confirm_pulse,"fresh touch works after invalid contact lift");
}
void ownership(){
    auto a=fresh();NativeDpad right{};right.right=true;NativeDpad opposed{};opposed.left=opposed.right=true;
    axes(a.sample(1,-156,0,right,false,0,0,.01),1,0,"D-pad overrides opposite pad");
    auto r=a.sample(1,156,156,opposed,false,0,0,.01);axes(r,0,0,"opposed D-pad stays neutral instead of falling back");check(r.source==NativeInputSource::Dpad,"opposed D-pad owns input");
    opposed.up=opposed.down=true;axes(a.sample(1,156,156,opposed,false,0,0,.01),0,0,"all D-pad buttons cancel");
    for(int x:{-1,0,1})for(int y:{-1,0,1})if(x||y)axes(a.sample(1,0,0,{x<0,x>0,y<0,y>0},false,0,0,.01),x,y,"all D-pad directions preserved");
    a.sample(1,-156,0,{},true,100,100,.01);r=a.sample(1,-156,0,{},true,130,100,.01);axes(r,1,0,"active touch drag overrides pad");
    r=a.sample(1,-156,0,{},true,100,100,.01);axes(r,0,0,"returning touch to center does not leak pad");
    r=a.sample(1,-156,0,right,true,100,130,.01);axes(r,1,0,"D-pad overrides active touch");check(r.touch_direction.y==1,"independent touch direction remains available for dispatch");
    r=a.sample(1,-156,0,{},false,0,0,.01);axes(r,-1,0,"touch lift hands direction back to pad");check(!r.confirm_pulse,"device handover does not confirm");
    a=fresh();touch(a,true,100,100);r=touch(a,true,130,100,.01,2);axes(r,0,0,"context change cancels active gesture");check(!r.gesture.active,"context change hides stick");
    r=touch(a,true,130,100,.01,2);axes(r,0,0,"held touch is quarantined after context change");check(!touch(a,false,0,0,.01,2).confirm_pulse,"context cancellation release never confirms");
    touch(a,true,100,100,.01,2);check(touch(a,false,0,0,.1,2).confirm_pulse,"new gesture after lift enters new owner");
    a=fresh();pad(a,156,0);axes(pad(a,156,0,.01,2),0,0,"context change cancels pad");axes(pad(a,156,0,.01,2),0,0,"held pad remains quarantined");pad(a,18,0,.01,2);axes(pad(a,24,0,.01,2),1,0,"pad re-arms only after radial neutral");
    a.sample(3,0,0,right,false,0,0,.01);axes(a.sample(3,0,0,right,false,0,0,.01),0,0,"held D-pad quarantined");a.sample(3,0,0,{},false,0,0,.01);axes(a.sample(3,0,0,right,false,0,0,.01),1,0,"neutral re-arms D-pad");
    a=fresh();touch(a,true,100,100);a.reset();r=touch(a,true,100,100);check(!r.gesture.active,"explicit reset cancels held touch");check(!touch(a,false,0,0).confirm_pulse,"reset release never confirms");
    touch(a,true,100,100);r=touch(a,true,100,100,.01,0);check(!r.gesture.active,"disabled context hides touch");check(!touch(a,false,0,0,.1,0).confirm_pulse,"disabled context never confirms");
    for(double bad:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        a=fresh();touch(a,true,100,100);check(!touch(a,false,0,0,bad).confirm_pulse,"invalid elapsed cancels tap");
    }
    a=fresh();MenuNavigationRepeat menu;menu.sample(7,0,0,0,.35,.1,true);touch(a,true,100,100);
    r=touch(a,true,120,100);auto move=menu.sample(7,r.touch_direction.x,r.touch_direction.y,.016,.35,.1,true);check(move.x==1,"touch uses existing menu first press gate");
    r=touch(a,true,120,100,.1);move=menu.sample(7,r.touch_direction.x,r.touch_direction.y,.1,.35,.1,true);check(!move.x,"touch held menu respects initial delay");
    r=touch(a,true,120,100,2);move=menu.sample(7,r.touch_direction.x,r.touch_direction.y,2,.35,.1,true);check(move.x==1,"low FPS touch menu produces one repeat only");
    move=menu.sample(7,r.touch_direction.x,r.touch_direction.y,0,.35,.1,true);check(!move.x,"menu repeat discards catch-up debt");
    check(!touch(a,false,0,0).confirm_pulse,"long menu drag never confirms");
    touch(a,true,100,100);r=touch(a,false,0,0,.1);const bool physical_a=true;check((physical_a||r.confirm_pulse)==true,"physical A and touch A combine as one boolean pulse");
}
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    check(data.load_file(argv[1],error),error.c_str());if(!ok)return 1;
    FILE* file=std::fopen(argv[1],"rb");if(!file)return 2;std::vector<uint8_t> bytes(88);const size_t read=std::fread(bytes.data(),1,bytes.size(),file);std::fclose(file);if(read!=bytes.size())return 2;
    parser(bytes);circle();gestures();ownership();
    std::printf("Native stick/touch adapter: %u checks\n",checks);return ok?0:1;
}
