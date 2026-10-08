#include <atomic>
#include <malloc.h>
#include "encore/encounter_residency.hpp"
#include <array>
#include <3ds.h>
#include <citro2d.h>
#include "encore/world.hpp"
#include "opening_renderer.hpp"
#include "battle_renderer.hpp"
#include "round_renderer.hpp"
#include "items_renderer.hpp"
#include "field_equipment_renderer.hpp"
#include "encore/field_equipment_menu.hpp"
#include "world_effect_renderer.hpp"
#include "house_renderer.hpp"
#include "phone_renderer.hpp"
#include "present_renderer.hpp"
#include "dialogue_choices_renderer.hpp"
#include "storage_renderer.hpp"
#include "save_menu_renderer.hpp"
#include "encore/session_save.hpp"
#include "encore/session_migration.hpp"
#include "encore/native_session.hpp"
#include "encore/fresh_house.hpp"
#include "encore/load_rng.hpp"
#include "encore/load_progress.hpp"
#include "encore/slot_preference.hpp"
#include "encore/localized_presentation.hpp"
#include "source_font_renderer.hpp"
#include "continue_renderer.hpp"
#include "new_game_renderer.hpp"
#include "introduction_renderer.hpp"
#include "house_button_prompts_renderer.hpp"
#include "loading_indicator.hpp"
#include "encore/loading_task_plan.hpp"
#ifdef ENCORE_FRAME_PROFILE
#include "encore/frame_profile.hpp"
#endif
#include <sys/stat.h>
#include <cerrno>
#include <ctime>
#include <memory>
#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/blackbars.hpp"
#include "encore/battle_action_presentation.hpp"
#include "encore/battle_outcome.hpp"
#include "audio_player.hpp"
#include "music_region_service.hpp"
#include "encore/battle_entry.hpp"
#include "encore/menu_navigation.hpp"
#include "encore/native_input.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/field_scene.hpp"
#include "encore/world_links.hpp"
#include "field_renderer.hpp"
#include "native_input_renderer.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <cstring>

using namespace encore;
namespace {
using upstream::ResourceRole;
upstream::ResourceCatalog resource_catalog;
std::string resource_path(ResourceRole role){return std::string("romfs:/")+resource_catalog.path(role);}
std::string resource_directory(ResourceRole role){const auto path=resource_path(role);return path.substr(0,path.rfind('/'));}
std::string companion_path(const std::string& path){
    if(path.compare(0,7,"romfs:/"))return {};
    const auto relative=resource_catalog.companion_path(std::string_view(path).substr(7));
    return relative.empty()?std::string{}:"romfs:/"+relative;
}
using GameplayScene=upstream::FreshHouseState;
std::unique_ptr<GameplayScene> gameplay_scene=std::make_unique<GameplayScene>();
struct DebugText {C2D_TextBuf buffer=nullptr;C2D_Text text{};std::string value;bool ready=false;};
DebugText debug_text[6];
bool create_debug_text(){for(auto& slot:debug_text){slot.buffer=C2D_TextBufNew(1024);if(!slot.buffer)return false;}return true;}
void free_debug_text(){for(auto& slot:debug_text){if(slot.buffer)C2D_TextBufDelete(slot.buffer);slot.buffer=nullptr;slot.ready=false;slot.value.clear();}}
const u32 ink=C2D_Color32(225,235,235,255),muted=C2D_Color32(139,162,170,255);
const u32 dark=C2D_Color32(13,24,35,255),accent=C2D_Color32(95,214,182,255);

upstream::TitleLocaleData title_locale_data;
upstream::LocaleCatalog locale_catalog;
upstream::LocaleSelection locale_selection;
upstream::LocalizedPresentation localized_text(locale_selection);
ctr::SourceFontRenderer locale_font;
std::string locale_status;
upstream::NativeSessionData native_session_data;
upstream::RestoreData restore_data;
upstream::ContinueMenuData continue_data;
upstream::ContinueMenu continue_menu;
ContinueRenderer continue_renderer;
upstream::NewGameSetupData new_game_data;
upstream::StartupSettingsData startup_settings_data;
upstream::HouseInspectionData house_inspection_data;
upstream::DrawerProgramData drawer_program_data;
upstream::HouseButtonPromptData house_prompt_data;
HouseButtonPromptsRenderer house_prompt_renderer;
upstream::HousePromptObservation house_prompt_observation;
upstream::HousePromptPose house_prompt_pose;
upstream::NewGameSetup new_game_setup;
NewGameRenderer new_game_renderer;
upstream::NewGameSetup accepted_new_game_view;
upstream::IntroductionData introduction_data;
upstream::Introduction introduction;
ctr::IntroductionRenderer introduction_renderer;
bool introduction_house_committed=false;
double introduction_playtime=0;
std::string introduction_render_error;
struct PendingNewGame {
 upstream::SessionSnapshot state;
 upstream::SourceRandom random{0};
 std::vector<uint32_t>uid_ledger;
 bool ready=false;
} pending_new_game;
std::vector<uint32_t>generated_uid_ledger;
u64 load_epoch_tick=0;
std::string continue_status;
bool continue_exit=false,restore_input_pending=false;
bool house_assets_ready=false,house_graphics_ready=false,continue_assets_ready=false,world_effect_assets_ready=false;
std::string loaded_battle_path;
upstream::SessionSnapshot session_state;
bool session_state_ready=false;
std::string save_status;bool save_open_failed=false;
upstream::DialogueChoicesData choice_data;
upstream::DialogueChoices dialogue_choices;
DialogueChoicesRenderer choice_renderer;
upstream::SaveMenuData save_menu_data;
upstream::SaveMenu save_menu;
SaveMenuRenderer save_renderer;
upstream::AudioBank menu_audio_bank;
uint32_t save_generation=0,last_save_slot=0;
upstream::PhoneData phone_data;

ctr::PhoneRenderer phone_renderer;
upstream::PresentData present_data;
ctr::PresentRenderer present_renderer;
upstream::WorldEffectData world_effect_data;
upstream::WorldEffect world_effect;
WorldEffectRenderer world_effect_renderer;
uint32_t world_effect_resource=upstream::kRoomNoIndex,world_effect_npc=upstream::kRoomNoIndex;
uint64_t world_effect_actors=0;
size_t world_effect_consumed=0;
upstream::Vec2 world_effect_center{};
upstream::RoomData opening_data;

OpeningActorRenderer opening_actor;
upstream::HouseData house_data;


HouseRenderer house_renderer;
upstream::Blackbars world_blackbars;
std::string house_error;uint64_t house_sound_requests=0;
struct RoomDrawItem {float y;uint32_t kind,index,order;};
std::vector<RoomDrawItem> room_draw_items;
upstream::BattleData battle_data;
upstream::BattleData house_font_data;
upstream::BattleRewardState session_rewards{};bool session_rewards_valid=false;
upstream::BattleEntry battle_entry;
BattleRenderer battle_renderer;
upstream::BattleRoundData round_data;
upstream::BattleRound battle_round;
upstream::BattleOutcome battle_outcome;
upstream::WorldBattleHost round_host;
upstream::BattleActionPresentation round_presentation;
upstream::SourceRandom battle_random{0};
RoundRenderer round_renderer;
upstream::ItemData items_data;
upstream::ItemDetailsData item_details_data;
ItemDetailsRenderer item_details_renderer;
upstream::FieldEquipmentData field_equipment_data;
upstream::FieldEquipmentMenu field_equipment_menu;
FieldEquipmentRenderer field_equipment_renderer;
encore::ctr::SourceFontRenderer field_counter_font;
BattleRenderer field_counter_text;
bool field_equipment_assets_ready=false;
std::string field_equipment_status;
upstream::InventoryState session_inventory;
upstream::StorageData storage_data;
upstream::StorageState session_storage;
upstream::StorageMenu storage_menu;
StorageRenderer storage_renderer;
encore::ctr::SourceFontRenderer storage_counter_font;
BattleRenderer storage_counter_text;
bool storage_open_failed=false,storage_assets_ready=false;uint32_t storage_generation=0;
upstream::BattleItemsMenu items_menu;
ItemsRenderer items_renderer;
std::string items_status;
bool round_ready=false;
std::string round_error;
ctr::AudioPlayer audio_player;
// Dormant until a real scene factory prepares/commits original Area events.
// No inferred Podunk entry, startup resource scan or early House music change.
ctr::MusicRegionService region_music;
std::string audio_status;
std::vector<std::string> battle_paths;
std::vector<uint32_t> battle_draw_order;
unsigned view_width=400,view_height=240;
bool reference_view=false;
double shader_time=0;
double compose_ms=0,upload_ms=0,background_submit_ms=0;
#ifdef ENCORE_FRAME_PROFILE
FrameProfile frame_profile(CPU_TICKS_PER_MSEC);
bool profile_background_composed=false,profile_background_uploaded=false;
FrameProfile::Context profile_context();
#define PROFILE_START() do { profile_background_composed=profile_background_uploaded=false;frame_profile.start(svcGetSystemTick(),profile_context()); } while(0)
#define PROFILE_MARK(i) frame_profile.mark(i,svcGetSystemTick())
#define PROFILE_FINISH() frame_profile.finish(profile_context())
#define PROFILE_BACKGROUND_DONE() (profile_background_composed=true)
#define PROFILE_UPLOAD_DONE() (profile_background_uploaded=true)
#else
#define PROFILE_START() ((void)0)
#define PROFILE_MARK(i) ((void)0)
#define PROFILE_FINISH() ((void)0)
#define PROFILE_BACKGROUND_DONE() ((void)0)
#define PROFILE_UPLOAD_DONE() ((void)0)
#endif
#ifdef ENCORE_TEXT_QA
FILE* qa_log=nullptr;
void qa_record(u32 down){
    static u64 start=0;static unsigned frames=0;static uint64_t previous=UINT64_MAX;
    const u64 now=osGetTime();if(!start)start=now;++frames;
    const uint64_t state=uint64_t(gameplay_scene->world.stage())|(uint64_t(gameplay_scene->house.phase())<<4)|(uint64_t(battle_entry.phase())<<8)|(uint64_t(battle_round.phase())<<16)|(uint64_t(battle_outcome.phase())<<20)|(uint64_t(items_menu.active())<<24)|(uint64_t(battle_entry.selection())<<25);
    if(!qa_log)return;
    const bool interval=now-start>=1000;
    if(interval||state!=previous||down){
        const auto pos=gameplay_scene->world.player().position;
        int32_t hp=-1,enemy=-1;if(round_ready){const auto b=round_data.view().binding();hp=round_presentation.hp().current_hp();enemy=battle_round.battler(b.enemy_participant).target_hp;}
        std::fprintf(qa_log,"t=%llu state=%llu world=%u house=%u entry=%u round=%u outcome=%u items=%u selection=%u turn=%u hp=%ld enemy=%ld pos=%.3f,%.3f phrase=%u down=%u fps=%.2f compose=%.3f upload=%.3f\n",(unsigned long long)now,(unsigned long long)state,unsigned(gameplay_scene->world.stage()),unsigned(gameplay_scene->house.phase()),unsigned(battle_entry.phase()),unsigned(battle_round.phase()),unsigned(battle_outcome.phase()),unsigned(items_menu.active()),unsigned(battle_entry.selection()),unsigned(battle_round.number()),long(hp),long(enemy),double(pos.x),double(pos.y),unsigned(gameplay_scene->world.phrase()),unsigned(down),now>start?1000.0*frames/double(now-start):0.0,compose_ms,upload_ms);
        std::fprintf(qa_log,"PHONE choice=%u save=%u slot=%u saved=%d bank=%u earned=%u status=%s\n",unsigned(dialogue_choices.phase()),unsigned(save_menu.phase()),unsigned(save_menu.selected_slot()),native_session_data.valid()?int(gameplay_scene->world.story_flag(native_session_data.saved_flag_id())):-1,session_rewards.bank,session_rewards.earned_cash,save_status.c_str());
        if(!round_error.empty())std::fprintf(qa_log,"ROUND_ERROR %s\n",round_error.c_str());
        if(!house_error.empty())std::fprintf(qa_log,"HOUSE_ERROR %s\n",house_error.c_str());
#ifdef ENCORE_FRAME_PROFILE
        // Publish a PREVIOUS completed window. This call, including these
        // diagnostic lines and fflush, is measured in the current QA stage.
        static uint64_t emitted_profile=0;
        const auto& profile=frame_profile.snapshot();
        if(profile.sequence&&profile.sequence!=emitted_profile){
            const auto& m=profile.mean_ms;const auto& c=profile.context;
            std::fprintf(qa_log,"FRAME_PROFILE sequence=%llu frames=%u first_tick=%llu last_tick=%llu battle=%u viewport=%ux%u battle_id=%u core_ms=%.6f sync_ms=%.6f room_ms=%.6f battle_ms=%.6f bottom_ms=%.6f end_ms=%.6f qa_ms=%.6f total_ms=%.6f discarded_frames=%llu\n",(unsigned long long)profile.sequence,unsigned(profile.frames),(unsigned long long)profile.first_tick,(unsigned long long)profile.last_tick,unsigned(c[0]),unsigned(c[1]),unsigned(c[2]),unsigned(c[3]),m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],(unsigned long long)frame_profile.discarded_frames());
            emitted_profile=profile.sequence;
        }
        if(profile_background_composed){
            const auto d=battle_renderer.background_diagnostics();
            std::fprintf(qa_log,"BACKGROUND_FRAME t=%llu composed=1 uploaded=%u direct=%u region_ready=%u region_used=%u preparation=%u samples=%llu skipped=%llu prepared_bytes=%llu compose_ms=%.6f upload_ms=%.6f\n",(unsigned long long)now,unsigned(profile_background_uploaded),unsigned(d.direct_texture),unsigned(d.region_ready),unsigned(d.region_used),unsigned(d.preparation),(unsigned long long)d.samples,(unsigned long long)d.skipped,(unsigned long long)d.prepared_bytes,compose_ms,profile_background_uploaded?upload_ms:0.0);
        }
#endif
        std::fflush(qa_log);previous=state;
    }
    if(interval){frames=0;start=now;}
}
#else
void qa_record(u32){}
#endif
#if defined(ENCORE_FRAME_PROFILE) && defined(ENCORE_TEXT_QA)
bool in_battle();bool battle_handoff_pending();
struct EntryLatencyTrace {u64 requested=0,submitted=0,visible_submitted=0,last_submit=0;unsigned frames=0;u32 submitted_vblank=0,visible_vblank=0;bool presented=false,visible_presented=false;uint32_t generation=0;std::string path;} entry_latency;
void record_entry_request(){
    if(!gameplay_scene->world.battle_request().requested||in_battle()||battle_handoff_pending())return;
    const auto generation=gameplay_scene->world.story_generation();
    if(entry_latency.requested&&!entry_latency.visible_presented&&entry_latency.generation==generation)return;
    entry_latency={};entry_latency.requested=svcGetSystemTick();entry_latency.generation=generation;
    const auto room=opening_data.view();const auto index=gameplay_scene->world.battle_request().battle_resource_index;
    if(index<room.resource_count())entry_latency.path=std::string(room.string(room.resource(index).path_string));
}
void entry_latency_note(const char* stage,u64 tick){
    if(qa_log)std::fprintf(qa_log,"ENTRY_LATENCY stage=%s path=%s elapsed_ms=%.6f compose_ms=%.6f upload_ms=%.6f mask=%u\n",stage,entry_latency.path.c_str(),double(tick-entry_latency.requested)/CPU_TICKS_PER_MSEC,compose_ms,upload_ms,unsigned(battle_entry.mask_active()));
}
void record_entry_presented(){
    if(!entry_latency.requested||gspIsPresentPending(GFX_TOP))return;
    const auto frame=C3D_FrameCounter(0);const auto tick=svcGetSystemTick();
    // A conservative observation bound after GPU completion and two VBlanks;
    // do not insert a wait or claim exact physical LCD scanout/hardware timing.
    if(entry_latency.submitted&&!entry_latency.presented&&frame-entry_latency.submitted_vblank>=2){entry_latency.presented=true;entry_latency_note("first_presented_upper_bound",tick);}
    if(entry_latency.visible_submitted&&!entry_latency.visible_presented&&frame-entry_latency.visible_vblank>=2){entry_latency.visible_presented=true;entry_latency_note("first_mask_presented_upper_bound",tick);}
}
void record_entry_submitted(){
    if(!entry_latency.requested||!in_battle())return;
    const auto tick=svcGetSystemTick();
    if(qa_log&&entry_latency.frames<120){
        std::fprintf(qa_log,"ENTRY_FRAME path=%s frame=%u elapsed_ms=%.6f interval_ms=%.6f compose_ms=%.6f upload_ms=%.6f mask=%u",entry_latency.path.c_str(),entry_latency.frames,double(tick-entry_latency.requested)/CPU_TICKS_PER_MSEC,entry_latency.last_submit?double(tick-entry_latency.last_submit)/CPU_TICKS_PER_MSEC:0.,compose_ms,upload_ms,unsigned(battle_entry.mask_active()));
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        std::fprintf(qa_log," gpu_mask=%u gpu_background=%u spans=%lu",unsigned(battle_renderer.gpu_mask_active()),unsigned(battle_renderer.gpu_background_active()),(unsigned long)battle_renderer.gpu_background_spans());
#endif
        std::fputc('\n',qa_log);++entry_latency.frames;entry_latency.last_submit=tick;
    }
    if(!entry_latency.submitted){entry_latency.submitted=tick;entry_latency.submitted_vblank=C3D_FrameCounter(0);entry_latency_note("first_submission",tick);}
    if(!entry_latency.visible_submitted&&(battle_entry.mask_active()||battle_entry.scene_time()>battle_data.view().parameter(upstream::BattleParameter::MaskDuration).x)){entry_latency.visible_submitted=tick;entry_latency.visible_vblank=C3D_FrameCounter(0);entry_latency_note("first_mask_submission",tick);}
}
#else
void record_entry_request(){}void record_entry_presented(){}void record_entry_submitted(){}
#endif
C3D_RenderTarget* loading_top=nullptr;
ctr::LoadingIndicatorRenderer loading_indicator;
float view_x();float view_y();
uint64_t loading_generation=0;
struct LoadingScope {
    const char* label;u64 started=0,last_frame=0;unsigned frames=0,next_task=0;
    LoadingTaskPlan plan;bool force_frame=false;
    ScopedLoadProgress observer;
    explicit LoadingScope(const char* value,unsigned tasks):label(value),started(osGetTime()),observer(pulse,this){plan.begin(tasks);report_load_progress(LoadPhase::Scene,0,0);}
    bool complete(){if(!plan.complete_task(next_task))return false;++next_task;report_load_progress(LoadPhase::Scene,next_task,next_task);return true;}
    template<class Work> bool step(Work work,const char* resource="task"){
        const auto tick=svcGetSystemTick();const bool ok=work();
        char line[240];const int n=std::snprintf(line,sizeof(line),"ENCORE_LOAD_TASK scope=%s task=%u resource=%s elapsed_ms=%.3f ok=%u\n",label,next_task,resource,double(svcGetSystemTick()-tick)/CPU_TICKS_PER_MSEC,unsigned(ok));
        if(n>0)svcOutputDebugString(line,size_t(n));
#ifdef ENCORE_TEXT_QA
        if(qa_log){std::fprintf(qa_log,"%s",line);std::fflush(qa_log);}
#endif
        if(!ok){plan.fail();return false;}return complete();
    }
    bool finish(){if(!plan.finish())return false;force_frame=true;report_load_progress(LoadPhase::Scene,next_task,next_task);force_frame=false;return true;}
    static void pulse(void* context,const LoadProgress& progress){
        (void)progress;
        auto& self=*static_cast<LoadingScope*>(context);const u64 now=osGetTime();
        if(!loading_top||!loading_indicator.ready()||(!self.force_frame&&self.frames&&now-self.last_frame<50))return;
        // First-use PCM checking can happen while title/naming music is active.
        // Feed existing queues without advancing game clocks or fade timelines.
        if(audio_player.available()){std::string audio_error;if(!audio_player.pump_streams(audio_error))audio_status=audio_error;}
        // Keep first-use reads out of the playing story and its door fades.
        // At the final HouseReady boundary the story visuals are retired, but
        // Introduction stays active until the House reveal completes. Permit
        // loading frames only while that destination is being prepared; once
        // committed, preserve the reveal again. Do not advance game clocks.
        if(introduction.active()&&(!introduction.house_ready()||introduction_house_committed))return;
        if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))return;
        C2D_TargetClear(loading_top,C2D_Color32(0,0,0,255));C2D_SceneBegin(loading_top);
        C2D_DrawRectSolid(view_x(),view_y(),0,float(view_width),float(view_height),loading_indicator.background_color());
        C3D_Mtx saved_view;C2D_ViewSave(&saved_view);C2D_ViewTranslate(view_x(),view_y());
        loading_indicator.draw_progress(int(view_width),int(view_height),double(now-self.started)/1000.0,0.0); // Fixed bottom-left; task progress is diagnostic only.
        C2D_ViewRestore(&saved_view);
        C3D_FrameEnd(0);self.last_frame=now;++self.frames;
