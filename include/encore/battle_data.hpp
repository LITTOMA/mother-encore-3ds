#include "encore/preparation_control.hpp"
#pragma once
#include "encore/movement.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
// Execution schema only. All scene coordinates, tuning, media and labels are external.
enum class BattleSection:uint16_t { Strings=1,Resources,Layouts,Tracks,Keys,Parameters,Participants,Menus,Metadata,Backgrounds,Events,Glyphs };
enum class BattleParameter:uint32_t {
    CanvasSize=1,SceneDuration,MaskDuration,MaskColor,MaskGrid,MenuDuration,
    EnemyMoveDuration,EnemyMoveScaleDuration,EnemyMoveScaleDelay,EnemyMoveInitialScale,
    PlayerJumpDuration,PlayerJumpHeight,CursorDuration,CursorRepeatDelay,CursorRepeatInterval,
    PlateSize,MenuSpacing,EnemyShakeInterval,EnemyShakeMagnitude,FontMetrics,PartyJumpStart,PartyJumpTarget,PartyJumpScale,PartySquash,PartyShow,EnemyTint,BackdropColor,DigitGrid,PartyNudge,PartyScreenOffset,CursorScale1,CursorScale2,CursorScale3,PartyQuake1,PartyQuake2,PartyQuake3,PartyQuake4,PartyQuake5,MaskOldColor,Count
};
enum class BattleDrawKind:uint32_t { Sprite=1,Rectangle,NinePatch,Text,Tiled };
enum class BattleRole:uint32_t { Static=0,EnemyTransition,PartyTransition,EnemySprite,PartySprite,PartyPlate,PartyName,PartyHP,PartyPP,MenuIcon,MenuCursor,TopBar,BottomBar,TargetBox,TargetText,TransitionMask };
enum class BattleProperty:uint32_t { Position=1,PositionX,PositionY,Scale,Alpha,Frame,Visible,Color,FlashColor,FlashModifier };
enum class BattleClock:uint32_t { Scene=0,Mask,Menu,EnemyMove,PartyJump,EnemyAppear,PartyShow,PartyLanding,Cursor };
enum class BattleEventKind:uint32_t { StartEnemyMove=1,StartPartyJump,ShowMenu,ShowEnemy,RemoveEnemyTransition };
struct BattleValue {float x=0,y=0,z=0,w=0;};
struct BattleResource {uint32_t id=0,path=0,kind=0,width=0,height=0,columns=0,rows=0;uint8_t sha256[32]{};};
struct BattleLayout {uint32_t id=0,kind=0,role=0,resource=0,frame=0,text=0,flags=0,binding=0;BattleValue rect{},color{};float depth=0;uint32_t margins[4]{};Vec2 display_anchor{};};
struct BattleTrack {uint32_t target=0,property=0,clock=0,first=0,count=0,update=0;};
struct BattleKey {float time=0,ease=0;BattleValue value{};};
struct BattleParticipant {uint32_t id=0,name=0,kind=0,flags=0;int32_t level=0,hp=0,maxhp=0,pp=0,maxpp=0,offense=0,defense=0,speed=0,iq=0,guts=0,xp=0,cash=0;};
struct BattleMenu {uint32_t id=0,label=0,layout=0,flags=0;};
struct BattleMetadata {uint32_t room_battle_id=0,enemy_name=0,encounter_audio=0,mask_resource=0,player_instance=0,enemy_instance=0,flags=0,reserved=0;};
struct BattleBackground {uint32_t resource=0,flags=0;float width=0,height=0,opacity=0,effect=0,effect_scale=0;Vec2 barrel{},oscillation_amplitude{},oscillation_frequency{},oscillation_speed{};
    Vec2 move{},ping_pong_speed{},compression_amplitude{},compression_frequency{},compression_speed{};
    uint32_t palette_resource=0xffffffffu,palette_frames=0,palette_fixed_row=0xffffffffu;float palette_speed=0;};
struct BattleGlyph {uint32_t codepoint=0,resource=0,u=0,v=0,w=0,h=0;float advance=0,offset_x=0,offset_y=0;};
struct BattleEvent {float time=0;uint32_t kind=0,target=0,flags=0;};
class BattleView {
public:
 bool valid()const{return bytes_!=nullptr;}
 uint32_t count(BattleSection)const;
 std::string_view string(uint32_t byte_offset)const;
 BattleResource resource(uint32_t)const;BattleLayout layout(uint32_t)const;
 BattleTrack track(uint32_t)const;BattleKey key(uint32_t)const;
 BattleValue parameter(BattleParameter)const;
 BattleParticipant participant(uint32_t)const;BattleMenu menu(uint32_t)const;
 BattleMetadata metadata()const;BattleBackground background(uint32_t)const;BattleEvent event(uint32_t)const;BattleGlyph glyph(uint32_t)const;
private:
 friend class BattleData;const uint8_t* bytes_=nullptr;size_t size_=0;
 const uint8_t* record(BattleSection,uint32_t)const;
};
class BattleData {
public:
 BattleData()=default;BattleData(const BattleData&)=delete;BattleData&operator=(const BattleData&)=delete;
 size_t resident_bytes()const{return bytes_.capacity();}
 void swap(BattleData& other){bytes_.swap(other.bytes_);}
 bool load(const uint8_t*,size_t,std::string&,const encore::PreparationControl* control=nullptr);bool load_file(const char*,std::string&);
 BattleView view()const{BattleView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
