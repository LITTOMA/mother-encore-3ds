#include "encore/house_button_prompts.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return ~c;}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
struct Reader {const uint8_t*p;size_t n;bool ok=true;uint32_t integer(){if(n<4){ok=false;return 0;}auto v=u32(p);p+=4;n-=4;return v;}uint32_t count(uint32_t max){auto v=integer();if(v>max){ok=false;return 0;}return v;}float real(){auto u=integer();float f;std::memcpy(&f,&u,4);if(!std::isfinite(f))ok=false;return f;}Vec2 point(){return {real(),real()};}std::string text(){auto k=count(4096);if(k>n){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p),k);p+=k;n-=k;if(s.find('\0')!=std::string::npos)ok=false;return s;}};
bool ray(Vec2 origin,Vec2 direction,float length,Vec2 center,Vec2 extents,float&distance){float low=0,high=length;for(unsigned axis=0;axis<2;++axis){const float o=axis?origin.y:origin.x,d=axis?direction.y:direction.x,c=axis?center.y:center.x,e=axis?extents.y:extents.x;if(d==0){if(o<c-e||o>c+e)return false;continue;}float a=(c-e-o)/d,b=(c+e-o)/d;if(a>b)std::swap(a,b);low=std::max(low,a);high=std::min(high,b);if(low>high)return false;}distance=low;return true;}
}
bool HouseButtonPromptData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>1024*1024||std::memcmp(p,"ENCPRMPT",8)||(u32(p+8)!=1&&u32(p+8)!=2)||u32(p+12)!=n||u32(p+20)!=u32(p+8))return fail(e,"Button prompt schema/size/capability rejected");
 if(crc(p+24,n-24)!=u32(p+16))return fail(e,"Button prompt CRC rejected");
 HouseButtonPromptData d;Reader r{p+24,n-24};d.ray_origin=r.point();d.ray_length=r.real();d.collision_mask=r.integer();auto count=r.count(16);for(uint32_t i=0;i<count;++i)d.choice_masks.push_back(r.integer());
 count=r.count(16);for(uint32_t i=0;i<count;++i){HousePromptResource a{r.text(),r.integer(),r.integer()};if(a.path.empty()||a.path[0]=='/'||a.path.find("..")!=std::string::npos||a.path.find('\\')!=std::string::npos||a.path.find(':')!=std::string::npos||!a.width||!a.height||a.width>1024||a.height>1024)return fail(e,"Button prompt resource rejected");d.resources.push_back(a);}
 d.prompt_rect={r.real(),r.real(),r.real(),r.real()};d.color=r.integer();count=r.count(16);for(uint32_t i=0;i<count;++i){HousePromptPreview p;p.resource=r.integer();p.category=r.integer();p.position=r.point();p.offset=r.point();if(p.resource>=d.resources.size()||(p.category!=1&&p.category!=2))return fail(e,"Button prompt preview rejected");d.previews.push_back(p);}
 count=r.count(128);std::set<std::pair<uint32_t,uint32_t>>ids;for(uint32_t i=0;i<count;++i){HousePromptTarget t;const auto kind=r.integer();t.kind=HousePromptKind(kind);t.index=r.integer();t.category=r.integer();t.source_path=r.text();t.position=r.point();t.center=r.point();t.extents=r.point();t.offset=r.point();if(kind<1||kind>(u32(p+8)==2?4u:3u)||t.index>=128||(t.category!=(kind==1?2u:1u))||t.source_path.empty()||t.extents.x<=0||t.extents.y<=0||!ids.insert({kind,t.index}).second)return fail(e,"Button prompt target rejected");d.targets.push_back(t);}
 if(!r.ok||r.n||d.choice_masks.empty()||d.resources.empty()||d.targets.empty()||d.ray_length<=0||!d.collision_mask||d.prompt_rect.z!=d.resources[0].width||d.prompt_rect.w!=d.resources[0].height)return fail(e,"Button prompt payload rejected");
 for(auto mask:d.choice_masks)if(mask>3)return fail(e,"Button prompt choice rejected");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool HouseButtonPromptData::load_file(const char*path,std::string&e){std::vector<uint8_t>b;if(!path||!encore::read_file(path,b,1024*1024,e))return false;return load(b.data(),b.size(),e);}
