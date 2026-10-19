#pragma once
#include "encore/field_story.hpp"
#include "encore/field_steps.hpp"
#include "encore/field_props.hpp"
#include "encore/mick_treats.hpp"
#include "encore/music_zones.hpp"
#include "encore/world_links.hpp"
#include <memory>
namespace encore::upstream {
// Immutable metadata owner. A prepared FieldScene may borrow its views until
// that scene is destroyed. Loading an existing owner is explicitly rejected.
class FieldSceneResources {
public:
 FieldSceneResources();~FieldSceneResources();
 FieldSceneResources(const FieldSceneResources&)=delete;
 FieldSceneResources&operator=(const FieldSceneResources&)=delete;
 bool load(const ResourceCatalog&,const WorldLinksData&,uint32_t scene,const char*root,RoomView flags,ItemView,std::string&);
 bool valid()const{return state_!=nullptr;}
 const WorldSceneResources&bindings()const;
 RoomView room()const;FieldMapView map()const;FieldStoryView story()const;FieldStepsView steps()const;
 const FieldPropsData&props()const;
 MickView mick()const;uint32_t npc_count()const;MickView npc(uint32_t)const;
 const MusicRegionData&music()const;const MusicZones&zones()const;
private:
 struct State;std::unique_ptr<State>state_;
};
}
