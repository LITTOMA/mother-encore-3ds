#include "encore/field_node_tree.hpp"
#include "encore/field_door.hpp"
#include <algorithm>
namespace encore::upstream {
bool FieldNodeTreeRuntime::initialize_door_continuation(
    const FieldDoorData &data,uint32_t stable,FieldNodeTreeHost host,std::string &e) {
  FieldDoorDescriptor door;
  if(!data.valid()||!data.find(stable,door)||root_||!nodes_.empty()||
     poisoned_||!host.construct_source||!host.native_allocated||
     !host.allocate_object||!host.allocate_fast_name||!host.bind||!host.dispatch||
     !host.deferred||!host.object_exists||!host.input_registration||
     !host.external_pause_process||!host.release||!host.object_domain||
     bool(host.enqueue_global)!=bool(host.flush_global)||
     source_index_.count(door.id)||source_index_.count(door.shape)||
     source_index_.count(door.marker)||source_index_.count(door.audio)) {
    e="Native House Door continuation source/factory rejected";return false;
  }
  const auto identity=data.identity();
  std::array<uint8_t,32> proof{};
  if(!data.source_hash(data.script(),proof)||proof!=data.script_sha()) {
    e="Native House Door continuation source pin/script differs";return false;
  }
  host_=std::move(host);
  const FieldTransform unit={Vec2{1,0},Vec2{0,1},Vec2{0,0}};
  const FieldColor white={1,1,1,1};
  auto make=[&](uint32_t id,const char *klass,int32_t index) {
    FieldNodeDescriptor r;r.id=id;r.parent=index<0?0:door.id;
    r.owner=index<0?0:door.id;r.canvas_parent=index<0?0:door.id;
    r.index=index;r.native_class=klass;r.name=klass;r.path=klass;
    r.flags=std::string_view(klass)=="AudioStreamPlayer"?0:11;
    r.local=unit;r.world=unit;r.modulate=white;r.self_modulate=white;
    return r;
  };
  auto root=make(door.id,"Area2D",-1);
  auto path=data.string(door.node);auto slash=path.rfind('/');
  root.name=std::string(path.substr(slash==path.npos?0:slash+1));root.path=".";
  root.ready=door.ready;root.pause=door.pause_mode;
  root.script=std::string(data.script());root.script_sha=proof;
  root.local={door.body_transform.x,door.body_transform.y,door.body_transform.origin};
  root.world=root.local;
  auto shape=make(door.shape,"CollisionShape2D",0);shape.local[2]=door.shape_offset;
  auto marker=make(door.marker,"Position2D",1);
  const auto &b=door.body_transform;const auto &m=door.marker_transform;
  const float determinant=b.x.x*b.y.y-b.x.y*b.y.x;
  auto inverse=[&](Vec2 p,bool origin){
    if(origin){p.x-=b.origin.x;p.y-=b.origin.y;}
    return Vec2{(b.y.y*p.x-b.y.x*p.y)/determinant,
                (-b.x.y*p.x+b.x.x*p.y)/determinant};
  };
  marker.local={inverse(m.x,false),inverse(m.y,false),inverse(m.origin,true)};
  marker.world={m.x,m.y,m.origin};
  auto audio=make(door.audio,"AudioStreamPlayer",2);audio.canvas_parent=0;
  FieldObjectId out=0;
  if(!instantiate_records(identity,{root,shape,marker,audio},stable,true,out,e))return false;
  root_=out;
  for(auto id:{door.id,door.shape,door.marker,door.audio}) {
    auto i=std::find_if(nodes_.begin(),nodes_.end(),[&](const auto &v){
      return v.second.source==id&&sources_.at(v.first).identity.scene_id==identity.scene_id;
    });
    if(i==nodes_.end()||!source_index_.emplace(id,i->first).second){
      poisoned_=true;e="Native Door continuation complete source index rejected";return false;
    }
  }
  e.clear();return true;
}
} // namespace encore::upstream