bool HouseButtonPromptData::validate_bindings(HouseView h,PhoneView p,std::string&e,HouseInspectionView inspections)const{
 if(!valid_||!h.valid()||!p.valid()||!same(ray_origin,h.interaction().ray_origin)||ray_length!=h.interaction().ray_length||collision_mask!=h.interaction().collision_mask||targets.size()!=h.count(HouseSection::Npcs)+h.count(HouseSection::OpenableDoors)+p.count(PhoneSection::Objects)+inspections.count(HouseInspectionSection::Objects))return fail(e,"Button prompt checked bindings disagree");
 for(const auto&t:targets){Vec2 position,center,extents;std::string_view path;
  if(t.kind==HousePromptKind::Npc){if(t.index>=h.count(HouseSection::Npcs))return fail(e,"Button prompt NPC index rejected");auto n=h.npc(t.index);position=n.position;center=n.interact_center;extents=n.interact_extents;path=h.string(n.source_path);}
  else if(t.kind==HousePromptKind::Door){if(t.index>=h.count(HouseSection::OpenableDoors))return fail(e,"Button prompt door index rejected");auto n=h.openable_door(t.index);position=n.position;center=n.interact_center;extents=n.interact_extents;path=h.string(n.source_path);}
  else if(t.kind==HousePromptKind::Phone){if(t.index>=p.count(PhoneSection::Objects))return fail(e,"Button prompt phone index rejected");auto n=p.object(t.index);position=n.position;center=n.interact_center;extents=n.interact_extents;path=p.string(n.source_path);}
  else {if(!inspections.valid()||t.index>=inspections.count(HouseInspectionSection::Objects))return fail(e,"Button prompt inspection index rejected");auto n=inspections.object(t.index);position=n.position;center=n.interact_center;extents=n.interact_extents;path=inspections.string(n.source_path);if(!same(n.prompt_offset,t.offset))return fail(e,"Button prompt inspection offset disagrees");}
  if(!same(position,t.position)||!same(center,t.center)||!same(extents,t.extents)||path!=t.source_path)return fail(e,"Button prompt target geometry disagrees");
 }e.clear();return true;
}
bool evaluate_house_button_prompt(const HouseButtonPromptData&d,uint32_t choice,const HousePromptObservation&o,HousePromptPose&out,std::string&e){
 out={};if(!d.valid()||choice>=d.choice_masks.size()||o.targets.size()!=d.targets.size()||!finite(o.player)||!finite(o.direction)||std::isnan(o.occlusion_distance)||o.occlusion_distance<0)return fail(e,"Button prompt observation rejected");
 for(const auto&t:o.targets)if(!finite(t.position))return fail(e,"Button prompt observed position rejected");
 if(o.paused||o.crouching){e.clear();return true;}const float length=std::hypot(o.direction.x,o.direction.y);if(!std::isfinite(length)||length<=0)return fail(e,"Button prompt direction rejected");
 const Vec2 origin{o.player.x+d.ray_origin.x,o.player.y+d.ray_origin.y},direction{o.direction.x/length,o.direction.y/length};if(!finite(origin))return fail(e,"Button prompt ray origin rejected");float nearest=o.occlusion_distance;uint32_t selected=house_no_index;
 for(uint32_t i=0;i<d.targets.size();++i){const auto&t=d.targets[i];const auto&s=o.targets[i];if(!s.visible)continue;const Vec2 center{t.center.x+s.position.x-t.position.x,t.center.y+s.position.y-t.position.y};if(!finite(center))return fail(e,"Button prompt translated collider rejected");float distance=0;if(ray(origin,direction,d.ray_length,center,t.extents,distance)&&distance<nearest){nearest=distance;selected=i;}}
 if(selected!=house_no_index){const auto&t=d.targets[selected];const auto&s=o.targets[selected];if(s.enabled&&s.supported&&(d.choice_masks[choice]&t.category)){out.visible=true;out.target=selected;out.position={s.position.x+t.offset.x,s.position.y+t.offset.y};}}
 e.clear();return true;
}
bool evaluate_npc_button_prompt(const HouseButtonPromptData&d,uint32_t choice,Vec2 player,Vec2 direction,bool paused,bool crouching,Vec2 actor,Vec2 center,Vec2 extents,HousePromptPose&out,std::string&e){
 out={};bool found=false;Vec2 offset{};uint32_t category=0;
 if(!d.valid()||choice>=d.choice_masks.size()||!finite(player)||!finite(direction)||!finite(actor)||!finite(center)||!finite(extents)||extents.x<=0||extents.y<=0)return fail(e,"NPC prompt observation rejected");
 for(const auto&t:d.targets)if(t.kind==HousePromptKind::Npc){if(!found){offset=t.offset;category=t.category;found=true;}else if(!same(offset,t.offset)||category!=t.category)return fail(e,"NPC prompt offset/category disagree");}
 if(!found||!category)return fail(e,"NPC prompt contract is absent");
 if(paused||crouching){e.clear();return true;}
 const float length=std::hypot(direction.x,direction.y);if(!std::isfinite(length)||length<=0)return fail(e,"Button prompt direction rejected");
 const Vec2 origin{player.x+d.ray_origin.x,player.y+d.ray_origin.y},unit{direction.x/length,direction.y/length};if(!finite(origin))return fail(e,"Button prompt ray origin rejected");
 float distance=0;if(ray(origin,unit,d.ray_length,center,extents,distance)&&(d.choice_masks[choice]&category)){out.visible=true;out.position={actor.x+offset.x,actor.y+offset.y};}
 e.clear();return true;
}
}
