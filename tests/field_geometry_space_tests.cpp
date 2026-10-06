#include "encore/field_geometry_space.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
// Manual resource/query contract cases only. This does not run source Ready or
// approve adapters for the actual scene. It is never an automatic development job.
using namespace encore::upstream;
// Call only with a genuinely admitted scene, an overlapping real actor and a
// source Area. No fixture adapter is permitted to approve the scene here.
void field_bilateral_monitoring_manual_cases(FieldGeometrySpace& space,
    const FieldGeometryView& source, FieldGeometryActor actor, uint32_t area){
    std::string error;uint32_t original_layer=0,original_mask=0;bool found=false;
    for(uint32_t i=0;i<source.owner_count();++i){auto owner=source.owner(i);
        if(source.node(owner.node).stable_id==area){assert(owner.kind==4);
            original_layer=owner.layer;original_mask=owner.mask;found=true;break;}}
    assert(found&&actor.layer&&actor.mask);actor.area=false;
    FieldGeometryNodeUpdate update;update.stable_id=area;update.fields=8;
    std::vector<FieldGeometryContact> result;
    auto present=[&](){for(const auto& c:result)if(c.stable_id==area)return true;return false;};
    update.layer=actor.mask;update.mask=0;assert(space.apply_updates({update},error));
    assert(space.monitoring_areas(actor,4096,result,error)&&present());
    update.layer=0;update.mask=actor.layer;assert(space.apply_updates({update},error));
    assert(space.monitoring_areas(actor,4096,result,error)&&present());
    update.mask=0;assert(space.apply_updates({update},error));
    assert(space.monitoring_areas(actor,4096,result,error)&&!present());
    update.layer=original_layer;update.mask=original_mask;assert(space.apply_updates({update},error));
}
int main(int argc,char** argv){
    assert(argc==2);std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};assert(bytes.size()>296);
    FieldIdentity id;auto u32=[](const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;};id.scene_id=u32(bytes.data()+36);std::memcpy(id.upstream_commit.data(),bytes.data()+40,20);std::memcpy(id.source_sha256.data(),bytes.data()+60,32);
    FieldGeometryView source;std::string error;assert(source.load(bytes.data(),bytes.size(),id,error));FieldGeometrySpace space;assert(space.configure(source,64,error));assert(space.indexed_instance_count()==1184);
    std::vector<FieldGeometryContact> contacts{{99,99,99,99}};FieldGeometryFilter filter;filter.layer_mask=~0u;filter.areas=true;
    assert(!space.candidates({{0,0},{100,100}},filter,100,contacts,error));assert(contacts[0].stable_id==99); // Unapproved Ready cannot become empty collision.
    std::array<uint8_t,32> wrong{};uint32_t scripted=0;for(uint32_t i=0;i<source.node_count();++i)if(!source.string(source.node(i).script).empty()){scripted=source.node(i).stable_id;break;}assert(scripted);
    assert(!space.bind_script(scripted,wrong,1,1,error));FieldGeometryNodeUpdate update;update.stable_id=scripted;update.fields=1;update.local={{1,0},{0,1},{1,1}};assert(!space.apply_updates({update},error));
    bool collided=true;FieldGeometryRayHit hit;hit.stable_id=99;assert(!space.ray({0,0},{0,0},filter,100,collided,hit,error));assert(collided&&hit.stable_id==99);
    FieldGeometryActor unsupported;unsupported.kind=FieldGeometryKind::Capsule;assert(!space.overlap_actor(unsupported,filter,100,contacts,error));assert(contacts[0].stable_id==99);
    assert(!space.configure(source,0,error));assert(space.indexed_instance_count()==1184);
}
