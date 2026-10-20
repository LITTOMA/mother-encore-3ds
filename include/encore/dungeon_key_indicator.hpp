#pragma once
#include "encore/dungeon_keys.hpp"
#include <array>
#include <memory>

namespace encore::upstream {
// ENCKUI01 format1/capability1. State/tween parameters only; no scene, glyph,
// icon, persisted counter or save admission. Nested key policy owns identities.
struct KeyIndicatorConfig {
 std::array<double,9>parameters{};uint32_t flags=0,diagnostics=0,curve=0,default_curve=0;int32_t threshold=0;
 std::string initial_text,prefix,suffix;
};
class DungeonKeyIndicatorData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const KeyIndicatorConfig&config()const;
 DungeonKeysView keys()const;
 bool same_contract(const DungeonKeyIndicatorData&)const;
private:struct State;std::shared_ptr<const State>state_;
};
enum class KeyIndicatorDiagnosticKind:uint32_t {AppendAfterStarted=1};
struct KeyIndicatorDiagnostic {uint64_t epoch=0;KeyIndicatorDiagnosticKind kind=KeyIndicatorDiagnosticKind::AppendAfterStarted;int32_t count=0;};
struct KeyIndicatorView {Vec2 position{};std::string text;int32_t count=0;bool open=false,visible=false,running=false;};
class DungeonKeyIndicator {
public:
 bool prepare(const DungeonKeyIndicatorData&,uint64_t,std::string&);
 bool update_counter(uint64_t,int32_t,std::string&);bool close(uint64_t,std::string&);
 bool advance(uint64_t,double,std::string&);bool retire(uint64_t,std::string&);
 const DungeonKeyIndicatorData&data()const{return data_;}
 uint64_t epoch()const{return epoch_;}const KeyIndicatorView&view()const{return view_;}
 const std::vector<KeyIndicatorDiagnostic>&diagnostics()const{return diagnostics_;}
 void clear_diagnostics(){diagnostics_.clear();}
private:
 struct Segment {bool y=false,explicit_from=false,hide=false,began=false;double from=0,to=0;float duration=0,elapsed=0;uint32_t curve=0;};
 bool validate(uint64_t,std::string&)const;bool open(int32_t,std::string&);bool update(int32_t,std::string&);
 void replace(Segment);void finish_close();void start_segment();
 DungeonKeyIndicatorData data_;uint64_t epoch_=0,last_epoch_=0;
 KeyIndicatorView view_;std::vector<Segment>segments_;size_t cursor_=0;bool has_tween_=false,started_=false;
 uint32_t root_curve_=0;
 std::vector<KeyIndicatorDiagnostic>diagnostics_;
};
}
