#pragma once
#include "encore/field_geometry.hpp"
#include "encore/field_node_tree.hpp"
#include <map>
namespace encore::upstream {
class FieldGlobalRegistry;
class PlayerInitializationData;
class PlayerReadyData;
class PlayerMotionData;
enum class HouseLadderText : size_t {
 Script, PartyBase, PlayerScript, FollowerScript, EnterSignal, ExitSignal,
 EnterMethod, ExitMethod, LadderMethod, UnladderMethod, LadderAnimation,
 PositionPath, ShadowPath, ClimbingField, AnimationTreeMember,
 AnimationPlayerMember, RunVoice, IdleAnimation, GlobalPosition,
 PlaybackSpeed, Count
};
struct HouseLadderConnection { std::string signal,method; uint32_t flags=0,arguments=0; };
// Source/data only; it never supplies native Area2D ownership or Ready.
class HouseReturnLadderData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,const FieldNodeTreeData&,
           const FieldGeometryView&,const PlayerInitializationData&,
           const PlayerReadyData&,const PlayerMotionData&,std::string&);
 bool load_file(const char*,const FieldIdentity&,const FieldNodeTreeData&,
                const FieldGeometryView&,const PlayerInitializationData&,
                const PlayerReadyData&,const PlayerMotionData&,std::string&);
 bool matches(const FieldNodeTreeData&,const FieldGeometryView&,
              const PlayerInitializationData&,const PlayerReadyData&,
              const PlayerMotionData&,std::string&)const;
 bool player_matches(const PlayerInitializationData&,const PlayerReadyData&,
                     const PlayerMotionData&,std::string&)const;
 bool valid()const{return valid_;}
 const FieldIdentity&identity()const{return identity_;}
 const auto&ir_sha256()const{return ir_;}
 const std::string&scene()const{return scene_;}
 const std::string&node()const{return node_;}
 uint32_t id()const{return id_;} uint32_t shape_id()const{return shape_;}
 uint32_t owner_index()const{return owner_;}
 uint32_t shape_index()const{return shape_index_;}
 const std::string&text(HouseLadderText role)const{return texts_[size_t(role)];}
 const auto&connections()const{return connections_;}
 const auto&non_party_bodies()const{return nonparty_;}
 float stopped_speed()const{return stopped_speed_;}
 float position_y()const{return position_y_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;FieldIdentity identity_{};
 std::array<uint8_t,32>ir_{},initialization_ir_{},ready_ir_{},motion_ir_{};
 std::string scene_,node_;uint32_t id_=0,shape_=0,owner_=0,shape_index_=0;
 uint32_t layer_=0,mask_=0,owner_flags_=0,shape_flags_=0;
 FieldTransform local_{},world_{};float stopped_speed_=0,position_y_=0;
 std::array<std::string,size_t(HouseLadderText::Count)>texts_{};
 std::array<HouseLadderConnection,2>connections_{};
 std::vector<uint32_t>nonparty_;
 std::map<std::string,std::array<uint8_t,32>>sources_;
};
// The concrete original AnimationPlayer implements this on its existing
// clock. No optional function or copied field can satisfy this owner contract.
class PlayerLadderAnimationNative {
public:
 virtual ~PlayerLadderAnimationNative()=default;
 virtual const FieldGlobalRegistry*ladder_registry()const=0;
 virtual const FieldNodeTreeRuntime*ladder_tree()const=0;
 virtual bool ladder_animation(FieldObjectId,std::string_view,std::string&)const=0;
 virtual bool ladder_play(FieldObjectId,std::string_view,std::string&)=0;
 virtual bool ladder_stop(FieldObjectId,std::string&)=0;
 virtual bool ladder_speed(FieldObjectId,double,std::string&)=0;
};
}
