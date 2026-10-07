// Manual source only. Deliberately not registered with the routine test suite.
// The caller supplies the actual checked House tree and generated pack bytes.
#include "encore/house_return_button_prompt.hpp"
#include "encore/crc32.hpp"
#include "../platform/ctr/house_return_button_prompt_native.hpp"
#include <cassert>
using namespace encore::upstream;
namespace {
void word(std::vector<uint8_t>&b,size_t at,uint32_t v){
 for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));
}
void repair_crc(std::vector<uint8_t>&b){word(b,32,encore::crc32(b.data()+120,b.size()-120));}
}
void house_button_prompt_parser_manual(const FieldNodeTreeData&tree,
 const std::array<uint8_t,32>&tree_ir,const std::vector<uint8_t>&source){
 std::string e;HouseReturnButtonPromptData positive;
 assert(positive.load(source.data(),source.size(),tree,tree_ir,e));
 assert(positive.valid()&&!positive.scene_admitted());
 auto rejects=[&](const std::vector<uint8_t>&b){HouseReturnButtonPromptData d;
  assert(!d.load(b.data(),b.size(),tree,tree_ir,e));assert(!d.valid());};
 for(size_t size:{size_t(0),size_t(119),source.size()-1}){
  std::vector<uint8_t>b(source.begin(),source.begin()+size);rejects(b);
 }
 for(size_t field:{size_t(8),size_t(12),size_t(16),size_t(20)}){
  auto b=source;word(b,field,0);rejects(b);
 }
 for(size_t proof:{size_t(36),size_t(56),size_t(120),size_t(152),size_t(184)}){
  auto b=source;b[proof]^=1;if(proof>=120)repair_crc(b);rejects(b);
 }
 auto wrong_tree_ir=tree_ir;wrong_tree_ir[0]^=1;HouseReturnButtonPromptData mismatch;
 assert(!mismatch.load(source.data(),source.size(),tree,wrong_tree_ir,e));
 // A failed parse may not publish either embedded owner or replace a loaded one.
 auto bad=source;word(bad,12,2);
 assert(!positive.load(bad.data(),bad.size(),tree,tree_ir,e));
 assert(positive.valid()&&positive.tree_ir_sha256()==tree_ir);
}
// These probes require a fresh actual factory/lifecycle fixture per call. They
// do not create a fake Bus, Ready receipt or completion owner. A manual runner
// must wire the same real Registry/Tree/player/global/native leaf dispatch.
void house_button_prompt_completion_outside_signal_manual(
 encore::ctr::HouseReturnButtonPromptNative&owner,uint32_t actual_prompt){
 std::string e;
 auto&runtime=const_cast<FieldPromptRuntime&>(owner.runtime());
 assert(!runtime.source_animation_finished(actual_prompt,FieldPromptClipRole::Show,e));
 assert(!runtime.source_press_resume(actual_prompt,FieldPromptClipRole::Press,e));
}
void house_button_prompt_missing_waiter_manual(
 encore::ctr::HouseReturnButtonPromptNative&owner,FieldObjectSignals&bus,
 const HouseReturnButtonPromptData&data,FieldObjectId actual_prompt){
 std::string e;encore::ctr::HouseButtonPromptWaitState wait;
 assert(owner.wait_state(actual_prompt,wait,e)&&wait.pending);
 assert(bus.disconnect(wait.player,data.native().finished_signal(),wait.object,
                       data.wait().method,e));
 const auto*press=data.core().clip(FieldPromptClipRole::Press);assert(press);
 assert(!owner.phase(wait.player,FieldTreePhase::IdleInternal,
                     press->length,false,false,e));
}
void house_button_prompt_wrong_ap_manual(
 encore::ctr::HouseReturnButtonPromptNative&owner,FieldObjectSignals&bus,
 const HouseReturnButtonPromptData&data,FieldObjectId other_actual_ap){
 std::string e;assert(data.core().clip(FieldPromptClipRole::Press));
 assert(!bus.emit(other_actual_ap,data.native().finished_signal(),
                {data.core().clip(FieldPromptClipRole::Press)->name},e));
}
void house_button_prompt_wrong_clip_manual(
 encore::ctr::HouseReturnButtonPromptNative&owner,FieldObjectSignals&bus,
 const HouseReturnButtonPromptData&data,FieldObjectId actual_prompt){
 std::string e;encore::ctr::HouseButtonPromptWaitState wait;
 assert(owner.wait_state(actual_prompt,wait,e)&&wait.pending);
 assert(!bus.emit(wait.player,data.native().finished_signal(),
                 {data.core().clip(FieldPromptClipRole::Hide)->name},e));
}
void house_button_prompt_duplicate_resume_manual(
 const FieldGlobalRegistry&registry,const encore::ctr::HouseButtonPromptWaitState&wait,
 const HouseReturnButtonPromptData&data){
 auto reference=registry.native_reference(wait.object);assert(reference);
 FieldDeferredMessage m; m.object=wait.object;m.kind=FieldDeferredKind::Call;
 m.member=data.wait().method;m.args={data.core().clip(FieldPromptClipRole::Press)->name,FieldObjectRef{wait.object}};
 std::string e;
 // The fixture retains the real consumed Reference before the actual idle tail
 // retires it; a replay has no Bus dispatch scope and cannot resume twice.
 assert(!const_cast<FieldGlobalNativeReference*>(reference.get())->dispatch(m,e));
}
// Invoke this probe from the actual hide signal receiver while the real Press
// FunctionState resumes. The first completion is already consumed, and its
// Reference remains alive until the same native idle notification returns.
void house_button_prompt_same_signal_reentry_manual(
 encore::ctr::HouseReturnButtonPromptNative&owner,FieldObjectSignals&bus,
 const HouseReturnButtonPromptData&data,FieldObjectId actual_prompt,
 FieldObjectId actual_hide_receiver,std::string_view actual_hide_method){
 std::string e;encore::ctr::HouseButtonPromptWaitState wait;
 assert(bus.emitting_to(actual_prompt,data.hide_signal(),actual_hide_receiver,
                       actual_hide_method));
 assert(owner.wait_state(actual_prompt,wait,e)&&!wait.pending);
 assert(!bus.emit(wait.player,data.native().finished_signal(),
                 {data.core().clip(FieldPromptClipRole::Press)->name},e));
}
// Remaining live-driver cases (separate real fixtures, no execution claimed):
// 1. Press accepted by its actual source caller, remove actual FunctionState
//    connection before the sole AP idle reaches its end: missing resume rejects.
// 2. Route animation_finished from a different actual AP or with Hide instead
//    of Press: exact emitter/clip rejects and cannot clear pressing.
// 3. Replay the real FunctionState callback after the first resume: pending=false
//    rejects; no second hide emission or new completion lease is created.
// 4. Reenter the SAME finished signal during the persistent/one-shot callback:
//    actual Bus scope plus core finishing cursor rejects duplicate consumption.
// 5. Invoke serialized offset with wrong parent/value/outside InteractDialog's
//    constructor prefix: actual source_offset_live rejects; no local transform
//    is written. Correct pre-Enter setter changes only the actual offset field.
// 6. Supply another loaded font catalog or unregistered/default Flash material:
//    prepare/construct rejects rather than admitting source Ready or GPU drawing.
