#pragma once
#include "encore/field_node_tree.hpp"
#include <map>
#include <string_view>

namespace encore::upstream {
// Independent source resource. It owns no native Camera, node Timer or clock.
class HouseReturnCameraControlData {
public:
  struct Node {uint32_t id=0,ready=0;std::string path,native_class;};
  bool load(const uint8_t*,size_t,const upstream::FieldNodeTreeData&,
            const std::array<uint8_t,32>&actual_tree_ir,std::string&);
  bool load_file(const char*,const upstream::FieldNodeTreeData&,
            const std::array<uint8_t,32>&actual_tree_ir,std::string&);
  bool valid()const{return valid_;}
  const auto&identity()const{return identity_;}
  const auto&ir_sha256()const{return ir_;}
  const auto&tree_ir_sha256()const{return tree_ir_;}
  const auto&nodes()const{return nodes_;}
  const std::string&script()const{return script_;}
  const std::string&sound()const{return sound_;}
  const std::string&bus()const{return bus_;}
  const std::string&ui_source()const{return ui_source_;}
  const std::string&ui_game_over_member()const{return ui_game_over_member_;}
  bool ui_game_over_initial()const{return ui_game_over_initial_;}
  const auto&input_engine_commit()const{return input_engine_commit_;}
  const auto&input_source_sha256()const{return input_source_;}
  const auto&input_start_sha256()const{return input_start_;}
  const auto&ui_game_over_getter_sha256()const{return ui_game_over_getter_;}
  bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
  double wait()const{return values_[0];} double margin()const{return values_[1];}
  double magnitude()const{return values_[2];} double length()const{return values_[3];}
  double delay()const{return values_[4];}
  upstream::Vec2 direction()const{return{float(values_[5]),float(values_[6])};}
  bool auto_start()const{return auto_start_;}
  uint32_t player_camera()const{return player_camera_;}
  const std::string&player_state_member()const{return player_state_;}
  uint32_t joy_device()const{return uint32_t(values_[7]);}
  double joy_weak()const{return values_[8];} double joy_strong()const{return values_[9];}
private:
  bool valid_=false,auto_start_=false;uint32_t player_camera_=0;
  upstream::FieldIdentity identity_{};
  std::array<uint8_t,32>ir_{},tree_ir_{};
  std::array<Node,3>nodes_{};std::array<double,10>values_{};
  std::string script_,prototype_,sound_,bus_,player_state_;
  std::string ui_source_,ui_game_over_member_;bool ui_game_over_initial_=false;
  std::array<uint8_t,32>ui_game_over_getter_{},input_source_{},input_start_{};
  std::array<uint8_t,20>input_engine_commit_{};
  std::map<std::string,std::array<uint8_t,32>>sources_,methods_;
};
} // namespace encore::upstream
