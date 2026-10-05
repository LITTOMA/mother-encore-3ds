#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
enum class DrawerSection:uint16_t {Strings=1,Templates,Commands,Binding};
enum class DrawerOpcode:uint32_t {ShowText=1,AwaitText,BranchFlag,BranchSpace,GrantItem,PlaySound,SetFlag,Jump,End};
struct DrawerItemTemplate {uint32_t id=0,source=0,doses=0,key_item=0;};
struct DrawerCommand {uint32_t opcode=0,a=0,b=0,c=0,d=0;};
struct DrawerBinding {uint32_t source_path=0,inspection_source=0;};
class DrawerProgramView {
public:
 bool valid()const{return bytes_!=nullptr;}
 explicit operator bool()const{return valid();}
 uint32_t count(DrawerSection)const;std::string_view string(uint32_t)const;
 DrawerItemTemplate item_template(uint32_t)const;DrawerCommand command(uint32_t)const;
 DrawerBinding binding()const;
 const uint8_t*reviewed_commit()const{return bytes_?bytes_+32:nullptr;}
private:
 friend class DrawerProgramData;const uint8_t*bytes_=nullptr;size_t size_=0;
 const uint8_t*record(DrawerSection,uint32_t)const;
};
class DrawerProgramData {
public:
 DrawerProgramData()=default;DrawerProgramData(const DrawerProgramData&)=delete;DrawerProgramData&operator=(const DrawerProgramData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 // Successful reload invalidates borrowed views; rejected reload preserves them.
 DrawerProgramView view()const{DrawerProgramView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
class DrawerHost {
public:
 virtual ~DrawerHost()=default;
 // Validators are read-only. All referenced content is checked before execution.
 virtual bool validate_text(uint32_t,std::string&)=0;
 virtual bool validate_flag(std::string_view,std::string&)=0;
 virtual bool validate_item(DrawerItemTemplate,std::string_view,std::string&)=0;
 virtual bool validate_sound(std::string_view,std::string&)=0;
 virtual bool show_text(uint32_t,std::string&)=0;
 virtual bool flag(std::string_view,bool&,std::string&)=0;
 virtual bool inventory_space()const=0;
 virtual bool grant_item(DrawerItemTemplate,std::string_view,std::string&)=0;
 virtual bool play_sound(std::string_view,std::string&)=0;
 virtual bool set_flag(std::string_view,bool,std::string&)=0;
};
enum class DrawerState:uint8_t {Idle,WaitingText,Complete,Failed};
class DrawerProgramRuntime {
public:
 bool start(DrawerProgramView,DrawerHost&,std::string&);
 // Only call after the host's entire displayed text span has been acknowledged.
 bool advance_text(std::string&);
 DrawerState state()const{return state_;}uint32_t pc()const{return pc_;}
 void reset(){*this=DrawerProgramRuntime{};}
private:
 bool step(std::string&);DrawerProgramView view_;DrawerHost*host_=nullptr;
 uint32_t pc_=0;DrawerState state_=DrawerState::Idle;
};
}
