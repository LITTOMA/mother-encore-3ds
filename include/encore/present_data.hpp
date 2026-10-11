#pragma once
#include "encore/movement.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// ENCPRS01: original House gift boxes (Present.tscn / ItemHolder.gd). Geometry,
// sprites, clips, sparkles and effect programmes are data; see docs/HOUSE_PRESENTS.md.
constexpr uint32_t kPresentNone=0xffffffffu;
enum class PresentSection:uint32_t {Bytes=1,Strings,Textures,Clips,FrameKeys,AudioKeys,SparkleFrames,SparkleOrder,Sparkles,
    Objects,Templates,Commands,Programs,Texts};
constexpr uint32_t kPresentSectionCount=14;
enum class PresentClipRole:uint32_t {Unwrapped=1,Wrapped=2};
enum class PresentPolicy:uint32_t {Flag=1,ObjectFlag=2};
enum class PresentOpcode:uint32_t {ShowText=1,AwaitText,BranchFlag,GrantItem,PlaySound,SetFlag,Jump,End,PlayClip};
struct PresentTexture {uint32_t path=0;uint16_t width=0,height=0,columns=0,rows=0;};
struct PresentClip {uint32_t role=0,first_frame_key=0,frame_key_count=0,first_audio_key=0,audio_key_count=0;float length=0;};
struct PresentFrameKey {float time=0;uint32_t frame=0;};
struct PresentAudioKey {float time=0;uint32_t playing=0;};
struct PresentRegion {uint16_t x=0,y=0,w=0,h=0;};
struct PresentSparkles {uint32_t texture=0,first_order=0,order_count=0;float fps=0;Vec2 offset{};float initial_range=0;};
struct PresentObject {
    uint32_t path=0,texture=0,policy=0,flag=0,item=kPresentNone;
    Vec2 position{},interact_center{},interact_extents{};
    uint32_t program=kPresentNone,sound=0;
};
struct PresentTemplate {uint32_t item=0,doses=0,key_item=0;};
struct PresentCommand {uint32_t opcode=0,a=0,b=0,c=0,d=0;};
struct PresentProgram {uint32_t first=0,count=0;};
struct PresentText {uint32_t dialogue_id=0,source_path=0;};

class PresentView {
public:
    bool valid()const{return bytes_!=nullptr;}
    explicit operator bool()const{return valid();}
    uint32_t count(PresentSection)const;
    std::string_view string(uint32_t)const;
    PresentTexture texture(uint32_t)const;PresentClip clip(uint32_t)const;
    PresentFrameKey frame_key(uint32_t)const;PresentAudioKey audio_key(uint32_t)const;
    PresentRegion region(uint32_t)const;uint32_t order(uint32_t)const;PresentSparkles sparkles()const;
    PresentObject object(uint32_t)const;PresentTemplate item_template(uint32_t)const;
    PresentCommand command(uint32_t)const;PresentProgram program(uint32_t)const;PresentText text(uint32_t)const;
    const uint8_t* reviewed_commit()const{return bytes_?bytes_+32:nullptr;}
private:
    friend class PresentData;
    const uint8_t* bytes_=nullptr;size_t size_=0;
    const uint8_t* record(PresentSection,uint32_t)const;
};
class PresentData {
public:
    PresentData()=default;PresentData(const PresentData&)=delete;PresentData& operator=(const PresentData&)=delete;
    // A rejected load keeps the previously admitted bytes and views.
    bool load(const uint8_t*,size_t,std::string&);
    bool load_file(const char*,std::string&);
    PresentView view()const{PresentView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:
    std::vector<uint8_t> bytes_;
};
}
