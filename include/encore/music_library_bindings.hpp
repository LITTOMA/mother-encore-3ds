#pragma once
#include "encore/music_regions.hpp"
namespace encore::upstream {
struct MusicLibraryFile {std::string path;uint32_t bytes=0,crc=0;};
struct MusicLibraryBinding {uint32_t track=0;MusicLibraryFile metadata,audio;};
// Independent binary file bindings. Descriptor admission is metadata-only;
// selected PCM validation and physical publication belong to the transport.
// No filenames, game song IDs, scene calls or path construction rules are code.
class MusicLibraryBindings final {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool admit(const char*asset_root,const AudioBank&source_bank,std::string&);
 bool read_track(uint32_t,MusicRegionData&,AudioBank&,std::string&)const;
 bool valid()const{return valid_;}
 bool admitted()const{return admitted_;}
 const MusicRegionData&index()const{return index_;}
 const MusicLibraryFile&index_file()const{return index_file_;}
 const std::vector<MusicLibraryBinding>&bindings()const{return bindings_;}
 const MusicLibraryBinding*find(uint32_t)const;
 const std::string&asset_root()const{return root_;}
private:
 bool valid_=false,admitted_=false;
 MusicLibraryFile index_file_;
 std::vector<MusicLibraryBinding>bindings_;
 MusicRegionData index_;
 AudioBank source_bank_;
 std::string root_;
};
}
