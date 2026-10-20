#include "encore/field_swing_resources.hpp"
#include "encore/resource_catalog.hpp"
#include <new>
namespace encore::upstream {
struct FieldSwingResources::State {
 FieldSwingData swing;FieldSwingGraphicsData graphics;
 FieldSwingActionData action;FieldSwingEntryData entry;
};
FieldSwingResources::FieldSwingResources()=default;
FieldSwingResources::~FieldSwingResources()=default;
const FieldSwingData&FieldSwingResources::swing()const{static const FieldSwingData empty;return state_?state_->swing:empty;}
const FieldSwingGraphicsData&FieldSwingResources::graphics()const{static const FieldSwingGraphicsData empty;return state_?state_->graphics:empty;}
const FieldSwingActionData&FieldSwingResources::action()const{static const FieldSwingActionData empty;return state_?state_->action:empty;}
const FieldSwingEntryData&FieldSwingResources::entry()const{static const FieldSwingEntryData empty;return state_?state_->entry:empty;}
bool FieldSwingResources::load(const ResourceCatalog&catalog,const char*root,std::string&error){
 if(state_){error="Player action metadata owner is immutable; prepare a new owner";return false;}
 if(!catalog.valid()||!root){error="Player action catalog/root is unavailable";return false;}
 std::unique_ptr<State>next(new(std::nothrow)State);
 if(!next){error="Player action metadata allocation failed";return false;}
 std::vector<uint8_t>bytes;
 auto read=[&](ResourceRole role){return catalog.read(role,uint32_t(role),root,bytes,error);};
 if(!read(ResourceRole::FieldSwing)||!next->swing.load(bytes.data(),bytes.size(),error)||
    !read(ResourceRole::FieldSwingGraphics)||!next->graphics.load(bytes.data(),bytes.size(),error)||
    !read(ResourceRole::FieldSwingAction)||!next->action.load(bytes.data(),bytes.size(),error)||
    !read(ResourceRole::FieldSwingEntry)||!next->entry.load(bytes.data(),bytes.size(),error)||
    !next->graphics.bind(next->swing,error)||!next->action.bind(next->swing,error)||
    !next->entry.bind(next->action,next->swing,error))return false;
 state_=std::move(next);error.clear();return true;
}
}
