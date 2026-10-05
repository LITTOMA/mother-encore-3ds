#pragma once
#include "battle_preparation_budget.hpp"
// Private preparation owners. No world/session/RNG pointer crosses the worker.
struct BattlePrewarm {
    upstream::BattleData battle;upstream::BattleRoundData round;
    BattleRenderer renderer;RoundRenderer actions;
    std::vector<std::string> paths,resident;std::vector<uint32_t> order;
    std::vector<BattleRenderer::IndexedResident> indexed;
    std::string path,round_path,error;unsigned width=0,height=0;
    std::atomic<unsigned> status{0};std::atomic<bool> cancelled{false};
    Thread thread=nullptr;bool ready=false,reported_miss=false,probe_ready=false,headers_ready=false;
    size_t battle_bytes=0,round_bytes=0;
    u64 started=0,ready_at=0;uint64_t max_frame_gap=0;
    double pack_ms=0,max_gpu_ms=0;unsigned gpu_steps=0;
    BattlePrepareTimings timing;
    EncounterResidencyTicket ticket;EncounterResidencyBytes header_peak,peak,retained;
    ~BattlePrewarm(){renderer.free();actions.free();}
    size_t metadata_bytes()const{
        size_t bytes=sizeof(*this)+battle.resident_bytes()+round.resident_bytes()+actions.cpu_bytes();
        bytes+=paths.capacity()*sizeof(std::string)+resident.capacity()*sizeof(std::string)+order.capacity()*sizeof(uint32_t)+indexed.capacity()*sizeof(BattleRenderer::IndexedResident);
        for(const auto& v:paths)bytes+=v.capacity();for(const auto& v:resident)bytes+=v.capacity();
        for(const auto& v:indexed)bytes+=v.path.capacity()+v.palette.capacity()*sizeof(uint32_t);
        return bytes+path.capacity()+round_path.capacity()+error.capacity();
    }
    static void run(void* context){
        auto& p=*static_cast<BattlePrewarm*>(context);
        const PreparationControl control{[](void* state){return static_cast<BattlePrewarm*>(state)->cancelled.load(std::memory_order_relaxed);},&p};
        if(!p.probe_ready){
            const bool ok=!control.stopped()&&battle_budget::size(p.path,1024*1024,p.battle_bytes,p.error)&&battle_budget::size(p.round_path,2*1024*1024,p.round_bytes,p.error);
            // Reserve packed inputs, validated copies and bounded parser/tree
            // bookkeeping before reading either complete container.
            p.header_peak={12*(p.battle_bytes+p.round_bytes)+p.metadata_bytes()+1024*1024,0};
            p.probe_ready=ok;p.status.store(ok?4:2,std::memory_order_release);return;
        }
        if(!p.headers_ready){
            const auto start=svcGetSystemTick();std::vector<uint8_t> bytes;
            bool ok=battle_budget::read(p.path,p.battle_bytes,bytes,control,p.error)&&p.battle.load(bytes.data(),bytes.size(),p.error,&control);
            std::vector<uint8_t>().swap(bytes);
            ok=ok&&battle_budget::read(p.round_path,p.round_bytes,bytes,control,p.error)&&p.round.load(bytes.data(),bytes.size(),p.error,&control);
            std::vector<uint8_t>().swap(bytes);p.pack_ms=double(svcGetSystemTick()-start)/CPU_TICKS_PER_MSEC;
            if(ok&&p.round.view().binding().battle_id!=p.battle.view().metadata().room_battle_id){p.error="Prewarm battle companion identity mismatch";ok=false;}
            if(ok)ok=battle_budget::estimate(p.battle.view(),p.round.view(),p.width,p.height,p.resident,p.metadata_bytes()+32*1024,control,p.peak,p.error);
            p.headers_ready=ok;p.status.store(ok?3:2,std::memory_order_release);return;
        }
        const auto round_view=p.round.view();p.renderer.use_indexed_residents(&p.indexed);
        const bool ok=!control.stopped()&&prepare_battle_presentation(p.battle.view(),p.renderer,p.paths,p.order,p.width,p.height,p.error,true,&p.resident,&p.timing,&round_view,&control);
        p.renderer.use_indexed_residents(nullptr);p.status.store(ok?1:2,std::memory_order_release);
    }
};
std::unique_ptr<BattlePrewarm> battle_prewarm;
std::vector<std::unique_ptr<BattlePrewarm>> battle_resident;
EncounterResidency encounter_admission;uint64_t encounter_scene_epoch=0;bool encounter_admission_cancelled=false;
EncounterResidencyBytes unproven_active;
BattlePrewarm* battle_ready(const std::string& path){for(auto& p:battle_resident)if(p->path==path&&p->ready)return p.get();return nullptr;}
void battle_prewarm_trace(const char* phase,const BattlePrewarm& p,double commit_ms=0){
    char line[440];const int n=std::snprintf(line,sizeof(line),"ENCORE_BATTLE_PREWARM phase=%s path=%s elapsed_ms=%llu pack_ms=%.3f resource_ms=%.3f background_ms=%.3f max_gpu_ms=%.3f gpu_steps=%u commit_ms=%.3f linear_free=%lu late=%u max_frame_gap_ms=%llu cpu_retained=%lu cpu_peak=%lu\n",phase,p.path.c_str(),(unsigned long long)(osGetTime()-p.started),p.pack_ms,p.timing.resource_ms,p.timing.background_ms,p.max_gpu_ms,p.gpu_steps,commit_ms,(unsigned long)linearSpaceFree(),unsigned(p.reported_miss),(unsigned long long)p.max_frame_gap,(unsigned long)p.retained.cpu,(unsigned long)p.peak.cpu);
    if(n>0)svcOutputDebugString(line,std::min(size_t(n),sizeof(line)-1));
#ifdef ENCORE_TEXT_QA
    if(qa_log){std::fprintf(qa_log,"%s",line);std::fflush(qa_log);}
#endif
}
void cancel_battle_prewarm(){
    battle_resident.clear();encounter_admission.retire();unproven_active={};
    if(!battle_prewarm)return;
    auto& p=*battle_prewarm;p.cancelled.store(true,std::memory_order_relaxed);
    if(p.thread){threadJoin(p.thread,UINT64_MAX);threadFree(p.thread);p.thread=nullptr;}
    battle_prewarm_trace("cancel",p);battle_prewarm.reset();
}
bool battle_prewarm_ready(const std::string& path){const auto* p=battle_ready(path);return p&&encounter_admission.ready(p->ticket);}
void note_battle_prewarm_miss(const std::string& path){
    if(battle_prewarm&&battle_prewarm->path==path&&!battle_prewarm->reported_miss){battle_prewarm->reported_miss=true;const std::string line="ENCORE_BATTLE_PREWARM phase=late path="+path+"\n";svcOutputDebugString(line.data(),line.size());}
}
void report_battle_entry_commit(const std::string& path,double ms,uint64_t load_activity){
    char line[220];const int n=std::snprintf(line,sizeof(line),"ENCORE_BATTLE_ENTRY path=%s commit_ms=%.3f retained=%u linear_free=%lu load_activity=%llu\n",path.c_str(),ms,unsigned(battle_resident.size()),(unsigned long)linearSpaceFree(),(unsigned long long)load_activity);if(n>0)svcOutputDebugString(line,size_t(n));
#ifdef ENCORE_TEXT_QA
    if(qa_log){std::fprintf(qa_log,"%s",line);std::fflush(qa_log);}
#endif
}
bool commit_battle_prewarm(const std::string& path,std::string& error){
    if(!battle_prewarm_ready(path)){error="Battle presentation is not prepared";return false;}
    auto& p=*battle_ready(path);const std::string previous=loaded_battle_path;const bool keep_previous=battle_renderer.fully_resident();battle_data.swap(p.battle);round_data.swap(p.round);battle_renderer.swap(p.renderer);round_renderer.swap(p.actions);battle_paths.swap(p.paths);battle_draw_order.swap(p.order);
    battle_renderer.attach_source_font(&locale_font);loaded_battle_path=path;p.path=previous;p.ready=keep_previous&&!previous.empty();
    const auto& declared=encounter_admission.declaration();for(size_t i=0;i<declared.encounters.size();++i)if("romfs:/"+declared.encounters[i].battle_pack==previous)p.ticket={declared.scene_epoch,i};
    // The old ready encounter remains in the same bounded scene working set.
    // Repeated/random encounter selection never reloads or preselects RNG.
    if(!keep_previous){p.path.clear();p.renderer.free();p.actions.free();}
    return true;
}
uint32_t predicted_battle_resource(){
    using namespace upstream;const auto room=opening_data.view();const auto& world=gameplay_scene->world;
    if(world.battle_request().requested)return world.battle_request().battle_resource_index;
    // Read-only scan of a running original script. No command dispatch, branch
    // evaluation, actor tick, clock or RNG call occurs during anticipation.
    if(world.cutscene_active()&&world.story_program_index()<room.program_count()){
        const auto program=room.program(world.story_program_index());
        for(uint32_t i=world.story_next_command_index();i<program.command_count;++i){const auto c=room.command(program.first_command+i);
            if(c.opcode==uint16_t(DialogueActionKind::QueueBattle)&&c.target_index<room.battle_count())return room.battle(c.target_index).battle_resource_index;
        }
    }
    float distance=std::numeric_limits<float>::infinity();uint32_t result=kRoomNoIndex;const auto player=world.player().position;
    for(uint32_t i=0;i<room.battle_count();++i){const auto b=room.battle(i);
        if(b.actor_instance_index>=world.actor_count()||!world.instance_visible(b.actor_instance_index)||(b.win_flag_index!=kRoomNoIndex&&world.story_flag(room.string(room.flag(b.win_flag_index).name_string))))continue;
        const auto pos=world.actor(b.actor_instance_index).position;const auto dx=pos.x-player.x,dy=pos.y-player.y;const float d=dx*dx+dy*dy;
        if(d<distance){distance=d;result=b.battle_resource_index;}
    }
    return result;
}
void update_battle_prewarm(bool world_phase,uint64_t frame_gap=0,bool admission=false){
    if(battle_prewarm)battle_prewarm->max_frame_gap=std::max(battle_prewarm->max_frame_gap,frame_gap);
    if(!world_phase)return; // Menus may obscure an admitted world without retiring its epoch.
    if(!admission&&(in_battle()||battle_handoff_pending()))return;
    if(!encounter_admission.epoch()){
        SceneEncounterDependencies declaration;declaration.scene_epoch=++encounter_scene_epoch;const auto room=opening_data.view();
        for(uint32_t i=0;i<room.battle_count();++i){const auto index=room.battle(i).battle_resource_index;
            if(index>=room.resource_count()||room.resource(index).kind!=uint16_t(upstream::RoomResourceKind::CheckedBattlePack)){house_error="Scene encounter declaration has an uncompiled dependency";return;}
            const std::string path(room.string(room.resource(index).path_string));bool found=false;for(const auto& e:declaration.encounters)found|=e.battle_pack==path;if(found)continue;
            const auto round=resource_catalog.companion_path(path);
            if(round.empty()){house_error="Scene encounter declaration companion binding rejected";return;}
            declaration.encounters.push_back({path,path,round});
        }
        std::string error;
        // Component budgets include both the current battle and all incoming
        // candidate owners. The platform's remaining heap is an additional cap.
        // Cold admission now owns the round atlases as well as procedural GPU
        // surfaces: the former 6 MiB grant assumed those atlases were loaded
        // at boot. The fully cold checked Lamp upper bound is 11,273,472 bytes.
        // Keep a bounded 12 MiB ceiling and the actual-free-LINEAR constraint.
        const size_t current=battle_renderer.prepared_cpu_bytes()+battle_data.resident_bytes()+round_data.resident_bytes();const auto heap=mallinfo();const size_t used=size_t(heap.uordblks);const size_t outside=used>current?used-current:0;
        const size_t total=envGetHeapSize();const size_t available=total>outside+2*1024*1024?total-outside-2*1024*1024:0;
        const EncounterResidencyBytes budget{std::min<size_t>(28*1024*1024,available),std::min<size_t>(12*1024*1024,size_t(linearSpaceFree())+battle_renderer.prepared_linear_bytes())};
        unproven_active=battle_renderer.fully_resident()?EncounterResidencyBytes{}:EncounterResidencyBytes{current,battle_renderer.prepared_linear_bytes()};
        if(!encounter_admission.declare(declaration,budget,unproven_active,error)){house_error=error;return;}
        for(size_t i=0;i<declaration.encounters.size();++i)if("romfs:/"+declaration.encounters[i].battle_pack==loaded_battle_path&&battle_renderer.fully_resident()){const EncounterResidencyBytes retained{current,battle_renderer.prepared_linear_bytes()};if(!encounter_admission.reserve(i,retained,error)||!encounter_admission.publish({declaration.scene_epoch,i},retained,error)){house_error=error;return;}}
    }
    const auto resource=predicted_battle_resource();const auto room=opening_data.view();
    std::string path;
    const auto missing=[&](uint32_t index){
        if(index>=room.resource_count()||room.resource(index).kind!=uint16_t(upstream::RoomResourceKind::CheckedBattlePack))return std::string{};
        const std::string candidate="romfs:/"+std::string(room.string(room.resource(index).path_string));
        return (candidate==loaded_battle_path&&battle_renderer.fully_resident())||battle_ready(candidate)?std::string{}:candidate;
    };
    // Priority changes only queue order. The dependency union, never a sampled
    // random encounter, defines admission and retention for this scene epoch.
    if(!loaded_battle_path.empty()&&!battle_renderer.fully_resident())path=loaded_battle_path;
    if(path.empty())path=missing(resource);
    if(path.empty())for(uint32_t i=0;i<room.battle_count();++i){path=missing(room.battle(i).battle_resource_index);if(!path.empty())break;}
    if(path.empty()&&!battle_prewarm)return;
    if(!battle_prewarm){
        const auto round=companion_path(path);if(round.empty()){house_error="Prewarm companion binding rejected";return;}
        auto p=std::make_unique<BattlePrewarm>();p->path=path;p->round_path=round;p->width=view_width;p->height=view_height;p->started=osGetTime();
        const auto& declaration=encounter_admission.declaration();for(size_t i=0;i<declaration.encounters.size();++i)if("romfs:/"+declaration.encounters[i].battle_pack==path)p->ticket={declaration.scene_epoch,i};
        size_t snapshot_peak=2*1024*1024+2*battle_renderer.indexed_snapshot_bytes();
        for(const auto& entry:battle_resident)snapshot_peak+=2*entry->renderer.indexed_snapshot_bytes();
        for(const auto* sheet:ctr::loading_menu_flavor_detail::sheets)snapshot_peak+=2*(sizeof(std::string)+sheet->source_path.capacity());
        if(!encounter_admission.reserve(p->ticket.index,{snapshot_peak,0},p->error)){house_error=p->error;return;}
        if(p->width>400||p->height>240){house_error="Prewarm viewport exceeds residency budget";return;}
        p->resident.reserve(ctr::loading_menu_flavor_detail::sheets.size());for(auto* sheet:ctr::loading_menu_flavor_detail::sheets)p->resident.push_back(sheet->source_path);
        p->indexed=battle_renderer.indexed_residents();for(const auto& entry:battle_resident){const auto indices=entry->renderer.indexed_residents();p->indexed.insert(p->indexed.end(),indices.begin(),indices.end());}
        s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);
        p->thread=threadCreate(BattlePrewarm::run,p.get(),32*1024,int(std::min<s32>(priority+1,0x3f)),-2,false);
        if(!p->thread){house_error="Battle prewarm worker could not start";return;}
        battle_prewarm=std::move(p);return;
    }
    auto& p=*battle_prewarm;const auto status=p.status.load(std::memory_order_acquire);if(!status||p.ready)return;
    if(p.thread){threadJoin(p.thread,UINT64_MAX);threadFree(p.thread);p.thread=nullptr;}
    if(status==3||status==4){
        if(!encounter_admission.revise(p.ticket,status==4?p.header_peak:p.peak,p.error)){house_error=p.error+": "+p.path;p.status.store(2);return;}
        p.status.store(0);s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);
        p.thread=threadCreate(BattlePrewarm::run,&p,32*1024,int(std::min<s32>(priority+1,0x3f)),-2,false);
        if(!p.thread){p.status.store(2);p.error="Reserved battle worker could not restart";house_error=p.error;}return;
    }
    if(status==2){house_error="Battle prewarm failed: "+p.error;return;}
    // This pump runs before FrameBegin. All preceding GPU users have finished
    // before publication, and only new independent textures are admitted here.
    const auto started=svcGetSystemTick();bool done=false;
    if(!p.renderer.finish_gpu_step(done,p.error)){house_error="Battle prewarm GPU failed: "+p.error;p.status.store(2);return;}
    ++p.gpu_steps;p.max_gpu_ms=std::max(p.max_gpu_ms,double(svcGetSystemTick()-started)/CPU_TICKS_PER_MSEC);
    if(done){
        // Publish encounter readiness only after its jingle is checked. Deferred
        // PCM admission must not add a first-use scan to the encounter request.
        if(audio_player.available()&&!audio_player.prepare(p.battle.view().metadata().encounter_audio,p.error)){house_error="Battle prewarm audio failed: "+p.error;p.status.store(2);return;}
        if(!p.actions.load(p.round.view(),"romfs:/",p.error,true)){house_error="Battle prewarm actions failed: "+p.error;p.status.store(2);return;}
        std::vector<BattleRenderer::IndexedResident>().swap(p.indexed);std::vector<std::string>().swap(p.resident);
        p.retained={p.renderer.prepared_cpu_bytes()+p.metadata_bytes(),p.renderer.admitted_linear_bytes()};
        if(!encounter_admission.publish(p.ticket,p.retained,p.error)){house_error=p.error;p.status.store(2);return;}
        p.ready=true;p.ready_at=osGetTime();battle_prewarm_trace("ready",p);const std::string prepared_path=p.path;
        battle_resident.push_back(std::move(battle_prewarm));
        // A synchronous boot renderer is not a readiness certificate. Upgrade
        // it to the same immutable indexed residency before exposing the scene.
        if(!battle_renderer.fully_resident()&&(loaded_battle_path.empty()||prepared_path==loaded_battle_path)){
            wait_for_gpu_idle();std::string error;if(!commit_battle_prewarm(prepared_path,error)){house_error=error;return;}
            if(!encounter_admission.retire_other_live(unproven_active)){house_error="Encounter old-owner budget retirement rejected";return;}
            unproven_active={};battle_resident.erase(std::remove_if(battle_resident.begin(),battle_resident.end(),[](const auto& owner){return !owner->ready&&owner->path.empty();}),battle_resident.end());
        }
    }
}

