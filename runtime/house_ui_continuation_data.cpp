#include "encore/crc32.hpp"
#include "encore/house_ui_continuation.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
bool nonzero(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t b) { return b != 0; });
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  double real() {
    uint64_t bits=u();bits|=uint64_t(u())<<32;double value=0;std::memcpy(&value,&bits,8);
    if(!std::isfinite(value))ok=false;
    return value;
  }
  float real32() {
    const auto bits=u();float value=0;std::memcpy(&value,&bits,4);
    if(!std::isfinite(value))ok=false;
    return value;
  }
  template <size_t N> std::array<uint8_t, N> bytes() {
    std::array<uint8_t, N> b{};
    if (!ok || at > n || n - at < N) {
      ok = false;
      return b;
    }
    std::copy_n(p + at, N, b.begin());
    at += N;
    return b;
  }
  std::string text() {
    auto k = u();
    if (!ok || !k || k > 4096 || at > n || n - at < k) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t c = 0;
    if (s.find('\0') != s.npos || !utf8_count(s, c))
      ok = false;
    return s;
  }
};
} // namespace
const HouseUiSourceMethod *
HouseUiContinuationData::method(uint32_t role) const {
  auto x = std::find_if(methods_.begin(), methods_.end(),
                        [&](const auto &v) { return v.role == role; });
  return x == methods_.end() ? nullptr : &*x;
}
const HouseUiSourceSignal *
HouseUiContinuationData::signal(uint32_t role) const {
  auto x = std::find_if(signals_.begin(), signals_.end(),
                        [&](const auto &v) { return v.role == role; });
  return x == signals_.end() ? nullptr : &*x;
}
bool HouseUiContinuationData::source_hash(std::string_view p,
                                          std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (!valid_ || i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool HouseUiContinuationData::load(const uint8_t *p, size_t n,
                                   const FieldUiManagerData &ui,
                                   std::string &e) {
  if (valid_)
    return fail(e, "House UI policy owner cannot be replaced");
  if (!p || !ui.valid() || n < 128 || n > 65536 ||
      std::memcmp(p, "ENCHUIC1", 8) || (word(p + 8) < 1 || word(p + 8) > 4) ||
      word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e0064 ||
      word(p + 28) != word(p+8) || word(p + 32) != 1 || word(p + 124))
    return fail(e, "House UI header/CRC/format/capability rejected");
  HouseUiContinuationData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  const auto expected = ui.identity();
  if (d.identity_.scene_id != expected.scene_id ||
      d.identity_.upstream_commit != expected.upstream_commit ||
      d.identity_.source_sha256 != expected.source_sha256 || !nonzero(d.ir_))
    return fail(e, "House UI actual independent source identity rejected");
  d.capability_=word(p+28);d.key_policy_=d.capability_>=2;
  Reader r{p, n};
  d.ui_ir_ = r.bytes<32>();
  d.script_ = r.text();
  if (!nonzero(d.ui_ir_) || d.script_ != ui.source_script())
    return fail(e, "House UI independent source owner rejected");
  auto count = r.u();
  if (count != (d.capability_>=4?18u:d.capability_==3?6u:d.key_policy_?5u:2u))
    return fail(e, "House UI complete source closure rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto path = r.text();
    auto h = r.bytes<32>();
    std::array<uint8_t, 32> old{};
    if (path.find("..") != path.npos ||
        path.find_first_of("\\:") != path.npos || !nonzero(h) ||
        !d.sources_.emplace(path, h).second ||
        (ui.source_hash(path, old) && old != h))
      return fail(e, "House UI source path/hash rejected");
  }
  auto source = d.sources_.find(d.script_);
  if (source == d.sources_.end() || source->second != d.identity_.source_sha256)
    return fail(e, "House UI owning script source rejected");
  count = r.u();
  if (count != 3)
    return fail(e, "House UI complete boolean field policy rejected");
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    HouseUiSourceField f;
    f.role = r.u();
    auto initial = r.u();
    f.initial = initial != 0;
    f.member = r.text();
    if (f.role != i + 1 || initial > 1 || !names.insert(f.member).second)
      return fail(e, "House UI source field schema rejected");
    d.fields_.push_back(std::move(f));
  }
  count = r.u();
  if (count != (d.capability_>=4?27u:d.capability_==3?18u:d.key_policy_?14u:10u))
    return fail(e, "House UI source method roster rejected");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseUiSourceMethod m;
    m.role = r.u();
    m.name = r.text();
    m.sha = r.bytes<32>();
    if (m.role != i + 1 || !names.insert(m.name).second || !nonzero(m.sha))
      return fail(e, "House UI source method proof rejected");
    d.methods_.push_back(std::move(m));
  }
  count = r.u();
  if (count != 3)
    return fail(e, "House UI actual signal roster rejected");
  names.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseUiSourceSignal s;
    s.role = r.u();
    s.arity = r.u();
    s.name = r.text();
    if (s.role != i + 1 || s.arity || !names.insert(s.name).second)
      return fail(e, "House UI native source signal arity rejected");
    d.signals_.push_back(std::move(s));
  }
  if(d.key_policy_){
    d.key_member_=r.text();d.key_default_=r.u();auto initial=r.u();
    d.key_open_=initial!=0;d.key_scene_=r.text();d.key_script_=r.text();
    d.enemy_member_=r.text();auto enemies=r.u();d.house_scene_=r.text();
    std::array<uint8_t,32> expected_key{};
    if(d.key_member_.empty()||d.enemy_member_.empty()||enemies!=0||d.sources_.count(d.house_scene_)!=1||d.key_default_!=0||initial!=0||
       !ui.source_hash(d.key_scene_,expected_key)||
       d.sources_.find(d.key_scene_)==d.sources_.end()||
       d.sources_.at(d.key_scene_)!=expected_key||
       d.sources_.find(d.key_script_)==d.sources_.end())
      return fail(e,"House key indicator original resource/closed state rejected");
  }
  if(d.capability_>=3){
    auto member=r.text();auto initial=r.u();auto dialogue=r.text();
    if(member.empty()||initial!=0||d.sources_.count(dialogue)!=1||!d.method(15)||!d.method(16)||!d.method(17)||!d.method(18))
      return fail(e,"House UI original stack/close source policy rejected");
    d.dialogue_policy_.stack_member=member;d.dialogue_policy_.dialogue_script=dialogue;
  }
  if(d.capability_>=4){
    auto &v=d.dialogue_policy_;const auto stack=v.stack_member,dialogue=v.dialogue_script;
    v.stack_member=r.text();v.dialogue_member=r.text();v.canvas_member=r.text();
    v.dialogue_scene=r.text();v.dialogue_script=r.text();v.abstract_script=r.text();
    v.add_child_method=r.text();v.close_method=r.text();v.queue_free_method=r.text();
    v.actors_member=r.text();v.queued_battle_member=r.text();v.set_respawn_member=r.text();
    v.input_sound_node=r.text();v.open_sound_name=r.text();v.open_sound_source=r.text();
    if(v.input_sound_node.empty()||v.input_sound_node.front()=='/'||v.input_sound_node.find(':')!=std::string::npos||v.input_sound_node.find("..")!=std::string::npos||
       v.open_sound_name.empty()||v.open_sound_source.empty()||v.open_sound_source.front()=='/'||v.open_sound_source.find(':')!=std::string::npos||v.open_sound_source.find("..")!=std::string::npos)
      return fail(e,"House UI source InputSound/open sound binding rejected");
    v.actor_count=r.u();auto queued=r.u(),respawn=r.u();
    v.queued_battle=queued!=0;v.set_respawn=respawn!=0;
    if(v.actor_count||queued>1||respawn>1||v.actors_member.empty()||v.queued_battle_member.empty()||v.set_respawn_member.empty()||
       std::set<std::string>{v.actors_member,v.queued_battle_member,v.set_respawn_member}.size()!=3)
      return fail(e,"House UI source dialogue observation declarations rejected");
    std::set<std::string> members{v.stack_member,v.dialogue_member,v.canvas_member};
    if(v.stack_member!=stack||v.dialogue_script!=dialogue||members.size()!=3||members.count("")||
       v.add_child_method.empty()||v.close_method.empty()||v.queue_free_method.empty()||
       v.close_method==v.queue_free_method||d.sources_.count(v.dialogue_scene)!=1||
       d.sources_.count(v.dialogue_script)!=1||d.sources_.count(v.abstract_script)!=1||
       !d.method(19)||!d.method(20)||!d.method(21))
      return fail(e,"House UI dialogue stack/scene/native-method policy rejected");
    auto &business=d.business_;auto widget_count=r.u();
    if(widget_count!=3)return fail(e,"House UI closed-widget roster rejected");
    for(uint32_t i=0;i<widget_count;++i){
      HouseUiClosedWidget w;w.role=r.u();auto opened=r.u();w.initial_open=opened!=0;
      w.scene=r.text();w.script=r.text();w.member=r.text();w.close_method=r.text();
      if(w.role!=i+1||opened||d.sources_.count(w.scene)!=1||d.sources_.count(w.script)!=1||w.member.empty()||w.close_method.empty())
        return fail(e,"House UI closed-widget source declaration rejected");
      business.widgets.push_back(std::move(w));
    }
    business.timer_member=r.text();business.fade_scene=r.text();business.fade_script=r.text();business.cut_signal=r.text();
    business.global_script=r.text();business.phone_member=r.text();business.end_signal=r.text();
    business.close_sound_source=r.text();business.close_sound_name=r.text();auto null_timer=r.u();business.initial_timer_null=null_timer!=0;
    business.fade_type=r.u();business.transition=r.u();business.ease=r.u();business.restore_target=r.real();business.restore_duration=r.real();business.spin_stop_unit_offset=r.real();
    if(null_timer!=1||business.fade_type!=1||business.transition!=1||business.ease!=1||business.restore_duration<=0||business.restore_duration>120||
       business.restore_target<0||business.restore_target>1||business.spin_stop_unit_offset!=0||d.sources_.count(business.fade_scene)!=1||d.sources_.count(business.fade_script)!=1||
       d.sources_.count(business.global_script)!=1||business.phone_member.empty()||business.timer_member.empty()||business.cut_signal.empty()||business.end_signal.empty()||
       business.close_sound_source.empty()||business.close_sound_source.find("..")!=std::string::npos||business.close_sound_source.find(':')!=std::string::npos||
       business.close_sound_source.front()=='/'||business.close_sound_name.empty())
      return fail(e,"House UI actual closed business/tween source bindings rejected");
    for(uint32_t i=22;i<=27;++i)if(!d.method(i))return fail(e,"House UI business method source proof absent");
    business.effect_type=r.u();business.effect_ease=r.u();const auto cubic=r.u(),loop=r.u();
    business.curve_cubic=cubic!=0;business.curve_loop=loop!=0;
    business.effect_target=r.real();business.effect_duration=r.real();business.spin_speed=r.real();business.spin_length=r.real();
    business.spin_from=r.real();business.spin_to=r.real();business.curve_length=float(r.real());business.curve_interval=float(r.real());
    business.screen_size={float(r.real()),float(r.real())};business.initial_path_position={float(r.real()),float(r.real())};
    for(auto &color:business.effect_color)color=float(r.real());
    const auto point_count=r.u();
    if(point_count<2||point_count>4096)return fail(e,"House UI source Curve2D point count rejected");
    for(uint32_t i=0;i<point_count;++i)business.curve_points.push_back({r.real32(),r.real32()});
    const auto curve_input=r.bytes<32>();std::set<std::string> engine;
    for(uint32_t i=0;i<2;++i){const auto path=r.text();const auto proof=r.bytes<32>();
      if(path.empty()||!engine.insert(path).second||!nonzero(proof))return fail(e,"House UI source curve engine proof rejected");}
    if(business.effect_type!=1||business.effect_ease!=2||cubic!=1||loop!=1||!nonzero(curve_input)||
       business.effect_target<0||business.effect_target>1||business.effect_duration<=0||business.effect_duration>120||
       business.spin_speed<=0||business.spin_speed>1024||business.spin_length<=0||business.spin_length>120||
       std::abs(business.spin_from)>1e6||std::abs(business.spin_to)>1e6||business.curve_length<=0||business.curve_length>1e6||
       business.curve_interval<=0||business.curve_interval>business.curve_length||
       business.screen_size.x<=0||business.screen_size.y<=0||business.screen_size.x>8192||business.screen_size.y>8192||
       !std::isfinite(business.initial_path_position.x)||!std::isfinite(business.initial_path_position.y)||
       std::abs(business.initial_path_position.x)>8192||std::abs(business.initial_path_position.y)>8192||
       std::any_of(business.curve_points.begin(),business.curve_points.end(),[](Vec2 v){return std::abs(v.x)>8192||std::abs(v.y)>8192;})||
       business.curve_points.front().x!=business.curve_points.back().x||business.curve_points.front().y!=business.curve_points.back().y||
       business.curve_length<=float(point_count-2)*business.curve_interval||business.curve_length>float(point_count-1)*business.curve_interval||
       std::any_of(business.effect_color.begin(),business.effect_color.end(),[](float v){return v<0||v>1;}))
      return fail(e,"House UI source positive Fade/closed Curve2D policy rejected");
    for(const auto &path:{v.dialogue_scene,v.dialogue_script,v.abstract_script}){
      std::array<uint8_t,32> source{};
      if(ui.source_hash(path,source)&&source!=d.sources_.at(path))
        return fail(e,"House UI dialogue closure differs from actual UI source");
    }
  }
  if (!r.ok || r.at != n)
    return fail(e, "House UI truncated/trailing resource rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool HouseUiContinuationData::load_file(const char *path,
                                        const FieldUiManagerData &ui,
                                        std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return fail(e, "Cannot open House UI policy");
  bool ok = std::fseek(f, 0, SEEK_END) == 0;
  auto n = ok ? std::ftell(f) : -1;
  ok = ok && n >= 128 && n <= 65536 && std::fseek(f, 0, SEEK_SET) == 0;
  std::vector<uint8_t> b(ok ? size_t(n) : 0);
  ok = ok && std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), ui, e)
            : fail(e, "House UI policy read rejected");
}
} // namespace encore::upstream
