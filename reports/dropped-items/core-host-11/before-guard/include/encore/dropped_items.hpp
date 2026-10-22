#pragma once
#include "encore/session_save.hpp"
#include "encore/load_rng.hpp"
#include "encore/breakables.hpp"
#include <array>
#include <memory>

namespace encore::upstream {
// Schema roles only. Source values, identities and bindings are in ENCDRP01.
enum class DroppedScalar : uint32_t {
 CollectTravelSeconds,CollectDelaySeconds,CollectScaleSeconds,
 ScaleStartX,ScaleStartY,ScaleEndX,ScaleEndY,
 RevertStartDegrees,RevertEndDegrees,RevertSeconds,
 DisappearStage1,DisappearStage2,DisappearStage3,DisappearStage4,
 BlinkInitialSpeed,BlinkSecondSpeed,BlinkThirdSpeed,
 SparkleMinimum,SparkleMaximum,SparkleFPS,SparkleFrames,
 PromptOffsetX,PromptOffsetY,SparkleOffsetX,SparkleOffsetY,
 CollectTransition,CollectEase,ScaleTransition,ScaleEase,
 RevertTransition,RevertEase,Count
};
enum class DroppedTransition : uint32_t {Quart=3,Quad=4,Elastic=6};
enum class DroppedEase : uint32_t {In=0,Out=1};
enum class DroppedClipRole : uint32_t {Reset=1,Blink=2};
struct DroppedTemplate {
 uint32_t id=0,doses=0,width=0,height=0;
 bool key_item=false;std::string item,texture;
};
struct DroppedOwner {
 uint32_t id=0,order=0,template_id=0,bits=0;
 std::string path,name,parent,flag;BreakablePoint position;
 bool can_pickup()const{return bits&1u;}
 bool reset_consumed()const{return bits&2u;}
 bool object_flag()const{return bits&4u;}
};
struct DroppedClipKey {float time=0;bool value=false;uint32_t update=0;};
struct DroppedClip {
 uint32_t id=0;DroppedClipRole role=DroppedClipRole::Reset;
 std::string name;float duration=0;bool loop=false;
 std::vector<DroppedClipKey>keys;
};
struct DroppedInfo {
 uint32_t scene=0,capacity=0,body_layer=0,body_mask=0,interaction_layer=0,interaction_mask=0,default_bits=0;
 bool monitoring=false;
 std::string source,name,prefab,script,holder,flag_script,inventory,item,success_dialogue,full_dialogue,empty_dialogue;
};
class DroppedItemsData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}
 uint64_t generation()const{return generation_;}
 const DroppedInfo&info()const{return info_;}
 double value(DroppedScalar role)const{return policy_.at(size_t(role));}
 const std::vector<DroppedTemplate>&templates()const{return templates_;}
 const std::vector<DroppedOwner>&owners()const{return owners_;}
 const std::vector<DroppedClip>&clips()const{return clips_;}
 const std::array<std::array<float,4>,2>&shapes()const{return shapes_;}
 const std::array<std::string,2>&shape_paths()const{return shape_paths_;}
 const DroppedTemplate*item(uint32_t)const;
 const DroppedOwner*owner(uint32_t)const;
 const DroppedClip*clip(DroppedClipRole)const;
 bool compatible(const BreakablesData&,std::string&)const;
private:
 bool valid_=false;uint64_t generation_=0;DroppedInfo info_;
 std::array<double,size_t(DroppedScalar::Count)>policy_{};
 std::array<std::array<float,4>,2>shapes_{};
 std::array<std::string,2>shape_paths_{};
 std::vector<DroppedTemplate>templates_;std::vector<DroppedOwner>owners_;std::vector<DroppedClip>clips_;
};

enum class ItemHolderGrantKind : uint32_t {None,Acquired,Full};
enum class ItemHolderGrantTarget : uint32_t {Preview,Party,Keys};
struct ItemHolderGrantResult {
 ItemHolderGrantKind kind=ItemHolderGrantKind::None;
 ItemHolderGrantTarget target=ItemHolderGrantTarget::Preview;
 uint32_t character_index=UINT32_MAX;
 // Acquired items are resolved in the selected inventory by this opaque UID.
 // A Full result owns only the source global.item preview, never a saved item.
 SessionItem item;
};
// Shared ItemHolder inventory operation. The caller prepares the complete item
// catalog/session domain before admitting a scene, then owns source flag,
// collect animation, dialogue and global.item lifetimes. Both success and Full
// execute the checked original Item.get_uid constructor through the clock
// adapter. No save schema or released session domain is expanded by this API.
// Failure preserves this operation's inventory, RNG, ledger and output. Clock
// reads are external entropy samples and cannot be undone.
bool grant_item_holder_item(const DroppedItemsData&,uint32_t template_id,
 SessionSnapshot&,SourceRandom&,std::vector<uint32_t>& generated_uid_ledger,
 const LoadRngClockProvider&,ItemHolderGrantResult&,std::string& error);
}
