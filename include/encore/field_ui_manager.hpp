#pragma once
#include "encore/field_global_registry.hpp"
#include "encore/source_random.hpp"
namespace encore::upstream {
class FieldBattleBgResources;
struct FieldUiPreload {uint32_t id=0;bool onready=false;std::string name,path,native_class;std::array<uint8_t,32>sha{};};
struct FieldUiInstance {std::string name,native_class;uint32_t resource=0,recipe=0;};
class FieldUiManagerData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
 bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return valid_;}FieldIdentity identity()const{return identity_;}
 const std::string&source_script()const{return script_;}
 const std::vector<FieldUiPreload>&preloads()const{return preloads_;}
 const std::vector<FieldUiInstance>&instances()const{return instances_;}
 bool method_hash(std::string_view,std::array<uint8_t,32>&)const;
 const FieldNodeRecipeData*recipe(std::string_view)const;
 const FieldNodeRecipeData*recipe(uint32_t)const;
 const std::vector<std::string>&flavors()const{return flavors_;}
 const std::vector<std::array<uint32_t,8>>&palette()const{return palette_;}
 float color_distance_threshold()const{return threshold_;}
 const std::array<FieldColor,8>&old_colors()const{return old_;}
 const std::array<FieldColor,8>&new_colors()const{return fresh_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;FieldIdentity identity_{};std::string script_;float threshold_=0;
 std::vector<FieldUiPreload>preloads_;std::vector<FieldUiInstance>instances_;
 std::vector<FieldNodeRecipeData>recipes_;std::vector<std::string>flavors_;
 std::vector<std::array<uint32_t,8>>palette_;std::array<FieldColor,8>old_{},fresh_{};
 std::map<std::string,std::array<uint8_t,32>>sources_,functions_;
};
// Actual Resource objects owned by the global ObjectDB. Resources never enter
// the SceneTree; instance() uses the whole checked native source recipe.
class FieldUiPackedScene final:public FieldGlobalSourceResource {
public:
 FieldUiPackedScene(FieldGlobalExternalBinding,std::shared_ptr<const FieldNodeRecipeData>);
 const char*resource_class()const override{return "PackedScene";}
 FieldGlobalExternalBinding binding()const override{return binding_;}
 bool state(FieldGlobalExternalState&,std::string&)const override;
 bool deferred(const FieldDeferredMessage&,std::string&)override;
 bool persist_append(FieldObjectId,std::string&)override;
 bool assign_stable_canvas(FieldObjectId,std::string&)override;
 bool instance(FieldNodeTreeRuntime&,FieldObjectId&,std::string&)const;
 const FieldNodeRecipeData&recipe()const{return *recipe_;}
 bool valid()const;
private:FieldGlobalExternalBinding binding_;std::shared_ptr<const FieldNodeRecipeData>recipe_;
};
class FieldUiMenuShader final:public FieldGlobalSourceResource {
public:
 FieldUiMenuShader(FieldGlobalExternalBinding,const FieldUiManagerData&);
 const char*resource_class()const override{return "ShaderMaterial";}
 FieldGlobalExternalBinding binding()const override{return binding_;}
 bool state(FieldGlobalExternalState&,std::string&)const override;
 bool deferred(const FieldDeferredMessage&,std::string&)override;
 bool persist_append(FieldObjectId,std::string&)override;
 bool assign_stable_canvas(FieldObjectId,std::string&)override;
 bool set_flavor(int32_t,std::string&);
 FieldColor shade(FieldColor)const;
 const std::array<FieldColor,8>&new_colors()const{return new_;}
private:FieldGlobalExternalBinding binding_;const FieldUiManagerData*data_;
 std::array<FieldColor,8>old_{},new_{};
};
struct FieldUiManagerHost {
 // Must return a complete checked original PackedScene, never a hash proxy.
 std::function<bool(const FieldUiPreload&,std::shared_ptr<const FieldNodeRecipeData>&,std::string&)>load_recipe;
 std::shared_ptr<FieldNodeTreeRuntime>tree;
 FieldGlobalRegistry::NodeDispatch dispatch;
 std::function<bool(uint64_t& unix_seconds,uint64_t& ticks_usec,std::string&)>clock;
 std::function<bool(std::string& flavor,std::string&)>menu_flavor;
 std::function<bool(std::string&)>emit_menu_flavor_updated;
 // Actual directory/PackedScene typed loader is required at source cursor3.
 FieldBattleBgResources*backgrounds=nullptr;
};
class FieldUiManagerRuntime final:public FieldGlobalExternalObject {
public:
 bool initialize(const FieldUiManagerData&,FieldGlobalRegistry&,SourceRandom&,FieldGlobalExternalBinding,FieldUiManagerHost,std::string&);
 FieldGlobalExternalBinding binding()const override{return binding_;}
 bool state(FieldGlobalExternalState&,std::string&)const override;
 bool deferred(const FieldDeferredMessage&,std::string&)override;
 bool persist_append(FieldObjectId,std::string&)override;
 bool assign_stable_canvas(FieldObjectId,std::string&)override;
 // Actual native parent notification; no false default inside/Ready state.
 bool entered(FieldObjectId,std::string&);
 bool exited(std::string&);
 bool initialize_fields(std::string&);
 // Source callable after field construction; neither Tree entry nor Ready is required.
 bool set_menu_flavors(std::string_view,std::string&);
 bool checked_menu_shader(const FieldUiMenuShader*&,std::string&)const;
 bool advance_ready(std::string&);
 uint32_t ready_cursor()const{return ready_cursor_;}
 const std::map<std::string,FieldObjectId>&instance_objects()const{return instances_;}
 const std::map<std::string,FieldObjectId>&background_resources()const{return backgrounds_;}
 bool tail_pending()const{return ready_cursor_==5;}
private:
 bool preload(const FieldUiPreload&,std::string&);
 const FieldUiManagerData*data_=nullptr;FieldGlobalRegistry*registry_=nullptr;
 SourceRandom*random_=nullptr;FieldGlobalExternalBinding binding_{};FieldUiManagerHost host_{};
 FieldObjectId parent_=0,stable_=0;bool inside_=false,ready_=false;
 uint32_t ready_cursor_=0;size_t fields_cursor_=0,instances_cursor_=0;
 std::map<uint32_t,FieldObjectId>resources_;std::map<uint32_t,FieldUiPackedScene*>packed_;
 FieldUiMenuShader*shader_=nullptr;
 std::map<std::string,FieldObjectId>instances_,backgrounds_;
};
}
