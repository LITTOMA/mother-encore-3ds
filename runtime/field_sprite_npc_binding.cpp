#include "encore/field_sprite_bridge.hpp"
#include "encore/field_npc.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
bool field_sprite_npc_binding(const FieldSpriteData&sprites,const FieldNpcData&npcs,std::string&e){
 auto reject=[&](const char*s){e=s;return false;};if(!sprites.valid()||!npcs.valid()||sprites.source_pin()!=npcs.source_pin()||sprites.scene_id()!=npcs.scene_id())return reject("Sprite/NPC source pin/scene mismatch");std::set<uint32_t>parents;
 for(const auto&d:sprites.records()){if(d.kind!=FieldSpriteKind::Character)continue;auto n=std::find_if(npcs.npcs().begin(),npcs.npcs().end(),[&](const auto&x){return x.id==d.parent_id;});if(n==npcs.npcs().end()||!parents.insert(d.parent_id).second||d.node.substr(0,d.node.rfind('/'))!=n->node||d.ready_ordinal>=n->ready_ordinal)return reject("Sprite/NPC parent identity or source Ready mismatch");const auto*t=sprites.texture(d.setup_texture);const auto*a=sprites.animation(d.setup_animation);if(!t||!a||t->source!=n->sprite||a->source!=n->animation||t->width!=n->width||t->height!=n->height||a->columns!=n->columns||a->rows!=n->rows||a->motions.size()!=n->motions.size()||d.connections.size()!=n->connections.size())return reject("Sprite/NPC checked texture/animation binding mismatch");
  Vec2 offset=d.offset;const auto*initial=sprites.animation(d.initial_animation);
  if(initial&&d.sprite){const auto*it=sprites.texture(d.sprite);if(!it)return reject("Sprite initial texture binding");if(d.flags&1)offset.y=-float(int32_t(it->height/(initial->rows*2)));offset.x+=initial->offset.x;offset.y+=initial->offset.y;}
  if(d.flags&1)offset.y=-float(int32_t(t->height/(a->rows*2)));
  const float expected_x=offset.x+a->offset.x+d.extra_offset.x;const float expected_y=offset.y+a->offset.y+d.extra_offset.y;
  if(expected_x!=n->sprite_offset.x||expected_y!=n->sprite_offset.y)return reject("Sprite/NPC source default/extra offset mismatch");
  for(size_t i=0;i<a->motions.size();++i){const auto&m=a->motions[i];const auto&v=n->motions[i];if(m.name!=v.name||m.loop!=v.loop||m.directions.size()!=v.directions.size())return reject("Sprite/NPC motion topology mismatch");for(size_t j=0;j<m.directions.size();++j){const auto&q=m.directions[j];const auto&r=v.directions[j];if(q.vector.x!=r.vector.x||q.vector.y!=r.vector.y||q.duration!=r.duration||q.keys.size()!=r.keys.size())return reject("Sprite/NPC direction/clock mismatch");for(size_t k=0;k<q.keys.size();++k)if(q.keys[k].time!=r.keys[k].time||q.keys[k].frame!=r.keys[k].frame)return reject("Sprite/NPC discrete frame key mismatch");}}
  for(size_t i=0;i<d.connections.size();++i){const auto&q=d.connections[i];const auto&r=n->connections[i];if(q.from!=r.from||q.to!=r.to||q.mode!=r.mode)return reject("Sprite/NPC transition source mismatch");}
 }
 if(parents.size()!=npcs.npcs().size())return reject("Sprite/NPC incomplete child admission");e.clear();return true;
}
}
