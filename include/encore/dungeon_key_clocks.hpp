#pragma once
#include "encore/dungeon_keys.hpp"
#include "encore/dungeon_key_state.hpp"
#include <array>
#include <memory>

namespace encore::upstream {
enum class KeyClockKind:uint32_t {Key=1,Door=2,Child=3};
enum class KeyClockRole:uint32_t {Reset=1,Use=2,Open=3,Opened=4};
enum class KeyClockTarget:uint32_t {Frame=1,Visible=2,Position=3,Audio=4,ChildVisible=5,RootParent=6,OpenBody=7,ChildPlay=8};
struct KeyClockKey {float time=0,ease=0;uint32_t value=0;Vec2 position{};};
struct KeyClockTrack {KeyClockTarget target=KeyClockTarget::Frame;uint32_t mode=0,index=0;std::vector<KeyClockKey>keys;};
struct KeyClockClip {uint32_t id=0,profile=0;KeyClockRole role=KeyClockRole::Reset;float duration=0;std::vector<KeyClockTrack>tracks;};
struct KeyClockProfile {
 uint32_t id=0,frames=0,frame=0,mask=0,child=0,diagnostics=0;KeyClockKind kind=KeyClockKind::Key;
 Vec2 position{};std::array<uint32_t,4>roles{};std::string source;
};
struct KeyClockAudio {uint32_t profile=0,bus=0;std::string source;std::array<uint8_t,32>sha{};float gain=0,pitch=0;};
// Independent ENCKLC01 format1/capability1. Typed source clocks and exact
// nested key identity policy only; no map, media, renderer or save admission.
class DungeonKeyClocksData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}uint32_t scene()const;
 DungeonKeysView metadata()const;
 const std::vector<KeyClockProfile>&profiles()const;const std::vector<KeyClockClip>&clips()const;const std::vector<KeyClockAudio>&audios()const;
 const KeyClockProfile*profile(uint32_t)const;const KeyClockClip*clip(uint32_t)const;
private:struct State;std::shared_ptr<const State>state_;
};
struct KeyClockPose {uint32_t frame=0;Vec2 position{};bool visible=false,audio=false,child_visible=false,queued=false;};
enum class KeyClockEventKind:uint32_t {Audio=1,OpenBody=2,ChildPlay=3,RootParent=4,MissingHandler=5,InvalidSignal=6,DeleteQueued=7,Completed=8};
struct KeyClockEvent {KeyClockEventKind kind=KeyClockEventKind::Completed;uint32_t profile=0,clip=0,value=0;};
// Native AnimationPlayer subset. Method requests remain deferred for the
// owner; original known signal errors are explicit diagnostic events. This
// player cannot deliver a body event, invent a callback or write game flags.
class DungeonKeyClockPlayer {
public:
 bool initialize(const DungeonKeyClocksData&,uint32_t,std::string&);
 bool play(uint32_t,std::string&);bool advance(double,std::string&);
 bool write_audio(bool,std::string&);bool audio_finished(std::string&);
 const KeyClockPose&pose()const{return pose_;}bool playing()const{return playing_;}
 float position()const{return time_;}uint32_t assigned()const{return clip_;}uint32_t profile_id()const{return profile_;}
 const std::vector<KeyClockEvent>&events()const{return events_;}void clear_events(){events_.clear();}
private:
 bool step(double,std::string&);bool apply(const KeyClockTrack&,const KeyClockKey&,std::string&);
 bool emit(KeyClockEventKind,uint32_t,std::string&);
 DungeonKeyClocksData data_;uint32_t profile_=0,clip_=0;float time_=0;bool playing_=false;
 KeyClockPose pose_;std::vector<uint32_t>cursors_;std::vector<KeyClockEvent>events_;
};
struct DungeonKeyClockView {DungeonKeyState object;KeyClockPose root,child;bool has_child=false,root_playing=false,child_playing=false;uint32_t root_clip=0,child_clip=0;float root_time=0,child_time=0;};
enum class DungeonKeyClockEventSource:uint32_t {Method=1,Clock=2};
struct DungeonKeyClockEvent {
 DungeonKeyClockEventSource source=DungeonKeyClockEventSource::Method;uint64_t epoch=0;uint32_t owner=0;
 DungeonKeyEvent method{};KeyClockEvent clock{};
};
// Source-owner bridge: child clocks precede parent clocks, methods wait for
// the explicit idle boundary, and deletion follows deferred source writes.
// Prepared media/world consumers must atomically accept requests before
// publishing this candidate. No scene or audio backend is created here.
class DungeonKeyClocks {
public:
 bool prepare(const DungeonKeyClocksData&,uint64_t,int32_t,const std::vector<DungeonKeyFlagValue>&,std::string&);
 bool valid()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}int32_t counter()const{return core_.counter();}
 bool state(uint64_t,uint32_t,DungeonKeyClockView&,std::string&)const;
 bool flag(uint64_t,uint32_t,bool&,std::string&)const;
 bool pickup(uint64_t,uint32_t,bool current_player,bool paused,std::string&);
 bool interact(uint64_t,uint32_t,std::string&);
 bool play_animation(uint64_t,uint32_t,KeyClockRole,std::string&);
 bool audio_finished(uint64_t,uint32_t,std::string&);
 bool advance(uint64_t,double,std::string&);bool idle_frame(uint64_t,std::string&);bool retire(uint64_t,std::string&);
 const std::vector<DungeonKeyClockEvent>&events()const{return events_;}void clear_events(){events_.clear();}
 size_t pending()const{return pending_.size();}const DungeonKeyClocksData&data()const{return data_;}
private:
 struct Owner {uint32_t id=0;bool has_child=false;DungeonKeyClockPlayer root,child;};
 struct Deferred {size_t owner=0;KeyClockEventKind kind=KeyClockEventKind::OpenBody;};
 bool validate(uint64_t,std::string&)const;bool owner(uint64_t,uint32_t,size_t&,bool live,std::string&)const;
 bool methods(std::string&);bool clocks(size_t,DungeonKeyClockPlayer&,std::string&);
 bool publish(size_t,const DungeonKeyEvent&,std::string&);bool publish(size_t,const KeyClockEvent&,std::string&);
 DungeonKeyClocksData data_;DungeonKeyRuntime core_;uint64_t epoch_=0,last_epoch_=0;
 std::vector<Owner>owners_;std::vector<Deferred>pending_;std::vector<DungeonKeyClockEvent>events_;
};
}