#ifdef ENCORE_TEXT_QA
        if(qa_log){std::fprintf(qa_log,"LOADING stage=%s elapsed=%llu heartbeat=%u phase=%u completed=%llu total=%llu\n",self.label,(unsigned long long)(now-self.started),self.frames,unsigned(progress.phase),(unsigned long long)progress.completed,(unsigned long long)progress.total);std::fprintf(qa_log,"LOADING_ACTOR frame=%d elapsed=%llu viewport=%ux%u task_percent=%u\n",loading_indicator.frame_at(double(now-self.started)/1000.0),(unsigned long long)(now-self.started),view_width,view_height,unsigned(self.plan.percent()));std::fflush(qa_log);}
#endif
    }
    ~LoadingScope(){
        if(plan.state()==LoadingTaskPlanState::Running)plan.fail();
        ++loading_generation;
#ifdef ENCORE_TEXT_QA
        if(qa_log){std::fprintf(qa_log,"LOAD_DONE stage=%s elapsed=%llu heartbeats=%u committed=%u tasks=%llu/%llu\n",label,(unsigned long long)(osGetTime()-started),frames,unsigned(plan.state()==LoadingTaskPlanState::Succeeded),(unsigned long long)plan.completed_tasks(),(unsigned long long)plan.total_tasks());std::fflush(qa_log);}
#endif
    }
};
void upload_battle_surface(){const auto start=svcGetSystemTick();battle_renderer.upload_surface();upload_ms=double(svcGetSystemTick()-start)/CPU_TICKS_PER_MSEC;PROFILE_UPLOAD_DONE();}
float view_x(){return (400-float(view_width))*0.5f;}
float view_y(){return (240-float(view_height))*0.5f;}
// An empty frame waits for the prior command queue without referencing textures.
void wait_for_gpu_idle(){if(C3D_FrameBegin(0))C3D_FrameEnd(0);}
void texture_owner_checkpoint(const char*phase){
    char line[192];const int count=std::snprintf(line,sizeof(line),"ENCORE_TEXTURE_OWNER phase=%s linear_free=%lu locale_font_bytes=%lu\n",phase,(unsigned long)linearSpaceFree(),(unsigned long)locale_font.resident_bytes());
    if(count>0)svcOutputDebugString(line,std::min<size_t>(size_t(count),sizeof(line)-1));
#ifdef ENCORE_TEXT_QA
    if(qa_log){std::fprintf(qa_log,"%s",line);std::fflush(qa_log);}
#endif
}
bool localized_glyph_advance(void*,uint32_t cp,float&advance){
    if(locale_font.handles(cp))return locale_font.glyph_advance(cp,advance);
    const auto view=house_font_data.view();for(uint32_t i=0;i<view.count(upstream::BattleSection::Glyphs);++i){const auto g=view.glyph(i);if(g.codepoint==cp&&g.advance>0){advance=g.advance;return true;}}return false;
}
void bind_localized_house(upstream::HousePresentation&presentation){
    presentation.set_locale_resolver(upstream::LocalizedPresentation::resolve_house,&localized_text);
    presentation.set_glyph_advance(localized_glyph_advance,nullptr);
    const auto*face=locale_font.catalog().selected();presentation.set_font_line_height(face&&!face->legacy_ascii?face->height:0);
}
bool select_native_locale(std::string_view code,bool persist,std::string&error){
    const std::string previous(locale_selection.code());const bool reload_title=continue_renderer.ready();if(!locale_selection.select(code,error))return false;
    const auto index=locale_catalog.locale_index(locale_selection.code());const auto&info=locale_catalog.locales()[size_t(index)];
    wait_for_gpu_idle();
    bool ok=locale_font.select_font_at_safe_boundary(info.font,error)&&locale_font.admit_selected_font(error);
    if(ok&&reload_title)ok=continue_renderer.load(continue_data,"romfs:/",error);
    if(ok&&persist){::mkdir("sdmc:/3ds",0777);::mkdir("sdmc:/3ds/encore-native",0777);ok=upstream::write_locale_preference(locale_selection,"sdmc:/3ds/encore-native/language.encprefs",error);}
    if(!ok){const std::string problem=error;std::string restore;if(locale_selection.select(previous,restore)){const auto i=locale_catalog.locale_index(previous);if(!locale_font.select_font_at_safe_boundary(locale_catalog.locales()[size_t(i)].font,restore)||!locale_font.admit_selected_font(restore))house_error="Locale rollback failed: "+restore;if(reload_title&&!continue_renderer.load(continue_data,"romfs:/",restore))house_error="Title locale rollback failed: "+restore;}error=problem;return false;}
    texture_owner_checkpoint("locale-admitted");battle_renderer.attach_source_font(&locale_font);new_game_setup.set_locale(&locale_selection);items_renderer.set_locale(&locale_selection);item_details_renderer.set_locale(locale_selection.code());choice_renderer.set_locale(&locale_selection);bind_localized_house(gameplay_scene->presentation);field_equipment_renderer.set_locale(&locale_selection);field_equipment_menu.set_chinese(locale_selection.code()=="zh_Hans_CN");locale_status.clear();return true;
}
bool initialize_localization(std::string&error){
    if(locale_catalog.valid())return select_native_locale(locale_selection.code(),false,error);
    if(!locale_catalog.load_file(resource_path(ResourceRole::Localization).c_str(),error)||!locale_selection.bind(locale_catalog,error)||!locale_font.load_catalog_at_safe_boundary(resource_path(ResourceRole::SourceFonts).c_str(),resource_directory(ResourceRole::SourceFonts).c_str(),error))return false;
    if(!title_locale_data.load_file(resource_path(ResourceRole::TitleLocale).c_str(),error)||!continue_renderer.attach_localized_title(title_locale_data,locale_selection,error))return false;
    std::string restored;const auto status=upstream::read_locale_preference(locale_catalog,"sdmc:/3ds/encore-native/language.encprefs",restored,locale_status);
    const auto code=status==upstream::LocalePreferenceRead::Loaded?std::string_view(restored):locale_catalog.fallback();
    const std::string notice=locale_status;if(!select_native_locale(code,false,error))return false;locale_status=notice;return true;
}
void cycle_native_locale(int direction){
    int index=locale_catalog.locale_index(locale_selection.code());for(size_t i=0;i<locale_catalog.locales().size();++i){index=(index+direction+int(locale_catalog.locales().size()))%int(locale_catalog.locales().size());const auto&choice=locale_catalog.locales()[size_t(index)];if(choice.source_enabled&&choice.native_ready){std::string error;if(!select_native_locale(choice.code,true,error))locale_status=error;return;}}
}
bool battle_handoff_pending(){return battle_outcome.phase()==upstream::BattleOutcomePhase::PostWinRequested;}
bool in_battle(){return battle_entry.phase()!=upstream::BattleEntryPhase::Idle&&battle_outcome.phase()!=upstream::BattleOutcomePhase::Complete&&!battle_handoff_pending();}
#ifdef ENCORE_FRAME_PROFILE
FrameProfile::Context profile_context(){const bool battle=in_battle();return {{uint32_t(battle),view_width,view_height,battle?battle_data.view().metadata().room_battle_id:0u}};}
#endif
u32 battle_color(upstream::BattleValue c){return C2D_Color32f(c.x,c.y,c.z,c.w);}
struct BattlePrepareTimings {double resource_ms=0,background_ms=0;};
bool prepare_battle_presentation(upstream::BattleView view,BattleRenderer& target,std::vector<std::string>& paths,std::vector<uint32_t>& order,unsigned width,unsigned height,std::string& error,bool cpu_only=false,const std::vector<std::string>* resident=nullptr,BattlePrepareTimings* timings=nullptr,const upstream::RoundView* round=nullptr,const PreparationControl* control=nullptr){
    using namespace upstream;
    paths.clear();paths.reserve(view.count(BattleSection::Resources)+(round?round->count(RoundSection::Resources):0));
    std::vector<BattleRenderer::Resource>resources;
    for(uint32_t i=0;i<view.count(BattleSection::Resources);++i)paths.push_back("romfs:/"+std::string(view.string(view.resource(i).path)));
    for(uint32_t i=0;i<paths.size();++i){const auto r=view.resource(i);resources.push_back({paths[i].c_str(),r.kind,r.width,r.height,r.columns,r.rows,resident&&std::find(resident->begin(),resident->end(),ctr::loading_texture_key(paths[i].c_str()))!=resident->end()});}
    if(round)for(uint32_t i=0;i<round->count(RoundSection::Resources);++i){const auto r=round->resource(i);const std::string path="romfs:/"+std::string(round->string(r.path));
        const auto old=std::find(paths.begin(),paths.end(),path);
        if(old!=paths.end()){const auto& prior=resources[size_t(old-paths.begin())];if(prior.kind!=r.kind||prior.width!=r.width||prior.height!=r.height){error="Encounter dependency union has conflicting texture identity";return false;}continue;}
        paths.push_back(path);resources.push_back({paths.back().c_str(),r.kind,r.width,r.height,r.columns,r.rows,resident&&std::find(resident->begin(),resident->end(),ctr::loading_texture_key(path.c_str()))!=resident->end()});
    }
    const auto resource_start=svcGetSystemTick();if(!target.load(resources,width,height,error,cpu_only,control))return false;
    if(timings)timings->resource_ms=double(svcGetSystemTick()-resource_start)/CPU_TICKS_PER_MSEC;
    std::vector<BattleRenderer::Glyph>glyphs;
    for(uint32_t i=0;i<view.count(BattleSection::Glyphs);++i){const auto g=view.glyph(i);glyphs.push_back({g.codepoint,g.resource,g.u,g.v,g.w,g.h,g.advance,g.offset_x,g.offset_y});}
    if(!target.set_glyphs(std::move(glyphs),error))return false;
    std::vector<BattleRenderer::BackgroundLayer>layers;
    for(uint32_t i=0;i<view.count(BattleSection::Backgrounds);++i){const auto b=view.background(i);BattleRenderer::BackgroundLayer l;
        l.resource=b.resource;l.barrel=(b.flags&1)!=0;l.repeat=(b.flags&2)!=0;l.width=b.width;l.height=b.height;l.opacity=b.opacity;l.effect=b.effect;l.effect_scale=b.effect_scale;
        l.barrel_x=b.barrel.x;l.barrel_y=b.barrel.y;l.amplitude_x=b.oscillation_amplitude.x;l.amplitude_y=b.oscillation_amplitude.y;
        l.frequency_x=b.oscillation_frequency.x;l.frequency_y=b.oscillation_frequency.y;l.speed_x=b.oscillation_speed.x;l.speed_y=b.oscillation_speed.y;
        l.move_x=b.move.x;l.move_y=b.move.y;l.ping_pong_speed_x=b.ping_pong_speed.x;l.ping_pong_speed_y=b.ping_pong_speed.y;
        l.compression_amplitude_x=b.compression_amplitude.x;l.compression_amplitude_y=b.compression_amplitude.y;
        l.compression_frequency_x=b.compression_frequency.x;l.compression_frequency_y=b.compression_frequency.y;
        l.compression_speed_x=b.compression_speed.x;l.compression_speed_y=b.compression_speed.y;
        l.palette_shifting=(b.flags&4)!=0;l.palette_resource=b.palette_resource;l.palette_frames=b.palette_frames;l.palette_speed=b.palette_speed;l.palette_fixed_row=b.palette_fixed_row;layers.push_back(l);
    }
    const auto background_start=svcGetSystemTick();if(!target.set_background(layers,error,control))return false;
    const auto canvas=view.parameter(BattleParameter::CanvasSize);
    if(cpu_only&&!target.prepare_transition(view.metadata().mask_resource,int((float(width)-canvas.x)*0.5f),int((float(height)-canvas.y)*0.5f),error,control))return false;
    if(timings)timings->background_ms=double(svcGetSystemTick()-background_start)/CPU_TICKS_PER_MSEC;
    order.clear();for(uint32_t i=0;i<view.count(BattleSection::Layouts);++i)order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](uint32_t a,uint32_t b){return view.layout(a).depth<view.layout(b).depth;});
    return true;
}
bool load_battle_presentation(std::string& error){
    BattlePrepareTimings timing;const bool ok=prepare_battle_presentation(battle_data.view(),battle_renderer,battle_paths,battle_draw_order,view_width,view_height,error,false,nullptr,&timing);
    char line[200];const int n=std::snprintf(line,sizeof(line),"ENCORE_BATTLE_SYNC resources_ms=%.3f background_ms=%.3f ok=%u\n",timing.resource_ms,timing.background_ms,unsigned(ok));if(n>0)svcOutputDebugString(line,size_t(n));
    if(ok)texture_owner_checkpoint("battle-renderer-loaded");
    return ok;
}
#include "battle_prewarm.hpp"
// Naming/title text needs font assets, not live encounter backgrounds, masks,
// actor atlases or their 400x240 working surface. Release that exclusive owner
// before admitting six naming actors; a subsequent encounter reloads normally.
bool load_ui_font_only(std::string&error){
    cancel_battle_prewarm();
    using namespace upstream;const auto view=house_font_data.view();std::vector<uint32_t>ids;
    for(uint32_t i=0;i<view.count(BattleSection::Glyphs);++i){const auto id=view.glyph(i).resource;if(std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);}
    std::vector<std::string>paths;paths.reserve(ids.size());for(auto id:ids)paths.push_back("romfs:/"+std::string(view.string(view.resource(id).path)));
    std::vector<BattleRenderer::Resource>resources;for(size_t i=0;i<ids.size();++i){const auto r=view.resource(ids[i]);resources.push_back({paths[i].c_str(),r.kind,r.width,r.height,r.columns,r.rows});}
    if(!battle_renderer.load(resources,1,1,error))return false;
    std::vector<BattleRenderer::Glyph>glyphs;for(uint32_t i=0;i<view.count(BattleSection::Glyphs);++i){const auto g=view.glyph(i);const auto id=uint32_t(std::find(ids.begin(),ids.end(),g.resource)-ids.begin());glyphs.push_back({g.codepoint,id,g.u,g.v,g.w,g.h,g.advance,g.offset_x,g.offset_y});}
    loaded_battle_path.clear();const bool ok=battle_renderer.set_glyphs(std::move(glyphs),error);if(ok)texture_owner_checkpoint("ui-font-only");return ok;
}
// Title and naming need checked world metadata for save admission, not the
// room's image owners. Prepare those once before exposing a new/loaded house.
bool ensure_house_graphics(std::string&error){
    if(house_graphics_ready)return true;
    wait_for_gpu_idle();LoadingScope loading("Preparing house graphics",4);
    if(!loading.step([&]{return opening_actor.load(opening_data.view(),error);},"room-atlases")||
       !loading.step([&]{return house_renderer.load(house_data.view(),"romfs:/",error)&&(present_data.view().valid()||present_data.load_file(resource_path(ResourceRole::HousePresents).c_str(),error))&&present_renderer.load(present_data.view(),"romfs:/",error);},"house-atlases")||
       !loading.step([&]{if(!items_renderer.load(items_data.view(),"romfs:/",error)||!item_details_renderer.load(item_details_data.view(),items_data.view(),"romfs:/",error))return false;
           item_details_renderer.set_locale(locale_selection.code());items_renderer.set_details(&item_details_renderer);storage_renderer.set_details(&item_details_renderer);return true;},"item-atlases")||
       !loading.step([&]{
           if(!audio_player.available())return true;
           const auto room=opening_data.view();
           for(uint32_t i=0;i<room.resource_count();++i){const auto r=room.resource(i);
               if(r.kind!=uint16_t(upstream::RoomResourceKind::AudioRequestOnly))continue;
               // Prefetch source identity, not saved/gameplay identity: one
               // source can have separate room and cutscene bank stable IDs.
               bool found=false;
               for(uint32_t j=0;j<menu_audio_bank.count();++j){const auto asset=menu_audio_bank.asset(j);
                   if(asset.source_path==room.string(r.path_string)&&asset.source_sha256==r.sha256){
                       found=true;if(!audio_player.prepare(asset.stable_id,error))return false;break;}
               }
               if(!found){error="House audio source is absent from checked bank";return false;}
           }
           return true;
       },"house-audio")||!loading.finish())return false;
    house_graphics_ready=true;return true;
}
bool initialize_world_effect(std::string&error){
    world_effect_resource=upstream::kRoomNoIndex;world_effect_consumed=0;world_effect_actors=0;world_effect_npc=upstream::kRoomNoIndex;
    const auto room=opening_data.view();
    for(uint32_t i=0;i<room.resource_count();++i)if(room.resource(i).kind==uint16_t(upstream::RoomResourceKind::CheckedWorldEffectPack)){
        if(world_effect_resource!=upstream::kRoomNoIndex){error="Multiple world effects require an explicit presentation extension";return false;}
        world_effect_resource=i;
    }
    if(world_effect_resource==upstream::kRoomNoIndex){error="World effect resource is missing";return false;}
    const std::string path="romfs:/"+std::string(room.string(room.resource(world_effect_resource).path_string));
    if(!world_effect_assets_ready){if(!world_effect_data.load_file(path.c_str(),error)||!world_effect_renderer.load(world_effect_data.view(),view_width,view_height,error))return false;world_effect_assets_ready=true;}
    if(!world_effect.initialize(world_effect_data.view()))return false;
    return true;
}
bool advance_world_effect(double delta,std::string&error){
    const auto&requests=gameplay_scene->world.effect_requests();
    if(world_effect_consumed>requests.size()){error="World effect request history reset";return false;}
    while(world_effect_consumed<requests.size()){
        const auto&r=requests[world_effect_consumed++];if(r.resource_index!=world_effect_resource){error="Unsupported world effect resource";return false;}
        if(r.appear){world_effect_center=r.center;world_effect_actors=r.actor_mask;world_effect_npc=r.npc_index;if(!world_effect.appear()){error="World effect appear rejected";return false;}}
        else if(!world_effect.disappear()){error="World effect disappear rejected";return false;}
    }
    if(!world_effect.advance(delta)){error="World effect time rejected";return false;}
    if(!world_effect.active()){world_effect_actors=0;world_effect_npc=upstream::kRoomNoIndex;}
    return true;
}
bool resolve_dialogue_value(void*state,upstream::HouseTokenKind kind,std::string&value){
    if(!session_rewards_valid)return false;
    if(kind==upstream::HouseTokenKind::EarnedCash){value=std::to_string(session_rewards.earned_cash);session_rewards.earned_cash=0;return static_cast<upstream::OpeningWorld*>(state)->set_story_flag(native_session_data.earned_cash_flag_id(),false,false);}
    if(kind==upstream::HouseTokenKind::BankCash){value=std::to_string(session_rewards.bank);return true;}
    if(kind==upstream::HouseTokenKind::CurrentCash){value=std::to_string(session_rewards.cash);return true;}
    return false;
}
bool play_source_audio(const std::string&,ctr::AudioLane,float,float);
// Scene-independent effect owner: scene text/flags are handled by HouseRuntime.
// Every mutation uses stable inventory/RNG owners shared by the actual game.
class DrawerEffects final:public upstream::DrawerHost {
 uint32_t definition(std::string_view name)const{const auto items=items_data.view();for(uint32_t i=0;i<items.count(upstream::ItemSection::Definitions);++i)if(items.string(items.definition(i).source)==name)return i;return upstream::item_no_index;}
public:
 bool validate_text(uint32_t,std::string&e)override{e="Scene text must be owned by HouseRuntime";return false;}
 bool validate_flag(std::string_view,std::string&e)override{e="Scene flags must be owned by HouseRuntime";return false;}
 bool validate_item(upstream::DrawerItemTemplate t,std::string_view name,std::string&e)override{
  const auto index=definition(name);if(index==upstream::item_no_index||t.key_item){e="Unbound normal inventory grant";return false;}
  const auto d=items_data.view().definition(index);if(d.flags&uint32_t(upstream::ItemDefinitionFlag::Equipment)){e="Grant defaults are not compatible with item definition";return false;}
  for(const auto&p:native_session_data.acquisitions())if(p.item_id==name&&p.doses==t.doses&&p.max_count==1){e.clear();return true;}
  e="Grant is absent from checked session acquisition policy";return false;
 }
 bool validate_sound(std::string_view name,std::string&e)override{
  uint32_t matches=0;for(uint32_t i=0;i<menu_audio_bank.count();++i){const auto a=menu_audio_bank.asset(i);if(a.source_path==name||(a.source_path.substr(0,6)=="res://"&&a.source_path.substr(6)==name))++matches;}
  if(matches!=1){e="Inspection sound mapping absent/ambiguous";return false;}e.clear();return true;
 }
 bool show_text(uint32_t,std::string&e)override{e="Scene text must be owned by HouseRuntime";return false;}
 bool flag(std::string_view,bool&,std::string&e)override{e="Scene flags must be owned by HouseRuntime";return false;}
 bool inventory_space()const override{return session_inventory.has_space();}
 bool grant_item(upstream::DrawerItemTemplate t,std::string_view name,std::string&e)override{
  if(!validate_item(t,name,e))return false;const auto index=definition(name);
  if(!session_inventory.can_append(index,t.doses,e))return false;
  auto random=battle_random;auto ledger=generated_uid_ledger;auto inventory=session_inventory;
  const upstream::LoadRngClockProvider clock=[](upstream::LoadRngClockSample&sample,std::string&why){const auto now=std::time(nullptr);if(now<0){why="Clock unavailable for source item entropy";return false;}const u64 ticks=svcGetSystemTick()-load_epoch_tick;sample.unix_seconds=uint64_t(now);sample.ticks_usec=(ticks/SYSCLOCK_ARM11)*1000000+(ticks%SYSCLOCK_ARM11)*1000000/SYSCLOCK_ARM11;return true;};
  std::vector<upstream::LoadUidAllocation> trace;
  if(!upstream::apply_load_uid_allocations(random,ledger,{{1,1}},clock,e,&trace)||trace.size()!=1){if(e.empty())e="Source item UID allocation incomplete";return false;}
  if(!inventory.append(index,t.doses,trace.front().generated_uid,e))return false;
  // Saved UIDs remain opaque. A collision with a retained UID rejects the
  // whole transaction rather than adding an invented source random draw.
  session_inventory=std::move(inventory);battle_random=random;generated_uid_ledger=std::move(ledger);e.clear();return true;
 }
 bool play_sound(std::string_view name,std::string&e)override{if(!validate_sound(name,e))return false;if(!play_source_audio(std::string(name),ctr::AudioLane::Effect,0,1)){e=house_error.empty()?audio_status:house_error;return false;}return true;}
 bool set_flag(std::string_view,bool,std::string&e)override{e="Scene flags must be owned by HouseRuntime";return false;}
} drawer_effects;
// Present key-item grants: Inventory.add_item_available -> key_items (never full),
// with Item.new's source randomize/randi UID on the shared stream and ledger.
class PresentEffects final:public upstream::PresentEffects {
public:
 bool validate_item(upstream::PresentTemplate t,std::string_view name,std::string&e)override{
  if(!t.key_item){e="Present grant is not a reviewed key item";return false;}
  for(const auto&k:native_session_data.key_acquisitions())if(k.item_id==name&&k.doses==t.doses){e.clear();return true;}
  e="Present grant is absent from checked session key acquisitions";return false;
 }
 bool validate_sound(std::string_view name,std::string&e)override{return drawer_effects.validate_sound(name,e);}
 bool grant_item(upstream::PresentTemplate t,std::string_view name,std::string&e)override{
  if(!validate_item(t,name,e))return false;
  for(const auto&item:session_state.key_items)if(item.item_id==name){e="Present key item already held";return false;}
  auto random=battle_random;auto ledger=generated_uid_ledger;
  const upstream::LoadRngClockProvider clock=[](upstream::LoadRngClockSample&sample,std::string&why){const auto now=std::time(nullptr);if(now<0){why="Clock unavailable for source item entropy";return false;}const u64 ticks=svcGetSystemTick()-load_epoch_tick;sample.unix_seconds=uint64_t(now);sample.ticks_usec=(ticks/SYSCLOCK_ARM11)*1000000+(ticks%SYSCLOCK_ARM11)*1000000/SYSCLOCK_ARM11;return true;};
  std::vector<upstream::LoadUidAllocation> trace;
  if(!upstream::apply_load_uid_allocations(random,ledger,{{1,1}},clock,e,&trace)||trace.size()!=1){if(e.empty())e="Source key item UID allocation incomplete";return false;}
  session_state.key_items.push_back({std::string(name),false,int64_t(t.doses),trace.front().generated_uid});
  battle_random=random;generated_uid_ledger=std::move(ledger);e.clear();return true;
 }
 bool play_sound(std::string_view name,std::string&e)override{return drawer_effects.play_sound(name,e);}
} present_effects;
// The Present's own AudioStreamPlayer gets its own lane so Stop never cuts other sounds.
void process_present_audio(){
 for(const auto&r:gameplay_scene->presents.take_audio()){
  if(!audio_player.available())continue;std::string error;
  if(r.kind==upstream::PresentAudioKind::Stop){if(!audio_player.stop_lane(ctr::AudioLane::AuxiliaryEffect0,error))audio_status=error;}
  else if(!play_source_audio(std::string(r.sound),ctr::AudioLane::AuxiliaryEffect0,0,1)&&house_error.empty())house_error="Present sound rejected";
 }
}
bool bind_house_presents(GameplayScene& scene,std::string& error){
 if(!present_data.view().valid()&&!present_data.load_file(resource_path(ResourceRole::HousePresents).c_str(),error))return false;
 if(!scene.house.bind_presents(scene.presents,present_data.view(),present_effects)){error=scene.house.error();return false;}
 return true;
}
// Linked exterior field scenes. The House owner stays the only save/LOAD
// authority; a field owns movement, TileMap collision and explicit boundaries.
upstream::WorldLinksData world_links;
upstream::RoomData field_room_data;
upstream::FieldMapData field_map_data;
std::unique_ptr<upstream::FieldScene> field_scene;
OpeningActorRenderer field_actor;
FieldRenderer field_renderer;
upstream::SceneDoorTransition scene_door;
uint64_t door_sound_requests=0;
std::string field_status;
bool in_field(){return field_scene!=nullptr;}
bool bind_house_routes(upstream::HouseRuntime& house,std::string& error){
    using namespace upstream;
    if(!world_links.valid()&&!world_links.load_file(resource_path(ResourceRole::WorldLinks).c_str(),error))return false;
    const auto room=opening_data.view();const auto index=world_links.find_scene(room.scene().stable_id);
    if(index==WorldLinksData::kNotFound||world_links.scene(index).source!=room.string(room.scene().source_scene_string)){error="House scene is absent from checked world links";return false;}
    if(!house.bind_scene_routes(world_links,room.scene().stable_id)){error=house.error();return false;}
    return true;
}
bool initialize_house_interactions(std::string& error){
    house_error.clear();house_sound_requests=0;
    if(!initialize_localization(error))return false;
    if(!gameplay_scene->presentation.begin(house_data.view(),house_font_data.view(),battle_random)){error=gameplay_scene->presentation.error();return false;}
    bind_localized_house(gameplay_scene->presentation);
    if(!gameplay_scene->house.initialize(house_data.view(),gameplay_scene->world,gameplay_scene->presentation)){error=gameplay_scene->house.error();return false;}
    if(!house_inspection_data.view().valid()&&!house_inspection_data.load_file(resource_path(ResourceRole::HouseInspections).c_str(),error))return false;
    if(!drawer_program_data.view().valid()&&!drawer_program_data.load_file(resource_path(ResourceRole::DrawerProgram).c_str(),error))return false;
    if(!house_assets_ready){
        if(!new_game_data.load_file(resource_path(ResourceRole::NewGame).c_str(),error))return false;
        if(!phone_data.load_file(resource_path(ResourceRole::Phone).c_str(),error)||!phone_renderer.load(phone_data.view(),"romfs:/",error))return false;
        if(!choice_data.load_file(resource_path(ResourceRole::Choices).c_str(),error)||!choice_renderer.load(choice_data,"romfs:/",error)||!save_menu_data.load_file(resource_path(ResourceRole::SaveMenu).c_str(),error)||!save_renderer.load(save_menu_data,"romfs:/",error)||!menu_audio_bank.load_file(resource_path(ResourceRole::Audio).c_str(),error)||!native_session_data.load_file(resource_path(ResourceRole::Session).c_str(),error))return false;
        if(!startup_settings_data.load_file(resource_path(ResourceRole::Settings).c_str(),error))return false;
        if(startup_settings_data.speeds!=native_session_data.text_speeds()||startup_settings_data.flavors!=native_session_data.menu_flavors()||startup_settings_data.prompts!=native_session_data.button_prompts()){error="Startup UI/session setting choices disagree";return false;}
        wait_for_gpu_idle();if(!ctr::loading_menu_flavor_configure(startup_settings_data.skin_paths,startup_settings_data.source_palette,startup_settings_data.palettes,startup_settings_data.palette_threshold,startup_settings_data.default_indices[1])){error="Checked UI palette binding failed";return false;}
        if(!house_prompt_data.load_file(resource_path(ResourceRole::Prompts).c_str(),error)||!house_prompt_data.validate_bindings(house_data.view(),phone_data.view(),error,house_inspection_data.view())||!house_prompt_renderer.load(house_prompt_data,error))return false;
        new_game_renderer.bind_prompts(house_prompt_renderer);
        house_assets_ready=true;
    }
    if(!gameplay_scene->house.bind_drawer(drawer_program_data.view(),drawer_effects)||!gameplay_scene->house.bind_inspections(house_inspection_data.view())){error=gameplay_scene->house.error();return false;}
    if(!bind_house_presents(*gameplay_scene,error))return false;
    if(!gameplay_scene->phone.initialize(phone_data.view())||!gameplay_scene->house.bind_phone(gameplay_scene->phone)){error=gameplay_scene->house.error();return false;}
    dialogue_choices.close();save_menu.close();save_open_failed=false;
    gameplay_scene->house.bind_choices(choice_data,dialogue_choices);
    if(!bind_house_routes(gameplay_scene->house,error))return false;
    if(!session_state_ready){session_state=native_session_data.defaults();session_state_ready=true;last_save_slot=0;}
    if(!gameplay_scene->presentation.set_text_speed(session_state.settings.text_speed)){error="Session dialogue speed rejected";return false;}
    wait_for_gpu_idle();if(!ctr::loading_menu_flavor_select(uint32_t(startup_settings_data.flavor_index(session_state.settings.menu_flavor)))){error="Session menu flavor rejected";return false;}
    gameplay_scene->world.set_party_leader(native_session_data.leader_id());
    if(!gameplay_scene->house.set_player_nickname(session_state.characters.front().nickname)){error="Default nickname rejected";return false;}
    gameplay_scene->presentation.set_text_value_callback(resolve_dialogue_value,&gameplay_scene->world);
    if(!initialize_world_effect(error))return false;
    return true;
}
bool update_house_prompt(std::string&error){
 using namespace upstream;const auto&world=gameplay_scene->world;auto&observation=house_prompt_observation;observation.player=world.player().position;observation.direction=world.player().direction;observation.crouching=world.player().crouch;observation.paused=field_equipment_menu.visible()||world.stage()!=OpeningStage::Walking||world.house_paused()||gameplay_scene->house.blocks_player()||gameplay_scene->house.entering_door()||new_game_setup.active()||(introduction.active()&&!introduction.house_unpaused())||continue_menu.is_open()||in_battle()||battle_handoff_pending();
 observation.targets.clear();observation.targets.reserve(house_prompt_data.targets.size());
 for(const auto&t:house_prompt_data.targets){HousePromptTargetObservation o;o.position=t.position;
  if(t.kind==HousePromptKind::Npc){const auto pose=gameplay_scene->presentation.npc_pose(t.index);o.position=pose.position;o.visible=pose.visible;o.enabled=true;o.supported=gameplay_scene->house.npc_interaction_supported(t.index);}
  else if(t.kind==HousePromptKind::Door){const auto&state=gameplay_scene->house.openable_state(t.index);o.visible=true;o.enabled=!state.unlocked&&!state.action;o.supported=gameplay_scene->house.door_interaction_supported(t.index);}
  else if(t.kind==HousePromptKind::Phone){o.visible=true;o.enabled=true;o.supported=gameplay_scene->house.phone_interaction_supported(t.index);}
  else{o.visible=gameplay_scene->house.inspection_visible(t.index);o.enabled=true;o.supported=gameplay_scene->house.inspection_interaction_supported(t.index);}
  observation.targets.push_back(o);
 }
 return evaluate_house_button_prompt(house_prompt_data,uint32_t(startup_settings_data.prompt_index(session_state.settings.button_prompts)),observation,house_prompt_pose,error);
}
bool prepare_source_audio(const std::string& path,std::string&error){
    for(uint32_t i=0;i<menu_audio_bank.count();++i){const auto asset=menu_audio_bank.asset(i);
        if(asset.source_path==path||(asset.source_path.substr(0,6)=="res://"&&asset.source_path.substr(6)==path)){
            if(!audio_player.available()){error.clear();return true;}
            return audio_player.prepare(asset.stable_id,error);
        }
    }
    error="Sound is absent from checked audio bank";return false;
}
bool prepare_title_audio(std::string&error){
    if(!prepare_source_audio(continue_data.title_music(),error))return false;
    for(uint32_t i=0;i<uint32_t(upstream::ContinueSound::Count);++i)
        if(!prepare_source_audio(continue_data.sound(upstream::ContinueSound(i)),error))return false;
    return true;
}
bool play_source_audio(const std::string& path,ctr::AudioLane lane=ctr::AudioLane::Effect,float gain=0,float pitch=1){
    for(uint32_t i=0;i<menu_audio_bank.count();++i){const auto asset=menu_audio_bank.asset(i);if(asset.source_path==path||(asset.source_path.substr(0,6)=="res://"&&asset.source_path.substr(6)==path)){if(!audio_player.available())return true;std::string error;
        if(!audio_player.prepared(asset.stable_id)){LoadingScope loading("Preparing sound",1);if(!loading.step([&]{return audio_player.prepare(asset.stable_id,error);},"pcm-validation")||!loading.finish()){audio_status=error;return false;}}
        if(!audio_player.play(asset.stable_id,lane,error,gain,0,pitch)){audio_status=error;return false;}return true;}}
    house_error="Menu sound is absent from checked audio bank";return false;
}
bool load_introduction(std::string&error){
    if(!introduction_data.valid()&&!introduction_data.load_file(resource_path(ResourceRole::Introduction).c_str(),error))return false;
    const auto& destination=introduction_data.house_destination();const auto& state=native_session_data.defaults();
    if(destination.scene!=state.scene_id||destination.x!=state.position_x||destination.y!=state.position_y||destination.dx!=state.direction_x||destination.dy!=state.direction_y||!destination.set_respawn||!destination.unpause){error="Introduction destination is outside the supported startup scene";return false;}
    if(!introduction_data.locale(std::string(locale_selection.code()))){error="Introduction locale is not supported";return false;}
    error.clear();return true;
}
bool open_storage_menu(std::string&error){
    using namespace upstream;
    if(!session_rewards_valid||!session_storage.valid()){error="Storage lacks a prepared live session";return false;}
    if(!storage_assets_ready){
        LoadingScope loading("Preparing storage",3);
        if(!loading.step([&]{return storage_renderer.load(storage_data.view(),items_data.view(),"romfs:/",error);},"storage-textures")||
           !loading.step([&]{return load_introduction(error)&&storage_counter_font.load_catalog_at_safe_boundary((std::string("romfs:/")+introduction_data.font_catalog).c_str(),"romfs:/fonts/introduction/",error)&&storage_counter_font.select_font_at_safe_boundary(storage_data.view().binding(StorageBinding::CounterFont),error)&&storage_counter_font.admit_selected_font(error);},"storage-counter-font")||
           !loading.step([&]{return storage_menu.initialize(storage_data.view(),session_inventory,session_storage,session_rewards,locale_selection.code()=="zh_Hans_CN");},"storage-model")||!loading.finish())return false;
        storage_counter_text.attach_source_font(&storage_counter_font);storage_assets_ready=true;
    }else if(!storage_menu.initialize(storage_data.view(),session_inventory,session_storage,session_rewards,locale_selection.code()=="zh_Hans_CN")){error=storage_menu.error();return false;}
    storage_renderer.set_locale(&locale_selection);
    if(!storage_menu.open()){error=storage_menu.error();return false;}return true;
}
bool begin_introduction(std::string&error){
    if(!pending_new_game.ready||new_game_setup.phase()!=upstream::NamingPhase::Accepted){error="Introduction needs an accepted isolated startup";return false;}
    if(!load_introduction(error)||!introduction_renderer.load(introduction_data,error))return false;
    // Admit every actual play source before the animation starts. Preparation
    // keeps bounded stream buffers; it never plays a voice or consumes game RNG.
    std::vector<std::string> sources{introduction_data.music};for(const auto&p:introduction_data.text_sounds)sources.push_back(p);
    for(const auto&scene:introduction_data.scenes)for(const auto&event:scene.events)if(event.kind==5)sources.push_back(event.source);
    std::vector<uint32_t> ids;
    for(const auto&path:sources){uint32_t matches=0,id=0;for(uint32_t i=0;i<menu_audio_bank.count();++i){const auto a=menu_audio_bank.asset(i);if(a.source_path==path||(a.source_path.substr(0,6)=="res://"&&a.source_path.substr(6)==path)){++matches;id=a.stable_id;}}
        if(matches!=1){error="Introduction sound mapping absent/ambiguous";introduction_renderer.free();return false;}
        if(audio_player.available()&&!audio_player.prepared(id)&&std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);
    }
    if(!ids.empty()){LoadingScope loading("Preparing introduction audio",uint32_t(ids.size()));for(auto id:ids)if(!loading.step([&]{return audio_player.prepare(id,error);},"introduction-pcm-admission")){introduction_renderer.free();return false;}if(!loading.finish()){error="Introduction audio preparation incomplete";introduction_renderer.free();return false;}}
    if(!introduction.begin(introduction_data,pending_new_game.random,std::string(locale_selection.code()),float(view_width),float(view_height),error)){introduction_renderer.free();return false;}
    introduction_house_committed=false;introduction_playtime=0;introduction_render_error.clear();error.clear();return true;
}
bool consume_introduction_audio(std::string&error){
    for(const auto& event:introduction.take_audio()){
        using Kind=upstream::IntroAudioKind;using Lane=upstream::IntroAudioLane;
        if((event.kind!=Kind::Play&&event.kind!=Kind::FadeMusic&&event.kind!=Kind::StopNamed)||(event.lane!=Lane::Music&&event.lane!=Lane::Text&&event.lane!=Lane::Effect)||event.effect_slot>1){error="Unknown introduction audio operation/lane";return false;}
        if(event.kind==Kind::Play){
            const auto lane=event.lane==Lane::Music?ctr::AudioLane::Music:event.lane==Lane::Text?ctr::AudioLane::Effect:event.effect_slot==0?ctr::AudioLane::AuxiliaryEffect0:ctr::AudioLane::AuxiliaryEffect1;
            if(!play_source_audio(event.source_path,lane,float(event.gain_db),float(event.pitch))){error=house_error.empty()?audio_status:house_error;return false;}
        }else if(audio_player.available()){
            if(event.kind==Kind::FadeMusic){if(!audio_player.fade_all_music(event.seconds,error))return false;}
            else if(event.kind==Kind::StopNamed){if(!audio_player.stop_lane(event.effect_slot==0?ctr::AudioLane::AuxiliaryEffect0:ctr::AudioLane::AuxiliaryEffect1,error))return false;}
            else{error="Unknown introduction audio operation";return false;}
        }
    }
    error.clear();return true;
}
std::string session_slot_path(uint32_t slot){return "sdmc:/3ds/encore-native/save-"+std::to_string(slot)+".encsave";}
upstream::SaveSlotMetadata slot_metadata(const upstream::SessionSnapshot&state){
    upstream::SaveSlotMetadata metadata;metadata.occupied=true;metadata.scene_label=state.scene_label;metadata.menu_flavor=state.settings.menu_flavor;metadata.playtime_seconds=state.playtime_seconds;metadata.party=state.party;
    for(const auto&character:state.characters){metadata.highest_level=std::max(metadata.highest_level,uint32_t(character.level));if(!state.party.empty()&&character.character_id==state.party.front())metadata.lead_name=character.nickname;}
    return metadata;
}
bool collect_session_snapshot(upstream::SessionSnapshot&result,std::string&error){
    using namespace upstream;if(!session_state_ready||!session_rewards_valid){error="Session save lacks live battle state";return false;}
    auto state=session_state;const auto player=gameplay_scene->world.player();state.position_x=player.position.x;state.position_y=player.position.y;state.direction_x=player.direction.x;state.direction_y=player.direction.y;
    const auto room=opening_data.view();state.flags.clear();for(uint32_t i=0;i<room.flag_count();++i){const std::string id(room.string(room.flag(i).name_string));state.flags.push_back({id,gameplay_scene->world.story_flag(id)});}
    for(auto index:gameplay_scene->house.seen_dialogue_keys()){
        const std::string id(house_data.view().string(index));auto found=std::find_if(state.seen_dialogue_flags.begin(),state.seen_dialogue_flags.end(),[&](const auto&value){return value.id==id;});if(found==state.seen_dialogue_flags.end())state.seen_dialogue_flags.push_back({id,true});else found->value=true;
    }
    const std::time_t now=std::time(nullptr);if(const auto*value=std::gmtime(&now)){char stamp[32];if(std::strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",value))state.saved_at=stamp;}
    NativeSnapshotInput input;input.state=std::move(state);input.stats=&session_rewards;input.inventory=&session_inventory;input.storage=&session_storage;
    return build_native_session_snapshot(native_session_data,room,house_data.view(),round_data.view(),items_data.view(),input,result,error);
}
// Prepare equipment in independent state; the platform commits only after the
// shared session contract accepts all inventory identities and derived stats.
struct PreparedFieldEquipment {
    upstream::InventoryState inventory;
    upstream::BattleSessionStats stats;
    std::array<int32_t,7> projected{};
};
bool prepare_field_equipment(uint32_t slot,bool none,uint32_t uid,PreparedFieldEquipment& result,std::string&error){
    using namespace upstream;
    const auto view=field_equipment_data.view();
    if(!view.valid()||slot>=view.count(FieldSection::Slots)||!session_rewards_valid||!session_state_ready){error="Equipment lacks a checked live session";return false;}
    SessionSnapshot snapshot;if(!collect_session_snapshot(snapshot,error))return false;
    if(snapshot.characters.size()!=1||snapshot.party.size()!=1||snapshot.characters.front().character_id!=view.binding(FieldBinding::Owner)){error="Equipment character scope rejected";return false;}
    auto inventory=session_inventory;
    bool cleared=false;
    for(const auto&item:session_inventory.instances())if(item.equipped&&items_data.view().definition(item.definition).equipment_slot==slot){
        if(!inventory.equip_uid(item.id,false,error))return false;cleared=true;
    }
    if(none){if(!cleared){error="Equipment None requires an equipped item";return false;}}
    else {
        const auto&instances=inventory.instances();const auto at=std::find_if(instances.begin(),instances.end(),[&](const ItemInstance&i){return i.id==uid;});
        bool admitted=false;
        if(at!=instances.end())for(uint32_t i=0;i<view.count(FieldSection::Equipment);++i){const auto policy=view.equipment(i);if(policy.definition==at->definition&&policy.slot==slot)admitted=true;}
        if(!admitted||at==instances.end()||items_data.view().definition(at->definition).equipment_slot!=slot){error="Equipment candidate identity/slot rejected";return false;}
        if(!inventory.equip_uid(uid,true,error))return false;
    }
    auto character=snapshot.characters.front();character.inventory.clear();
    for(const auto&item:inventory.instances())character.inventory.push_back({std::string(items_data.view().string(items_data.view().definition(item.definition).source)),item.equipped!=0,int64_t(item.doses),item.id});
    std::array<int32_t,7> derived{};if(!native_session_derived_stats(native_session_data,items_data.view(),character,derived,error))return false;
    auto stats=session_rewards;stats.maxhp=derived[0];stats.maxpp=derived[1];stats.offense=derived[2];stats.defense=derived[3];stats.speed=derived[4];stats.iq=derived[5];stats.guts=derived[6];
    stats.hp=std::min(stats.hp,stats.maxhp);stats.pp=std::min(stats.pp,stats.maxpp);
    NativeSnapshotInput input;input.state=std::move(snapshot);input.stats=&stats;input.inventory=&inventory;input.storage=&session_storage;
    SessionSnapshot checked;if(!build_native_session_snapshot(native_session_data,opening_data.view(),house_data.view(),round_data.view(),items_data.view(),input,checked,error))return false;
    result.inventory=std::move(inventory);result.stats=std::move(stats);result.projected=derived;error.clear();return true;
}
bool open_field_equipment(std::string&error){
    using namespace upstream;
    if(!session_state_ready||!session_rewards_valid){error="Field menu needs a prepared session";return false;}
    const auto view=field_equipment_data.view();
    if(!field_equipment_assets_ready){
        LoadingScope loading("Preparing equipment menu",3);
        if(!loading.step([&]{return field_equipment_renderer.load(view,items_data.view(),"romfs:/",error);},"equipment-textures")||
           !loading.step([&]{return load_introduction(error)&&field_counter_font.load_catalog_at_safe_boundary((std::string("romfs:/")+introduction_data.font_catalog).c_str(),"romfs:/fonts/introduction/",error)&&field_counter_font.select_font_at_safe_boundary(view.binding(FieldBinding::NumberFont),error)&&field_counter_font.admit_selected_font(error);},"equipment-number-font")||
           !loading.step([&]{for(uint32_t i=uint32_t(FieldBinding::PauseOpenSound);i<=uint32_t(FieldBinding::BackSound);++i)if(!prepare_source_audio(std::string(view.binding(FieldBinding(i))),error))return false;return true;},"equipment-sounds")||!loading.finish())return false;
        field_counter_text.attach_source_font(&field_counter_font);field_equipment_assets_ready=true;
    }
    field_equipment_renderer.set_locale(&locale_selection);field_equipment_renderer.set_details(&item_details_renderer);
    FieldEquipmentHost host;
    host.read=[&](FieldEquipmentSnapshot&out,std::string&e){
        SessionSnapshot snapshot;if(!collect_session_snapshot(snapshot,e))return false;
        if(snapshot.characters.size()!=1||snapshot.party.size()!=1){e="Field menu party scope rejected";return false;}
        FieldEquipmentSnapshot next;next.owner=snapshot.characters.front().character_id;next.nickname=snapshot.characters.front().nickname;next.level=session_rewards.level;next.cash=session_rewards.cash;next.inventory=session_inventory;
        next.stats={{session_rewards.maxhp,session_rewards.maxpp,session_rewards.offense,session_rewards.defense,session_rewards.speed,session_rewards.iq,session_rewards.guts}};
        out=std::move(next);e.clear();return true;
    };
    host.preview=[](uint32_t slot,bool none,uint32_t uid,std::array<int32_t,7>&out,std::string&e){PreparedFieldEquipment candidate;if(!prepare_field_equipment(slot,none,uid,candidate,e))return false;out=candidate.projected;return true;};
    host.commit=[](uint32_t slot,bool none,uint32_t uid,std::string&e){PreparedFieldEquipment candidate;if(!prepare_field_equipment(slot,none,uid,candidate,e))return false;session_inventory=std::move(candidate.inventory);session_rewards=std::move(candidate.stats);return true;};
    if(!field_equipment_menu.initialize(view,std::move(host),locale_selection.code()=="zh_Hans_CN")||!field_equipment_menu.open()){error=field_equipment_menu.error();return false;}
    field_equipment_status.clear();return true;
}

bool read_prepared_slot(uint32_t slot,upstream::SessionSnapshot&snapshot,upstream::PreparedSessionRestore&prepared,std::string&error){
    if(slot<1||slot>save_menu_data.slot_count()){error="Selected slot is out of range";return false;}
    return upstream::read_compatible_session_restore(session_slot_path(slot).c_str(),resource_path(ResourceRole::SessionMigration).c_str(),native_session_data,opening_data.view(),house_data.view(),round_data.view(),items_data.view(),house_font_data.view(),snapshot,prepared,error);
}
bool scan_record_slots(std::vector<upstream::SaveSlotMetadata>&slots,std::string&error){
    slots.assign(save_menu_data.slot_count(),{});
    for(uint32_t i=0;i<slots.size();++i){const auto path=session_slot_path(i+1);errno=0;FILE*file=std::fopen(path.c_str(),"rb");if(!file){if(errno==ENOENT)continue;error="Cannot inspect existing save slot";return false;}std::fclose(file);
        upstream::SessionSnapshot saved;upstream::PreparedSessionRestore prepared;if(!read_prepared_slot(i+1,saved,prepared,error))return false;slots[i]=slot_metadata(saved);}
    error.clear();return true;
}
bool remember_slot(uint32_t slot,std::string&error){
    last_save_slot=slot;return upstream::write_slot_preference("sdmc:/3ds/encore-native/settings.encprefs",save_menu_data.slot_count(),slot,error);
}
bool open_record_menu(std::string&error){
    std::vector<upstream::SaveSlotMetadata>slots;if(!scan_record_slots(slots,error))return false;
    save_generation=gameplay_scene->world.story_generation();save_status.clear();return save_menu.open(save_menu_data,slots,last_save_slot?last_save_slot:1,error);
}
bool write_record_slot(uint32_t slot,std::string&error){
    if(slot<1||slot>save_menu_data.slot_count()){error="Invalid requested save slot";return false;}
    for(const char*path:{"sdmc:/3ds","sdmc:/3ds/encore-native"})if(::mkdir(path,0777)!=0&&errno!=EEXIST){error="Cannot create native save directory";return false;}
    upstream::SessionSnapshot snapshot;if(!collect_session_snapshot(snapshot,error))return false;
    if(!upstream::write_session_save(session_slot_path(slot).c_str(),snapshot,native_session_data.compatibility(),error)){
        if(error.find("Refusing to replace invalid/incompatible primary session save:")==0)error="Old/incompatible slot preserved. Choose an empty slot to Record.";
        return false;
    }
    if(!gameplay_scene->world.set_story_flag(native_session_data.saved_flag_id(),true,false)){error="Written save could not update live save flag";return false;}
    const auto metadata=slot_metadata(snapshot);if(!save_menu.acknowledge_save(true,&metadata,error))return false;
    std::string preference_error;remember_slot(slot,preference_error);error=preference_error;return true;
}
void initialize_audio(){audio_status.clear();if(!audio_player.initialize(resource_path(ResourceRole::Audio).c_str(),"romfs:/",audio_status))audio_status="Audio unavailable: "+audio_status;}
bool open_continue(std::string&error,LoadingScope* parent=nullptr){
    cancel_battle_prewarm();
    std::unique_ptr<LoadingScope> owned;
    if(!parent)owned=std::make_unique<LoadingScope>("Preparing title",continue_assets_ready?(continue_renderer.ready()?2:3):5);
    auto& loading=parent?*parent:*owned;
    continue_menu.close();
    if(!continue_assets_ready){if(!loading.step([&]{return continue_data.load_file(resource_path(ResourceRole::Continue).c_str(),error);})||!loading.step([&]{return continue_renderer.load(continue_data,"romfs:/",error);})||!loading.step([&]{return restore_data.load_file(resource_path(ResourceRole::Restore).c_str(),opening_data.view(),house_data.view(),error);}))return false;continue_assets_ready=true;}
    else if(!continue_renderer.ready()&&!loading.step([&]{return continue_renderer.load(continue_data,"romfs:/",error);}))return false;
    uint32_t remembered=0;std::string preference_error;const auto result=upstream::read_slot_preference("sdmc:/3ds/encore-native/settings.encprefs",save_menu_data.slot_count(),remembered,preference_error);
    if(result==upstream::SlotPreferenceReadResult::Error)continue_status="Slot preference unavailable: "+preference_error;
    else continue_status="Original Load/Play; names, settings, Introduction and House";
    last_save_slot=remembered;std::vector<upstream::SaveSlotMetadata>slots;
    if(!loading.step([&]{return scan_record_slots(slots,error);}))return false;
    return loading.step([&]{return continue_menu.open(continue_data,save_menu_data,slots,remembered,error);})&&(!owned||loading.finish());
}
bool apply_loaded_slot(uint32_t slot,std::string&error){
    cancel_battle_prewarm();
    LoadingScope loading("Loading saved game",4);
    using namespace upstream;SessionSnapshot snapshot;PreparedSessionRestore prepared;
    if(!loading.step([&]{return read_prepared_slot(slot,snapshot,prepared,error);}))return false;
    std::unique_ptr<GameplayScene> candidate;
    if(!loading.step([&]{return prepare_fresh_house(prepared,restore_data,opening_data.view(),house_data.view(),house_font_data.view(),phone_data.view(),battle_random,{float(view_width),float(view_height)},candidate,error);}))return false;
    std::vector<LoadInventoryAllocation>allocations;
    for(const auto&row:restore_data.inventory_load_order()){
        size_t count=0;
        if(row.rebuilds_inventory){
            if(row.kind==RestoreInventoryKind::KeyItems)count=snapshot.key_items.size();
            else if(row.kind==RestoreInventoryKind::Storage)count=snapshot.storage.size();
            else {const auto found=std::find_if(snapshot.characters.begin(),snapshot.characters.end(),[&](const auto&character){return character.character_id==row.character_id;});count=found==snapshot.characters.end()?row.projected_items.size():found->inventory.size();}
        }
        allocations.push_back({row.order_id,uint32_t(count)});
    }
    auto next_random=battle_random;auto next_ledger=generated_uid_ledger;
    const LoadRngClockProvider clocks=[](LoadRngClockSample&sample,std::string&why){const auto unix_now=std::time(nullptr);if(unix_now<0){why="Clock unavailable for source LOAD entropy";return false;}const u64 ticks=svcGetSystemTick()-load_epoch_tick;sample.unix_seconds=uint64_t(unix_now);sample.ticks_usec=(ticks/SYSCLOCK_ARM11)*1000000+(ticks%SYSCLOCK_ARM11)*1000000/SYSCLOCK_ARM11;return true;};
    if(!loading.step([&]{return apply_load_uid_allocations(next_random,next_ledger,allocations,clocks,error);}))return false;
    if(!candidate->house.bind_drawer(drawer_program_data.view(),drawer_effects)||!candidate->house.bind_inspections(house_inspection_data.view())){error=candidate->house.error();return false;}
    if(!bind_house_presents(*candidate,error))return false;
    bind_localized_house(candidate->presentation);
    if(!candidate->presentation.set_text_speed(prepared.state.settings.text_speed)){error="Prepared session text speed rejected";return false;}
    candidate->house.bind_choices(choice_data,dialogue_choices);
    candidate->presentation.set_text_value_callback(resolve_dialogue_value,&candidate->world);
    if(!bind_house_routes(candidate->house,error))return false;
    if(!candidate->finish_scene_ready()||!candidate->world.pause_for_house()){error="Fresh scene ready boundary rejected";return false;}
    // Source order: load_game UID fallbacks, then the scene's Sparkles _ready draws.
    if(!candidate->presents.scene_ready(next_random,error))return false;
    if(!ensure_house_graphics(error)||!admit_encounter_scene(error))return false;
    // Stable owners are committed together. All validation/clock failures above
    // leave the old scene, inventory, random stream and game files untouched.
    battle_entry=BattleEntry{};battle_round=BattleRound{};battle_outcome=BattleOutcome{};round_presentation=BattleActionPresentation{};round_ready=false;round_error.clear();
    wait_for_gpu_idle();ctr::loading_menu_flavor_select(uint32_t(startup_settings_data.flavor_index(prepared.state.settings.menu_flavor)));
    session_state=std::move(prepared.state);session_rewards=std::move(prepared.stats);session_inventory=std::move(prepared.inventory);session_storage=std::move(prepared.storage);session_state_ready=session_rewards_valid=true;
    battle_random=next_random;generated_uid_ledger=std::move(next_ledger);gameplay_scene.swap(candidate);restore_input_pending=true;
    dialogue_choices.close();save_menu.close();save_open_failed=storage_open_failed=false;storage_menu=upstream::StorageMenu{};field_equipment_menu=upstream::FieldEquipmentMenu{};field_equipment_status.clear();save_status.clear();items_status.clear();house_error.clear();house_sound_requests=0;
    if(!items_menu.initialize(session_inventory)){error=items_menu.error();return false;}
    world_effect.reset();world_effect_consumed=0;world_effect_actors=0;world_effect_npc=kRoomNoIndex;audio_player.reset_scene_requests();
    std::string preference_error;remember_slot(slot,preference_error);continue_status="Loaded slot "+std::to_string(slot);if(!preference_error.empty())continue_status+="; preference: "+preference_error;
    error.clear();return loading.complete()&&loading.finish();
}
bool stage_new_game(std::string&error){
    using namespace upstream;PendingNewGame pending;pending.random=battle_random;pending.uid_ledger=generated_uid_ledger;
    const LoadRngClockProvider clocks=[](LoadRngClockSample&sample,std::string&why){const auto unix_now=std::time(nullptr);if(unix_now<0){why="Clock unavailable for source startup entropy";return false;}const u64 ticks=svcGetSystemTick()-load_epoch_tick;sample.unix_seconds=uint64_t(unix_now);sample.ticks_usec=(ticks/SYSCLOCK_ARM11)*1000000+(ticks%SYSCLOCK_ARM11)*1000000/SYSCLOCK_ARM11;return true;};
    if(!stage_new_game_startup(native_session_data,restore_data,pending.random,pending.uid_ledger,clocks,pending.state,error))return false;
    pending.ready=true;pending_new_game=std::move(pending);return true;
}
bool commit_named_new_game(std::string&error){
    using namespace upstream;LoadingScope loading("Starting named game",3);
    if(!pending_new_game.ready){error="Missing isolated startup candidate";return false;}
    auto snapshot=pending_new_game.state;snapshot.playtime_seconds=introduction_playtime;auto random=pending_new_game.random;PreparedSessionRestore prepared;
    if(!loading.step([&]{return new_game_setup.apply(snapshot,native_session_data,error)&&prepare_session_restore(native_session_data,opening_data.view(),house_data.view(),round_data.view(),items_data.view(),house_font_data.view(),snapshot,prepared,error);}))return false;
    std::unique_ptr<GameplayScene>candidate;
    if(!loading.step([&]{return prepare_fresh_house(prepared,restore_data,opening_data.view(),house_data.view(),house_font_data.view(),phone_data.view(),random,{float(view_width),float(view_height)},candidate,error);}))return false;
    if(!candidate->house.bind_drawer(drawer_program_data.view(),drawer_effects)||!candidate->house.bind_inspections(house_inspection_data.view())){error=candidate->house.error();return false;}
    if(!bind_house_presents(*candidate,error))return false;
    bind_localized_house(candidate->presentation);
    if(!candidate->presentation.set_text_speed(prepared.state.settings.text_speed)){error="Prepared session text speed rejected";return false;}
    candidate->house.bind_choices(choice_data,dialogue_choices);candidate->presentation.set_text_value_callback(resolve_dialogue_value,&candidate->world);
    if(!bind_house_routes(candidate->house,error))return false;
    BattleItemsMenu checked_menu;if(!checked_menu.initialize(prepared.inventory)||!candidate->finish_scene_ready()){error="Named startup scene/menu preparation rejected";return false;}
    if(!candidate->presents.scene_ready(random,error))return false;
    if(!ensure_house_graphics(error)||!admit_encounter_scene(error))return false;
    if(!loading.complete()||!loading.finish())return false;
    // Pending names and UID allocations become visible only with the new owner.
    // Failures above preserve the prior session, RNG, ledger and every save file.
    battle_entry=BattleEntry{};battle_round=BattleRound{};battle_outcome=BattleOutcome{};round_presentation=BattleActionPresentation{};round_ready=false;round_error.clear();
    wait_for_gpu_idle();ctr::loading_menu_flavor_select(uint32_t(startup_settings_data.flavor_index(prepared.state.settings.menu_flavor)));
    session_state=std::move(prepared.state);session_rewards=std::move(prepared.stats);session_inventory=std::move(prepared.inventory);session_storage=std::move(prepared.storage);session_state_ready=session_rewards_valid=true;
    battle_random=random;generated_uid_ledger=std::move(pending_new_game.uid_ledger);candidate->world.attach_random(battle_random);candidate->presentation.rebind_random(battle_random);gameplay_scene.swap(candidate);pending_new_game=PendingNewGame{};restore_input_pending=false;last_save_slot=0;
    dialogue_choices.close();save_menu.close();save_open_failed=storage_open_failed=false;storage_menu=upstream::StorageMenu{};field_equipment_menu=upstream::FieldEquipmentMenu{};field_equipment_status.clear();save_status.clear();items_status.clear();house_error.clear();house_sound_requests=0;items_menu.initialize(session_inventory);
    world_effect.reset();world_effect_consumed=0;world_effect_actors=0;world_effect_npc=kRoomNoIndex;
    region_music.shutdown();audio_player.reset_scene_requests();
    if(audio_player.available()&&!audio_player.stop_lane(ctr::AudioLane::Effect,error))return false;
    error.clear();return true;
}
// The uiManager fade clips and circle shader live in the reviewed Introduction data.
bool begin_scene_door(uint32_t route,std::string& error){
    if(!load_introduction(error))return false;
    return scene_door.begin(world_links,route,introduction_data,error);
}
void process_scene_door_events(){
    using K=upstream::SceneDoorEventKind;
    for(const auto& e:scene_door.take_events()){
        if(e.kind==K::SetFlag){
            // Door._set_flag writes globaldata.flags directly; no flags_updated signal.
            auto& world=in_field()?field_scene->world:gameplay_scene->world;
            if(!world.set_story_flag(e.text,e.value,false))field_status="Door flag write rejected";
        }else if(e.kind==K::FadeMusic){std::string error;if(audio_player.available()&&!audio_player.fade_music(e.seconds,error))audio_status=error;}
        // Door sounds are not in the checked audio bank yet; requests are counted like House door sounds.
        else if(e.kind==K::PlaySound)++door_sound_requests;
        else if(e.kind==K::Unpause){
            if(in_field()){if(!field_scene->finish_transition())field_status=field_scene->error();}
            else if(!gameplay_scene->world.unpause_from_house())house_error="Scene door unpause rejected";
        }
    }
}
void release_field_graphics(){wait_for_gpu_idle();field_renderer.free();field_actor.free();}
void release_house_graphics(){
    cancel_battle_prewarm();wait_for_gpu_idle();
    opening_actor.free();house_renderer.free();present_renderer.free();room_draw_items.clear();house_graphics_ready=false;
}
bool load_field_resources(std::string& error){
    using namespace upstream;
    if(field_room_data.empty()&&!field_room_data.load_file(resource_path(ResourceRole::FieldRoom).c_str(),error))return false;
    if(!field_map_data.view().valid()&&!field_map_data.load_file(resource_path(ResourceRole::FieldMap).c_str(),error))return false;
    // Flags cross scenes by stable name; the field room must carry the House table.
    const auto field=field_room_data.view(),house=opening_data.view();
    if(field.flag_count()!=house.flag_count()){error="Field flag table differs from the House table";return false;}
    for(uint32_t i=0;i<field.flag_count();++i)if(field.string(field.flag(i).name_string)!=house.string(house.flag(i).name_string)||field.flag(i).stable_id!=house.flag(i).stable_id){error="Field flag identity differs from the House table";return false;}
    return true;
}
bool enter_field(const upstream::WorldRoute& route,std::string& error){
    using namespace upstream;
    LoadingScope loading("Entering field",4);
    SessionSnapshot snapshot;
    if(!loading.step([&]{return collect_session_snapshot(snapshot,error);},"house-state")||
       !loading.step([&]{return load_field_resources(error);},"field-metadata"))return false;
    const auto room=field_room_data.view();std::vector<bool> flags(room.flag_count(),false);
    for(uint32_t i=0;i<room.flag_count();++i)flags[i]=gameplay_scene->world.story_flag(room.string(room.flag(i).name_string));
    auto candidate=std::make_unique<FieldScene>();
    if(!loading.step([&]{return candidate->prepare(room,field_map_data.view(),world_links,flags,route.destination,route.direction,{float(view_width),float(view_height)},error);},"field-scene"))return false;
    release_house_graphics();
    if(!loading.step([&]{return field_actor.load(room,error)&&field_renderer.load(field_map_data.view(),std::string(locale_selection.code()),error);},"field-atlases")){
        release_field_graphics();std::string restore;if(!ensure_house_graphics(restore))house_error="House graphics restore failed: "+restore;return false;}
    if(!loading.finish())return false;
    // Commit: the House owner stays parked (paused in SceneTransition) behind the field.
    candidate->world.attach_random(battle_random);
    session_state=std::move(snapshot);field_scene=std::move(candidate);field_status.clear();
    dialogue_choices.close();save_menu.close();storage_menu=StorageMenu{};field_equipment_menu=FieldEquipmentMenu{};
    world_effect.reset();world_effect_consumed=0;world_effect_actors=0;world_effect_npc=kRoomNoIndex;
    error.clear();return true;
}
bool enter_house_from_field(const upstream::WorldRoute& route,std::string& error){
    using namespace upstream;
    LoadingScope loading("Entering house",2);
    const auto room=opening_data.view();
    NativeSnapshotInput input;input.state=session_state;
    input.state.position_x=route.destination.x;input.state.position_y=route.destination.y;input.state.direction_x=route.direction.x;input.state.direction_y=route.direction.y;
    input.state.flags.clear();
    for(uint32_t i=0;i<room.flag_count();++i){const std::string id(room.string(room.flag(i).name_string));input.state.flags.push_back({id,field_scene->world.story_flag(id)});}
    input.stats=&session_rewards;input.inventory=&session_inventory;input.storage=&session_storage;
    SessionSnapshot snapshot;PreparedSessionRestore prepared;std::unique_ptr<GameplayScene> candidate;
    if(!loading.step([&]{return build_native_session_snapshot(native_session_data,room,house_data.view(),round_data.view(),items_data.view(),input,snapshot,error)&&
            prepare_session_restore(native_session_data,room,house_data.view(),round_data.view(),items_data.view(),house_font_data.view(),snapshot,prepared,error)&&
            prepare_fresh_house(prepared,restore_data,room,house_data.view(),house_font_data.view(),phone_data.view(),battle_random,{float(view_width),float(view_height)},candidate,error);},"house-scene"))return false;
    if(!candidate->house.bind_drawer(drawer_program_data.view(),drawer_effects)||!candidate->house.bind_inspections(house_inspection_data.view())){error=candidate->house.error();return false;}
    if(!bind_house_presents(*candidate,error))return false;
    bind_localized_house(candidate->presentation);
    if(!candidate->presentation.set_text_speed(prepared.state.settings.text_speed)){error="Prepared session text speed rejected";return false;}
    candidate->house.bind_choices(choice_data,dialogue_choices);candidate->presentation.set_text_value_callback(resolve_dialogue_value,&candidate->world);
    if(!bind_house_routes(candidate->house,error))return false;
    if(!candidate->finish_scene_ready()||!candidate->world.pause_for_house()){error="Returned House ready boundary rejected";return false;}
    auto ready_random=battle_random;
    if(!candidate->presents.scene_ready(ready_random,error))return false;
    release_field_graphics();
    if(!loading.step([&]{return ensure_house_graphics(error)&&admit_encounter_scene(error);},"house-atlases")){
        std::string restore;release_house_graphics();
        if(!field_actor.load(field_room_data.view(),restore)||!field_renderer.load(field_map_data.view(),std::string(locale_selection.code()),restore))field_status="Field graphics restore failed: "+restore;
        return false;}
    if(!loading.finish())return false;
    // Same owner commit as New Game / LOAD, without LOAD's source UID reallocation.
    battle_entry=BattleEntry{};battle_round=BattleRound{};battle_outcome=BattleOutcome{};round_presentation=BattleActionPresentation{};round_ready=false;round_error.clear();
    session_state=std::move(prepared.state);session_rewards=std::move(prepared.stats);session_inventory=std::move(prepared.inventory);session_storage=std::move(prepared.storage);session_state_ready=session_rewards_valid=true;
    battle_random=ready_random;candidate->world.attach_random(battle_random);candidate->presentation.rebind_random(battle_random);gameplay_scene.swap(candidate);field_scene.reset();
    dialogue_choices.close();save_menu.close();save_open_failed=storage_open_failed=false;storage_menu=StorageMenu{};field_equipment_menu=FieldEquipmentMenu{};field_equipment_status.clear();save_status.clear();items_status.clear();house_error.clear();house_sound_requests=0;
    if(!items_menu.initialize(session_inventory)){error=items_menu.error();return false;}
    world_effect.reset();world_effect_consumed=0;world_effect_actors=0;world_effect_npc=kRoomNoIndex;audio_player.reset_scene_requests();
    error.clear();return true;
}
// Called when SceneDoorTransition requests the swap; failure keeps the source scene.
void perform_scene_swap(){
    std::string error;const auto route=scene_door.route();
    const bool ok=in_field()?enter_house_from_field(route,error):enter_field(route,error);
    if(ok&&scene_door.swapped(error))return;
    scene_door.cancel();
    if(in_field()){field_status="Scene change rejected: "+error;field_scene->abort_transition(field_status);}
    else {house_error.clear();gameplay_scene->house.abort_scene_transition("Scene change rejected; B returns");field_status=error;}
}
void reset_field_state(){scene_door.cancel();if(in_field()){release_field_graphics();field_scene.reset();}field_status.clear();door_sound_requests=0;}
void draw_door_masks(float focus_x,float focus_y){
    if(!scene_door.covering())return;
    const auto masks=upstream::door_transition_masks(introduction_data,scene_door.kind(),scene_door.cut(),float(view_width),float(view_height),focus_x,focus_y);
    for(const auto& m:masks){const auto c=m.color;C2D_DrawRectSolid(view_x()+m.rect.x,view_y()+m.rect.y,0,m.rect.width,m.rect.height,C2D_Color32(c>>24,(c>>16)&255,(c>>8)&255,c&255));}
}
bool reset_development_game(std::string&error){
    reset_field_state();
    cancel_battle_prewarm();
    LoadingScope loading("Starting new game",5);
    if(!ensure_house_graphics(error))return false;
    wait_for_gpu_idle();restore_input_pending=false;session_rewards_valid=session_state_ready=false;session_inventory.initialize(items_data.view());session_storage.initialize(items_data.view(),native_session_data.storage_capacity());storage_menu=upstream::StorageMenu{};field_equipment_menu=upstream::FieldEquipmentMenu{};field_equipment_status.clear();storage_open_failed=false;items_menu.initialize(session_inventory);items_status.clear();
    battle_entry=upstream::BattleEntry{};battle_round=upstream::BattleRound{};battle_outcome=upstream::BattleOutcome{};round_presentation=upstream::BattleActionPresentation{};round_ready=false;round_error.clear();
    if(!loading.complete())return false;
    auto fresh=std::make_unique<GameplayScene>();if(!fresh->world.initialize(opening_data.view(),{float(view_width),float(view_height)})){error=fresh->world.error();return false;}gameplay_scene.swap(fresh);if(!loading.complete())return false;
    if(loaded_battle_path!=resource_path(ResourceRole::Battle).c_str()){if(!battle_data.load_file(resource_path(ResourceRole::Battle).c_str(),error)||!load_battle_presentation(error)||!round_data.load_file(resource_path(ResourceRole::Round).c_str(),error)||!round_renderer.load(round_data.view(),"romfs:/",error))return false;loaded_battle_path=resource_path(ResourceRole::Battle).c_str();}
    if(!loading.complete()||!loading.step([&]{return initialize_house_interactions(error);}))return false;
    if(!gameplay_scene->presents.scene_ready(battle_random,error))return false;
    region_music.shutdown();audio_player.reset_scene();return loading.complete()&&loading.finish();
}
bool begin_battle(std::string&error){
    const auto entry_started=svcGetSystemTick();const auto activity_started=load_activity_revision();
    using namespace upstream;
    const auto room=opening_data.view();
    const auto resource_index=gameplay_scene->world.battle_request().battle_resource_index;
    if(resource_index>=room.resource_count()||room.resource(resource_index).kind!=uint16_t(RoomResourceKind::CheckedBattlePack)){error="Battle request lacks checked resource";return false;}
    const auto resource=room.resource(resource_index);const std::string path="romfs:/"+std::string(room.string(resource.path_string));
    if((loaded_battle_path!=path||!battle_renderer.fully_resident())&&!battle_prewarm_ready(path)){note_battle_prewarm_miss(path);return true;}
    if(session_state_ready){const std::string id=gameplay_scene->world.battle_request().enemy;auto it=std::find_if(session_state.encountered.begin(),session_state.encountered.end(),[&](const auto&entry){return entry.id==id;});if(it==session_state.encountered.end())session_state.encountered.push_back({id,true});else it->value=true;}
    wait_for_gpu_idle();battle_entry=BattleEntry{};battle_round=BattleRound{};round_presentation=BattleActionPresentation{};battle_outcome=BattleOutcome{};round_ready=false;round_error.clear();items_status.clear();
    if(!items_menu.initialize(session_inventory)){error=items_menu.error();return false;}
    const auto round_path=companion_path(path);
    if(round_path.empty()){error="Battle resource has no checked companion binding";return false;}
    if((loaded_battle_path!=path||!battle_renderer.fully_resident())&&!commit_battle_prewarm(path,error))return false;
    if(round_data.view().binding().battle_id!=battle_data.view().metadata().room_battle_id){error="Battle companion identity mismatch";return false;}
    const auto content=battle_data.view();const auto native=content.parameter(BattleParameter::CanvasSize);
    const auto camera=gameplay_scene->world.cutscene_camera().center();const auto player=gameplay_scene->world.player().position;const auto&enemy=gameplay_scene->world.actor(gameplay_scene->world.battle_request().actor_index);
    // Source-coordinate snapshot is centered within the expanded display. The
    // display adapter restores the offset; world/physics positions never change.
    const Vec2 origin{camera.x-native.x*0.5f,camera.y-native.y*0.5f};
    BattleEntrySnapshot snapshot{{player.x-origin.x,player.y-origin.y},{enemy.position.x-origin.x,enemy.position.y-origin.y},gameplay_scene->world.cutscene_camera().is_shaking(),enemy.frame,0,enemy.sprite_offset};
    const auto nudge=content.parameter(BattleParameter::PartyNudge);if(std::abs(snapshot.player_screen.x-nudge.x)<nudge.y)snapshot.nudge_sign=(battle_random.randi()%2)==1?-1:1;
    if(session_rewards_valid){snapshot.party_hp=session_rewards.hp;snapshot.party_pp=session_rewards.pp;}
    if(!battle_entry.begin(content,room,gameplay_scene->world.battle_request(),snapshot)){error=battle_entry.error();return false;}
    if(!gameplay_scene->world.accept_battle_entry()){error="World refused battle ownership";return false;}
    report_battle_entry_commit(path,double(svcGetSystemTick()-entry_started)/CPU_TICKS_PER_MSEC,load_activity_revision()-activity_started);return true;
}
bool resolve_battle_name(void*,upstream::RoundView view,uint32_t index,std::string&value){
    if(!session_state_ready||session_state.characters.empty())return false;
    if(locale_selection.code()==locale_catalog.fallback())return new_game_data.battle_text(view,index,session_state.characters.front().nickname,value);
    return localized_text.battle(view,index,session_state.characters.front().nickname,value,locale_status);
}
bool begin_round(){
    using namespace upstream;
    const auto view=battle_data.view();const auto binding=round_data.view().binding();
    if(binding.battle_id!=view.metadata().room_battle_id){round_error="Original encounter entry ready; combat actions pending";return false;}
    if(!round_presentation.begin(round_data.view(),view,battle_random,session_rewards_valid?&session_rewards:nullptr)){round_error=round_presentation.error();return false;}
    if(!round_presentation.set_text_speed(session_state.settings.text_speed)){round_error="Battle session text speed rejected";return false;}
    round_presentation.set_text_resolver(resolve_battle_name,nullptr);
    bool party=false,enemy=false,plate=false;
    for(uint32_t i=0;i<view.count(BattleSection::Layouts);++i){
        const auto layout=view.layout(i);auto pose=battle_entry.pose(i);
        if(layout.flags&2){pose.rect.x-=pose.rect.z/2;pose.rect.y-=pose.rect.w/2;}
        if(layout.role==uint32_t(BattleRole::PartySprite)){party=round_presentation.set_actor_base(binding.player_participant,pose);}
        else if(layout.role==uint32_t(BattleRole::EnemySprite)){enemy=round_presentation.set_actor_base(binding.enemy_participant,pose);}
        else if(layout.role==uint32_t(BattleRole::PartyPlate)&&layout.kind==uint32_t(BattleDrawKind::NinePatch)){plate=round_presentation.set_plate_base(pose.rect);}
    }
    if(!party||!enemy||!plate){round_error="Missing action presentation bindings";return false;}
    round_host.bind(round_presentation,gameplay_scene->world,round_data.view());
    if(!battle_outcome.initialize(round_data.view(),opening_data.view(),session_rewards_valid?&session_rewards:nullptr)){round_error=battle_outcome.error();return false;}
    if(!battle_round.begin(round_data.view(),view,battle_random,round_host,session_rewards_valid?&session_rewards:nullptr)){round_error=battle_round.error();return false;}
    return round_ready=true;
}
void battle_top(){
    compose_ms=upload_ms=background_submit_ms=0;
    struct IoGuard {uint64_t before=load_activity_revision();~IoGuard(){const auto count=load_activity_revision()-before;if(count){char line[100];const int n=std::snprintf(line,sizeof(line),"ENCORE_BATTLE_RENDER_IO events=%llu\n",(unsigned long long)count);svcOutputDebugString(line,size_t(n));
#ifdef ENCORE_TEXT_QA
    if(qa_log){std::fprintf(qa_log,"%s",line);std::fflush(qa_log);}
#endif
    }}} io_guard;

    using namespace upstream;
    const auto content=battle_data.view();const auto native=content.parameter(BattleParameter::CanvasSize);
    const float dx=float(view_width)-native.x,dy=float(view_height)-native.y;
    const bool background_visible=!round_ready||round_presentation.battle_background_visible();
    // Do not rasterize an invisible backbuffer before either source draw
    // branch becomes active. No pixels from that old work reached the display.
    if(background_visible&&(battle_entry.mask_active()||battle_entry.scene_time()>content.parameter(BattleParameter::MaskDuration).x)){
    const auto compose_start=svcGetSystemTick();
    if(battle_entry.mask_active()){
        uint32_t frame=0;for(auto i:battle_draw_order)if(content.layout(i).role==uint32_t(BattleRole::TransitionMask)){frame=battle_entry.pose(i).frame;break;}
        if(!battle_renderer.compose_transition_background(shader_time,battle_color(content.parameter(BattleParameter::BackdropColor)),content.metadata().mask_resource,frame,battle_color(content.parameter(BattleParameter::MaskOldColor)),battle_color(content.parameter(BattleParameter::MaskColor)),int(dx*0.5f),int(dy*0.5f)))return;
    }else if(!battle_renderer.compose_background(shader_time,battle_color(content.parameter(BattleParameter::BackdropColor)),true))return;
    compose_ms=double(svcGetSystemTick()-compose_start)/CPU_TICKS_PER_MSEC;
    PROFILE_BACKGROUND_DONE();
    upload_battle_surface();const auto submit_start=svcGetSystemTick();
    battle_renderer.draw_surface(view_x(),view_y(),view_width,view_height,!battle_entry.mask_active());
    // CPU command submission, including the preceding C2D flush; this is not GPU completion time.
    background_submit_ms=double(svcGetSystemTick()-submit_start)/CPU_TICKS_PER_MSEC;
    }
    const auto return_overlays=round_ready?round_presentation.return_overlays():std::vector<BattleActionPose>{};
    const auto round_overlays=round_ready?round_presentation.overlays():std::vector<BattleActionPose>{};
    auto draw_action=[&](BattleActionPose p){p.rect.x+=view_x();p.rect.y+=view_y();if(!round_renderer.draw(p,battle_renderer,dx,dy))round_error="Action renderer rejected checked pose";};
    for(const auto& overlay:round_overlays)if(overlay.role==uint32_t(RoundMediaRole::BackgroundDim))draw_action(overlay);
    for(auto index:battle_draw_order){
        const auto layout=content.layout(index);const auto role=BattleRole(layout.role);if(role==BattleRole::TransitionMask)continue;
        auto pose=battle_entry.pose(index);
        if(round_ready){
            const auto binding=round_data.view().binding();
            if(!return_overlays.empty()){
                // Preserve source CanvasLayer order: curtains precede the
                // returning world sprite, which precedes the party info plate.
                if(role==BattleRole::TopBar||role==BattleRole::BottomBar){draw_action(return_overlays[role==BattleRole::TopBar?0:1]);continue;}
                if(role==BattleRole::PartyTransition){draw_action(round_presentation.return_party_pose());continue;}
            }
            if(role==BattleRole::PartySprite||role==BattleRole::EnemySprite){if(role==BattleRole::EnemySprite&&!round_presentation.enemies_visible())continue;draw_action(round_presentation.actor_pose(role==BattleRole::PartySprite?binding.player_participant:binding.enemy_participant));continue;}
            if((role==BattleRole::MenuIcon||role==BattleRole::MenuCursor||role==BattleRole::TargetBox||role==BattleRole::TargetText)&&(!round_presentation.commands_visible()||items_menu.active()))continue;
            if(role==BattleRole::PartyPlate||role==BattleRole::PartyName||role==BattleRole::PartyHP||role==BattleRole::PartyPP)pose.rect.y+=round_presentation.plate_offset()+round_presentation.return_plate_offset();
            if(role==BattleRole::PartyHP){const auto place=2-layout.binding;pose.frame=round_presentation.hp().digit_frame(place);pose.visible=round_presentation.hp().digit_visible(place);}
            if(role==BattleRole::PartyPP){const auto place=2-layout.binding;const uint32_t divisor=place==2?100:place==1?10:1;const auto digit=(uint32_t(round_presentation.current_pp())/divisor)%10;pose.frame=digit*content.resource(layout.resource).columns;pose.visible=place!=2||digit!=0;}
        }
        if(!pose.visible)continue;
        float x=pose.rect.x+dx*layout.display_anchor.x+view_x(),y=pose.rect.y+dy*layout.display_anchor.y+view_y();
        float w=pose.rect.z+((layout.flags&4)?dx:0),h=pose.rect.w;
        if(role==BattleRole::PartyTransition){const auto p=content.parameter(BattleParameter::PartyJumpStart);const float t=battle_entry.party_jump_time();if(t>=p.z)y+=dy*(1-layout.display_anchor.y)*std::clamp((t-p.z)/p.w,0.0f,1.0f);}
        if(layout.flags&2){x-=w*0.5f;y-=h*0.5f;}
        x=std::floor(x+0.5f);y=std::floor(y+0.5f);
        const auto color=battle_color(pose.color);
        if(layout.kind==uint32_t(BattleDrawKind::Rectangle)){battle_renderer.draw_rect(x,y,w,h,color);continue;}
        if(layout.kind==uint32_t(BattleDrawKind::Text)){
            const auto original_label=role==BattleRole::PartyName&&session_state_ready?std::string_view(session_state.characters.front().nickname):(role==BattleRole::TargetText?content.string(content.menu(battle_entry.selection()).label):content.string(layout.text));
            std::string translated_label(original_label);if(role==BattleRole::TargetText){std::string error;if(!locale_catalog.bound("battle.menu/"+std::to_string(battle_entry.selection()),original_label,locale_selection.code(),translated_label,error)){locale_status=error;continue;}}const std::string_view label(translated_label);
            const float width=battle_renderer.text_width(label.data());const auto metrics=content.parameter(BattleParameter::FontMetrics);
            battle_renderer.draw_text(label.data(),std::floor(x+(w-width)*0.5f+0.5f),std::floor(y+(h-metrics.z)*0.5f+0.5f),1,1,color);continue;
        }
        if(layout.kind==uint32_t(BattleDrawKind::Tiled)){battle_renderer.draw_tiled(layout.resource,x,y,w,h,color);continue;}
        if(layout.kind==uint32_t(BattleDrawKind::NinePatch)){
            const auto resource=content.resource(layout.resource);
            if(layout.margins[0]+layout.margins[2]>=resource.width||layout.margins[1]+layout.margins[3]>=resource.height)battle_renderer.draw_tiled(layout.resource,x,y,w,h,color);
            else battle_renderer.draw_ninepatch(layout.resource,x,y,w,h,layout.margins,color);
            continue;
        }
        if(pose.flash_modifier>0)battle_renderer.draw_sprite(layout.resource,pose.frame,x,y,w,h,battle_color(pose.flash_color),pose.flash_modifier);
        else if(role==BattleRole::EnemyTransition)battle_renderer.draw_sprite(layout.resource,pose.frame,x,y,w,h,C2D_Color32f(0,0,0,pose.color.w),1-pose.color.x);
        else battle_renderer.draw_sprite(layout.resource,pose.frame,x,y,w,h,color);
    }
    for(const auto& overlay:round_overlays)if(overlay.role!=uint32_t(RoundMediaRole::BackgroundDim))draw_action(overlay);
    if(!items_renderer.draw(items_menu,battle_renderer,float(view_width),float(view_height),view_x(),view_y()))round_error="Items renderer rejected checked layout";
}
void reference_borders(){
    if(!reference_view)return;
    const auto black=C2D_Color32(0,0,0,255);const float x=view_x(),y=view_y();
    C2D_DrawRectSolid(0,0,0,400,y,black);C2D_DrawRectSolid(0,240-y,0,400,y,black);
    C2D_DrawRectSolid(0,0,0,x,240,black);C2D_DrawRectSolid(400-x,0,0,x,240,black);
}

bool load_house(std::string& error){
    if(!opening_data.load_file(resource_path(ResourceRole::Room).c_str(),error)||!world_blackbars.load_file(resource_path(ResourceRole::Blackbars).c_str(),error))return false;
    const auto room=opening_data.view();
    if(!gameplay_scene->world.initialize(room,{float(view_width),float(view_height)})){error=gameplay_scene->world.error();return false;}
    room_draw_items.reserve(size_t(room.overlay_count())+room.actor_instance_count()+32);
    return true;
}
void free_house(){cancel_battle_prewarm();std::string ignored;field_equipment_menu=upstream::FieldEquipmentMenu{};field_equipment_renderer.set_details(nullptr);field_equipment_renderer.free();field_counter_font.reset_at_safe_boundary(ignored);field_counter_text.free();field_equipment_assets_ready=false;locale_font.reset_at_safe_boundary(ignored);house_prompt_renderer.free();loading_indicator.free();continue_renderer.free();choice_renderer.free();save_renderer.free();phone_renderer.free();storage_renderer.free();storage_counter_font.reset_at_safe_boundary(ignored);storage_counter_text.free();storage_assets_ready=false;items_renderer.set_details(nullptr);storage_renderer.set_details(nullptr);item_details_renderer.free();items_renderer.free();region_music.shutdown();audio_player.shutdown();house_renderer.free();present_renderer.free();round_renderer.free();battle_renderer.free();opening_actor.free();room_draw_items.clear();field_renderer.free();field_actor.free();field_scene.reset();}
void text(unsigned index,float x,float y,float scale,const std::string& value,u32 color=ink,float width=380){
    auto& slot=debug_text[index];
    if(!slot.ready||slot.value!=value){
        C2D_TextBufClear(slot.buffer);slot.value=value;
        slot.ready=C2D_TextParse(&slot.text,slot.buffer,value.c_str())!=nullptr;
        if(slot.ready)C2D_TextOptimize(&slot.text);
    }
    if(slot.ready)C2D_DrawText(&slot.text,C2D_WithColor|C2D_WordWrap,x,y,0,scale,scale,color,width);
}
void house_camera(float&cx,float&cy){
    const auto room=opening_data.view();cx=cy=0;
    if(gameplay_scene->world.cutscene_active()||gameplay_scene->world.has_cutscene_actors()||gameplay_scene->world.cutscene_camera().is_moving()||gameplay_scene->world.stage()==upstream::OpeningStage::BattleRequested){
        const auto center=gameplay_scene->world.cutscene_camera().center();
        cx=center.x-view_width*0.5f;cy=center.y-view_height*0.5f;
    }else{opening_camera(room,gameplay_scene->world.player().position,float(view_width),float(view_height),cx,cy);const auto shake=gameplay_scene->world.cutscene_camera().display_offset();cx+=shake.x;cy+=shake.y;}
    cx-=view_x();cy-=view_y();
}
void house_top(){
    const auto room=opening_data.view();const auto scene=room.scene();float cx=0,cy=0;house_camera(cx,cy);
    for(uint32_t i=0;i<room.map_draw_count();++i){
        const auto draw=room.map_draw(i);
        if(draw.x+draw.w<=cx||draw.x>=cx+view_width||draw.y+draw.h<=cy||draw.y>=cy+view_height)continue;
        opening_actor.draw_map(i,cx,cy);
    }
    for(uint32_t i=0;i<house_data.view().count(upstream::HouseSection::OpenableDoors);++i)if(!house_renderer.draw_door(gameplay_scene->presentation.openable_door_pose(i,gameplay_scene->house.openable_state(i).sprite_visible),cx,cy))house_error="Openable door renderer rejected checked pose";
    // Source Y-sort origin is independent of the texture drawing offset.
    // Reserve once at load; the frame loop decodes records without heap graphs.
    room_draw_items.clear();uint32_t order=0;
    for(uint32_t i=0;i<room.overlay_count();++i)if(!(room.overlay(i).flags&uint16_t(upstream::RoomOverlayFlag::Foreground)))room_draw_items.push_back({room.overlay(i).sort_y,0,i,order++});
    for(uint32_t i=0;i<phone_data.view().count(upstream::PhoneSection::Objects);++i)room_draw_items.push_back({gameplay_scene->phone.pose(i).sort_origin.y,3,i,order++});
    if(present_renderer.loaded())for(uint32_t i=0;i<gameplay_scene->presents.count();++i)room_draw_items.push_back({gameplay_scene->presents.pose(i).position.y,4,i,order++});
    for(uint32_t i=0;i<room.actor_instance_count();++i){
        if(!gameplay_scene->world.instance_visible(i)||(world_effect.active()&&(world_effect_actors&(uint64_t(1)<<i))))continue;
        bool placed_npc=false;for(uint32_t j=0;j<house_data.view().count(upstream::HouseSection::Npcs);++j)placed_npc|=house_data.view().npc(j).room_actor_index==i;
        if(placed_npc&&!gameplay_scene->world.actor_bound(i))continue;
        if(in_battle()&&((i==scene.player_instance_index&&!gameplay_scene->world.battle_player_visible())||i==gameplay_scene->world.battle_request().actor_index))continue;
        const auto position=i==scene.player_instance_index&&!gameplay_scene->world.actor_bound(i)?
            gameplay_scene->world.player().position:gameplay_scene->world.actor(i).position;
        room_draw_items.push_back({position.y,1,i,order++});
    }
    for(uint32_t i=0;i<house_data.view().count(upstream::HouseSection::Npcs);++i){if(world_effect.active()&&i==world_effect_npc)continue;const auto binding=house_data.view().npc(i).room_actor_index;if(binding!=upstream::house_no_index&&gameplay_scene->world.actor_bound(binding))continue;const auto npc=gameplay_scene->presentation.npc_pose(i);if(npc.visible)room_draw_items.push_back({npc.position.y,2,i,order++});}
    std::sort(room_draw_items.begin(),room_draw_items.end(),[](const RoomDrawItem& a,const RoomDrawItem& b){
        return a.y<b.y||(a.y==b.y&&a.order<b.order);
    });
    for(const auto& draw:room_draw_items){
        if(draw.kind==0)opening_actor.draw_overlay(draw.index,cx,cy);
        else if(draw.kind==3){if(!phone_renderer.draw(gameplay_scene->phone.pose(draw.index),cx,cy))house_error="Phone renderer rejected checked pose";}
        else if(draw.kind==4){if(!present_renderer.draw(gameplay_scene->presents.pose(draw.index),cx,cy))house_error="Present renderer rejected checked pose";}
        else if(draw.kind==2){if(!house_renderer.draw_npc(gameplay_scene->presentation.npc_pose(draw.index),cx,cy))house_error="NPC renderer rejected checked pose";}
        else if(draw.index==scene.player_instance_index&&!gameplay_scene->world.actor_bound(draw.index))
            opening_actor.draw_player(gameplay_scene->world.player().position,gameplay_scene->world.animation(),cx,cy);
        else opening_actor.draw_actor(gameplay_scene->world.actor(draw.index),cx,cy);
    }
    // Source Above is a later sibling of Objects, so it masks every sorted
    // object and actor. Its original atlas alpha defines the wall corners.
    for(uint32_t i=0;i<room.overlay_count();++i){
        const auto draw=room.overlay(i);
        if(!(draw.flags&uint16_t(upstream::RoomOverlayFlag::Foreground)))continue;
        if(draw.x+draw.w<=cx||draw.x>=cx+view_width||draw.y+draw.h<=cy||draw.y>=cy+view_height)continue;
        opening_actor.draw_overlay(i,cx,cy);
    }
    if(world_effect.active()){
        if(!world_effect_renderer.compose(float(shader_time),world_effect.sample(),house_error))return;
        world_effect_renderer.draw(world_effect_center.x-float(view_width)*.5f-cx,world_effect_center.y-float(view_height)*.5f-cy);
        room_draw_items.clear();order=0;
        for(uint32_t i=0;i<room.actor_instance_count();++i)if((world_effect_actors&(uint64_t(1)<<i))&&gameplay_scene->world.instance_visible(i))room_draw_items.push_back({gameplay_scene->world.actor(i).position.y,1,i,order++});
        if(world_effect_npc<house_data.view().count(upstream::HouseSection::Npcs)){const auto npc=gameplay_scene->presentation.npc_pose(world_effect_npc);if(npc.visible)room_draw_items.push_back({npc.position.y,2,world_effect_npc,order++});}
        std::stable_sort(room_draw_items.begin(),room_draw_items.end(),[](const RoomDrawItem&a,const RoomDrawItem&b){return a.y<b.y;});
        for(const auto&draw:room_draw_items){if(draw.kind==2){if(!house_renderer.draw_npc(gameplay_scene->presentation.npc_pose(draw.index),cx,cy))house_error="Lifted NPC renderer rejected";}else opening_actor.draw_actor(gameplay_scene->world.actor(draw.index),cx,cy);}
    }
    // Source layer-1 bars cover the complete world canvas (including effects),
    // before layer-3 dialogue. Width/bottom anchoring adapt to the native view.
    const auto bars=world_blackbars.pose(float(view_width),float(view_height));
    for(const auto&bar:bars.bars)if(bar.h>0)BattleRenderer::draw_rect(view_x()+bar.x,view_y()+bar.y,bar.w,bar.h,bars.color);

}
void house_bottom(){
    const auto room=opening_data.view();const auto scene=room.scene();
    text(0,14,9,0.48f,"ROOM RUNTIME",accent,292);
    text(1,14,39,0.4f,std::string(room.string(scene.version_string))+"\n"+std::string(room.string(scene.display_name_string)),ink,292);
    const auto stage=gameplay_scene->world.stage();
    const auto battle_phase=battle_entry.phase();
    const char* scope=stage==upstream::OpeningStage::Walking?"Native movement + room Y-sort":
        stage==upstream::OpeningStage::ScriptRunning?"Scripted sequence\nSource actions and audio requests":
        stage==upstream::OpeningStage::BattleRequested?"Battle handoff pending":gameplay_scene->world.error();
    if(in_battle())scope=battle_phase==upstream::BattleEntryPhase::Commands?"Battle menu: original entry ready\nAction execution pending":battle_phase==upstream::BattleEntryPhase::ActionRequested?battle_entry.error():battle_phase==upstream::BattleEntryPhase::Error?battle_entry.error():"Original battle entry sequence";
    if(round_ready){
        switch(battle_round.phase()){
        case upstream::BattleRoundPhase::Commands:scope="Original battle command menu";break;
        case upstream::BattleRoundPhase::Targeting:scope="Choose original target: A confirm / B back";break;
        case upstream::BattleRoundPhase::Running:scope="Original battle action sequence";break;
        case upstream::BattleRoundPhase::VictoryPending:scope="Enemy defeated; rewards / world return pending";break;
        case upstream::BattleRoundPhase::DefeatPending:scope="Party defeated; result handling pending";break;
        case upstream::BattleRoundPhase::Unsupported:case upstream::BattleRoundPhase::Error:scope=battle_round.error();break;
        default:break;
        }
    }
    switch(battle_outcome.phase()){
    case upstream::BattleOutcomePhase::VictoryBanner:scope="Original victory sequence";break;
    case upstream::BattleOutcomePhase::ExperienceDialogue:scope="Experience: A / B after text to continue";break;
    case upstream::BattleOutcomePhase::LevelDialogue:scope="Original level growth: A / B after text";break;
    case upstream::BattleOutcomePhase::PostWinRequested:scope="Battle complete; original post-win scene running";break;
    case upstream::BattleOutcomePhase::Returning:scope="Returning to the original room";break;
    case upstream::BattleOutcomePhase::Complete:scope="Victory rewards applied; room movement restored";break;
    case upstream::BattleOutcomePhase::Error:scope=battle_outcome.error();break;
    default:break;
    }
    if(gameplay_scene->house.phase()==upstream::HousePhase::Dialogue)scope="Original conversation: A / B advances after text";
    else if(gameplay_scene->house.entering_door()||gameplay_scene->house.phase()==upstream::HousePhase::DoorAwaitIdle)scope="Original same-scene door transition";
    else if(gameplay_scene->house.phase()==upstream::HousePhase::Unsupported||gameplay_scene->house.phase()==upstream::HousePhase::Error)scope=gameplay_scene->house.error();
    std::string story_scope;
    if(gameplay_scene->house.story_pending()){story_scope=(gameplay_scene->house.story_executing()?"Original story running: ":"Original story requested: ")+std::string(gameplay_scene->house.story_path());if(!gameplay_scene->house.story_executing())story_scope+="\nScript/battle pending; B returns after fade";scope=story_scope.c_str();}
    if(continue_menu.is_open()&&!continue_status.empty())scope=continue_status.c_str();
    if(introduction.active())scope="Original Introduction / Mt. Itoi\nSTART: source skip while animation plays\nHouse movement unlocks at door fade boundary";
    else if(new_game_setup.active())scope=new_game_setup.phase()==upstream::NamingPhase::Editing?"Names / food: A choose, B erase/back\nX case, Y commands, L/R previous/next\nOriginal Introduction follows final Yes":"Original settings: A select, B back\nFinal Yes starts Introduction; No/B restarts names\nOriginal Introduction follows final Yes";
    if(save_open_failed)scope=save_status.c_str();
    if(save_menu.is_open())scope=save_status.empty()?"Record: choose a slot; B closes":save_status.c_str();
    if(!house_error.empty())scope=house_error.c_str();
    if(!round_error.empty())scope=round_error.c_str();
    if(field_equipment_menu.visible())scope="Original Pause / Equip; START menu, A choose, B back";
    if(!field_equipment_status.empty())scope=field_equipment_status.c_str();
    if(items_menu.active())scope=items_status.empty()?"Inventory: A select, B back, L info":items_status.c_str();
    if(!gameplay_scene->world.healthy())scope=gameplay_scene->world.error();
    text(2,14,84,0.35f,scope,muted,292);
#ifdef ENCORE_FRAME_PROFILE
    if(in_battle()){
        char timing[240];const auto& profile=frame_profile.snapshot();const auto& m=profile.mean_ms;
        std::snprintf(timing,sizeof(timing),"Last %u-frame %ux%u mean ms: %.2f\ncore %.2f  begin/sync %.2f\nroom %.2f  battle %.2f\nbottom %.2f  end %.2f  QA %.2f",unsigned(profile.frames),unsigned(profile.context[1]),unsigned(profile.context[2]),m[7],m[0],m[1],m[2],m[3],m[4],m[5],m[6]);
        text(3,14,128,0.29f,timing,ink,292);
    }else
#endif
    text(3,14,143,0.37f,"D-pad: move / choose    A: select\nSELECT: reference view + restart\nExit: title menu",ink,292);
    char position[240];const auto& player=gameplay_scene->world.player();
    if(stage==upstream::OpeningStage::Walking){
        const auto instance=room.actor_instance(scene.player_instance_index);
        std::snprintf(position,sizeof(position),"%s: %.0f, %.0f  frame %u",room.string_data(instance.display_name_string),double(player.position.x),double(player.position.y),unsigned(gameplay_scene->world.animation().frame));
    }else std::snprintf(position,sizeof(position),"Phrase %u    audio cues %u",unsigned(gameplay_scene->world.phrase()),unsigned(gameplay_scene->world.audio_request_count()));
    if(in_battle())std::snprintf(position,sizeof(position),"CPU background %.1f ms  upload %.1f ms",compose_ms,upload_ms);
    if(round_ready){const auto b=round_data.view().binding();std::snprintf(position,sizeof(position),"Round %u  HP %ld  enemy %ld  RNG %llu",unsigned(battle_round.number()),long(battle_round.battler(b.player_participant).target_hp),long(battle_round.battler(b.enemy_participant).target_hp),(unsigned long long)battle_random.raw_draw_count());}
    if(in_battle()){
        const char* backend="CPU";
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        if(battle_renderer.gpu_background_active())backend="GPU spans";
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        if(battle_renderer.gpu_texture_active())backend=battle_renderer.gpu_mapped_texture_active()?(battle_renderer.gpu_mapped_stats().region_pixels?"GPU index runs":"GPU mapped textures"):"GPU texture strips";
#endif
#endif
        const auto used=std::strlen(position);
        std::snprintf(position+used,sizeof(position)-used,"\nBackground: %s",backend);
#if defined(ENCORE_EXPERIMENTAL_GPU_BACKGROUND) && defined(ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS)
        if(battle_renderer.gpu_mapped_texture_active()){
            const auto end=std::strlen(position);
            std::snprintf(position+end,sizeof(position)-end," %u quads / %u pass\nCPU prep %.1fms submit %.1fms / samples %u",unsigned(battle_renderer.gpu_texture_strips()),battle_renderer.gpu_texture_passes(),compose_ms,background_submit_ms,unsigned(battle_renderer.gpu_region_samples()));
        }
#endif
    }
    // Only changing numeric diagnostics are sampled at10Hz. Error, scope and
    // audio status are checked every frame and update their own cache at once.
    static std::string telemetry;static u64 telemetry_time=0;static bool telemetry_battle=false;
    const auto now=osGetTime();
    if(battle_outcome.phase()==upstream::BattleOutcomePhase::Complete){const auto state=battle_outcome.state();std::snprintf(position,sizeof(position),"HP %ld EXP %u Bank %u Cash %u\nPosition %.0f, %.0f",long(state.hp),unsigned(state.experience),unsigned(state.bank),unsigned(state.cash),double(player.position.x),double(player.position.y));}
    if(telemetry.empty()||now-telemetry_time>=100||telemetry_battle!=in_battle()){
        telemetry=position;telemetry_time=now;telemetry_battle=in_battle();
    }
    if(continue_menu.is_open()&&continue_menu.phase()==upstream::ContinuePhase::Title){
        const auto index=locale_catalog.locale_index(locale_selection.code());const auto&info=locale_catalog.locales()[size_t(index)];
        const auto label=std::string("L/R: ")+std::string(locale_selection.text("OPTIONS_LANGUAGE").text)+": "+std::string(info.name);
        if(!battle_renderer.draw_text(label.c_str(),14,190,1,1,ink))locale_status=locale_font.last_error();
        if(!locale_status.empty())text(4,14,174,0.30f,locale_status,muted,292);
    }else text(4,14,in_battle()?180:192,0.30f,gameplay_scene->world.healthy()?telemetry:gameplay_scene->world.error(),muted,292);
    text(5,14,212,0.27f,audio_status.empty()?"Audio backend initialized (output unverified)":audio_status,muted,292);
}

void field_top(){
    const auto player=field_scene->world.player().position;
    const auto origin=field_scene->camera_origin(player,{float(view_width),float(view_height)});
    if(!field_renderer.draw(*field_scene,field_actor,origin.x,origin.y,float(view_width),float(view_height),view_x(),view_y(),shader_time))field_status="Field renderer rejected checked map binding";
}
void field_bottom(){
    using namespace upstream;
    const auto map=field_scene->map();const auto room=field_scene->world.content();
    text(0,14,9,0.48f,"FIELD RUNTIME",accent,292);
    text(1,14,39,0.4f,std::string(room.string(room.scene().version_string))+"\n"+std::string(room.string(room.scene().display_name_string))+": original TileMap layers and collision",ink,292);
    std::string scope;
    switch(field_scene->phase()){
    case FieldPhase::Walking:scope="Native movement + TileMap collision\nNPCs, enemies, music and object interactions are not ported";break;
    case FieldPhase::DoorAwaitIdle:case FieldPhase::Transition:scope="Original door transition";break;
    default:scope=field_scene->error();break;
    }
    if(!field_status.empty())scope=field_status;
    const auto player=field_scene->world.player().position;
    const auto nearby=field_scene->nearby_notices(player,200.f,3);
    if(!nearby.empty()){scope+="\nNearby, not ported:";for(const auto&n:nearby){scope+=" ";scope+=std::string(map.text(map.notice(n.index).label));scope+=";";}}
    text(2,14,84,0.35f,scope,muted,292);
    text(3,14,150,0.37f,"D-pad: move   B at a stop: return\nSTART menus are not ported in this field\nSELECT: reference view + restart",ink,292);
    char position[160];const auto instance=room.actor_instance(room.scene().player_instance_index);
    std::snprintf(position,sizeof(position),"%s: %.0f, %.0f  door sound requests %llu",room.string_data(instance.display_name_string),double(player.x),double(player.y),(unsigned long long)door_sound_requests);
    text(4,14,192,0.30f,position,muted,292);
    text(5,14,212,0.27f,audio_status.empty()?"Audio backend initialized (output unverified)":audio_status,muted,292);
}
void error_console(const std::string& error){
    u16 width=0,height=0;
    auto* framebuffer=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,&width,&height);
    if(framebuffer)std::memset(framebuffer,0,size_t(width)*height*3);
    consoleInit(GFX_BOTTOM,nullptr);std::printf("ENCORE NATIVE\n\n%s\n\nSTART to exit.\n",error.c_str());
    while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();}
}
}
int main(int argc,char** argv){
    (void)argc;(void)argv;load_epoch_tick=svcGetSystemTick();gfxInitDefault();gfxSet3D(false);
    const Result romfs_result=romfsInit();
    if(R_FAILED(romfs_result)){char b[100];std::snprintf(b,sizeof(b),"RomFS init failed: 0x%08lx",(unsigned long)romfs_result);error_console(b);gfxExit();return 1;}
#ifdef ENCORE_TEXT_QA
    qa_log=std::fopen("sdmc:/encore-native-qa.log","w");
    if(qa_log){std::fprintf(qa_log,"ENCORE_TEXT_QA v1 no image capture; cold startup\n");std::fflush(qa_log);}
#endif
    std::string error;
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)){error_console("C3D_Init failed");romfsExit();gfxExit();return 1;}
    if(!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)){C3D_Fini();error_console("C2D_Init failed");romfsExit();gfxExit();return 1;}
    C2D_Prepare();
    auto* top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);auto* bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
    const bool debug_ready=create_debug_text();
    if(!top||!bottom||!debug_ready){
        free_debug_text();
        C2D_Fini();C3D_Fini();error_console("GPU target/text buffer allocation failed");romfsExit();gfxExit();return 1;
    }
    upstream::NativeInputData native_input_data;
    upstream::NativeInputAdapter native_input;
    loading_top=top;
    if(!resource_catalog.load_file("romfs:/data/native.encresources",error)||!resource_catalog.verify_files("romfs:/",error)){free_house();free_debug_text();C2D_Fini();C3D_Fini();error_console(error);romfsExit();gfxExit();return 1;}
    if(!loading_indicator.load("romfs:/",resource_catalog.path(ResourceRole::LoadingIndicator).c_str(),error)){free_house();free_debug_text();C2D_Fini();C3D_Fini();error_console(error);romfsExit();gfxExit();return 1;}
    { LoadingScope loading("Starting game",23);
    // Keep checked metadata for saves/reference view, but admit only UI glyph
    // textures. Full encounter preparation belongs to house scene admission.
    if(!loading.step([&]{return load_house(error);},"room-metadata")||
       !loading.step([&]{return house_font_data.load_file(resource_path(ResourceRole::Battle).c_str(),error);},"font-metadata")||
       !loading.step([&]{return battle_data.load_file(resource_path(ResourceRole::Battle).c_str(),error);},"battle-metadata")||
       !loading.step([&]{return load_ui_font_only(error);},"ui-font-textures")||
       !loading.step([&]{return round_data.load_file(resource_path(ResourceRole::Round).c_str(),error);},"round-metadata")||
       !loading.step([&]{return house_data.load_file(resource_path(ResourceRole::House).c_str(),error);},"house-metadata")||
       !loading.step([&]{return items_data.load_file(resource_path(ResourceRole::Items).c_str(),error);},"item-metadata")||
       !loading.step([&]{return item_details_data.load_file(resource_path(ResourceRole::ItemDetails).c_str(),error)&&item_details_data.view().bind_items(items_data.view(),error);},"item-details-metadata")||
       !loading.step([&]{return field_equipment_data.load_file(resource_path(ResourceRole::FieldEquipment).c_str(),error)&&field_equipment_data.view().bind_items(items_data.view(),error);},"field-equipment-metadata")||
       !loading.step([&]{return initialize_house_interactions(error);},"menus-localization-session")||
       !loading.step([&]{return session_inventory.initialize(items_data.view());},"inventory")||
       !loading.step([&]{return items_menu.initialize(session_inventory);},"item-menu")||
       !loading.step([&]{return storage_data.load_file(resource_path(ResourceRole::Storage).c_str(),error)&&storage_data.view().bind_items(items_data.view(),error);},"storage-metadata")||
       !loading.step([&]{return session_storage.initialize(items_data.view(),native_session_data.storage_capacity());},"storage-state")){
        free_house();free_debug_text();C2D_Fini();C3D_Fini();
        error_console(error);romfsExit();gfxExit();return 1;
    }
    if(!loading.step([&]{return native_input_data.load_file(resource_path(ResourceRole::Input).c_str(),error);},"input-metadata")||!loading.step([&]{return native_input.configure(native_input_data,error);},"input-adapter")){
        free_house();free_debug_text();C2D_Fini();C3D_Fini();error_console(error);romfsExit();gfxExit();return 1;
    }
    if(!loading.step([&]{initialize_audio();return true;},"audio-stream-backend")){error="Startup loading plan rejected";free_house();free_debug_text();C2D_Fini();C3D_Fini();error_console(error);romfsExit();gfxExit();return 1;}

    // One shared stream for mechanics and action presentation. The full original
    // startup/entry random stream is not yet ported; seeded oracle runs explicitly
    // establish their menu boundary instead of claiming whole-game continuation.
#ifdef ENCORE_REFERENCE_SEED
    battle_random.seed(ENCORE_REFERENCE_SEED);
#else
    battle_random.seed(svcGetSystemTick());
#endif
    if(!open_continue(error,&loading)||!loading.step([&]{return prepare_title_audio(error);},"title-audio")||!loading.finish()){free_house();free_debug_text();C2D_Fini();C3D_Fini();error_console(error);romfsExit();gfxExit();return 1;}
    }
    {char line[120];const int n=std::snprintf(line,sizeof(line),"ENCORE_STARTUP_READY elapsed_ms=%.3f\n",double(svcGetSystemTick()-load_epoch_tick)/CPU_TICKS_PER_MSEC);if(n>0)svcOutputDebugString(line,size_t(n));}
    upstream::MenuNavigationRepeat menu_navigation;
    std::atomic<bool> input_resume_reset{false};aptHookCookie input_hook{};
    aptHook(&input_hook,[](APT_HookType kind,void* state){
        if(kind==APTHOOK_ONSUSPEND||kind==APTHOOK_ONRESTORE||kind==APTHOOK_ONSLEEP||kind==APTHOOK_ONWAKEUP)
            static_cast<std::atomic<bool>*>(state)->store(true,std::memory_order_relaxed);
    },&input_resume_reset);
    // Content IDs need not fit bit fields; compare the complete owner tuple
    // and issue a session token rather than hashing/packing arbitrary IDs.
    using InputOwner=std::array<uint32_t,13>;
    InputOwner prior_input_owner{};bool have_input_owner=false;uint32_t input_owner_token=0;
    const auto input_context=[&]()->uint32_t{
        if(!gameplay_scene->world.healthy()||!house_error.empty()||!round_error.empty())return 0;
        const bool battle=in_battle();
        const InputOwner owner{{uint32_t(battle),uint32_t(gameplay_scene->world.stage()),uint32_t(gameplay_scene->house.phase())+(in_field()?64u+uint32_t(field_scene->phase()):0u),battle?uint32_t(battle_entry.phase()):0u,battle&&round_ready?uint32_t(battle_round.phase())+1u:0u,gameplay_scene->world.pending_dialogue_id(),uint32_t(dialogue_choices.phase()),uint32_t(save_menu.phase())+(save_open_failed?100u:0u),uint32_t(continue_menu.phase()),uint32_t(new_game_setup.phase())*32u+new_game_setup.field_index(),uint32_t(introduction.phase()),uint32_t(storage_menu.phase())+(storage_open_failed?100u:0u),uint32_t(field_equipment_menu.phase())}};
        if(!have_input_owner||owner!=prior_input_owner){prior_input_owner=owner;have_input_owner=true;if(++input_owner_token==0)++input_owner_token;}
        return input_owner_token;
    };
    u64 last=osGetTime();uint64_t accumulator=0;
    while(aptMainLoop()){
        PROFILE_START();
        const uint64_t frame_load_generation=loading_generation;
        if(input_resume_reset.exchange(false,std::memory_order_relaxed)){
            native_input.reset();menu_navigation.reset();last=osGetTime();accumulator=0;
        }
        hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();
        // START is not a global quit shortcut: UI editing must not discard the
        // pending session. Normal exit is the explicit title-menu request.
        if((down&KEY_SELECT)&&!introduction.active()){
            wait_for_gpu_idle();new_game_renderer.free();new_game_setup.close();pending_new_game=PendingNewGame{};native_input.reset();menu_navigation.reset();reference_view=!reference_view;const auto native=battle_data.view().parameter(upstream::BattleParameter::CanvasSize);view_width=reference_view?unsigned(native.x):400;view_height=reference_view?unsigned(native.y):240;loaded_battle_path.clear();world_effect_assets_ready=false;
            if(!reset_development_game(error)||!open_continue(error))break;last=osGetTime();accumulator=0;
        }
        u64 now=osGetTime();uint64_t elapsed=now-last;last=now;
        // Bound catch-up below; an expensive rendered frame must still advance.
        // Treating every frame over250ms as suspension froze new shader paths.
        // Input is sampled once per rendered/HID frame, never replayed by the
        // physics catch-up loop. Changing input ownership quarantines a held
        // touch or stick until release, so transitions cannot inherit gestures.
        const bool field_was_open=field_equipment_menu.visible();
        const auto field_view=field_equipment_data.view();
        const bool may_open_field=!field_was_open&&!new_game_setup.active()&&!introduction.active()&&!continue_menu.is_open()&&!restore_input_pending&&!in_battle()&&!battle_handoff_pending()&&!storage_menu.active()&&!storage_open_failed&&!save_menu.is_open()&&!save_open_failed&&!dialogue_choices.active()&&house_error.empty()&&round_error.empty()&&gameplay_scene->world.healthy()&&gameplay_scene->world.stage()==upstream::OpeningStage::Walking&&!gameplay_scene->world.house_paused()&&!gameplay_scene->world.cutscene_active()&&!gameplay_scene->house.blocks_player()&&!gameplay_scene->house.entering_door()&&!in_field()&&!scene_door.active();
        if(may_open_field&&(down&uint32_t(field_view.parameter(upstream::FieldParameter::OpenMask)))){
            wait_for_gpu_idle();
            if(!gameplay_scene->world.pause_for_house())field_equipment_status="Field menu pause rejected";
            else if(!open_field_equipment(error)){field_equipment_status=error;field_equipment_menu=upstream::FieldEquipmentMenu{};gameplay_scene->world.unpause_from_house();}
        }
        const uint32_t sampled_context=input_context();
        circlePosition circle{};hidCircleRead(&circle);
        touchPosition touch{};hidTouchRead(&touch);
        const auto native_controls=native_input.sample(sampled_context,circle.dx,circle.dy,
            {(held&KEY_DLEFT)!=0,(held&KEY_DRIGHT)!=0,(held&KEY_DUP)!=0,(held&KEY_DDOWN)!=0},
            (held&KEY_TOUCH)!=0,touch.px,touch.py,double(elapsed)/1000.0);
        const double real_dt=double(std::min<uint64_t>(elapsed,100))/1000.0;
        const double time_scale=round_ready?round_presentation.advance_real_time(real_dt):1.0;
        const double dt=double(float(real_dt*time_scale));
        accumulator+=std::min<uint64_t>(elapsed,100)*60;
        const bool intro_active_at_frame_start=introduction.active();
        const bool world_visible=!new_game_setup.active()&&(!introduction.active()||introduction_house_committed)&&(!continue_menu.is_open()||continue_menu.pose().world_visible);
        const bool world_input=world_visible&&!field_equipment_menu.visible()&&!restore_input_pending&&(!introduction.active()||introduction.house_unpaused());
        while(accumulator>=1000){
            if(!world_visible){accumulator-=1000;continue;}
            upstream::WalkInput input;
            input.x=int8_t(input_context()==sampled_context?native_controls.direction.x:0);
            input.y=int8_t(input_context()==sampled_context?native_controls.direction.y:0);
            input.toggle=(held&KEY_B)!=0;input.paused=!world_input;
            if(in_field()){
                if(field_scene->phase()!=upstream::FieldPhase::Error){
                    bool ok=field_scene->before_physics(input);
                    if(ok&&field_scene->world.healthy())field_scene->world.advance(input);
                    ok=ok&&field_scene->after_physics();
                    if(!ok)field_status=field_scene->error();
                }
                accumulator-=1000;continue;
            }
            if(house_error.empty()&&!gameplay_scene->house.before_physics(input))house_error=gameplay_scene->house.error();
            if(house_error.empty()&&!gameplay_scene->presentation.physics_frame(double(float(time_scale/60.0)),gameplay_scene->world.player().position))house_error=gameplay_scene->presentation.error();
            if(gameplay_scene->world.healthy()&&house_error.empty())gameplay_scene->world.advance(input);
            if(house_error.empty()&&!gameplay_scene->house.after_physics())house_error=gameplay_scene->house.error();
            if(round_ready&&in_battle()&&!round_presentation.physics_frame(double(float(time_scale/60.0))))round_error=round_presentation.error();
            accumulator-=1000;
        }
        gameplay_scene->world.attach_random(battle_random);
        shader_time+=dt;if((world_input||field_equipment_menu.visible())&&session_state_ready)session_state.playtime_seconds+=dt;
        uint32_t requested_route=upstream::WorldLinksData::kNotFound;
        if(in_field()){
            if(world_visible&&field_scene->world.healthy())field_scene->world.idle_frame(dt);
            if(world_visible&&field_scene->phase()!=upstream::FieldPhase::Error&&!field_scene->idle_frame(dt,world_input&&(down&KEY_B)!=0))field_status=field_scene->error();
            if(field_scene->phase()==upstream::FieldPhase::Walking&&!field_status.empty()&&field_status.rfind("Scene change rejected",0)==0)field_status.clear();
            door_sound_requests+=field_scene->take_sounds().size();
            requested_route=field_scene->take_route();
        }else{
            if(world_visible&&gameplay_scene->world.healthy())gameplay_scene->world.idle_frame(dt);
            if(world_visible&&house_error.empty()&&!gameplay_scene->presentation.idle_frame(dt))house_error=gameplay_scene->presentation.error();
            if(world_visible&&house_error.empty()&&!gameplay_scene->house.idle_frame(dt,world_input&&((down&KEY_A)!=0||(input_context()==sampled_context&&native_controls.confirm_pulse)),world_input&&(down&KEY_B)!=0))house_error=gameplay_scene->house.error();
            if(world_visible&&house_error.empty()&&!advance_world_effect(dt,house_error)){}
            house_sound_requests+=gameplay_scene->house.take_sounds().size()+gameplay_scene->presentation.take_audio_events().size();
            process_present_audio();
            requested_route=gameplay_scene->house.take_scene_route();
        }
        if(requested_route!=upstream::WorldLinksData::kNotFound){
            if(!begin_scene_door(requested_route,error)){
                field_status="Scene door rejected: "+error;
                if(in_field())field_scene->abort_transition(field_status);else gameplay_scene->house.abort_scene_transition("Scene door rejected; B returns");
            }
        }
        if(scene_door.active()){
            process_scene_door_events();
            if(!scene_door.idle_frame(dt,error)){field_status=error;scene_door.cancel();}
            else if(scene_door.phase()==upstream::SceneDoorPhase::SwapRequested){wait_for_gpu_idle();perform_scene_swap();last=osGetTime();accumulator=0;native_input.reset();menu_navigation.reset();}
            process_scene_door_events();
        }else if(scene_door.phase()==upstream::SceneDoorPhase::Done)scene_door.cancel();
        if(battle_handoff_pending()&&!battle_outcome.advance_post_win())round_error=battle_outcome.error();
        record_entry_request();
        update_battle_prewarm(!in_field()&&!scene_door.active()&&world_visible&&!continue_menu.is_open()&&!new_game_setup.active()&&!restore_input_pending,elapsed);
        if(!in_field()&&gameplay_scene->world.battle_request().requested&&!in_battle()&&!battle_handoff_pending()){if(!begin_battle(error))audio_status=error;}
        else if(in_battle())battle_entry.idle_frame(dt,gameplay_scene->world.cutscene_camera().is_shaking());
        const int physical_x=((held&(KEY_DRIGHT|KEY_CPAD_RIGHT))?1:0)-((held&(KEY_DLEFT|KEY_CPAD_LEFT))?1:0);
        const int physical_y=((held&(KEY_DDOWN|KEY_CPAD_DOWN))?1:0)-((held&(KEY_DUP|KEY_CPAD_UP))?1:0);
        const bool physical_direction=(held&(KEY_DLEFT|KEY_DRIGHT|KEY_DUP|KEY_DDOWN|KEY_CPAD_LEFT|KEY_CPAD_RIGHT|KEY_CPAD_UP|KEY_CPAD_DOWN))!=0;
        const bool same_input_owner=input_context()==sampled_context;
        const int held_x=physical_direction?physical_x:(same_input_owner?native_controls.touch_direction.x:0);
        const int held_y=physical_direction?physical_y:(same_input_owner?native_controls.touch_direction.y:0);
        const auto repeat=items_data.view().parameter(upstream::ItemParameter::InputRepeat);
        uint32_t navigation_context=0;
        if(in_battle()&&round_ready&&round_error.empty()){
            const auto p=battle_round.phase();
            if(p==upstream::BattleRoundPhase::Commands)navigation_context=1;
            else if(p==upstream::BattleRoundPhase::Items)navigation_context=2;
            else if(p==upstream::BattleRoundPhase::Targeting)navigation_context=3;
        }
        if(dialogue_choices.active())navigation_context=4;
        if(save_menu.is_open())navigation_context=5;
        if(storage_menu.active()||storage_open_failed)navigation_context=6;
        if(field_equipment_menu.visible())navigation_context=3000+uint32_t(field_equipment_menu.phase());
        if(continue_menu.is_open()&&!continue_menu.pose().world_visible)navigation_context=1000+uint32_t(continue_menu.phase());
        if(new_game_setup.active())navigation_context=2000+new_game_setup.field_index()+32*uint32_t(new_game_setup.phase());
        const auto navigation=menu_navigation.sample(navigation_context,held_x,held_y,double(elapsed)/1000.0,repeat.x,repeat.y,navigation_context==2||navigation_context>=4);
        if(new_game_setup.active()&&!introduction.active()){
            const bool input_allowed=same_input_owner;
            if(new_game_setup.phase()==upstream::NamingPhase::Confirmation)accepted_new_game_view=new_game_setup;
            if(!new_game_setup.step(dt,{input_allowed?navigation.x:0,input_allowed?navigation.y:0,input_allowed&&((down&KEY_A)!=0||native_controls.confirm_pulse),input_allowed&&(down&KEY_B)!=0,input_allowed&&(down&KEY_X)!=0,input_allowed&&(down&KEY_Y)!=0,input_allowed&&(down&KEY_R)!=0,input_allowed&&(down&KEY_L)!=0},error))continue_status=error;
            const int preview_flavor=startup_settings_data.flavor_index(new_game_setup.preview_flavor());if(preview_flavor<0)continue_status="Settings preview palette rejected";else if(ctr::loading_menu_flavor_selected()!=uint32_t(preview_flavor)){wait_for_gpu_idle();ctr::loading_menu_flavor_select(uint32_t(preview_flavor));}
            for(const auto&sound:new_game_setup.take_sounds())play_source_audio(sound);
            if(new_game_setup.phase()==upstream::NamingPhase::Cancelled){wait_for_gpu_idle();new_game_renderer.free();new_game_setup.close();ctr::loading_menu_flavor_select(uint32_t(startup_settings_data.flavor_index(session_state.settings.menu_flavor)));pending_new_game=PendingNewGame{};if(!open_continue(error))house_error=error;native_input.reset();menu_navigation.reset();accumulator=0;}
            else if(new_game_setup.phase()==upstream::NamingPhase::Accepted){
                wait_for_gpu_idle();
                if(!begin_introduction(error)){const auto problem=error;introduction.close();introduction_renderer.free();new_game_renderer.free();new_game_setup.close();pending_new_game=PendingNewGame{};ctr::loading_menu_flavor_select(uint32_t(startup_settings_data.flavor_index(session_state.settings.menu_flavor)));if(!open_continue(error))house_error=error;continue_status="Original introduction rejected: "+problem;}
                native_input.reset();menu_navigation.reset();accumulator=0;
            }
        }else if(continue_menu.is_open()){
            if(continue_menu.phase()==upstream::ContinuePhase::Title&&same_input_owner){if(down&KEY_L)cycle_native_locale(-1);else if(down&KEY_R)cycle_native_locale(1);}
            const bool release_ready=continue_menu.phase()==upstream::ContinuePhase::WorldReveal&&restore_input_pending;
            const bool input_allowed=same_input_owner&&!continue_menu.pose().world_visible;
            if(!continue_menu.step(dt,{input_allowed?navigation.y:0,input_allowed?navigation.x:0,input_allowed&&((down&KEY_A)!=0||native_controls.confirm_pulse),input_allowed&&(down&KEY_B)!=0},error,true))continue_status=error;
            upstream::ContinueEvent event;
            while(continue_menu.poll_event(event)){
                using Kind=upstream::ContinueEventKind;
                if(event.kind==Kind::SoundRequested)play_source_audio(event.path);
                else if(event.kind==Kind::TitleMusicRequested)play_source_audio(event.path,ctr::AudioLane::Music);
                else if(event.kind==Kind::LoadFadeRequested||event.kind==Kind::MusicFadeRequested){if(audio_player.available()&&!audio_player.fade_music(event.seconds,error))audio_status=error;}
                else if(event.kind==Kind::ExitRequested)continue_exit=true;
                else if(event.kind==Kind::LoadRequested){
                    const bool loaded=apply_loaded_slot(event.slot,error);if(!loaded)continue_status="Load rejected: "+error;
                    std::string acknowledge_error;if(!continue_menu.acknowledge_load(loaded,acknowledge_error))continue_status=acknowledge_error;
                    native_input.reset();menu_navigation.reset();accumulator=0;
                }else if(event.kind==Kind::BoundaryRequested){
                    if(event.boundary==upstream::ContinueBoundary::NewGame){
                        if(!stage_new_game(error)){continue_status=error;break;}
                        if(!load_introduction(error)){continue_status=error;pending_new_game=PendingNewGame{};break;}
                        // The title is no longer drawn after this boundary. Release its
                        // GPU allocations before loading naming atlases, rather than
                        // retaining mutually exclusive presentation owners at peak.
                        wait_for_gpu_idle();texture_owner_checkpoint("before-title-release");continue_renderer.free();texture_owner_checkpoint("title-released");
                        const auto restore_title=[&]{
                            const std::string naming_error=error;
                            new_game_renderer.free();new_game_setup.close();pending_new_game=PendingNewGame{};
                            if(!open_continue(error))house_error=error;
                            continue_status=naming_error;
                        };
                        if(!load_ui_font_only(error)){restore_title();break;}
                        if(!new_game_renderer.ready()){LoadingScope loading("Preparing naming",1);if(!loading.step([&]{return new_game_renderer.load(new_game_data,startup_settings_data,error);})||!loading.finish()){restore_title();break;}}
                        texture_owner_checkpoint("naming-loaded");if(!new_game_setup.open(new_game_data,startup_settings_data,error)){restore_title();break;}
                        if(!play_source_audio(introduction_data.music,ctr::AudioLane::Music,introduction_data.music_gain)){error=audio_status;restore_title();break;}
                        continue_menu.close();native_input.reset();menu_navigation.reset();accumulator=0;
                    }
                    else continue_status="This original menu action is not ported; B returns";
                }
            }
            if(release_ready){if(!gameplay_scene->world.unpause_from_house())house_error="Loaded player ready boundary rejected";restore_input_pending=false;}
        }
        if(continue_exit)break;
        const auto abort_intro=[&]{const auto problem=error;wait_for_gpu_idle();introduction.close();introduction_renderer.free();new_game_renderer.free();new_game_setup.close();pending_new_game=PendingNewGame{};native_input.reset();menu_navigation.reset();accumulator=0;if(!open_continue(error))house_error=error;continue_status="Original introduction rejected: "+problem;introduction_render_error.clear();};
        if(intro_active_at_frame_start&&introduction.active()){
            if(introduction_house_committed){const auto camera=gameplay_scene->world.cutscene_camera().center(),player=gameplay_scene->world.player().position;introduction.set_focus(player.x-camera.x+view_width*.5f,player.y-camera.y+view_height*.5f);}
            if(!introduction_render_error.empty()){error=introduction_render_error;abort_intro();}
            else if(!introduction.step(real_dt,same_input_owner&&((down&KEY_A)!=0||native_controls.confirm_pulse),same_input_owner&&(down&KEY_START)!=0,error)||!consume_introduction_audio(error))abort_intro();
            else{
                if(introduction.playtime_started()){if(!introduction_house_committed)introduction_playtime+=real_dt;else if(!world_input)session_state.playtime_seconds+=real_dt;}
                if(introduction.house_ready()&&!introduction_house_committed){
                    // No introduction image/font is consumed after this source
                    // boundary; retain only its final door-overlay data.
                    wait_for_gpu_idle();
                    if(!introduction_renderer.retire_scene_visuals(introduction.pose(),error))abort_intro();
                    else if(!commit_named_new_game(error))abort_intro();
                    else{introduction_house_committed=true;introduction.rebind_random(battle_random);new_game_setup.close();wait_for_gpu_idle();new_game_renderer.free();native_input.reset();menu_navigation.reset();accumulator=0;}
                }
                if(introduction.active()&&introduction.pose().scene_visible&&new_game_renderer.ready()){wait_for_gpu_idle();new_game_renderer.free();texture_owner_checkpoint("naming-released");}
                if(introduction.complete()){wait_for_gpu_idle();introduction.close();introduction_renderer.free();native_input.reset();menu_navigation.reset();accumulator=0;}
            }
        }
        if(dialogue_choices.active()){
            const bool confirm=same_input_owner&&((down&KEY_A)!=0||native_controls.confirm_pulse),cancel=same_input_owner&&(down&KEY_B)!=0;
            if(!dialogue_choices.step(dt,{navigation.x,navigation.y,confirm,cancel},error))house_error=error;
            upstream::DialogueChoicesEvent event;
            while(dialogue_choices.poll_event(event)){
                if(event.kind==upstream::DialogueChoicesEventKind::Selected){
                    if(!gameplay_scene->house.select_story_option(event.target_pc,gameplay_scene->world.story_generation()))house_error=gameplay_scene->house.error();
                    if(event.sound_after_target)play_source_audio(choice_data.sound(event.sound));
                }else play_source_audio(choice_data.sound(event.sound));
            }
        }
        if(field_equipment_menu.visible()){
            const bool allowed=field_was_open&&same_input_owner;
            if(!field_equipment_menu.input(allowed?navigation.x:0,allowed?navigation.y:0,allowed&&((down&uint32_t(field_view.parameter(upstream::FieldParameter::ConfirmMask)))||native_controls.confirm_pulse),allowed&&(down&uint32_t(field_view.parameter(upstream::FieldParameter::CancelMask))),allowed&&(down&uint32_t(field_view.parameter(upstream::FieldParameter::ScopeMask))),allowed&&(down&uint32_t(field_view.parameter(upstream::FieldParameter::OpenMask))))||!field_equipment_menu.idle_frame(dt)){
                field_equipment_status=field_equipment_menu.error();field_equipment_menu=upstream::FieldEquipmentMenu{};
            }
            for(const auto event:field_equipment_menu.take_sounds()){
                const auto binding=upstream::FieldBinding(uint32_t(upstream::FieldBinding::PauseOpenSound)+uint32_t(event)-1);
                if(!play_source_audio(std::string(field_view.binding(binding))))field_equipment_status=audio_status.empty()?house_error:audio_status;
            }
            if(!field_equipment_menu.visible()&&!gameplay_scene->world.unpause_from_house())field_equipment_status="Field menu resume rejected";
        }
        const bool storage_was_open=storage_menu.active();
        if(gameplay_scene->world.take_storage_request()){
            storage_generation=gameplay_scene->world.story_generation();
            if(!open_storage_menu(error)){house_error=error+"; B closes";storage_open_failed=true;}
        }
        if(storage_open_failed&&same_input_owner&&(down&KEY_B)){storage_open_failed=false;if(!gameplay_scene->house.close_story_submenu(storage_generation))house_error=gameplay_scene->house.error();}
        if(storage_menu.active()){
            const bool allowed=storage_was_open&&same_input_owner;
            if(!storage_menu.input(allowed?navigation.x:0,allowed?navigation.y:0,allowed&&((down&KEY_A)||native_controls.confirm_pulse),allowed&&(down&KEY_B))||!storage_menu.idle_frame(dt))house_error=storage_menu.error();
            for(const auto event:storage_menu.take_sounds()){
                if(event==upstream::StorageSoundEvent::Back){const auto view=items_data.view();play_source_audio(std::string(view.string(view.sound_for(upstream::ItemSoundEvent::Close).path)));}
                else play_source_audio(std::string(storage_data.view().binding(upstream::StorageBinding(uint32_t(upstream::StorageBinding::MoveSound)+uint32_t(event)-1))));
            }
            if(!storage_menu.active()&&!gameplay_scene->house.close_story_submenu(storage_generation))house_error=gameplay_scene->house.error();
        }
        const bool record_was_open=save_menu.is_open();
        if(gameplay_scene->world.take_save_request()){
            save_generation=gameplay_scene->world.story_generation();
            if(!open_record_menu(error)){save_status=error+"; B closes";save_open_failed=true;}
        }
        if(save_open_failed&&same_input_owner&&(down&KEY_B)){save_open_failed=false;if(!gameplay_scene->house.close_story_submenu(save_generation))house_error=gameplay_scene->house.error();}
        if(save_menu.is_open()){
            const bool valid_input=record_was_open&&same_input_owner;
            if(!save_menu.step(dt,{valid_input?navigation.y:0,valid_input?navigation.x:0,valid_input&&((down&KEY_A)!=0||native_controls.confirm_pulse),valid_input&&(down&KEY_B)!=0},error))house_error=error;
            upstream::SaveMenuEvent event;
            while(save_menu.poll_event(event)){
                if(event.kind==upstream::SaveMenuEventKind::SoundRequested)play_source_audio(save_menu_data.sound(event.sound));
                else if(event.kind==upstream::SaveMenuEventKind::SaveRequested){if(!write_record_slot(event.slot,error)){save_status=error;std::string menu_error;if(!save_menu.acknowledge_save(false,nullptr,menu_error))house_error=menu_error;}else save_status=error.empty()?"Saved":"Saved; slot preference: "+error;}
                else if(!gameplay_scene->house.close_story_submenu(save_generation))house_error=gameplay_scene->house.error();
            }
        }
        if(in_battle()){
            if(!round_ready&&round_error.empty()&&battle_entry.phase()==upstream::BattleEntryPhase::Commands)begin_round();
            const int direction=navigation.x;const bool confirm=(down&KEY_A)!=0||(same_input_owner&&native_controls.confirm_pulse),cancel=(down&KEY_B)!=0;
            if(round_ready&&round_error.empty()){
                const auto phase=battle_round.phase();
                if(phase==upstream::BattleRoundPhase::Commands){
                    const int menu_direction=navigation.x;
                    battle_entry.input(menu_direction,confirm,cancel,true);
                    if(battle_entry.phase()==upstream::BattleEntryPhase::ActionRequested&&!battle_round.request_menu(battle_entry.requested_action()))round_error=battle_round.error();
                    if(battle_round.phase()==upstream::BattleRoundPhase::Items){items_status.clear();if(!items_menu.open())round_error=items_menu.error();}
                }else if(phase==upstream::BattleRoundPhase::Items){
                    const int horizontal=navigation.x;
                    const int vertical=navigation.y;
                    const auto scope_mask=uint32_t(items_data.view().parameter(upstream::ItemParameter::InputBinding).z);
                    if(!items_menu.input(horizontal,vertical,confirm,cancel,(down&scope_mask)!=0,true))round_error=items_menu.error();
                    const auto result=items_menu.take_result();
                    if(result==upstream::ItemMenuResult::Back){items_status.clear();if(!battle_round.return_from_items())round_error=battle_round.error();}
                    else if(result==upstream::ItemMenuResult::Selected)items_status="This item action is not implemented; B returns";
                }else if(phase==upstream::BattleRoundPhase::Targeting||phase==upstream::BattleRoundPhase::Unsupported){if(!battle_round.target_input(direction,confirm,cancel))round_error=battle_round.error();}
                else round_presentation.input(confirm,cancel);
                if(!items_menu.idle_frame(dt))round_error=items_menu.error();
                for(const auto event:items_menu.take_sounds()){const auto sound=items_data.view().sound_for(event);if(audio_player.available()&&sound.audio_id){std::string audio_error;if(!audio_player.play(sound.audio_id,ctr::AudioLane::Effect,audio_error))audio_status=audio_error;}}
                if(!round_presentation.idle_frame(dt))round_error=round_presentation.error();
                if(!battle_round.idle_frame(dt))round_error=battle_round.error();
                if(battle_round.take_menu_return())battle_entry.resume_commands(phase!=upstream::BattleRoundPhase::Items&&phase!=upstream::BattleRoundPhase::Targeting&&phase!=upstream::BattleRoundPhase::Unsupported);
                if(battle_round.phase()==upstream::BattleRoundPhase::VictoryPending&&battle_outcome.phase()==upstream::BattleOutcomePhase::Idle)
                    if(!battle_outcome.begin(battle_round,round_presentation,gameplay_scene->world))round_error=battle_outcome.error();
                for(const auto event:round_presentation.take_return_events()){
                    using E=upstream::RoundEventKind;
                    bool ok=true;const auto native=battle_data.view().parameter(upstream::BattleParameter::CanvasSize);
                    if(event==E::HideBattleBackground)ok=gameplay_scene->world.resume_battle_camera();
                    else if(event==E::JumpPartyToWorld){
                        const auto center=gameplay_scene->world.cutscene_camera().center();const auto pos=gameplay_scene->world.player().position;
                        // Presentation applies the source sprite root offset once.
                        ok=round_presentation.begin_party_return({pos.x-center.x+view_width*.5f,pos.y-center.y+view_height*.5f},{view_width-native.x,view_height-native.y});
                    }else if(event==E::PartyReturnLanded){
                        const auto turn=round_data.view().parameter(upstream::RoundParameter::ReturnPartyTurn);
                        const auto frames=round_data.view().parameter(upstream::RoundParameter::ReturnPartyFrames);
                        ok=gameplay_scene->world.land_battle_player({turn.x,turn.y},uint16_t(frames.y));
                    }else if(event==E::RotatePartyOriginal)ok=gameplay_scene->world.rotate_battle_player(round_data.view().parameter(upstream::RoundParameter::ReturnPartyTurn).z);
                    if(!ok)round_error="Battle return callback rejected";
                }
                if(!battle_outcome.idle_frame())round_error=battle_outcome.error();
                if(battle_handoff_pending()){session_rewards=battle_outcome.state();session_rewards_valid=true;}
            }
        }
        if(in_field()){if(!field_scene->world.end_scene_frame())field_status=field_scene->world.error();}
        else if(!gameplay_scene->world.end_scene_frame())house_error=gameplay_scene->world.error();
        // uiManager owns bars for the entire dialogue lifetime, including
        // showbox:false movement phrases. Closing text alone is not an end.
        const bool bars_open=!in_field()&&world_visible&&!in_battle()&&(field_equipment_menu.visible()||gameplay_scene->world.cutscene_active()||
            (gameplay_scene->presentation.dialogue_active()&&!gameplay_scene->presentation.dialogue_closing()));
        if(!world_blackbars.update(bars_open,dt))house_error="World blackbar animation rejected";
        if(audio_player.available()){
            // A parked House owner behind a field must not replay its request history.
            if((!in_field()&&!audio_player.consume(opening_data.view(),gameplay_scene->world.audio_requests(),error))||!audio_player.update(dt,error))audio_status=error;
            if(in_battle()&&battle_entry.encounter_audio_pending())if(!audio_player.play(battle_entry.take_encounter_audio(),ctr::AudioLane::Jingle,error))audio_status=error;
        }else if(in_battle()&&battle_entry.encounter_audio_pending())battle_entry.take_encounter_audio();
        if(!region_music.update(dt,audio_player,error))audio_status=error;
        if(frame_load_generation!=loading_generation){last=osGetTime();accumulator=0;native_input.reset();menu_navigation.reset();}
        if(!in_field()&&!update_house_prompt(error))house_error=error;
        upstream::IntroductionPose intro_pose;
        bool draw_introduction=introduction.active();
        if(draw_introduction){intro_pose=introduction.pose();if(!introduction_renderer.prepare(intro_pose,error)){abort_intro();draw_introduction=false;}}
        const bool draw_house=!in_field()&&!new_game_setup.active()&&(!draw_introduction||introduction_house_committed)&&(!continue_menu.is_open()||continue_menu.pose().world_visible);
        const bool draw_field=in_field()&&!new_game_setup.active()&&!continue_menu.is_open();
        PROFILE_MARK(0);
        if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))continue;
        if(session_state_ready&&!session_state.characters.empty())item_details_renderer.set_nickname(session_state.characters.front().nickname);
        record_entry_presented();locale_font.begin_frame();storage_counter_font.begin_frame();field_counter_font.begin_frame();if(draw_introduction&&!introduction_renderer.begin_frame())introduction_render_error="Introduction font frame ownership rejected";
        PROFILE_MARK(1);
        C2D_TargetClear(top,loading_indicator.background_color());C2D_TargetClear(bottom,dark);
        C2D_SceneBegin(top);if(draw_house)house_top();if(draw_field)field_top();PROFILE_MARK(2);
        if(in_battle())battle_top();
        else if(draw_house){
            const auto canvas=house_data.view().parameter(upstream::HouseParameter::DisplayReference);
            float prompt_cx=0,prompt_cy=0;house_camera(prompt_cx,prompt_cy);if(!house_prompt_renderer.draw(house_prompt_pose,prompt_cx,prompt_cy))house_error="World prompt renderer rejected checked pose";
            const auto dialogue_pose=gameplay_scene->presentation.dialogue_pose();
            if(!house_renderer.draw_dialogue(dialogue_pose,battle_renderer,float(view_width)-canvas.x,float(view_height)-canvas.y,view_x(),view_y()))house_error="World dialogue renderer rejected checked pose";
            if(!choice_renderer.draw(dialogue_choices,battle_renderer,view_x()+dialogue_pose.box.x+dialogue_pose.anchor.x*(view_width-canvas.x),view_y()+dialogue_pose.box.y+dialogue_pose.anchor.y*(view_height-canvas.y)))house_error="Dialogue choices renderer rejected pose";
            if(save_menu.is_open()){const auto flavor=save_menu_data.flavor_index(session_state.settings.menu_flavor);C3D_Mtx saved_view;C2D_ViewSave(&saved_view);C2D_ViewTranslate(view_x(),view_y());if(flavor<0||!save_renderer.draw(save_menu,battle_renderer,uint32_t(flavor),float(view_width),float(view_height)))house_error="Save menu renderer rejected checked pose";C2D_ViewRestore(&saved_view);}
            if(storage_menu.active()&&!storage_renderer.draw(storage_menu,battle_renderer,storage_counter_text,float(view_width),float(view_height),view_x(),view_y()))house_error="Storage renderer rejected checked pose";
            if(field_equipment_menu.visible()&&!field_equipment_renderer.draw(field_equipment_menu,battle_renderer,field_counter_text,float(view_width),float(view_height),view_x(),view_y()))field_equipment_status=field_equipment_renderer.error();
            const auto fade=gameplay_scene->house.fade_color();if(fade.w>0)BattleRenderer::draw_rect(view_x(),view_y(),float(view_width),float(view_height),battle_color(fade));
        }
        if((draw_house||draw_field)&&scene_door.covering()){
            // Circle Focus follows the player's screen position in the current scene.
            float fx=0,fy=0;
            if(draw_field){const auto p=field_scene->world.player().position;const auto o=field_scene->camera_origin(p,{float(view_width),float(view_height)});fx=p.x-o.x;fy=p.y-o.y;}
            else {float hcx=0,hcy=0;house_camera(hcx,hcy);const auto p=gameplay_scene->world.player().position;fx=p.x-hcx-view_x();fy=p.y-hcy-view_y();}
            draw_door_masks(fx,fy);
        }
        if(continue_menu.is_open()){
            C3D_Mtx saved_view;C2D_ViewSave(&saved_view);C2D_ViewTranslate(view_x(),view_y());
            const auto camera=gameplay_scene->world.cutscene_camera().center(),player=gameplay_scene->world.player().position;
            if(!continue_renderer.draw(continue_menu,save_renderer,battle_renderer,float(view_width),float(view_height))||!continue_renderer.draw_overlay(continue_menu,float(view_width),float(view_height),player.x-camera.x+view_width*.5f,player.y-camera.y+view_height*.5f))continue_status="Continue renderer rejected checked pose";
            C2D_ViewRestore(&saved_view);
        }
        if(new_game_setup.active()&&!draw_introduction){C3D_Mtx saved;C2D_ViewSave(&saved);C2D_ViewTranslate(view_x(),view_y());if(!new_game_renderer.draw(new_game_setup,battle_renderer,float(view_width),float(view_height)))continue_status="Naming renderer rejected checked pose";C2D_ViewRestore(&saved);}
        if(draw_introduction){
            C3D_Mtx saved;C2D_ViewSave(&saved);C2D_ViewTranslate(view_x(),view_y());
            if(intro_pose.scene_visible){if(!introduction_renderer.draw(intro_pose,float(view_width),float(view_height)))introduction_render_error="Introduction renderer rejected checked pose";}
            else if(!introduction_house_committed&&!new_game_renderer.draw(accepted_new_game_view,battle_renderer,float(view_width),float(view_height)))introduction_render_error="Naming departure renderer rejected checked pose";
            if(!introduction_renderer.draw_overlay(intro_pose))introduction_render_error="Introduction door renderer rejected checked pose";
            C2D_ViewRestore(&saved);
        }
        reference_borders();PROFILE_MARK(3);
        C2D_SceneBegin(bottom);if(in_field())field_bottom();else house_bottom();ctr::draw_native_input(native_controls.gesture,native_input_data.tuning());PROFILE_MARK(4);
        C3D_FrameEnd(0);record_entry_submitted();locale_font.end_frame();storage_counter_font.end_frame();field_counter_font.end_frame();if(draw_introduction)introduction_renderer.end_frame();PROFILE_MARK(5);qa_record(down);PROFILE_MARK(6);PROFILE_FINISH();
    }
#ifdef ENCORE_TEXT_QA
    if(qa_log){std::fclose(qa_log);qa_log=nullptr;}
#endif
    aptUnhook(&input_hook);
    wait_for_gpu_idle();introduction.close();introduction_renderer.free();new_game_renderer.free();world_effect_renderer.free();free_house();free_debug_text();C2D_Fini();C3D_Fini();romfsExit();gfxExit();return 0;
}
