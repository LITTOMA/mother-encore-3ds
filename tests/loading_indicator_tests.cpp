// Assertions are the test oracle even in Release builds.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "encore/loading_indicator_data.hpp"
#include "platform/ctr/loading_indicator.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

struct TextureMetadata { Tex3DS_SubTexture sub{93,29,0,.90625f,.7265625f,0}; };
namespace {
int live_textures=0,live_metadata=0,observer_calls=0,draw_calls=0;
bool bad_dimensions=false,rotated=false;
float drawn_x=0,drawn_y=0;
Tex3DS_SubTexture drawn_sub{};
void observe(void*,const encore::LoadProgress&){++observer_calls;}
uint32_t crc(const uint8_t* p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
void put(std::vector<uint8_t>& bytes,size_t at,uint32_t value){for(int i=0;i<4;++i)bytes[at+i]=static_cast<uint8_t>(value>>(i*8));}
void reseal(std::vector<uint8_t>& bytes){put(bytes,12,static_cast<uint32_t>(bytes.size()));put(bytes,16,crc(bytes.data()+24,bytes.size()-24));}
std::vector<uint8_t> read(const std::string& path){FILE* f=std::fopen(path.c_str(),"rb");assert(f);std::vector<uint8_t>b;int c;while((c=std::fgetc(f))!=EOF)b.push_back(uint8_t(c));assert(!std::ferror(f)&&std::fclose(f)==0);return b;}
}

Tex3DS_Texture Tex3DS_TextureImportCallback(C3D_Tex* texture,C3D_TexCube*,bool,decompressCallback callback,void* context){
    unsigned char bytes[32];if(callback(context,bytes,sizeof(bytes))!=sizeof(bytes))return nullptr;
    texture->data=std::malloc(1);assert(texture->data);++live_textures;++live_metadata;
    auto* metadata=new TextureMetadata;if(bad_dimensions)++metadata->sub.width;return metadata;
}
void Tex3DS_TextureFree(Tex3DS_Texture metadata){--live_metadata;delete metadata;}
void C3D_TexDelete(C3D_Tex* texture){assert(texture->data);std::free(texture->data);texture->data=nullptr;--live_textures;}
void C3D_TexSetWrap(C3D_Tex*,int,int){}
void C3D_TexSetFilter(C3D_Tex*,int min,int mag){assert(min==GPU_NEAREST&&mag==GPU_NEAREST);}
size_t Tex3DS_GetNumSubTextures(Tex3DS_Texture){return 1;}
const Tex3DS_SubTexture* Tex3DS_GetSubTexture(Tex3DS_Texture metadata,size_t index){return index?nullptr:&metadata->sub;}
bool Tex3DS_SubTextureRotated(const Tex3DS_SubTexture*){return rotated;}
bool C2D_DrawImageAt(C2D_Image image,float x,float y,float z){assert(image.tex&&image.subtex&&z==0);drawn_x=x;drawn_y=y;drawn_sub=*image.subtex;++draw_calls;return true;}

int main(int argc,char** argv){
    assert(argc==2);const std::string root=argv[1];
    const auto bytes=read(root+"loading-preview/indicator.encload");std::string error;
    encore::upstream::LoadingIndicatorData data;
    assert(data.load(bytes.data(),bytes.size(),error)&&error.empty()&&data.valid());
    assert(data.texture_width()==93&&data.texture_height()==29&&data.frame_width()==31&&data.frame_height()==29);
    assert(data.background_color()==0xff1c1c1cu);
    for(const auto& expected:std::vector<std::pair<double,int>>{{0,20},{.1,20},{.21,21},{.41,22},{.61,21},{.81,20},{1.01,21}})
        assert(data.frame_at(expected.first)==expected.second);
    const double key=double(.2f);
    assert(data.frame_at(key)==20); // Original AnimationPlayer discrete [from,to) key rule.
    assert(data.frame_at(double(std::nextafter(.2f,1.f)))==21);
    assert(data.frame_at(-1)==-1&&data.frame_at(std::numeric_limits<double>::infinity())==-1&&
           data.frame_at(std::numeric_limits<double>::quiet_NaN())==-1&&data.frame_at(1e100)>=20);
    encore::upstream::LoadingIndicatorPlacement pose;
    assert(data.sample(400,240,.41,pose)&&pose.x==361&&pose.y==203&&pose.frame==2);
    assert(data.sample(320,180,.21,pose)&&pose.x==281&&pose.y==143&&pose.frame==1);
    assert(!data.sample(320,240,0,pose)&&!data.sample(0,0,0,pose));
    for(const auto& viewport:std::vector<std::pair<int,int>>{{400,240},{320,180}}) {
        const int width=viewport.first,height=viewport.second,right=width-39;
        const std::vector<int> expected_x=width==400?std::vector<int>{8,96,185,273,361}:
                                                     std::vector<int>{8,76,145,213,281};
        for(size_t i=0;i<expected_x.size();++i) {
            assert(data.sample_progress(width,height,.41,double(i)/4,pose));
            assert(pose.x==expected_x[i]&&pose.y==height-37&&pose.frame==2);
        }
        int previous_x=-1;
        for(int i=0;i<=1000;++i) {
            const double fraction=double(i)/1000;
            assert(data.sample_progress(width,height,.21,fraction,pose));
            assert(pose.x>=previous_x&&pose.x>=8&&pose.x<=right);
            assert(std::abs(double(pose.x)-(8+(right-8)*fraction))<=.5000000001);
            previous_x=pose.x;
        }
        for(double seconds:std::vector<double>{0,.1,key,double(std::nextafter(.2f,1.f)),.41,.61,.81,1.01,1e100}) {
            encore::upstream::LoadingIndicatorPlacement original;
            assert(data.sample(width,height,seconds,original));
            for(double fraction:std::vector<double>{0,.25,.5,.75,1}) {
                assert(data.sample_progress(width,height,seconds,fraction,pose));
                assert(pose.y==original.y&&pose.frame==original.frame);
                if(fraction==1)assert(pose.x==original.x);
            }
        }
    }
    pose={123,456,7};
    for(double fraction:std::vector<double>{-.01,1.01,-std::numeric_limits<double>::infinity(),
                                          std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        assert(!data.sample_progress(400,240,0,fraction,pose));
        assert(pose.x==123&&pose.y==456&&pose.frame==7);
    }
    assert(!data.sample_progress(320,240,0,.5,pose)&&!data.sample_progress(0,0,0,.5,pose));
    assert(!data.sample_progress(400,240,-1,.5,pose)&&
           !data.sample_progress(400,240,std::numeric_limits<double>::quiet_NaN(),.5,pose));
    assert(!encore::upstream::LoadingIndicatorData{}.sample_progress(400,240,0,.5,pose));
    // Legacy layout accepts one margin. Progress requires room for symmetric
    // margins; zero travel is valid and a single-pixel span rounds half right.
    for(uint32_t width:std::vector<uint32_t>{39,46,47,48}) {
        auto narrow=bytes;put(narrow,narrow.size()-8,width);reseal(narrow);
        encore::upstream::LoadingIndicatorData narrow_data;
        assert(narrow_data.load(narrow.data(),narrow.size(),error));
        assert(narrow_data.sample(int(width),240,0,pose)&&pose.x==int(width)-39);
        for(double fraction:std::vector<double>{0,.49,.5,1}) {
            const auto previous=pose;
            assert(narrow_data.sample_progress(int(width),240,0,fraction,pose)==(width>=47));
            if(width<47)assert(pose.x==previous.x&&pose.y==previous.y&&pose.frame==previous.frame);
            else assert(pose.x==8+int(width==48&&fraction>=.5));
        }
    }
    const auto reject=[&](std::vector<uint8_t> bad,bool checksum=true){if(checksum&&bad.size()>=24)reseal(bad);encore::upstream::LoadingIndicatorData invalid;assert(!invalid.load(bad.data(),bad.size(),error)&&!error.empty()&&!invalid.valid());};
    for(size_t size=0;size<bytes.size();++size){auto bad=bytes;bad.resize(size);reject(bad);}
    auto bad=bytes;bad.back()^=1;reject(bad,false);
    for(auto offset:std::vector<size_t>{8,20}){bad=bytes;put(bad,offset,99);reject(bad);}
    bad=bytes;bad[0]^=1;reject(bad);
    bad=bytes;bad.push_back(0);reject(bad);
    for(auto offset:std::vector<size_t>{28,32,36,40,44}){bad=bytes;put(bad,offset,0);reject(bad);}
    bad=bytes;put(bad,24,0x001c1c1c);reject(bad);
    bad=bytes;put(bad,48,1025);reject(bad);
    bad=bytes;put(bad,52,1025);reject(bad);
    bad=bytes;put(bad,56,0x7fc00000);reject(bad);
    bad=bytes;put(bad,60,17);reject(bad);
    bad=bytes;put(bad,64,0x3f800000);reject(bad);
    bad=bytes;put(bad,68,3);reject(bad);
    bad=bytes;put(bad,72,0);reject(bad);
    bad=bytes;put(bad,100,20);reject(bad); // Duplicate source-frame mapping.
    bad=bytes;bad[112]='/';reject(bad); // Unsafe relative texture path.
    bad=bytes;put(bad,bad.size()-16,38);reject(bad); // Even legacy placement cannot fit.
    bad=bytes;put(bad,bad.size()-4,1);reject(bad); // Frame cannot fit viewport.
    assert(!data.load(nullptr,bytes.size(),error));
    assert(data.valid()&&data.frame_at(.41)==22); // Failed reload never damages previous checked data.
    assert(!data.load_file(nullptr,error)&&!data.load_file("/missing-loading-indicator",error));

    encore::ScopedLoadProgress observer(observe);
    encore::ctr::LoadingIndicatorRenderer renderer;
    assert(!renderer.ready()&&renderer.frame_at(0)==-1&&!renderer.draw(400,240,0));
    assert(!renderer.draw_progress(400,240,0,0));
    assert(renderer.load(root.c_str(),"loading-preview/indicator.encload",error)&&renderer.ready()&&renderer.valid());
    assert(observer_calls==0&&live_textures==1&&live_metadata==1); // Bootstrap never reenters observer.
    encore::report_load_progress(encore::LoadPhase::Scene,1,1);assert(observer_calls==1);
    assert(renderer.background_color()==0xff1c1c1cu&&renderer.frame_at(.41)==22);
    assert(renderer.draw(400,240,.41)&&drawn_x==361&&drawn_y==203&&drawn_sub.width==31&&drawn_sub.height==29);
    assert(drawn_sub.left==.484375f&&drawn_sub.right==.7265625f);
    assert(renderer.draw(320,180,.21)&&drawn_x==281&&drawn_y==143&&drawn_sub.left==.2421875f);
    for(const auto& viewport:std::vector<std::pair<int,int>>{{400,240},{320,180}}) {
        for(double seconds:std::vector<double>{0,.21,.41,.61}) {
            assert(renderer.draw(viewport.first,viewport.second,seconds));
            const auto original_sub=drawn_sub;
            for(double fraction:std::vector<double>{0,.25,.5,.75,1}) {
                assert(data.sample_progress(viewport.first,viewport.second,seconds,fraction,pose));
                assert(renderer.draw_progress(viewport.first,viewport.second,seconds,fraction));
                assert(drawn_x==pose.x&&drawn_y==pose.y);
                assert(drawn_sub.left==original_sub.left&&drawn_sub.right==original_sub.right&&
                       drawn_sub.top==original_sub.top&&drawn_sub.bottom==original_sub.bottom&&
                       drawn_sub.width==original_sub.width&&drawn_sub.height==original_sub.height);
            }
        }
    }
    const auto prior=draw_calls;assert(!renderer.draw(320,240,0)&&!renderer.draw(400,240,-1));
    assert(!renderer.draw_progress(320,240,0,.5)&&!renderer.draw_progress(400,240,-1,.5));
    for(double fraction:std::vector<double>{-.01,1.01,std::numeric_limits<double>::infinity(),
                                          std::numeric_limits<double>::quiet_NaN()})
        assert(!renderer.draw_progress(400,240,0,fraction));
    assert(draw_calls==prior);
    assert(renderer.load(root.c_str(),"loading-preview/indicator.encload",error)&&live_textures==1&&live_metadata==1&&observer_calls==1);
    bad_dimensions=true;assert(!renderer.load(root.c_str(),"loading-preview/indicator.encload",error)&&!renderer.ready()&&live_textures==0&&live_metadata==0);
    bad_dimensions=false;rotated=true;assert(!renderer.load(root.c_str(),"loading-preview/indicator.encload",error)&&!renderer.ready()&&live_textures==0&&live_metadata==0);
    rotated=false;assert(renderer.load(root.c_str(),"loading-preview/indicator.encload",error));renderer.free();renderer.free();
    assert(!renderer.ready()&&live_textures==0&&live_metadata==0);
    assert(!renderer.load(nullptr,"loading-preview/indicator.encload",error)&&!renderer.load("/missing-loading-root/","loading-preview/indicator.encload",error));
    std::puts("PASS loading indicator: checked data, original timing, both viewports, progress placement/rounding/UV parity, bootstrap isolation, rejection and cleanup");
}
