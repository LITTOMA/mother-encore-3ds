#pragma once
#include "encore/blackbars.hpp"
#include "encore/field_scene_sources.hpp"
#include "encore/field_global_data.hpp"
#include "podunk_dialogue_lifecycle_ports.hpp"
#include "podunk_dialogue_options_adapter.hpp"
#include "podunk_dialogue_scene_native.hpp"
#include "podunk_programme_state.hpp"
#include "podunk_stable_canvas.hpp"
namespace encore::ctr {
class PodunkHouseDoorFade;
class PodunkMickHouseTalkerOwner {
public:
  virtual ~PodunkMickHouseTalkerOwner()=default;
  virtual const upstream::FreshHouseState *house()const=0;
  virtual bool set_talking(upstream::FieldObjectId,bool,std::string&)=0;
};
// Read-only borrow of the actual fixed session's owners and live source maps.
// No copied scheduler, notification flags or synthesized callback completion.
struct PodunkMickHouseNativeState {
  upstream::FieldGlobalRegistry *registry=nullptr;
  HouseUiContinuation *ui=nullptr;
  PodunkDialogueHost *dialogue=nullptr;
  PodunkDialogueRootOwner *root=nullptr;
  PodunkDialogueLifecyclePorts *lifecycle=nullptr;
  PodunkDialogueSceneNative *native=nullptr;
  PodunkDialogueProgrammePort *old_programme=nullptr;
  const upstream::FieldNodeRecipeData *recipe=nullptr;
  const upstream::FieldDialogueLifecycleData *life=nullptr;
  const upstream::FieldDialogueAudioData *audio_data=nullptr;
  std::string input_sound_node;
  const upstream::DialogueChoicesData *choice_data=nullptr;
  const upstream::LocaleSelection *locale=nullptr;
  upstream::DialogueChoices *choices=nullptr;
  upstream::FieldGlobalDataRuntime *global_data=nullptr;
  upstream::SourceRandom *random=nullptr;
  std::vector<uint32_t> *uid_ledger=nullptr;
  std::vector<upstream::FieldObjectId> objects,history;
  std::vector<PodunkDialogueWaitConnection> wait_connections;
  PodunkDialogueFrame frame{};
  upstream::FieldObjectId notifying=0,printer_owner=0,callback_receiver=0;
  size_t callback_depth=0,pending_messages=0;
  bool input_live=false,retired=false,business_pending=false;
};
struct PodunkMickInput {
  PodunkHouseContinuation *continuation = nullptr;
  const upstream::PodunkBundleData *bundle = nullptr;
  const upstream::FieldSceneSources *sources = nullptr;
  const upstream::FieldProgrammeData *programmes = nullptr;
  const upstream::BasementProgressionData *basement = nullptr;
  const upstream::FieldPsiData *psi = nullptr;
  const upstream::DialogueChoicesData *choice_data = nullptr;
  upstream::DialogueChoices *choices = nullptr;
  const upstream::LocaleSelection *locale = nullptr;
  upstream::HousePresentation *printer = nullptr;
  upstream::FieldSceneHost *scene = nullptr;
  upstream::FieldNpcRuntime *npc = nullptr;
  PodunkProgrammeState *state = nullptr;
  PodunkPlayerHost *player = nullptr;
  PodunkPlayerCamera *player_camera = nullptr;
  PodunkSceneCameras *cameras = nullptr;
  PodunkAudioServer *audio_server = nullptr;
  upstream::FieldNativeTimers *timers = nullptr;
  upstream::FieldGeometrySpace *geometry = nullptr;
  PodunkPlayerPhysicsWorld *physics = nullptr;
  upstream::Blackbars *bars = nullptr;
  PodunkHouseDoorFade *fade = nullptr;
  const HouseRenderer *house_renderer = nullptr;
  const BattleRenderer *font = nullptr;
  const SourceFontRenderer *source_font = nullptr;
  const upstream::PlayerMotionData *motion = nullptr;
  upstream::HouseView house;
  std::string asset_root;
  std::function<bool(upstream::Vec2 &, std::string &)> controls;
  std::function<bool(std::string_view, upstream::PlayerInputQuery, bool &,
                     std::string &)>
      query_input;
};
// The original Mick programme uses the same scene, printer, inventory, source
// factory and clocks. The source-only prepare installs callbacks before Ready;
// activate is only called after the real door transaction completes.
class PodunkMickSession final : public HouseUiReentryBorrowOwner {
public:
  PodunkMickSession();
  ~PodunkMickSession();
  bool prepare(PodunkMickInput, std::string &);
  bool activate(std::string &);
  void apply_npc(upstream::FieldNpcHost &);
  bool candidate(const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &) const;
  bool owns(upstream::FieldObjectId) const;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  bool phase(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
             upstream::FieldTreePhase, float, bool, bool, std::string &);
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool release(upstream::FieldObjectId, std::string &);
  bool declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                   std::string &) const;
  bool emits_ready(upstream::FieldObjectId) const;
  bool idle_begin(std::string &);
  bool idle_end(uint64_t, float, bool, std::string &);
  bool begin_input(const upstream::PlayerInputEvent &, std::string &);
  bool begin_house_input(upstream::FreshHouseState &,const upstream::PlayerInputEvent &,
                         std::string &);
  bool house_idle_tail(upstream::FreshHouseState &,uint64_t,float,bool,std::string &);
  void end_input();
  bool input_handled() const;
  bool draw(uint64_t, std::string &);
  bool telepathy_effect(upstream::FieldObjectId, bool, std::string &);
  bool active() const;
  // Retire old outdoor traversal after the real Door frees that source scene.
  // Resources/Canvas/dialogue owners stay alive for the checked House adapter.
  bool source_scene_retired() const;
  bool house_native_state(upstream::FreshHouseState &,PodunkMickHouseNativeState &,
                          std::string &) const;
  bool bind_house_talker(upstream::FreshHouseState &,PodunkMickHouseTalkerOwner &,
                         std::string &);
  bool rebind_house_programme(upstream::FreshHouseState &,
      const PodunkDialogueProgrammeBinding &,PodunkDialogueProgrammePort &,
      std::string &);
  bool observe(const HouseUiReentryInput &,const HouseUiContinuation &,bool rebound,
               HouseUiReentryBorrowState &,std::string &) const override;
  bool rebind(const HouseUiReentryInput &,HouseUiContinuation &,std::string &) override;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace encore::ctr
