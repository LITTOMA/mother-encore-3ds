#include "encore/dialogue_choices.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {
float DialogueChoices::Tween::value()const{if(duration<=0||elapsed>=duration)return to;const double x=1-elapsed/duration;return from+(to-from)*float(1-x*x*x*x);}
void DialogueChoices::Tween::start(float target,double seconds){from=value();to=target;elapsed=0;duration=seconds;}
void DialogueChoices::Tween::tick(double dt){elapsed=std::min(duration,elapsed+dt);}
bool DialogueChoices::prepare(const DialogueChoicesData&d,uint32_t group,std::string_view identity,uint32_t extent,std::string&e){
 if(phase_==DialogueChoicesPhase::Active||phase_==DialogueChoicesPhase::WaitingText){e="Dialogue choices already prepared";return false;}
 if(!d.validate_program(group,identity,extent,e))return false;
 data_=&d;group_=group;selected_=d.groups()[group].initial_selection;arrow_time_=0;events_.clear();arrow_x_={};arrow_y_={};phase_=DialogueChoicesPhase::WaitingText;e.clear();return true;
}
bool DialogueChoices::text_completed(std::string&e){
 if(phase_!=DialogueChoicesPhase::WaitingText){e="Dialogue choices require pending text completion";return false;}
 const auto&r=group()->options[selected_].rect;arrow_x_={r.x,r.x,0,0};arrow_y_={r.y,r.y,0,0};arrow_time_=0;phase_=DialogueChoicesPhase::Active;e.clear();return true;
}
bool DialogueChoices::step(double dt,const DialogueChoicesInput&i,std::string&e){
 if(!std::isfinite(dt)||dt<0||dt>1||i.horizontal<-1||i.horizontal>1||i.vertical<-1||i.vertical>1){e="Invalid dialogue choice frame/input";return false;}
 if(events_.size()>64){e="Dialogue choice events must be drained";return false;}
 e.clear();if(!active())return true;
 arrow_time_+=dt;arrow_x_.tick(dt);arrow_y_.tick(dt);
 if(i.cancel||i.confirm){
  const auto*g=group();DialogueChoicesEvent result;result.kind=DialogueChoicesEventKind::Selected;
  result.cancelled=i.cancel;result.target_pc=i.cancel?g->cancel_target_pc:g->options[selected_].target_pc;
  result.sound=i.cancel?DialogueChoiceSound::Cancel:DialogueChoiceSound::Accept;
  result.clear_dialogue=true;result.sound_after_target=true;events_.push_back(result);phase_=DialogueChoicesPhase::Resolved;return true;
 }
 if(!i.horizontal&&!i.vertical)return true;
 // Exact no-wrap GridContainer candidate path; hidden source children still
 // count for row/column calculations but cannot become a selection.
 const int columns=int(data_->columns()),count=int(data_->child_count());
 const int row=int(selected_)/columns,column=int(selected_)%columns;
 const int rows=(count-column+columns-1)/columns,total_rows=(count+columns-1)/columns;
 int candidate=int(selected_);bool proposed=false;
 if(column==0&&i.horizontal){if(i.horizontal==1&&columns>1){const int new_rows=(count-1+columns-1)/columns;candidate=new_rows-1<row?1+(new_rows-1)*columns:int(selected_)+1;proposed=true;}}
 else if(column==columns-1&&i.horizontal==1){}
 else if(row==rows-1&&i.vertical==1){if(rows<total_rows){candidate=count-1;proposed=true;}}
 else if(row==0&&i.vertical==-1){}
 else {candidate=int(selected_)+i.horizontal+i.vertical*columns;proposed=candidate!=int(selected_);}
 // All supported visible options are the initial contiguous source children;
 // next/previous valid-index scans cannot turn a hidden trailing child into one.
 if(proposed&&candidate>=0&&candidate<int(group()->options.size())&&candidate!=int(selected_)){
  selected_=uint32_t(candidate);const auto&r=group()->options[selected_].rect;
  arrow_x_.start(r.x,data_->arrow_move_seconds());arrow_y_.start(r.y,data_->arrow_move_seconds());
  events_.push_back({DialogueChoicesEventKind::SoundRequested,DialogueChoiceSound::Move,0,false,false,false});
 }
 return true;
}
bool DialogueChoices::poll_event(DialogueChoicesEvent&e){if(events_.empty())return false;e=events_.front();events_.pop_front();return true;}
void DialogueChoices::close(){data_=nullptr;phase_=DialogueChoicesPhase::Closed;events_.clear();}
DialogueChoicesPose DialogueChoices::pose()const{
 DialogueChoicesPose p;p.visible=active();p.selected=selected_;if(!data_)return p;
 const auto offset=data_->arrow_geometry();p.arrow_x=data_->grid().x+arrow_x_.value()+offset.x;p.arrow_y=data_->grid().y+arrow_y_.value()+offset.y;
 const double t=std::fmod(arrow_time_,data_->arrow_loop_seconds());for(const auto&key:data_->arrow_keys())if(key.time<=t+1e-12)p.arrow_frame=key.frame;return p;
}
}
