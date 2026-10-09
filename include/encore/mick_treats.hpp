#pragma once
#include "encore/movement.hpp"
#include "encore/source_random.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// ENCMIK01: Podunk Mick. Geometry, 4dir clips, dialogue programmes and the
// field-enter RNG ledger are data. Telepathy and the heart emote are not.
constexpr uint32_t kMickNone=0xffffffffu;
enum class MickSection:uint32_t {Bytes=1,Strings,Texture,Actor,Programmes,Commands,Texts,Clips,Rng};
constexpr uint32_t kMickSectionCount=9;
enum class MickOpcode:uint32_t {ShowText=1,AwaitText,RemoveKeyItem,SetFlag,End,Choice,PlaySound,JumpActor,TurnActor,MovePlayer,Wait};
enum class MickProgramKind:uint32_t {Talk=1,Bark=2};
enum class MickRngKind:uint32_t {Randi=1,Randf,RandRange,ArmWander};
struct MickTexture {uint32_t path=0;uint16_t width=0,height=0,columns=0,rows=0;};
struct MickActor {
    uint32_t path=0,sprite=0,frame=0,item=0,require_flag=0,consume_flag=0;
    Vec2 position{},sprite_position{},interact_center{},interact_extents{};
    uint32_t first_command=0,command_count=0;float ray_length=0;
    Vec2 collision_center{},collision_extents{};float sort_y=0;
    float speed=0,walk_frequency=0,wander_radius=0;
    Vec2 near_offset{},near_extents{},view_offset{};float view_radius=0;
    Vec2 ray_offset{},bark_center{},bark_extents{},initial_direction{};
    uint32_t rng_lo=0,rng_hi=0,bark_programme=0;
    uint32_t idle_clip=0,idle_clip_count=0,walk_clip=0,walk_clip_count=0,talk_clip=0,talk_clip_count=0,bark_path=0;
};
struct MickProgramme {uint32_t flag=0,first=0,count=0,kind=0;};
struct MickCommand {uint32_t opcode=0,a=0,b=0,c=0,d=0;};
struct MickText {uint32_t en=0,zh=0;};
struct MickClip {uint16_t anim=0,direction=0,frame=0,milliseconds=0;};
struct MickRngEvent {uint32_t kind=0;float x=0,y=0,w=0,h=0,a=0,b=0;};

class MickView {
public:
    bool valid()const{return bytes_!=nullptr;}
    explicit operator bool()const{return valid();}
    uint32_t count(MickSection)const;
    std::string_view string(uint32_t)const;
    MickTexture texture()const;
    MickActor actor()const;
    MickProgramme programme(uint32_t)const;
    MickCommand command(uint32_t)const;
    MickText text(uint32_t)const;
    MickClip clip(uint32_t)const;
    MickRngEvent rng_event(uint32_t)const;
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

// Platform owns key-item removal, sounds and bottom-screen text. FieldScene owns flags and motion.
class MickHost {
public:
    virtual ~MickHost()=default;
    virtual bool validate_flag(std::string_view,std::string&)=0;
    virtual bool validate_item(std::string_view,std::string&)=0;
    virtual bool flag(std::string_view,bool&,std::string&)=0;
    virtual bool set_flag(std::string_view,bool,std::string&)=0;
    virtual bool remove_key_item(std::string_view,std::string&)=0;
    virtual bool show_text(std::string_view,std::string&)=0;
    virtual bool play_sound(std::string_view,std::string&)=0;
};
enum class MickProgramState:uint8_t {Idle,WaitingText,WaitingChoice,WaitingMotion,Complete,Failed};

// Field collision and the player shove stay in FieldScene. The queries are the
// reviewed npc.gd wander slide and the mick_bark player step.
class MickWorld {
public:
    virtual ~MickWorld()=default;
    virtual bool probe(Vec2 from,Vec2 to,bool& clear)=0;
    virtual bool slide(Vec2 position,Vec2 velocity,Vec2& out)=0;
    virtual bool player_near(Vec2 center,Vec2 extents)=0;
    virtual bool player_in_view(Vec2 center,float radius)=0;
    virtual bool shove_player(Vec2 position,Vec2 direction,bool walking)=0;
};

class MickRuntime {
public:
    bool initialize(MickView,MickHost&,std::string&);
    MickView view()const{return view_;}
    bool ready()const{return ready_;}
    void set_locale(std::string_view code){locale_=std::string(code);}
    bool start_talk(std::string&);
    void face_toward(Vec2 target);
    bool start_bark(std::string&);
    bool advance_text(std::string&);
    bool move_choice(int delta);
    bool confirm_choice(bool cancel,std::string&);
    MickProgramState state()const{return state_;}
    bool continues()const;
    bool waiting_choice()const{return state_==MickProgramState::WaitingChoice;}
    uint32_t choice_selection()const{return choice_index_;}
    std::string_view choice_text(uint32_t)const;
    std::string_view localized(const MickText&,std::string_view locale)const;
    // Live pose. sort_y is the node position, matching the field Y-sort.
    Vec2 position()const{return position_;}
    Vec2 facing()const{return facing_;}
    float sort_y()const{return position_.y;}
    uint32_t sheet_frame()const;
    Vec2 sprite_center()const;
    float jump_lift()const{return jump_lift_;}
    Vec2 collision_center()const;
    Vec2 interact_center()const;
    // One 60 Hz field step. camera is the visible world rectangle.
    bool simulate(float dt,Vec2 player,Vec2 camera,Vec2 view,bool paused,MickWorld&,std::string&);
private:
    bool step(std::string&);bool fail(std::string&,const char*);
    bool select(MickProgramKind,std::string&);
    void tick_motion(float dt);
    void tick_wander(float dt,Vec2 player,bool paused,MickWorld&);
    void consume_rng(Vec2 camera,Vec2 view);
    void pick_destination();
    uint32_t direction_index()const;
    MickView view_;MickHost* host_=nullptr;uint32_t pc_=0,programme_first_=0,programme_count_=0;
    MickProgramState state_=MickProgramState::Idle;bool ready_=false;std::string locale_="en";
    SourceRandom rng_{0};
    std::vector<uint8_t> rng_done_;
    Vec2 position_{},facing_{},destination_{},spawn_{};
    float anim_time_=0,jump_lift_=0,jump_time_=0,jump_length_=0,jump_height_=0,jump_gap_=0;
    int jump_left_=0,turn_left_=0;
    float turn_wait_=0,turn_interval_=0,move_left_=0,move_speed_=0,wait_left_=0,wander_left_=-1;
    Vec2 turn_target_{},move_target_{},shove_dir_{};
    bool wander_armed_=false,looking_=false,probing_=false;
    uint32_t anim_=0,choice_index_=0,choice_text_[2]{},choice_pc_[2]{};
};
}