// Existing initial/LOAD scene admission only. All naming actors have already
// been released; no world, animation, inventory or RNG tick occurs here. This
// cannot be called from an encounter request or add a battle loading screen.
bool admit_encounter_scene(std::string& error){
    const auto started=osGetTime();encounter_admission_cancelled=false;
    while(!encounter_admission.scene_ready()){
        if(!aptMainLoop()){encounter_admission_cancelled=true;cancel_battle_prewarm();error="Scene admission interrupted";return false;}
        hidScanInput();if(hidKeysDown()&KEY_B){encounter_admission_cancelled=true;cancel_battle_prewarm();error="Scene preparation cancelled";return false;}
        if(audio_player.available()){std::string audio_error;if(!audio_player.pump_streams(audio_error))audio_status=audio_error;}
        update_battle_prewarm(true,0,true);
        if(!house_error.empty()){error=house_error;return false;}
        report_load_progress(LoadPhase::Scene,battle_resident.size(),encounter_admission.declaration().encounters.size());
        gspWaitForVBlank();
    }
    char line[220];const auto heap=mallinfo();const int n=std::snprintf(line,sizeof(line),"ENCORE_ENCOUNTER_SCENE_READY epoch=%llu candidates=%u heap_used=%lu heap_total=%lu linear_free=%lu elapsed_ms=%llu\n",(unsigned long long)encounter_admission.epoch(),unsigned(encounter_admission.declaration().encounters.size()),(unsigned long)heap.uordblks,(unsigned long)envGetHeapSize(),(unsigned long)linearSpaceFree(),(unsigned long long)(osGetTime()-started));
    if(n>0)svcOutputDebugString(line,size_t(n));
#ifdef ENCORE_TEXT_QA
    if(qa_log){std::fprintf(qa_log,"%s",line);std::fflush(qa_log);}
#endif
    return true;
}
