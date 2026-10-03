#include "encore/preparation_control.hpp"
#pragma once
#include "encore/battle_data.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
// Schema identifiers only; content, tuning, labels and media are in opening.encround.
constexpr uint32_t round_no_index=UINT32_MAX;
enum class RoundSection:uint16_t {Strings=1,Skills,EnemyChoices,Rules,Bindings,Texts,Resources,Media,Tracks,Keys,Events,PresentationBindings,Parameters,Victory,Encounter,Growth,BossShakes};
enum class RoundActionType:uint32_t {Damage=0,Other=4};
enum class RoundTargetType:uint32_t {Enemy=0,Self=5};
enum class RoundSkillType:uint32_t {None=0,Basic=1,Skill=2};
enum class RoundDamageType:uint32_t {None=0,Normal=1};
enum class RoundTrait:uint32_t {Guard=1};
enum class RoundRule:uint32_t {
 DefenseDivisor=1,GuardDivisor,MinimumDamage,GutsDivisor,MinimumCritPercent,
 PercentScale,AdrenalineMultiplier,SmashMultiplier,EnemyChoiceDelay,ActionStartDelay,
 TargetEndDelay,ActionEndDelay,RoundEndDelay,DefeatDialogDelay,HpFrameSeconds,
 HpTransitionFrames,HpBaseSpeed,HpDefendingMultiplier,HpFastMultiplier,
 TextSecondsPerChar,TextAcceptMultiplier,TextCancelMultiplier,TextAutoAdvanceSeconds,
 TextNormalMultiplier,TextFasterMultiplier,TextSlowerMultiplier,SmashTimeScale,SmashRealSeconds,VictoryBannerSeconds,Count
};
enum class RoundTextRole:uint32_t {Empty=0,SkillName=1,SkillDescription,SkillDialog,EnemyName,EnemyArticle,EnemyOutro,MortalDamage,NoEffect,Experience,LevelUp,StatGrowth,Learning};
enum class RoundAcknowledgment:uint32_t {AcceptOrCancelAfterFinished=1};
enum class RoundCurrencyPolicy:uint32_t {BankAndEarnedCash=1};
enum class RoundMediaRole:uint32_t {PartySprite=1,EnemySprite,HitEffect,PartyPlate,TargetPointer,Dialogue,FlyingNumber,RisingNumber,BackgroundDim,TransitionRect,WorldParty,BossFlash};
enum class RoundProperty:uint32_t {Position=1,PositionX,PositionY,Scale,Alpha,Frame,Visible,Color,FlashColor,FlashModifier,Offset,Rotation,GlowColor,GlowModifier,Modulate,Size,OutlineWidth,Radius};
enum class RoundInterpolation:uint32_t {Linear=0,QuartOut,QuartIn,QuadOut,CircIn,ElasticOut,SineOut,Cubic,QuadIn};
enum class RoundTrackMode:uint32_t {Replace=0,AddOrigin,ScaleInput};
enum class RoundEventKind:uint32_t {ApplyDamage=1,TryPause,Finished,TurnPartyToWorld,HideBattleBackground,HideEnemies,JumpPartyToWorld,RotatePartyOriginal,PartyReturnLanded,BossFlashStart,BossKillEnemies};
enum class RoundPresentationSlot:uint32_t {PartyIdle=1,PartyPrepare,PartyReturn,PartyGuard,EnemyFlash,EnemyAttack,EnemyHit,EnemyDefeat,PartyHit,TargetPointer,Dialogue,FlyingNumber,RisingNumber,BackgroundDim,PartyHit2,PartyHit3,PartyShow,PartyHide,PartyBounce,PartyShake,PlateQuake,EnemyDodge,PartyDodge,BackgroundUndim,TargetNameBox,DialogueText,TargetNameText,Smash,SmashBackground,PartyGuardPrepare,DialogueCursor,PartyVictory,VictoryBanner,ReturnTop,ReturnBottom,ReturnPlate,ReturnTimeline,PartyJumpToWorld,Count};
enum class RoundParameter:uint32_t {TextTiming=1,TextTagSpeeds,HpDigits,PartyHit,PartyShown,FlyingNumberRandom,DamageGlyphGrid,TargetPointerOffset,TargetGlow,DialogueMargins,DialogueTextLayout,DamageShadow,SmashTiming,SmashOffset,PartyBounceMotion,PlateHitIntensity,RisingNumberSize,ReturnPartyGeometry,ReturnPartyFrames,ReturnPartyTurn,ReturnCamera,Count};
struct RoundSkill {
 uint32_t id=0,source=0,name=0,description=0,dialog=0;
 uint32_t action_type=0,target_type=0,skill_type=0,damage_type=0,traits=0;
 int32_t power=0,variance=0,priority=0,miss_chance=0,pp_cost=0,hp_cost=0,crit_chance=0;
 uint32_t user_media=round_no_index,hit_media=round_no_index;
 int32_t fail_chance=0;
};
struct RoundEnemyChoice {uint32_t skill=0,weight=0;};
struct RoundBinding {
 uint32_t battle_id=0,player_participant=0,enemy_participant=0,basic_skill=0,guard_skill=0;
 uint32_t basic_menu=0,items_menu=0,guard_menu=0,locale=0,enemy_name=0,enemy_article=0;
 uint32_t enemy_outro=0,mortal_damage=0,no_effect=0,show_intro_outro=0,win_flag=0;
};
struct RoundText {uint32_t id=0,role=0,key=0,source_text=0,text=0;};
// A bounded reward contract; progression thresholds are verified against
// RoomView before state is committed. v5 conditionally applies one promotion.
struct RoundVictory {
 uint32_t initial_exp=0,initial_level=0,initial_bank=0,initial_cash=0,initial_earned_cash=0;
 uint32_t reward_exp=0,reward_cash=0,reward_item_count=0,level_cap=0,next_level_exp=0,max_exp=0;
 uint32_t exp_text=0,acknowledgment=0,currency_policy=0,earned_cash_flag=0,enemy_body_id=0;
};
enum class RoundStat:uint32_t {MaxHp=1,MaxPp,Offense,Defense,Speed,Iq,Guts};
struct RoundEncounter {uint32_t boss=0,keep_actor=0,post_win_script=0,boss_flash_media=round_no_index,promoted_level=0,following_level_exp=0,level_text=0,learned_skill=0,learned_text=0,stop_area_music_if_overworld=0;};
struct RoundGrowth {uint32_t stat=0,before=0,after=0,text=0;};
struct RoundBossShake {float time=0,magnitude=0,length=0,interval=0,weight=0;};
struct RoundMedia {
 uint32_t id=0,name=0,role=0,resource=round_no_index,first_track=0,track_count=0,first_event=0,event_count=0,flags=0;
 float duration=0;BattleValue rect{},color{};Vec2 anchor{};
};
struct RoundTrack {uint32_t media=0,property=0,first=0,count=0,update=0,interpolation=0,mode=0;};
struct RoundEvent {uint32_t media=0;float time=0;uint32_t kind=0;};
class RoundView {
public:
 bool valid()const{return bytes_!=nullptr;}
 uint32_t version()const;
 uint32_t count(RoundSection)const;std::string_view string(uint32_t)const;
 RoundSkill skill(uint32_t)const;RoundEnemyChoice enemy_choice(uint32_t)const;
 double rule(RoundRule)const;BattleValue parameter(RoundParameter)const;RoundBinding binding()const;RoundText text(uint32_t)const;RoundVictory victory()const;RoundEncounter encounter()const;RoundGrowth growth(uint32_t)const;RoundBossShake boss_shake(uint32_t)const;
 BattleResource resource(uint32_t)const;RoundMedia media(uint32_t)const;RoundTrack track(uint32_t)const;
 BattleKey key(uint32_t)const;RoundEvent event(uint32_t)const;
 uint32_t presentation(RoundPresentationSlot)const;
private:
 friend class BattleRoundData;const uint8_t*bytes_=nullptr;size_t size_=0;
 const uint8_t*record(RoundSection,uint32_t)const;
};
class BattleRoundData {
public:
 BattleRoundData()=default;BattleRoundData(const BattleRoundData&)=delete;BattleRoundData&operator=(const BattleRoundData&)=delete;
 size_t resident_bytes()const{return bytes_.capacity();}
 void swap(BattleRoundData& other){bytes_.swap(other.bytes_);}
 bool load(const uint8_t*,size_t,std::string&,const encore::PreparationControl* control=nullptr);bool load_file(const char*,std::string&);
 RoundView view()const{RoundView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
