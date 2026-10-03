#pragma once
#include "encore/preparation_control.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <memory>
#include <utility>

namespace encore {
// Immutable after preparation. All colors and indexed bytes are caller data.
// This is the exact output of the audited SCREEN_TEXTURE mask operation, not
// an animation player: no time, RNG, state, file IO or graphics API is present.
class TransitionMaskPlan {
public:
    struct Source {
        uint32_t width=0,height=0,frames=0;
        const uint8_t* indices=nullptr;size_t index_count=0;
        const uint32_t* palette=nullptr;size_t palette_count=0;
    };
    struct Canvas {uint32_t width=0,height=0;int32_t offset_x=0,offset_y=0;};
    struct Resources {size_t cpu_bytes=0,total_runs=0,maximum_frame_runs=0,maximum_output_spans=0;};
private:
    struct Run {uint16_t x,width;uint32_t color;};
    static_assert(sizeof(Run)==8,"mask run storage ABI");
    Canvas canvas_{};uint32_t frames_=0;size_t maximum_frame_runs_=0;
    std::unique_ptr<Run[]> runs_;
    std::unique_ptr<uint32_t[]> rows_;
    size_t run_count_=0,row_count_=0;
    static bool fail(std::string& error,const char* text){error=text;return false;}
    uint32_t source_x(uint32_t x,const Source& s)const {
        return uint32_t(std::clamp(int64_t(x)-canvas_.offset_x,int64_t(0),int64_t(s.width)-1));
    }
    uint32_t source_y(uint32_t y,const Source& s)const {
        return uint32_t(std::clamp(int64_t(y)-canvas_.offset_y,int64_t(0),int64_t(s.height)-1));
    }
    template<class Visitor>bool visit_source(const Source& s,Visitor visitor,const PreparationControl* control)const {
        size_t row=0;
        for(uint32_t f=0;f<s.frames;++f)for(uint32_t y=0;y<canvas_.height;++y,++row){
            if(control&&control->stopped())return false;
            const auto* pixels=s.indices+(size_t(f)*s.height+source_y(y,s))*s.width;
            uint32_t start=0,color=s.palette[pixels[source_x(0,s)]];
            for(uint32_t x=1;x<=canvas_.width;++x){
                const uint32_t next=x<canvas_.width?s.palette[pixels[source_x(x,s)]]:0;
                if(x<canvas_.width&&next==color)continue;
                visitor(row,start,x-start,color);start=x;color=next;
            }
        }
        return true;
    }
public:
    TransitionMaskPlan()=default;
    TransitionMaskPlan(const TransitionMaskPlan&)=delete;
    TransitionMaskPlan& operator=(const TransitionMaskPlan&)=delete;
    TransitionMaskPlan(TransitionMaskPlan&& other)noexcept{swap(other);}
    TransitionMaskPlan& operator=(TransitionMaskPlan&& other)noexcept{if(this!=&other){clear();swap(other);}return *this;}
    void swap(TransitionMaskPlan& other)noexcept{using std::swap;swap(canvas_,other.canvas_);swap(frames_,other.frames_);swap(maximum_frame_runs_,other.maximum_frame_runs_);swap(runs_,other.runs_);swap(rows_,other.rows_);swap(run_count_,other.run_count_);swap(row_count_,other.row_count_);}
    void clear(){canvas_={};frames_=0;maximum_frame_runs_=0;runs_.reset();rows_.reset();run_count_=row_count_=0;}
    bool ready()const{return frames_&&runs_&&rows_&&row_count_==size_t(frames_)*canvas_.height+1;}
    uint32_t frames()const{return frames_;}
    Canvas canvas()const{return canvas_;}
    size_t cpu_bytes()const{return run_count_*sizeof(Run)+row_count_*sizeof(uint32_t);}
    // Exact admitted reservation for a complete background partition of up to
    // background_capacity spans. Union boundaries give B + M - H, never B*M.
    Resources resources(size_t background_capacity)const {
        Resources result{cpu_bytes(),run_count_,maximum_frame_runs_,0};
        if(!ready()||background_capacity<canvas_.height)return result;
        const size_t pixels=size_t(canvas_.width)*canvas_.height;
        background_capacity=std::min(background_capacity,pixels);
        result.maximum_output_spans=std::min<size_t>(pixels,background_capacity+maximum_frame_runs_-canvas_.height);
        return result;
    }
    // Budget is checked before allocating run/row storage. A failure empties
    // this object; callers retain their exact CPU renderer as fallback.
    bool prepare(const Source& source,Canvas canvas,size_t cpu_budget,std::string& error,const PreparationControl* control=nullptr){
        clear();error.clear();
        if(control&&control->stopped())return fail(error,"Transition mask preparation cancelled");
        if(!source.width||source.width>1024||!source.height||source.height>1024||!source.frames||source.frames>256||
           !canvas.width||canvas.width>1024||!canvas.height||canvas.height>1024||source.width>canvas.width||source.height>canvas.height||
           !source.indices||!source.palette||!source.palette_count||source.palette_count>256)
            return fail(error,"Invalid transition mask source/canvas bounds");
        const uint64_t source_pixels=uint64_t(source.width)*source.height*source.frames;
        if(source_pixels>16*1024*1024||source_pixels!=source.index_count||uint64_t(canvas.width)*canvas.height*source.frames>32*1024*1024)
            return fail(error,"Invalid transition mask decoded length/work bound");
        for(size_t i=0;i<source.index_count;++i){
            if(!(i%8192)&&control&&control->stopped()){clear();return fail(error,"Transition mask preparation cancelled");}
            if(source.indices[i]>=source.palette_count)return fail(error,"Transition mask palette index out of range");
        }
        canvas_=canvas;
        size_t count=0,frame_count=0,last_frame=0;
        if(!visit_source(source,[&](size_t row,uint32_t,uint32_t,uint32_t){
            const size_t frame=row/canvas.height;
            if(frame!=last_frame){maximum_frame_runs_=std::max(maximum_frame_runs_,frame_count);frame_count=0;last_frame=frame;}
            ++count;++frame_count;
        },control)){clear();return fail(error,"Transition mask preparation cancelled");}
        maximum_frame_runs_=std::max(maximum_frame_runs_,frame_count);
        const size_t row_count=size_t(source.frames)*canvas.height+1;
        if(count>(SIZE_MAX-row_count*sizeof(uint32_t))/sizeof(Run)||count*sizeof(Run)+row_count*sizeof(uint32_t)>cpu_budget){
            clear();return fail(error,"Transition mask CPU reservation exceeds budget");
        }
        runs_.reset(new(std::nothrow) Run[count]);rows_.reset(new(std::nothrow) uint32_t[row_count]);
        if(!runs_||!rows_){clear();return fail(error,"Transition mask allocation failed");}
        run_count_=count;row_count_=row_count;
        size_t cursor=0,previous_row=SIZE_MAX;
        if(!visit_source(source,[&](size_t row,uint32_t x,uint32_t width,uint32_t color){
            if(row!=previous_row){rows_[row]=uint32_t(cursor);previous_row=row;}
            runs_[cursor++]={uint16_t(x),uint16_t(width),color};
        },control)){clear();return fail(error,"Transition mask preparation cancelled");}
        if(control&&control->stopped()){clear();return fail(error,"Transition mask preparation cancelled");}
        rows_[row_count_-1]=uint32_t(cursor);frames_=source.frames;return true;
    }
    bool requires_background(uint32_t frame,uint32_t old_color)const {
        if(!ready()||frame>=frames_)return true; // Invalid input must not skip work.
        const size_t row=size_t(frame)*canvas_.height;
        for(size_t i=rows_[row];i<rows_[row+canvas_.height];++i)
            if(runs_[i].color!=old_color&&(runs_[i].color>>24)==255)return true;
        return false;
    }
    template<class Span>bool validate_background(const Span* spans,size_t count)const {
        if(!spans||!count||count>size_t(canvas_.width)*canvas_.height)return false;
        uint32_t x=0,y=0;
        for(size_t i=0;i<count;++i){const auto& s=spans[i];
            if(y>=canvas_.height||s.y!=y||s.x!=x||!s.width||s.width>canvas_.width-x)return false;
            x+=s.width;if(x==canvas_.width){x=0;++y;}
        }
        return y==canvas_.height&&x==0;
    }
    // Emits the exact CPU surface as disjoint complete row runs (including
    // transparent RGB). The GPU consumer uses ordinary source-alpha blending.
    // Input/output must not alias. No allocation, IO, decode or color tuning.
    // Failed calls always publish count=0; partially written storage is inert.
    template<class InputSpan,class OutputSpan>
    bool compose(uint32_t frame,uint32_t old_color,uint32_t new_color,const InputSpan* background,size_t background_count,
                 OutputSpan* output,size_t capacity,size_t& count)const {
        count=0;if(!ready()||frame>=frames_||!output||!capacity)return false;
        const bool need_background=requires_background(frame,old_color);
        if(need_background&&!validate_background(background,background_count))return false;
        if(background&&background_count){
            if(background_count>SIZE_MAX/sizeof(InputSpan)||capacity>SIZE_MAX/sizeof(OutputSpan))return false;
            const uintptr_t a=reinterpret_cast<uintptr_t>(background),b=reinterpret_cast<uintptr_t>(output);
            const size_t as=background_count*sizeof(InputSpan),bs=capacity*sizeof(OutputSpan);
            if(a>UINTPTR_MAX-as||b>UINTPTR_MAX-bs||!(a+as<=b||b+bs<=a))return false;
        }
        size_t bg=0;
        auto emit=[&](uint32_t x,uint32_t y,uint32_t width,uint32_t color){
            if(count&&output[count-1].y==y&&uint32_t(output[count-1].x)+output[count-1].width==x&&output[count-1].color==color){
                output[count-1].width=uint16_t(output[count-1].width+width);return true;
            }
            if(count==capacity){count=0;return false;}
            OutputSpan span{};span.x=uint16_t(x);span.y=uint16_t(y);span.width=uint16_t(width);span.color=color;output[count++]=span;return true;
        };
        const size_t first_row=size_t(frame)*canvas_.height;
        for(uint32_t y=0;y<canvas_.height;++y){
            for(size_t i=rows_[first_row+y];i<rows_[first_row+y+1];++i){const auto& mask=runs_[i];
                if(mask.color==old_color||(mask.color>>24)!=255){
                    if(!emit(mask.x,y,mask.width,mask.color==old_color?new_color:mask.color))return false;
                }else{
                    const uint32_t end=uint32_t(mask.x)+mask.width;uint32_t x=mask.x;
                    while(bg<background_count&&(background[bg].y<y||(background[bg].y==y&&uint32_t(background[bg].x)+background[bg].width<=x)))++bg;
                    while(x<end){
                        if(bg==background_count){count=0;return false;}
                        const auto& s=background[bg];const uint32_t next=std::min(end,uint32_t(s.x)+s.width);
                        if(!emit(x,y,next-x,s.color))return false;
                        x=next;if(x==uint32_t(s.x)+s.width)++bg;
                    }
                }
            }
        }
        return true;
    }
};
}
