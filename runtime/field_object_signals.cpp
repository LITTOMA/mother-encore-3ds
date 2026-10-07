#include "encore/field_object_signals.hpp"
#include "encore/utf8.hpp"
#include <limits>

namespace encore::upstream {
namespace {
bool fail(std::string &e,const char *s) { e=s;return false; }
bool name(std::string_view s) {
  size_t count=0;
  return !s.empty() && s.find('\0')==s.npos && encore::utf8_count(s,count);
}
}
bool FieldObjectSignals::initialize(FieldGlobalRegistry &r,DeclarationQuery q,
                                    std::string &e) {
  if(registry_ || !q || r.poisoned() || !r.object_exists(r.kernel()) ||
     !r.object_exists(r.root()))
    return fail(e,"Object signals actual ObjectDB/declaration owner absent");
  registry_=&r;declaration_=std::move(q);e.clear();return true;
}
bool FieldObjectSignals::bind_emission_policy(EmissionQuery q,std::string&e){
 if(!registry_||emission_||!q)return fail(e,"Object signal emission policy unavailable/duplicate");
 emission_=std::move(q);e.clear();return true;
}
bool FieldObjectSignals::declaration(FieldObjectId id,std::string_view signal,
                                     uint32_t &arity,std::string &e) const {
  if(!registry_ || registry_->poisoned() || !registry_->object_exists(id) ||
     !name(signal))
    return fail(e,"Object signal emitter/declaration unavailable");
  return declaration_(id,signal,arity,e);
}
const std::string *FieldObjectSignals::intern(std::string_view s) {
  auto key=std::string(s);auto i=names_.find(key);
  if(i==names_.end())
    i=names_.emplace(key,std::make_unique<std::string>(key)).first;
  return i->second.get();
}
bool FieldObjectSignals::connect(FieldObjectId id,std::string_view signal,
                                 FieldObjectId target,std::string_view method,
                                 uint32_t flags,std::vector<FieldDeferredValue> binds,
                                 std::string &e) {
  uint32_t arity=0;
  if((flags & ~15u) || !name(method) ||
     !declaration(id,signal,arity,e) || !registry_->object_exists(target))
    return fail(e,"Object signal connection flag/source/target rejected");
  auto &slots=signals_[{id,std::string(signal)}];Target key{target,intern(method)};
  auto old=slots.find(key);
  if(old!=slots.end()) {
    if(!(flags & FieldSignalReferenceCounted) ||
       old->second.references==std::numeric_limits<int64_t>::max())
      return fail(e,"Object signal duplicate connection rejected");
    // A referenced duplicate retains the original flags and bound arguments.
    ++old->second.references;e.clear();return true;
  }
  slots.emplace(key,Slot{flags,flags & FieldSignalReferenceCounted ? 1 : 0,
                         std::move(binds)});
  e.clear();return true;
}
bool FieldObjectSignals::connected(FieldObjectId id,std::string_view signal,
                                   FieldObjectId target,std::string_view method,
                                   bool &out,std::string &e) const {
  uint32_t arity=0;
  if(!name(method) || !declaration(id,signal,arity,e) ||
     !registry_->object_exists(target))
    return fail(e,"Object signal connection query owner absent");
  auto s=signals_.find({id,std::string(signal)});auto n=names_.find(std::string(method));
  out=s!=signals_.end() && n!=names_.end() &&
      s->second.count(Target{target,n->second.get()});
  e.clear();return true;
}
bool FieldObjectSignals::disconnect(FieldObjectId id,std::string_view signal,
                                    FieldObjectId target,std::string_view method,
                                    std::string &e) {
  bool exists=false;
  if(!connected(id,signal,target,method,exists,e))return false;
  if(!exists)return fail(e,"Object signal disconnect has no connection");
  auto s=signals_.find({id,std::string(signal)});
  auto i=s->second.find(Target{target,names_.at(std::string(method)).get()});
  if(--i->second.references<=0)s->second.erase(i);
  if(s->second.empty())signals_.erase(s);
  e.clear();return true;
}
bool FieldObjectSignals::emit(FieldObjectId id,std::string_view signal,
                              const std::vector<FieldDeferredValue> &args,
                              std::string &e) {
  uint32_t arity=0;
  if(!declaration(id,signal,arity,e))return false;
  if(args.size()!=arity&&(!emission_||!emission_(id,signal,args.size(),e)))return fail(e,"Object signal arguments differ from source declaration/checked emission");
  if(blocked_.count(id)){e.clear();return true;}
  auto found=signals_.find({id,std::string(signal)});
  if(found==signals_.end()){e.clear();return true;}
  // Godot 3.x copies its target-ID/interned-method VMap for each emission.
  // Disconnect/reconnect during callbacks cannot rewrite this emission.
  const auto snapshot=found->second;
  std::vector<Target> one_shot;std::string first_error;
  for(const auto &entry:snapshot) {
    const auto &target=entry.first;const auto &slot=entry.second;
    if(!registry_->object_exists(target.object))continue;
    FieldDeferredMessage message;
    message.object=target.object;message.kind=FieldDeferredKind::Call;
    message.member=*target.method;message.args=args;
    message.args.insert(message.args.end(),slot.binds.begin(),slot.binds.end());
    std::string error;
    bool ok=false;
    if(slot.flags & FieldSignalDeferred)ok=registry_->enqueue(std::move(message),error);
    else {
      dispatching_.push_back({id,target.object,std::string(signal),*target.method});
      ok=registry_->dispatch(message,error);
      dispatching_.pop_back();
    }
    if(!ok && first_error.empty())
      first_error=error.empty()?"Object signal actual method dispatch failed":error;
    if(slot.flags & FieldSignalOneShot)one_shot.push_back(target);
  }
  // One-shot connections stay present during nested emissions, as upstream.
  for(const auto &target:one_shot) {
    if(!registry_->object_exists(id)||!registry_->object_exists(target.object))continue;
    bool exists=false;std::string error;
    if(!connected(id,signal,target.object,*target.method,exists,error) ||
       (exists && !disconnect(id,signal,target.object,*target.method,error))) {
      if(first_error.empty())first_error=error;
    }
  }
  if(!first_error.empty()){e=std::move(first_error);return false;}
  e.clear();return true;
}
bool FieldObjectSignals::emitting_to(FieldObjectId emitter,std::string_view signal,
                                    FieldObjectId target,std::string_view method) const {
  if(dispatching_.empty())return false;
  const auto &f=dispatching_.back();
  return f.emitter==emitter&&f.target==target&&f.signal==signal&&f.method==method;
}
bool FieldObjectSignals::block(FieldObjectId id,bool value,std::string &e) {
  if(!registry_||!registry_->object_exists(id))
    return fail(e,"Object signal block receiver absent");
  if(value)blocked_.insert(id);else blocked_.erase(id);
  e.clear();return true;
}
bool FieldObjectSignals::release(FieldObjectId id,std::string &e) {
  if(!registry_ || registry_->object_exists(id))
    return fail(e,"Object signal release precedes actual ObjectDB deletion");
  for(auto s=signals_.begin();s!=signals_.end();) {
    if(s->first.first==id){s=signals_.erase(s);continue;}
    for(auto i=s->second.begin();i!=s->second.end();)
      if(i->first.object==id)i=s->second.erase(i);else ++i;
    if(s->second.empty())s=signals_.erase(s);else ++s;
  }
  blocked_.erase(id);e.clear();return true;
}
bool FieldObjectSignals::duplicate_persistent(
    const std::map<FieldObjectId,FieldObjectId> &copies,std::string &e) {
  if(!registry_ || copies.empty())return fail(e,"Object signal duplication lacks actual nodes");
  std::set<FieldObjectId> unique;
  for(const auto &copy:copies)
    if(!registry_->tree_owner(copy.first)||!registry_->tree_owner(copy.second)||
       copy.first==copy.second || !unique.insert(copy.second).second)
      return fail(e,"Object signal duplication source/clone identity rejected");
  const auto snapshot=signals_;
  for(const auto &signal:snapshot) {
    auto emitter=copies.find(signal.first.first);
    if(emitter==copies.end())continue;
    for(const auto &entry:signal.second) {
      const auto &slot=entry.second;
      if(!(slot.flags & FieldSignalPersist))continue;
      // Node::_duplicate_signals copies only connections targeting a Node.
      if(!registry_->tree_owner(entry.first.object) &&
         (!registry_->external_object(entry.first.object) ||
          registry_->source_resource(entry.first.object)))continue;
      auto remapped=copies.find(entry.first.object);
      const auto target=remapped==copies.end()?entry.first.object:remapped->second;
      bool exists=false;
      if(!connected(emitter->second,signal.first.second,target,
                    *entry.first.method,exists,e))return false;
      // A real PackedScene instance may already have restored this connection.
      if(!exists && !connect(emitter->second,signal.first.second,target,
                            *entry.first.method,slot.flags,slot.binds,e))return false;
    }
  }
  e.clear();return true;
}
} // namespace encore::upstream
