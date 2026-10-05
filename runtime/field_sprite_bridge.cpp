#include "encore/field_sprite_bridge.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
namespace encore::upstream {namespace {
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=1000000&&std::abs(v.y)<=1000000;}
bool named(const std::vector<std::string>&v,const std::string&s){return std::find(v.begin(),v.end(),s)!=v.end();}
}
bool FieldSpriteRuntime::fail(const char*e){error_=e;return false;}
bool FieldSpriteRuntime::initialize(const FieldSpriteData&data,FieldSpriteHost host,std::string&e){
 if(!data.valid()||!host.create_tree||!host.rebuild_tree||!host.publish||!host.sprite_changed||!host.travel||!host.blend||!host.time_scale||!host.resolve_sprite||!host.current_scene){e="Sprite runtime data/typed Host incomplete";return false;}
 data_=&data;host_=std::move(host);instances_.clear();last_ready_=0;had_ready_=false;error_.clear();e.clear();return true;
}
const FieldSpriteInstance*FieldSpriteRuntime::instance(uint32_t id)const{auto i=instances_.find(id);return i==instances_.end()?nullptr:&i->second;}
FieldSpriteInstance*FieldSpriteRuntime::character(uint32_t id){auto i=instances_.find(id);if(i==instances_.end()||!i->second.ready||data_->record(i->second.descriptor_id)->kind!=FieldSpriteKind::Character){fail("CharacterSprite operation before checked Ready");return nullptr;}return&i->second;}
bool FieldSpriteRuntime::publish(FieldSpriteInstance&s){return host_.publish(s.id,s,error_);}
bool FieldSpriteRuntime::create(uint32_t id){if(!data_||instances_.count(id))return fail("Sprite duplicate/uninitialized instance");const auto*d=data_->record(id);if(!d)return fail("Sprite unknown checked source identity");FieldSpriteInstance s;s.id=id;s.descriptor_id=id;s.texture_id=d->texture;s.sprite_id=d->sprite;s.columns=d->columns;s.rows=d->rows;s.frame=d->frame;s.offset=d->offset;s.visible=(d->flags&2)!=0;instances_.emplace(id,std::move(s));return true;}
bool FieldSpriteRuntime::scene(const FieldSpriteData*&out){out=nullptr;if(!host_.current_scene(out,error_))return false;if(!out||!out->valid()||out->source_pin()!=data_->source_pin())return fail("Fetcher current scene has no matching checked source capability");return true;}
bool FieldSpriteRuntime::ready(uint32_t id){auto i=instances_.find(id);if(i==instances_.end()||i->second.ready)return fail("Sprite Ready missing/duplicate");auto&s=i->second;const auto*d=data_->record(id);if(had_ready_&&d->ready_ordinal<=last_ready_)return fail("Sprite Ready violates actual source postorder");
 if(d->kind==FieldSpriteKind::Character){if(!host_.create_tree(id,*d,error_))return false;s.tree_created=true;s.ready=true;if(d->initial_animation){if(!set_animation(id,d->initial_animation,{})||!set_spritesheet(id))return false;}}
 else {bool exists=false;uint32_t target=0;if(!host_.resolve_sprite(id,*d,exists,target,error_))return false;if(exists&&(!target||target!=d->target_id))return fail("Fetcher resolved Sprite identity mismatch");if(!exists&&target)return fail("Fetcher null source target identity mismatch");s.target=target;const FieldSpriteData*current=nullptr;if(!scene(current))return false;s.ready=true;}
 last_ready_=d->ready_ordinal;had_ready_=true;return d->kind==FieldSpriteKind::Character?publish(s):true;
}
bool FieldSpriteRuntime::set_sprite(uint32_t id,uint32_t texture){auto*s=character(id);if(!s)return false;if(texture&&!data_->texture(texture))return fail("CharacterSprite unknown source texture");s->sprite_id=texture;return true;}
bool FieldSpriteRuntime::set_animation(uint32_t id,uint32_t aid,const std::vector<FieldSpriteConnection>&connections){auto*s=character(id);if(!s)return false;const auto*a=data_->animation(aid);if(!a)return fail("CharacterSprite unadmitted source animation");for(const auto&c:connections)if(c.mode>2||c.from.empty()||c.to.empty())return fail("CharacterSprite source connection mode rejected");
 auto tags=s->directional_tags;for(uint32_t i=0;i<a->motions.size();++i)tags.push_back({aid,i});std::vector<std::string>all,states;for(const auto&m:a->motions)if(m.directions.size()==1){all.push_back(m.name);states.push_back(m.name);}
 for(const auto&t:tags){const auto*old=data_->animation(t.animation_id);if(!old||t.motion_index>=old->motions.size())return fail("CharacterSprite retained tag source lost");const auto&name=old->motions[t.motion_index].name;if(!named(states,name)){all.push_back(name);states.push_back(name);}}
 if(!named(states,data_->fallback()))states.push_back(data_->fallback());
 if(s->frame>=a->columns*a->rows)return fail("CharacterSprite grid/frame setter requires source admission");
 if(!host_.rebuild_tree(id,*a,tags,connections,states,error_))return false;
 s->time_scale=1;s->animation_id=aid;s->columns=a->columns;s->rows=a->rows;s->directional_tags=std::move(tags);s->all_tags=std::move(all);s->states=std::move(states);s->tree_active=true;
 // No current source changes the grid so far that its existing frame becomes
 // invalid. Reject that unreviewed native Sprite setter case instead of inventing
 // a reset/clamp rule. Frame evaluation remains the existing animation owner.
 if(s->frame>=s->columns*s->rows)return fail("CharacterSprite grid/frame setter requires source admission");
 return travel(id,data_->fallback())&&publish(*s);
}
bool FieldSpriteRuntime::set_spritesheet(uint32_t id){auto*s=character(id);if(!s)return false;const auto*d=data_->record(id);
 if(s->sprite_id){const auto*t=data_->texture(s->sprite_id);const auto*a=data_->animation(s->animation_id);if(!t||!a||!s->rows)return fail("CharacterSprite texture/YAML binding incomplete");if(t->width%s->columns||t->height%s->rows)return fail("CharacterSprite source grid extent mismatch");s->texture_id=t->id;if(d->flags&1){const double auto_y=double(t->height)/double(s->rows*2);if(!std::isfinite(auto_y)||auto_y>1000000)return fail("CharacterSprite auto offset numeric bound");s->offset.y=-float(static_cast<int32_t>(auto_y));}
  s->offset.x+=a->offset.x;s->offset.y+=a->offset.y;if(!finite(s->offset))return fail("CharacterSprite accumulated source offset overflow");s->default_offset=s->offset;s->visible=true;
 }
 // Source emits even when the exported sprite string is empty. All admitted
 // nonempty resource strings exist in the checked immutable texture catalog.
 return publish(*s)&&host_.sprite_changed(id,*d,*s,error_);
}
bool FieldSpriteRuntime::set_sprite_offset(uint32_t id,Vec2 extra){auto*s=character(id);if(!s)return false;if(!finite(extra))return fail("CharacterSprite source extra offset rejected");s->offset={s->default_offset.x+extra.x,s->default_offset.y+extra.y};return finite(s->offset)?publish(*s):fail("CharacterSprite offset addition overflow");}
bool FieldSpriteRuntime::parent_setup(uint32_t id){auto*s=character(id);if(!s)return false;const auto*d=data_->record(id);if(!set_sprite(id,d->setup_texture)||!set_animation(id,d->setup_animation,d->connections)||!set_spritesheet(id)||!set_sprite_offset(id,d->extra_offset))return false;s->parent_setup=true;return true;}
bool FieldSpriteRuntime::travel(uint32_t id,const std::string&state){auto*s=character(id);if(!s)return false;if(!named(s->states,state))return true;/* explicit original has_node guard */if(!host_.travel(id,state,error_))return false;s->current_state=state;return true;}
bool FieldSpriteRuntime::blend_position(uint32_t id,Vec2 v){auto*s=character(id);if(!s)return false;if(!finite(v))return fail("CharacterSprite blend vector rejected");s->direction=v;return host_.blend(id,v,s->directional_tags,error_);}
bool FieldSpriteRuntime::set_time_scale(uint32_t id,float scale){auto*s=character(id);if(!s)return false;if(!std::isfinite(scale)||std::abs(scale)>1000000)return fail("CharacterSprite source time scale rejected");s->time_scale=scale;return host_.time_scale(id,scale,s->all_tags,error_);}
bool FieldSpriteRuntime::frame(uint32_t id,uint32_t f){auto*s=character(id);if(!s)return false;if(f>=s->columns*s->rows)return fail("CharacterSprite frame outside checked grid");s->frame=f;return publish(*s);}
bool FieldSpriteRuntime::tree_active(uint32_t id,bool active){auto*s=character(id);if(!s)return false;s->tree_active=active;return publish(*s);}
bool FieldSpriteRuntime::visibility(uint32_t id,bool visible){auto*s=character(id);if(!s)return false;s->visible=visible;return publish(*s);}
bool FieldSpriteRuntime::process_fetcher(uint32_t id){auto i=instances_.find(id);if(i==instances_.end()||!i->second.ready||data_->record(id)->kind!=FieldSpriteKind::Fetcher)return fail("Fetcher process before checked Ready");auto&s=i->second;const auto*d=data_->record(id);const FieldSpriteData*current=nullptr;if(!scene(current))return false;
 if(!(d->flags&1)&&current->reflector_exists()&&!s.has_reflection){if(!s.target||!host_.reflection_create||!host_.reflection_add_child||!host_.reflection_valid||!host_.reflection_queue_free||!host_.sample)return fail("Reflective scene requires explicit Sprite/reflection Host");uint64_t reflection=0;if(!host_.reflection_create(id,*d,*current,s.target,reflection,error_))return false;if(!reflection)return fail("Source reflection creation returned null");s.reflection=reflection;if(!host_.reflection_add_child(reflection,d->parent_id,error_))return false;s.has_reflection=true;
 }else if(!current->reflector_exists()&&s.reflection){if(!host_.reflection_valid||!host_.reflection_queue_free)return fail("Source reflection deletion Host absent");bool valid=false;if(!host_.reflection_valid(s.reflection,valid,error_))return false;if(valid){if(!host_.reflection_queue_free(s.reflection,error_))return false;s.has_reflection=false;}}
 // Invalid non-null reflection does not reset _has_reflection in upstream.
 // queue_free keeps the object alive until the authoritative deferred boundary.
 return true;
}
bool FieldSpriteRuntime::sample(uint32_t id,FieldSpriteSample&out){auto i=instances_.find(id);if(i==instances_.end()||!i->second.ready||data_->record(id)->kind!=FieldSpriteKind::Fetcher||!i->second.target)return fail("Fetcher getter null/unadmitted Sprite");auto c=instances_.find(i->second.target);if(c!=instances_.end()&&data_->record(c->first)->kind==FieldSpriteKind::Character){const auto&s=c->second;out={s.texture_id,s.columns,s.rows,s.frame,s.visible};return true;}if(!host_.sample)return fail("Fetcher generic Sprite getter Host absent");if(!host_.sample(i->second.target,out,error_))return false;return out.columns&&out.rows&&out.columns<=1024&&out.rows<=1024&&out.frame<out.columns*out.rows?true:fail("Fetcher source Sprite getter bounds");}
bool FieldSpriteRuntime::destroy(uint32_t id){auto i=instances_.find(id);if(i==instances_.end())return fail("Sprite destroy unknown instance");instances_.erase(i);return true;/* authoritative scene tree owns child destruction */}
}
