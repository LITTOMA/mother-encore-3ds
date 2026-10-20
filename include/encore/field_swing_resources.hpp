#pragma once
#include "encore/field_swing_entry.hpp"
#include "encore/field_swing_graphics.hpp"
#include <memory>
namespace encore::upstream {
class ResourceCatalog;
// Immutable global metadata owner. Publish only after all four catalogued
// resources and their cross-bindings pass. Texture/GPU ownership is separate.
class FieldSwingResources {
public:
 FieldSwingResources();~FieldSwingResources();
 FieldSwingResources(const FieldSwingResources&)=delete;
 FieldSwingResources&operator=(const FieldSwingResources&)=delete;
 bool load(const ResourceCatalog&,const char*root,std::string&);
 bool valid()const{return state_!=nullptr;}
 const FieldSwingData&swing()const;
 const FieldSwingGraphicsData&graphics()const;
 const FieldSwingActionData&action()const;
 const FieldSwingEntryData&entry()const;
private:
 struct State;std::unique_ptr<State>state_;
};
}
