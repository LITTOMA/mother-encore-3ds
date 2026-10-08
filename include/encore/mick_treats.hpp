#pragma once
#include "encore/movement.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// ENCMIK01: Podunk Mick (npc21) DogTreats → gave_treats exit chain. Geometry,
// idle south frame, woof_treats phrase texts and the linear programme are data.
constexpr uint32_t kMickNone=0xffffffffu;
enum class MickSection:uint32_t {Bytes=1,Strings,Texture,Actor,Commands,Texts};
constexpr uint32_t kMickSectionCount=6;
enum class MickOpcode:uint32_t {ShowText=1,AwaitText,RemoveKeyItem,SetFlag,End};
struct MickTexture {uint32_t path=0;uint16_t width=0,height=0,columns=0,rows=0;};
struct MickActor {
    uint32_t path=0,sprite=0,frame=0,item=0,require_flag=0,consume_flag=0;
    Vec2 position{},sprite_position{},interact_center{},interact_extents{};
    uint32_t first_command=0,command_count=0;float ray_length=0;
};
struct MickCommand {uint32_t opcode=0,a=0,b=0,c=0,d=0;};
struct MickText {uint32_t en=0,zh=0;};

class MickView {
public:
    bool valid()const{return bytes_!=nullptr;}
    explicit operator bool()const{return valid();}
    uint32_t count(MickSection)const;
    std::string_view string(uint32_t)const;
    MickTexture texture()const;
    MickActor actor()const;
    MickCommand command(uint32_t)const;
    MickText text(uint32_t)const;
    const uint8_t* reviewed_commit()const{return bytes_?bytes_+32:nullptr;}
private:
    friend class MickData;
    const uint8_t* bytes_=nullptr;size_t size_=0;
    const uint8_t* record(MickSection,uint32_t)const;
};
class MickData {
public:
    MickData()=default;MickData(const MickData&)=delete;MickData& operator=(const MickData&)=delete;
    bool load(const uint8_t*,size_t,std::string&);
    bool load_file(const char*,std::string&);
    MickView view()const{MickView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:
    std::vector<uint8_t> bytes_;
};

// Platform owns key-item removal and bottom-screen text; FieldScene owns flags.
class MickHost {
public:
    virtual ~MickHost()=default;
    virtual bool validate_flag(std::string_view,std::string&)=0;
    virtual bool validate_item(std::string_view,std::string&)=0;
    virtual bool flag(std::string_view,bool&,std::string&)=0;
    virtual bool set_flag(std::string_view,bool,std::string&)=0;
    virtual bool remove_key_item(std::string_view,std::string&)=0;
    virtual bool show_text(std::string_view,std::string&)=0;
};
enum class MickProgramState:uint8_t {Idle,WaitingText,Complete,Failed};

class MickRuntime {
public:
    bool initialize(MickView,MickHost&,std::string&);
    MickView view()const{return view_;}
    bool ready()const{return ready_;}
    void set_locale(std::string_view code){locale_=std::string(code);}
    // True when got_dog_treats is set and gave_treats is still clear.
    bool available(std::string&)const;
    bool start(std::string&);
    bool advance_text(std::string&);
    MickProgramState state()const{return state_;}
    bool continues()const;
    std::string_view localized(const MickText&,std::string_view locale)const;
private:
    bool step(std::string&);bool fail(std::string&,const char*);
    MickView view_;MickHost* host_=nullptr;uint32_t pc_=0;
    MickProgramState state_=MickProgramState::Idle;bool ready_=false;std::string locale_="en";
};
}
