#pragma once
#include "encore/prompt_native.hpp"
namespace encore::upstream {
struct HouseButtonPromptConnection {
  uint32_t role=0, emitter=0, arguments=0, flags=0, bind=0;
  std::string signal,method;
};
struct HouseButtonPromptWait {
  uint32_t id=0,flags=0;
  std::string source,native_class,method;
  std::array<uint8_t,32> source_sha{};
};
struct HouseButtonPromptFont {
  std::string path,source;
  uint32_t bytes=0;
  std::array<uint8_t,32> sha{},receipt_sha{},source_sha{};
};
// Owns both actual data cores. Admission allocates no Nodes or GPU resources.
class HouseReturnButtonPromptData {
public:
  bool load(const uint8_t*,size_t,const FieldNodeTreeData&,
            const std::array<uint8_t,32>&actual_tree_ir,std::string&);
  bool load_file(const char*,const FieldNodeTreeData&,
                 const std::array<uint8_t,32>&actual_tree_ir,std::string&);
  bool valid()const{return valid_;}
  bool scene_admitted()const{return false;}
  const FieldIdentity&identity()const{return identity_;}
  const auto&ir_sha256()const{return ir_;}
  const auto&tree_ir_sha256()const{return tree_ir_;}
  const FieldPromptData&core()const{return core_;}
  const PromptNativeData&native()const{return native_;}
  const HouseButtonPromptWait&wait()const{return wait_;}
  const HouseButtonPromptFont&font()const{return font_;}
  const std::vector<HouseButtonPromptConnection>&connections()const{return connections_;}
  const HouseButtonPromptConnection*connection(uint32_t)const;
  const std::string&hide_signal()const{return hide_;}
  const std::string&offset_member()const{return offset_;}
  const std::string&enabled_member()const{return enabled_;}
  const std::string&material_source()const{return material_;}
  const std::string&shader_source()const{return shader_;}
  uint32_t shader_declaration()const{return shader_declaration_;}
  bool material_local_to_scene()const{return material_local_;}
  const auto&shader_code_sha256()const{return shader_code_;}
  const auto&material_glow()const{return glow_;}
  const std::vector<std::string>&methods()const{return methods_;}
  bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
  bool valid_=false;
  FieldIdentity identity_{};
  std::array<uint8_t,32>ir_{},tree_ir_{};
  FieldPromptData core_;
  PromptNativeData native_;
  HouseButtonPromptWait wait_;
  HouseButtonPromptFont font_;
  std::string hide_,offset_,enabled_,material_,shader_;
  std::map<std::string,std::array<uint8_t,32>>sources_;
  std::vector<HouseButtonPromptConnection>connections_;
  std::vector<std::string>methods_;
  uint32_t shader_declaration_=0;
  bool material_local_=false;
  std::array<uint8_t,32>shader_code_{};
  std::array<float,4>glow_{};
};
} // namespace encore::upstream
