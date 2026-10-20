#pragma once
#include "encore/movement.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace encore::upstream {
struct DungeonRoom {
 uint32_t id=0,index=0,layer=0,mask=0,bits=0;
 std::string path,flag;Vec2 center{},extents{},size{},offset{};
};
class DungeonRoomsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}uint32_t scene()const;int32_t floor()const;
 const std::string&source()const;const std::string&name()const;const std::string&region()const;
 const std::array<int32_t,4>&reset_limits()const;const std::vector<DungeonRoom>&rooms()const;
 const DungeonRoom*room(uint32_t id)const;
private:struct State;std::shared_ptr<const State>state_;
};
struct DungeonRoomFlag {uint32_t owner=0;bool visited=false;};
enum class DungeonRoomEventKind:uint32_t {ObjectFlag=1,CurrentRoom=2,Limits=3,Offset=4};
struct DungeonRoomEvent {
 DungeonRoomEventKind kind=DungeonRoomEventKind::ObjectFlag;uint32_t owner=0;uint64_t epoch=0;
 std::array<int32_t,4>limits{};Vec2 offset{};
};
// Checked enter/exit protocol for source room areas. World contact delivery
// must be unique and source ordered; other bodies are explicitly ignored.
// Current-room identity persists after exit. Idle continuations retain their
// original order, with cancellation by the area's current inside flag.
class DungeonRoomState {
public:
 bool prepare(const DungeonRoomsData&,uint64_t,const std::vector<DungeonRoomFlag>&,std::string&);
 bool valid()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}
 bool enter(uint64_t,uint32_t,bool current_player,std::string&);
 bool exit(uint64_t,uint32_t,bool current_player,std::string&);
 bool idle_frame(uint64_t,Vec2 viewport,std::string&);
 bool retire(uint64_t,std::string&);
 bool relative_position(Vec2 player,Vec2&,std::string&)const;
 uint32_t current_owner()const{return current_;}int32_t current_index()const;
 uint32_t active_count()const{return active_;}bool inside(uint32_t)const;bool visited(uint32_t)const;
 const std::array<int32_t,4>&limits()const{return limits_;}Vec2 offset()const{return offset_;}
 const DungeonRoomsData&data()const{return data_;}size_t pending()const{return pending_.size();}
 size_t event_count()const{return events_.size();}
 std::vector<DungeonRoomEvent>take_events(){auto v=std::move(events_);events_.clear();return v;}
private:
 size_t find(uint32_t)const;bool owner(uint64_t,uint32_t,size_t&,std::string&)const;
 bool emit(DungeonRoomEventKind,uint32_t,std::string&);bool reset(uint32_t,std::string&);
 DungeonRoomsData data_;uint64_t epoch_=0,last_epoch_=0;uint32_t current_=0,active_=0;
 std::vector<uint8_t>inside_,visited_;std::vector<size_t>pending_;std::vector<DungeonRoomEvent>events_;
 std::array<int32_t,4>limits_{};Vec2 offset_{};
};
}
