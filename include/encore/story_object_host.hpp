#pragma once
#include <cstdint>
#include <string>
#include <string_view>
namespace encore::upstream {
// A scene owns these checked objects. This interface admits queue_free only;
// it is not a general node lookup, script method or property dispatch facility.
class StoryObjectHost {
public:
 virtual ~StoryObjectHost()=default;
 virtual bool validate_story_object(uint32_t scene,std::string_view path,std::string&)const=0;
 virtual bool can_queue_free(std::string_view path,std::string&)const=0;
 virtual bool queue_free(std::string_view path,std::string&)=0;
};
}
