#include "house_return_gui_native.hpp"
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
FieldTransform mul(const FieldTransform&a,const FieldTransform&b){
 auto linear=[&](Vec2 p){return Vec2{a[0].x*p.x+a[1].x*p.y,a[0].y*p.x+a[1].y*p.y};};
 auto t=linear(b[2]);t.x+=a[2].x;t.y+=a[2].y;return{linear(b[0]),linear(b[1]),t};
}
bool invert(const FieldTransform&t,FieldTransform&out){
 const float det=t[0].x*t[1].y-t[0].y*t[1].x;
 if(det==0)return false;
 const float d=1.f/det;out[0]={t[1].y*d,-t[0].y*d};out[1]={-t[1].x*d,t[0].x*d};
 out[2]={-out[0].x*t[2].x-out[1].x*t[2].y,-out[0].y*t[2].x-out[1].y*t[2].y};
 for(const auto&v:out)if(!std::isfinite(v.x)||!std::isfinite(v.y))return false;return true;
}
Vec2 transform_point(const FieldTransform&t,Vec2 p){return{t[0].x*p.x+t[1].x*p.y+t[2].x,t[0].y*p.x+t[1].y*p.y+t[2].y};}
bool point(Vec2 p,Vec2 size){return p.x>=0&&p.y>=0&&p.x<size.x&&p.y<size.y;}
bool control_class(std::string_view c){return c=="Control"||c=="HBoxContainer"||c=="Label"||c=="TextureRect"||c=="ColorRect"||c=="ReferenceRect";}
}
bool HouseReturnGuiNative::prepare(HouseReturnControlsNative&c,FieldObjectSignals&signals,
 PodunkNativeRoot&r,std::string&e){
 if(controls_||!c.control_data()||!c.control_data()->valid()||!c.canvas_tree()||!c.canvas_registry()||c.native_root()!=&r||signals.registry()!=c.canvas_registry()||r.kernel_object()!=c.canvas_registry()->kernel()||!r.viewport_object()||(c.canvas_tree()->object_domain()&&c.canvas_tree()->object_domain()!=r.kernel_object()))return fail(e,"House GUI actual native Control/Tree/Viewport/ObjectDB rejected");
 const auto&eng=c.control_data()->engine_sources();for(auto name:{"scene/main/viewport.cpp","scene/gui/control.cpp","core/math/rect2.h"})if(!eng.count(name))return fail(e,"House GUI native engine source proof absent");
 controls_=&c;tree_=const_cast<FieldNodeTreeRuntime*>(c.canvas_tree());registry_=const_cast<FieldGlobalRegistry*>(c.canvas_registry());signals_=&signals;root_=&r;e.clear();return true;
}
bool HouseReturnGuiNative::actual(FieldObjectId id,HouseControlNativeState&out,std::string&e)const{
 if(!controls_||registry_->poisoned()||tree_->object_domain()!=root_->kernel_object()||!controls_->snapshot(id,out,e)||out.object!=id||!out.source||!out.constructed||!out.bound||registry_->tree_owner(id).get()!=tree_||tree_->source_object(out.source->id)!=id)return fail(e,"House GUI actual retained native Control rejected");return true;
}
bool HouseReturnGuiNative::enter_control(FieldObjectId id,std::string&e){
 HouseControlNativeState state;if(!actual(id,state,e)||registered_.count(id)||!state.entered||!tree_->state(id)->inside||!root_->viewport().inside)return fail(e,"House GUI registration requires actual native Enter");
 Registration record;record.source=state.source->id;auto*n=tree_->state(id);record.subwindow=bool(n->flags&4);
 if(!record.subwindow){bool ancestor=false;auto parent=n->parent;std::set<FieldObjectId>seen;
  while(parent){auto*p=tree_->state(parent);auto*d=tree_->descriptor(parent);if(!p||!d||!seen.insert(parent).second)return fail(e,"House GUI actual native parent chain rejected");
   if(!(p->flags&1))break;if(p->flags&4){record.subwindow=true;break;}
   if(control_class(d->native_class)){HouseControlNativeState own;if(!actual(parent,own,e))return false;ancestor=true;break;}parent=p->parent;
  }
  record.root=!ancestor&&!record.subwindow;
 }
 registered_.emplace(id,record);if(record.subwindow)subwindows_.push_back(id);else if(record.root)roots_.push_back(id);e.clear();return true;
}
bool HouseReturnGuiNative::exit_control(FieldObjectId id,std::string&e){
 auto i=registered_.find(id);HouseControlNativeState state;
 if(i==registered_.end()||!actual(id,state,e)||state.entered)return fail(e,"House GUI unregister requires actual native Exit");
 for(auto*v:{&roots_,&subwindows_})v->erase(std::remove(v->begin(),v->end(),id),v->end());registered_.erase(i);e.clear();return true;
}
bool HouseReturnGuiNative::live_registration(FieldObjectId id,std::string&e)const{
 auto i=registered_.find(id);HouseControlNativeState state;if(i==registered_.end()||!actual(id,state,e)||!state.entered||!tree_->state(id)->inside||i->second.source!=state.source->id)return fail(e,"House GUI actual live native registration absent");e.clear();return true;
}
bool HouseReturnGuiNative::retired_registration(FieldObjectId id,std::string&e)const{
 if(!controls_||!controls_->owns(id)||registered_.count(id)||
    std::find(roots_.begin(),roots_.end(),id)!=roots_.end()||
    std::find(subwindows_.begin(),subwindows_.end(),id)!=subwindows_.end())
  return fail(e,"House GUI native deletion lacks actual unregister receipt");
 e.clear();return true;
}
bool HouseReturnGuiNative::active(std::string&e)const{
 if(!controls_||!root_->viewport().inside||root_->viewport().failed||!root_->viewport().active||root_->viewport().current_scene!=tree_->root()||!registry_->object_exists(tree_->root()))return fail(e,"House GUI actual active current SceneTree branch absent");
 // Any newly added/dynamic Control must be owned and registered by its real
 // native owner. An incomplete branch cannot silently stop occluding input.
 std::vector<FieldObjectId>stack{tree_->root()};std::set<FieldObjectId>seen;
 while(!stack.empty()){auto id=stack.back();stack.pop_back();auto*s=tree_->state(id);auto*d=tree_->descriptor(id);if(!s||!d||!s->alive||!s->inside||!seen.insert(id).second)return fail(e,"House GUI active native tree closure rejected");
  if(control_class(d->native_class)&&!live_registration(id,e))return false;
  for(auto child:s->children)stack.push_back(child);
 }
 e.clear();return true;
}
bool HouseReturnGuiNative::ordered(std::vector<FieldObjectId>&out,std::string&e)const{
 std::vector<FieldObjectId>order,stack{tree_->root()};std::set<FieldObjectId>seen;
 while(!stack.empty()){auto id=stack.back();stack.pop_back();auto*s=tree_->state(id);if(!s||!s->alive||!seen.insert(id).second)return fail(e,"House GUI actual source ordering rejected");order.push_back(id);for(auto i=s->children.rbegin();i!=s->children.rend();++i)stack.push_back(*i);}
 out=std::move(order);return true;
}
bool HouseReturnGuiNative::find(FieldObjectId id,Vec2 position,HouseGuiPick&out,
 std::set<FieldObjectId>&seen,std::string&e){
 auto*s=tree_->state(id);auto*d=tree_->descriptor(id);if(!s||!d||!s->alive||!s->inside||!seen.insert(id).second)return fail(e,"House GUI native traversal rejected");
 if(!(s->flags&1)||!(s->flags&2))return true;
 FieldTransform world,inv;if(!tree_->world_transform(id,world,e))return false;
 auto matrix=mul(root_->viewport().canvas,world);const float det=matrix[0].x*matrix[1].y-matrix[0].y*matrix[1].x;
 if(det==0)return true;if(!invert(matrix,inv))return fail(e,"House GUI native affine transform is nonfinite");
 HouseControlNativeState c;const bool is_control=control_class(d->native_class);bool contains=false;
 if(is_control){if(!actual(id,c,e)||!live_registration(id,e))return false;contains=point(transform_point(inv,position),c.size);}
 // The source72 Controls have clips_input=false. The checked schema rejects
 // unsupported clipping/Label clipping rather than discarding children.
 for(auto i=s->children.rbegin();i!=s->children.rend();++i){auto*child=tree_->state(*i);if(!child)return fail(e,"House GUI native child absent");if(!(child->flags&1)||(child->flags&4))continue;if(!find(*i,position,out,seen,e))return false;if(out.object)return true;}
 if(is_control&&c.source->properties.at("mouse_filter").integer!=2&&contains){out.object=id;out.source=c.source->id;out.local_position=transform_point(inv,position);out.inverse=inv;}
 return true;
}
bool HouseReturnGuiNative::pick(Vec2 position,HouseGuiPick&out,std::string&e){
 if(!std::isfinite(position.x)||!std::isfinite(position.y)||!active(e))return fail(e,"House GUI source viewport position/current branch rejected");
 std::vector<FieldObjectId>order;if(!ordered(order,e))return false;
 auto sorted=[&](const std::vector<FieldObjectId>&list){auto v=list;std::sort(v.begin(),v.end(),[&](auto a,auto b){return std::find(order.begin(),order.end(),a)<std::find(order.begin(),order.end(),b);});return v;};
 HouseGuiPick result;for(const auto*list:{&subwindows_,&roots_}){auto nodes=sorted(*list);for(auto i=nodes.rbegin();i!=nodes.rend();++i){if(!tree_->visible_in_tree(*i))continue;std::set<FieldObjectId>seen;result.root=*i;if(!find(*i,position,result,seen,e))return false;if(result.object){out=std::move(result);e.clear();return true;}}}
 result.root=0;out=std::move(result);e.clear();return true;
}
bool HouseReturnGuiNative::action_input(const PlayerInputEvent&,bool,
 HouseGuiInputResult&out,std::string&e){
 if(!active(e))return false;
 for(const auto&v:registered_){HouseControlNativeState s;if(!actual(v.first,s,e)||s.source->properties.at("focus_mode").integer!=0)return fail(e,"House GUI source focus/navigation owner not implemented");}
 // Native Viewport's key/action branch has no focus receiver for these actual
 // source Controls. No gui_input signal or handled bit is manufactured.
 HouseGuiInputResult result;out=result;e.clear();return true;
}
bool HouseReturnGuiNative::pointer_input(FieldObjectId event,HouseGuiInputResult&,
 std::string&e){
 if(!active(e)||!event||!registry_->native_reference(event))return fail(e,"House GUI pointer input lacks an actual native InputEvent Reference owner");
 return fail(e,"House GUI native MouseButton/Motion payload, transformed Reference copy and propagation consumer not yet admitted");
}
bool HouseReturnGuiNative::declaration(FieldObjectId id,std::string_view signal,uint32_t&arity,std::string&e)const{
 HouseControlNativeState state;if(!actual(id,state,e))return false;
 if(signal=="gui_input")arity=1;else if(signal=="mouse_entered"||signal=="mouse_exited"||signal=="focus_entered"||signal=="focus_exited"||signal=="resized"||signal=="minimum_size_changed")arity=0;else return fail(e,"House GUI unknown native Control signal");e.clear();return true;
}
bool HouseReturnGuiNative::can_unbind(std::string&e)const{
 if(!registered_.empty()||!roots_.empty()||!subwindows_.empty())return fail(e,"House GUI unbind before actual Control Exit rejected");e.clear();return true;
}
}
