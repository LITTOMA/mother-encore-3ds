#include "encore/battle_action_presentation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace encore::upstream;
namespace {
void check(bool value,const char*message){if(!value){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
BattleActionPose dialogue(BattleActionPresentation&presentation,uint32_t media){
 for(const auto&pose:presentation.overlays())if(pose.media==media)return pose;
 check(false,"real dialogue produces its text overlay");return {};
}
}
int main(int argc,char**argv){
 check(argc==4,"baseline round, changed round and original entry supplied");
 BattleRoundData baseline,changed;BattleData entry;std::string error;
 check(baseline.load_file(argv[1],error),error.c_str());check(changed.load_file(argv[2],error),error.c_str());check(entry.load_file(argv[3],error),error.c_str());
 const auto a=baseline.view(),b=changed.view();
 check(a.version()==b.version()&&a.binding().battle_id==b.binding().battle_id,"layout change preserves format and encounter identity");
 for(unsigned section=1;section<=static_cast<unsigned>(RoundSection::Victory);++section)check(a.count(static_cast<RoundSection>(section))==b.count(static_cast<RoundSection>(section)),"layout change preserves section topology");
 SourceRandom random_a(13),random_b(13);BattleActionPresentation first,second;
 check(first.begin(a,entry.view(),random_a),first.error());check(second.begin(b,entry.view(),random_b),second.error());
 const auto media_a=a.presentation(RoundPresentationSlot::DialogueText),media_b=b.presentation(RoundPresentationSlot::DialogueText);
 check(media_a==media_b,"stable presentation binding preserved");
 BattleRoundCue cue;cue.kind=BattleRoundCueKind::Dialogue;cue.text=a.skill(a.binding().basic_skill).dialog;
 check(first.emit(cue,random_a),first.error());check(second.emit(cue,random_b),second.error());
 check(first.physics_frame(a.rule(RoundRule::TextSecondsPerChar)*2),first.error());check(second.physics_frame(b.rule(RoundRule::TextSecondsPerChar)*2),second.error());
 const auto pose_a=dialogue(first,media_a),pose_b=dialogue(second,media_b);
 check(!pose_a.text.empty()&&pose_a.text==pose_b.text,"original attack dialogue printed by both consumers");
 check(std::abs((pose_b.rect.x-pose_a.rect.x)-1)<1e-6,"same runtime executable consumes changed external dialogue position");
 check(pose_a.rect.y==pose_b.rect.y&&pose_a.rect.z==pose_b.rect.z&&pose_a.rect.w==pose_b.rect.w,"remaining dialogue geometry preserved");
 check(a.parameter(RoundParameter::DialogueTextLayout).x==pose_a.rect.x&&b.parameter(RoundParameter::DialogueTextLayout).x==pose_b.rect.x,"checked parameter and consumed overlay agree");
 check(random_a.state()==random_b.state(),"layout policy consumes no gameplay random draws");
 std::cout<<"round presentation recipe real dialogue differential passed\n";
}
