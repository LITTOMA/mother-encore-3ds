#pragma once
#include "podunk_scene_consumers.hpp"
#include "podunk_scene_visibility.hpp"
namespace encore::ctr {
struct PodunkReadyNativeBridgeInput {
  const upstream::FieldSceneSources *sources = nullptr;
  upstream::FieldNodeTreeRuntime *tree = nullptr;
  upstream::FieldGlobalRegistry *registry = nullptr;
  upstream::FieldObjectSignals *signals = nullptr;
  PodunkSceneNative *native = nullptr;
  PodunkSceneConsumers *consumers = nullptr;
  PodunkSceneVisibility *visibility = nullptr;
  std::function<bool(uint32_t,upstream::FieldObjectId&,std::string&)> source;
};
// Borrows the actual native bodies and the existing typed clip clocks.
class PodunkReadyNativeBridges {
public:
  bool prepare(PodunkReadyNativeBridgeInput,std::string&);
  bool apply(PodunkSceneMechanismOwners&,std::string&);
  bool method_owned(const upstream::FieldDeferredMessage&)const;
  bool dispatch(const upstream::FieldDeferredMessage&,std::string&);
private:
  using Args=std::vector<upstream::FieldDeferredValue>;
  bool actual(uint32_t,upstream::FieldObjectId&,std::string&)const;
  bool emote(uint32_t,const upstream::FieldEmoteInstance&,std::string&);
  bool bush(uint32_t,const upstream::FieldBushInstance&,std::string&);
  bool connect(uint32_t,uint32_t,std::function<bool(const Args&,std::string&)>,std::string&);
  PodunkReadyNativeBridgeInput input_;
  std::map<std::pair<upstream::FieldObjectId,std::string>,
           std::function<bool(const Args&,std::string&)>> methods_;
  bool applied_=false;
};
} // namespace encore::ctr
