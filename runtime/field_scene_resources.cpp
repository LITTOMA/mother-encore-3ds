#include "encore/field_scene_resources.hpp"
#include "encore/resource_catalog.hpp"
#include "encore/audio_data.hpp"
#include "encore/content.hpp"
namespace encore::upstream {
struct FieldSceneResources::State {
 WorldSceneResources bindings;std::string source;
 RoomData room;FieldMapData map;FieldStoryData story;MickData mick;FieldStepsData steps;FieldPropsData props;FieldObjectsData objects;
 std::vector<std::unique_ptr<MickData>>npcs;MusicRegionData music;MusicZones zones;
};
FieldSceneResources::FieldSceneResources()=default;
FieldSceneResources::~FieldSceneResources()=default;
const WorldSceneResources&FieldSceneResources::bindings()const{static const WorldSceneResources empty;return state_?state_->bindings:empty;}
RoomView FieldSceneResources::room()const{return state_?state_->room.view():RoomView{};}
FieldMapView FieldSceneResources::map()const{return state_?state_->map.view():FieldMapView{};}
FieldStoryView FieldSceneResources::story()const{return state_?state_->story.view():FieldStoryView{};}
FieldStepsView FieldSceneResources::steps()const{return state_?state_->steps.view():FieldStepsView{};}
const FieldPropsData&FieldSceneResources::props()const{static const FieldPropsData empty;return state_?state_->props:empty;}
const FieldObjectsData&FieldSceneResources::objects()const{static const FieldObjectsData empty;return state_?state_->objects:empty;}
MickView FieldSceneResources::mick()const{return state_?state_->mick.view():MickView{};}
uint32_t FieldSceneResources::npc_count()const{return state_?uint32_t(state_->npcs.size()):0;}
MickView FieldSceneResources::npc(uint32_t i)const{return state_&&i<state_->npcs.size()?state_->npcs[i]->view():MickView{};}
const MusicRegionData&FieldSceneResources::music()const{static const MusicRegionData empty;return state_?state_->music:empty;}
const MusicZones&FieldSceneResources::zones()const{static const MusicZones empty;return state_?state_->zones:empty;}
bool FieldSceneResources::load(const ResourceCatalog&catalog,const WorldLinksData&links,uint32_t scene,const char*root,RoomView flags,ItemView items,std::string&error){
 auto fail=[&](const char*why){error=why;return false;};
 if(state_)return fail("Field resource owner is immutable; prepare a new owner");
 if(!flags.valid()||!items.valid())return fail("Field resource shared domain is unavailable");
 if(!links.bind_catalog(catalog,error))return false;
 auto next=std::make_unique<State>();if(!links.resolve_scene(scene,catalog,next->bindings,error))return false;
 if(!next->bindings.scene.map_role)return fail("Requested resource set is not a field scene");
 next->source=std::string(next->bindings.scene.source);next->bindings.scene.source=next->source;
 std::vector<uint8_t>bytes;auto read=[&](ResourceRole role,uint32_t id){return catalog.read(role,id,root,bytes,error);};
 const auto&b=next->bindings;
 if(!read(ResourceRole::FieldRoom,b.scene.room_role)||!next->room.load(bytes.data(),bytes.size(),error)||!read(ResourceRole::FieldMap,b.scene.map_role)||!next->map.load(bytes.data(),bytes.size(),error))return false;
 const auto room=next->room.view();if(room.scene().stable_id!=scene||next->map.view().scene_id()!=scene||room.string(room.scene().source_scene_string)!=b.scene.source)return fail("Field resource scene/source identity differs");
 if(room.flag_count()!=flags.flag_count())return fail("Field resource flag domain count differs");
 for(uint32_t i=0;i<room.flag_count();++i)if(room.flag(i).stable_id!=flags.flag(i).stable_id||room.string(room.flag(i).name_string)!=flags.string(flags.flag(i).name_string))return fail("Field resource flag identity differs");
 if(b.story&&(!read(ResourceRole::FieldStory,b.story)||!next->story.load(bytes.data(),bytes.size(),error)||!next->story.view().bind(room,next->map.view(),items,error)))return false;
 if(b.mick){if(!read(ResourceRole::MickTreats,b.mick)||!next->mick.load(bytes.data(),bytes.size(),error))return false;if(next->mick.view().motion_only())return fail("Field gameplay motion owner has the wrong subtype");}
 for(auto id:b.npcs){auto data=std::make_unique<MickData>();if(!read(ResourceRole::FieldNpc,id)||!data->load(bytes.data(),bytes.size(),error))return false;if(!data->view().motion_only())return fail("Field NPC resource has gameplay ownership");next->npcs.push_back(std::move(data));}
 if(b.music){AudioBank bank;if(!read(ResourceRole::FieldMusic,b.music)||!next->music.load(bytes.data(),bytes.size(),error)||!read(ResourceRole::FieldMusicZones,b.zones)||!next->zones.load(bytes.data(),bytes.size(),error)||!next->zones.matches(next->music,error)||!read(ResourceRole::FieldAudio,b.audio)||!bank.load(bytes.data(),bytes.size(),error)||!next->music.matches(bank,error))return false;}
 if(b.steps){if(!read(ResourceRole::FieldSteps,b.steps)||!next->steps.load(bytes.data(),bytes.size(),error))return false;if(next->steps.view().scene_id()!=scene)return fail("Field stepping scene identity differs");}
 if(b.props){if(!read(ResourceRole::FieldProps,b.props)||!next->props.load(bytes.data(),bytes.size(),error)||!next->props.bind(room,next->map.view(),error))return false;if(!next->props.rendering_bound())return fail("Field props lack checked clone drawing order");}
 if((next->map.view().capabilities()>=6)!=bool(b.objects))return fail("Field object drawing companion is missing or unused");
 if(b.objects){
  if(!read(ResourceRole::FieldObjects,b.objects)||!next->objects.load(bytes.data(),bytes.size(),error)||!next->objects.bind(room,next->map.view(),error))return false;
  if(!next->objects.drawing_bound()||!next->mick.view().valid())return fail("Field object drawing/ready owner is unavailable");
  const auto actor=next->mick.view().actor();if(next->objects.ready_state()!=(uint64_t(actor.rng_lo)|(uint64_t(actor.rng_hi)<<32)))return fail("Field object ready stream differs from scene source");
  std::string path=root?root:"";if(!path.empty()&&path.back()!='/'&&path.back()!='\\')path+='/';path+=next->objects.texture();
  if(!encore::read_file(path.c_str(),bytes,1024*1024,error)||!next->objects.texture_matches(bytes.data(),bytes.size(),error))return false;
 }
 state_=std::move(next);error.clear();return true;
}
}
