#include "encore/field_canvas_art.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool path(const std::string &s) {
  return !s.empty() && s.size() < 2048 && s[0] != '/' &&
         s.find(':') == std::string::npos &&
         s.find('\\') == std::string::npos && s.find("..") == std::string::npos;
}
bool resource_path(const std::string&s){
 auto at=s.find("::");if(at==std::string::npos)return path(s);
 return path(s.substr(0,at))&&at+2<s.size()&&std::all_of(s.begin()+at+2,s.end(),[](char c){return c>='0'&&c<='9';});
}
bool nz(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t b) { return b != 0; });
}
struct R {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = u32(p + at);
    at += 4;
    return v;
  }
  float f() {
    auto bits = u();
    float v;
    std::memcpy(&v, &bits, 4);
    if (!std::isfinite(v) || std::abs(v) > 1000000)
      ok = false;
    return v;
  }
  bool b() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v == 1;
  }
  std::string t() {
    auto k = u();
    if (k > 8192 || at > n || k > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), k);
    at += k;
    size_t i = 0;
    uint32_t cp;
    while (i < s.size())
      if (!encore::utf8_next(s, i, cp) || !cp || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  void hash(std::array<uint8_t, 32> &h) {
    if (at > n || n - at < 32) {
      ok = false;
      return;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
  }
  FieldCanvasAsset asset() {
    FieldCanvasAsset a;
    a.id = u();
    a.width = u();
    a.height = u();
    a.bytes = u();
    a.crc = u();
    a.source = t();
    a.path = t();
    hash(a.source_sha);
    hash(a.output_sha);
    return a;
  }
};
} // namespace
const FieldCanvasRecord *FieldCanvasArtData::record(uint32_t id) const {
  auto it = record_index_.find(id);
  return it == record_index_.end() ? nullptr : &records_[it->second];
}
const FieldCanvasAsset *FieldCanvasArtData::texture(uint32_t id) const {
  auto it = texture_index_.find(id);
  return it == texture_index_.end() ? nullptr : &textures_[it->second];
}
const FieldCanvasControlBoundary*FieldCanvasArtData::control_boundary(uint32_t id)const {
 for(const auto&b:controls_)if(b.id==id)return &b;
 return nullptr;
}
bool FieldCanvasArtData::source_hash(std::string_view p,
                                     std::array<uint8_t, 32> &h) const {
  auto it = sources_.find(std::string(p));
  if (it == sources_.end())
    return false;
  h = it->second;
  return true;
}
bool FieldCanvasArtData::load_file(const char *p, const FieldIdentity &identity,
                                   std::string &e) {
  if (!p)
    return fail(e, "Canvas pack path missing");
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return fail(e, "Canvas pack unavailable");
  f.seekg(0, std::ios::end);
  auto size = f.tellg();
  if (size < 128 || size > 16 * 1024 * 1024)
    return fail(e, "Canvas pack bounded extent rejected");
  std::vector<uint8_t> b(size_t(size), 0);
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), size))
    return fail(e, "Canvas pack incomplete");
  return load(b.data(), b.size(), identity, e);
}
bool FieldCanvasArtData::load(const uint8_t *p, size_t n,
                              const FieldIdentity &identity, std::string &e) {
  const uint32_t format=p&&n>=128?u32(p+8):0;
  if (!p || n < 128 || n > 16 * 1024 * 1024 || std::memcmp(p, "ENCFCA01", 8) ||
      (format!=1&&format!=2) || u32(p + 12) != 128 || u32(p + 16) != n ||
      u32(p + 20) != crc(p + 128, n - 128) || u32(p + 24) != 0x454e0040 ||
      u32(p + 28) != format || u32(p + 32) != 1 || u32(p + 124))
    return fail(e, "Canvas format/family/capability/rules/CRC rejected");
  FieldCanvasArtData d;
  d.format_=format;
  d.identity_.scene_id = u32(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (d.identity_.scene_id != identity.scene_id ||
      d.identity_.upstream_commit != identity.upstream_commit ||
      d.identity_.source_sha256 != identity.source_sha256 ||
      !identity.scene_id || !nz(identity.source_sha256)||!nz(d.ir_))
    return fail(e, "Canvas checked scene identity mismatch");
  R r{p, n};
  d.scene_ = r.t();
  d.y_epsilon_ = r.f();
  d.alpha_prune_ = r.f();
  d.pixel_snap_ = r.b();
  r.hash(d.tree_ir_);
  if (!path(d.scene_) || !nz(d.tree_ir_) || d.y_epsilon_ <= 0 ||
      d.y_epsilon_ >= .01f || d.alpha_prune_ < 0 || d.alpha_prune_ >= 1)
    return fail(e, "Canvas source engine parameters rejected");
  auto count = r.u();
  if (!count || count > 65536)
    return fail(e, "Canvas texture count rejected");
  std::set<std::string> paths, original;std::set<uint32_t>page_ids;
  for (uint32_t i = 0; i < count; ++i) {
    FieldCanvasAsset a;
    if(format==1)a=r.asset();else{
     a.id=r.u();a.width=r.u();a.height=r.u();a.source=r.t();r.hash(a.source_sha);
     const auto pages=r.u();if(!pages||pages>4096)return fail(e,"Canvas virtual page count rejected");
     uint64_t area=0;
     for(uint32_t j=0;j<pages;++j){
      FieldCanvasPage q;q.id=r.u();q.x=r.u();q.y=r.u();q.width=r.u();q.height=r.u();q.bytes=r.u();q.crc=r.u();q.path=r.t();r.hash(q.output_sha);r.hash(q.crop_png_sha);
      if(!r.ok||!q.id||!page_ids.insert(q.id).second||!q.width||!q.height||q.width>1024||q.height>1024||q.x>a.width||q.y>a.height||q.width>a.width-q.x||q.height>a.height-q.y||!q.bytes||q.bytes>16*1024*1024||!path(q.path)||q.path.rfind("graphics/",0)!=0||!paths.insert(q.path).second||!nz(q.output_sha)||!nz(q.crop_png_sha))return fail(e,"Canvas source page identity/crop/bytes rejected");
      for(const auto&old:a.pages)if(uint64_t(q.x)<uint64_t(old.x)+old.width&&uint64_t(old.x)<uint64_t(q.x)+q.width&&uint64_t(q.y)<uint64_t(old.y)+old.height&&uint64_t(old.y)<uint64_t(q.y)+q.height)return fail(e,"Canvas source page overlap rejected");
      area+=uint64_t(q.width)*q.height;a.pages.push_back(std::move(q));
     }
     if(area!=uint64_t(a.width)*a.height)return fail(e,"Canvas complete source page coverage rejected");
    }
    if (!r.ok || !a.id || !a.width || !a.height || a.width > (format==1?1024u:65535u) ||
        a.height > (format==1?1024u:65535u) ||
        (format==1&&(!a.bytes||a.bytes>16*1024*1024||!path(a.path)||a.path.rfind("graphics/",0)!=0||!nz(a.output_sha)||!paths.insert(a.path).second)) ||
        !path(a.source) || !nz(a.source_sha) || !original.insert(a.source).second ||
        !d.texture_index_.emplace(a.id, d.textures_.size()).second)
      return fail(e, "Canvas texture identity/path/extent rejected");
    if(format==1){FieldCanvasPage page;page.id=a.id;page.width=a.width;page.height=a.height;page.bytes=a.bytes;page.crc=a.crc;page.path=a.path;page.output_sha=a.output_sha;a.pages.push_back(std::move(page));}
    d.textures_.push_back(std::move(a));
  }
  d.program_ = r.asset();
  auto &a = d.program_;
  if (a.id || a.width || a.height || !a.bytes || a.bytes > 1024 * 1024 ||
      a.bytes % 4 || !path(a.source) || !path(a.path) ||
      a.path.rfind("shaders/", 0) != 0 || !nz(a.source_sha) ||
      !nz(a.output_sha))
    return fail(e, "Canvas PICA metadata rejected");
  count = r.u();
  if (!count || count > 65536)
    return fail(e, "Canvas command count rejected");
  std::set<std::string> nodes;
  for (uint32_t i = 0; i < count; ++i) {
    FieldCanvasRecord v;
    v.id = r.u();
    v.kind = r.u();
    v.flags = r.u();
    v.texture = r.u();
    v.hframes = r.u();
    v.vframes = r.u();
    v.frame = r.u();
    v.centered = r.b();
    v.flip_h = r.b();
    v.flip_v = r.b();
    v.stretch = r.u();
    v.owner = FieldCanvasOwner(r.u());
    v.owner_id = r.u();
    v.shader = FieldCanvasShader(r.u());
    v.offset = {r.f(), r.f()};
    v.size = {r.f(), r.f()};
    v.node = r.t();
    v.owner_script = r.t();
    v.shader_source = r.t();
    r.hash(v.owner_sha);
    if(format==2){
     for(auto&c:v.color)c=r.f();
     v.speed_scale=r.f();v.ready_min=r.f();v.ready_max=r.f();v.playing=r.b();v.animation=r.t();v.ready_method=r.t();const auto animations=r.u();
     if(animations>128)return fail(e,"Canvas animation count rejected");
     std::set<std::string>names;
     for(uint32_t j=0;j<animations;++j){
      FieldCanvasAnimation a;a.name=r.t();a.speed=r.f();a.loop=r.b();const auto frames=r.u();
      if(a.name.empty()||!names.insert(a.name).second||a.speed<0||!frames||frames>65536)return fail(e,"Canvas native animation schema rejected");
      for(uint32_t k=0;k<frames;++k){FieldCanvasFrame f;f.texture=r.u();for(auto&x:f.region)x=r.f();for(auto&x:f.margin)x=r.f();f.filter_clip=r.b();f.atlas_source=r.t();auto*t=d.texture(f.texture);
       if(!t||f.region[0]<0||f.region[1]<0||f.region[2]<=0||f.region[3]<=0||f.region[0]+f.region[2]>t->width||f.region[1]+f.region[3]>t->height||f.filter_clip||std::any_of(f.margin.begin(),f.margin.end(),[](float x){return x!=0;}))return fail(e,"Canvas native animation texture/atlas capability rejected");
       a.frames.push_back(std::move(f));}
      v.animations.push_back(std::move(a));
     }
     v.material.present=r.b();if(v.material.present){
      auto&m=v.material;m.local_to_scene=r.b();m.priority=int32_t(r.u());m.source=r.t();r.hash(m.shader_code_sha);auto uniforms=r.u();if(!resource_path(m.source)||!nz(m.shader_code_sha)||m.priority||!uniforms||uniforms>256)return fail(e,"Canvas typed material origin/priority/uniform count rejected");std::set<std::string>names;
      for(uint32_t j=0;j<uniforms;++j){FieldCanvasUniform u;u.name=r.t();u.type=FieldCanvasUniformType(r.u());u.proof=FieldCanvasUniformProof(r.u());u.initialized=r.b();for(auto&x:u.value)x=r.f();u.integer=int32_t(r.u());
       if(u.name.empty()||!names.insert(u.name).second||uint32_t(u.type)>5||uint32_t(u.proof)>2||u.initialized==(u.proof==FieldCanvasUniformProof::Uninitialized)||(u.type==FieldCanvasUniformType::Bool&&(u.integer<0||u.integer>1))||(u.type==FieldCanvasUniformType::Sampler2D&&u.initialized))return fail(e,"Canvas typed material uniform schema rejected");
       m.uniforms.push_back(std::move(u));}
     }
    }
    auto tex = d.texture(v.texture);
    if (!r.ok || !v.id || v.kind > (format==1?1u:3u) || !(v.flags & 1) || v.flags >= 1024 ||
        !v.hframes || !v.vframes || v.hframes > 1024 || v.vframes > 1024 ||
        (v.kind!=2&&v.frame >= v.hframes * v.vframes) || v.size.x < 0 || v.size.y < 0 ||
        (v.texture && !tex) || !path(v.node) || !nodes.insert(v.node).second ||
        !d.record_index_.emplace(v.id, d.records_.size()).second ||
        uint32_t(v.owner) > uint32_t(format==1?FieldCanvasOwner::Landmark:FieldCanvasOwner::Sparkles) ||
        uint32_t(v.shader) > uint32_t(FieldCanvasShader::Flash) ||
        (v.kind == 0 && v.stretch) ||
        (v.kind == 1 && (v.hframes != 1 || v.vframes != 1 || v.frame ||
                         v.centered || (v.stretch != 2 && v.stretch != 3))))
      return fail(e, "Canvas command schema/texture/frame/stretch rejected");
    if(format==2){
     if(v.material.present!=(v.shader!=FieldCanvasShader::Default))return fail(e,"Canvas typed material/shader ownership rejected");
     if(v.kind==2){const FieldCanvasAnimation*current=nullptr;for(const auto&a:v.animations)if(a.name==v.animation)current=&a;
      if(v.owner!=FieldCanvasOwner::Sparkles||v.owner_id!=v.id||v.texture||v.hframes!=1||v.vframes!=1||v.stretch||!current||v.frame>=current->frames.size()||v.speed_scale<0||v.ready_method.empty()||v.ready_min<0||v.ready_min>=v.ready_max)return fail(e,"Canvas native AnimatedSprite source schema rejected");
     }else if(!v.animations.empty()||!v.animation.empty()||!v.ready_method.empty()||v.playing||v.speed_scale||v.ready_min||v.ready_max)return fail(e,"Canvas foreign native animation fields rejected");
     if(v.kind==3){if(v.owner!=FieldCanvasOwner::Native||v.texture||v.hframes!=1||v.vframes!=1||v.frame||v.stretch||v.centered||v.flip_h||v.flip_v||v.size.x<=0||v.size.y<=0)return fail(e,"Canvas native ColorRect source schema rejected");for(auto c:v.color)if(c<0||c>1)return fail(e,"Canvas ColorRect source color rejected");}
     else if(std::any_of(v.color.begin(),v.color.end(),[](float x){return x!=0;}))return fail(e,"Canvas foreign native color fields rejected");
    }
    if ((v.owner == FieldCanvasOwner::Native &&
         (v.owner_id || !v.owner_script.empty() || nz(v.owner_sha) ||
          v.shader != FieldCanvasShader::Default)) ||
        (v.owner != FieldCanvasOwner::Native &&
         (!v.owner_id || v.owner_script.empty() || !nz(v.owner_sha))) ||
        (v.shader == FieldCanvasShader::Default && !v.shader_source.empty()) ||
        (v.shader != FieldCanvasShader::Default && v.shader_source.empty()) ||
        (v.shader == FieldCanvasShader::Outline &&
         v.owner != FieldCanvasOwner::Jump) ||
        (v.shader == FieldCanvasShader::Distortion &&
         v.owner != FieldCanvasOwner::Melody) ||
        (v.kind == 1 && v.stretch == 2 && v.owner != FieldCanvasOwner::Melody))
      return fail(e, "Canvas shader/typed owner rejected");
    d.records_.push_back(std::move(v));
  }
  if(format==2){
   count=r.u();if(!count||count>65536)return fail(e,"Canvas native Control boundary count rejected");std::set<uint32_t>ids;
   for(uint32_t i=0;i<count;++i){FieldCanvasControlBoundary b;b.id=r.u();b.flags=r.u();b.owner_id=r.u();b.node=r.t();b.native_class=r.t();b.owner_script=r.t();r.hash(b.owner_sha);r.hash(b.native_properties_sha);b.text=r.t();b.font_source=r.t();r.hash(b.font_source_sha);b.size={r.f(),r.f()};b.align=r.u();b.valign=r.u();b.percent_visible=r.f();b.autowrap=r.b();b.clip_text=r.b();
    if(!r.ok||!b.id||!ids.insert(b.id).second||d.record(b.id)||!(b.flags&1)||b.flags>=1024||!b.owner_id||!path(b.node)||!path(b.owner_script)||!nz(b.owner_sha)||!nz(b.native_properties_sha)||b.size.x<0||b.size.y<0||b.align>3||b.valign>3||b.percent_visible<0||b.percent_visible>1||(b.native_class!="Label"&&b.native_class!="HBoxContainer"&&b.native_class!="Control")||(b.native_class=="Label"&&(!path(b.font_source)||!nz(b.font_source_sha)))||(b.native_class!="Label"&&(!b.text.empty()||!b.font_source.empty()||nz(b.font_source_sha)||b.align||b.valign||b.percent_visible||b.autowrap||b.clip_text)))return fail(e,"Canvas native Control boundary schema rejected");
    d.controls_.push_back(std::move(b));}
  }
  count = r.u();
  if (!count || count > 100000)
    return fail(e, "Canvas source inventory count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto s = r.t();
    std::array<uint8_t, 32> h{};
    r.hash(h);
    if (!path(s) || !nz(h) || !d.sources_.emplace(s, h).second)
      return fail(e, "Canvas duplicate/source inventory rejected");
  }
  if (!r.ok || r.at != n)
    return fail(e, "Canvas truncated/trailing binary rejected");
  std::array<uint8_t, 32> hash{};
  if (!d.source_hash(d.scene_, hash) || hash != identity.source_sha256)
    return fail(e, "Canvas scene inventory source rejected");
  for (const auto &t : d.textures_)
    if (!d.source_hash(t.source, hash) || hash != t.source_sha ||
        !d.source_hash(t.source + ".import", hash))
      return fail(e, "Canvas texture source/import missing");
  for (const auto &v : d.records_) {
    if (v.owner != FieldCanvasOwner::Native) {
      auto p = v.owner_script.substr(0, v.owner_script.find("::"));
      if (!d.source_hash(p, hash))
        return fail(e, "Canvas owner source missing");
    }
    if (v.shader != FieldCanvasShader::Default) {
      auto p = v.shader_source.substr(0, v.shader_source.find("::"));
      if (!d.source_hash(p, hash))
        return fail(e, "Canvas shader source missing");
    }
    if(v.material.present&&!d.source_hash(v.material.source.substr(0,v.material.source.find("::")),hash))return fail(e,"Canvas material origin source missing");
    for(const auto&a:v.animations)for(const auto&f:a.frames)if(!f.atlas_source.empty()&&!d.source_hash(f.atlas_source.substr(0,f.atlas_source.find("::")),hash))return fail(e,"Canvas AtlasTexture source missing");
  }
  for(const auto&b:d.controls_)if(!d.source_hash(b.owner_script.substr(0,b.owner_script.find("::")),hash)||(b.native_class=="Label"&&(!d.source_hash(b.font_source,hash)||hash!=b.font_source_sha)))return fail(e,"Canvas native Control source/font proof missing");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
