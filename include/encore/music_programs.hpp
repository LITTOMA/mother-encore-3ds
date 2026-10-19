#pragma once
#include "encore/music_scene_graph.hpp"
#include <functional>
namespace encore::upstream {
// Checked operations on the audited music manager only; not a script VM.
enum class MusicOp:uint32_t {Add=1,Play,Volume,FadeIn,FadeTo,FadeOut,FadeAll,Pause,Resume,Remove,Stop,CapturePosition,IfExists,IfPlaying,IfPaused,IfEqual,IfDifferent,IfBoolean,StopAreas};
enum class MusicTargetKind:uint32_t {None,Child,Latest,Song,Contains};
enum class MusicValueKind:uint32_t {Empty,String,Number,Boolean,Argument,Gain,Register};
struct MusicValue {MusicValueKind kind=MusicValueKind::Empty;uint32_t reference=0;double number=0;};
struct MusicInstruction {MusicOp op=MusicOp::Add;uint32_t skip=0;MusicTargetKind target=MusicTargetKind::None;MusicValue selector;std::vector<MusicValue>arguments;};
struct MusicProgram {uint32_t identity=0,delay_idle_frames=0;std::vector<MusicInstruction>instructions;};
struct MusicEncounterMusic {uint32_t battle=0;std::string music,intro;};
struct MusicArgument {enum class Kind:uint32_t{String,Number,Boolean};Kind kind=Kind::String;std::string text;double number=0;bool boolean=false;};
enum class MusicProgramStep:uint8_t{Ready,Pending,Failed};
using MusicPositionQuery=std::function<MusicProgramStep(uint64_t,uint64_t,double&,std::string&)>;
struct MusicProgramResult {MusicSceneGraph graph;uint32_t program=0,executed=0,changes=0;};
class MusicPrograms final {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}
 const std::vector<MusicProgram>&programs()const{return programs_;}
 const std::vector<std::string>&strings()const{return strings_;}
 const MusicProgram*find(uint32_t)const;
 const MusicEncounterMusic*encounter(uint32_t)const;
 const std::vector<MusicEncounterMusic>&encounters()const{return encounters_;}
 MusicProgramStep evaluate(uint32_t,const std::vector<MusicArgument>&,const MusicSceneGraph&,const MusicPositionQuery&,MusicProgramResult&,std::string&,uint32_t elapsed_idle_frames=0)const;
private:
 bool valid_=false;std::vector<std::string>strings_;std::vector<MusicProgram>programs_;std::vector<MusicEncounterMusic>encounters_;
};
}
