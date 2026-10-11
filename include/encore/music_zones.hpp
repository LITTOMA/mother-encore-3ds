#pragma once
#include "encore/music_regions.hpp"
#include "encore/room_data.hpp"
namespace encore::upstream {
struct MusicZone {std::string path;std::vector<std::vector<Vec2>>polygons;};
class MusicZones {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool matches(const MusicRegionData&,std::string&)const;
 bool overlaps(size_t,RoomView,Vec2)const;
 const std::vector<MusicZone>&zones()const{return zones_;}
private:std::vector<MusicZone>zones_;
};
}
