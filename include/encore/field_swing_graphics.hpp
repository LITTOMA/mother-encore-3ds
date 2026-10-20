#pragma once
#include "encore/field_swing.hpp"
namespace encore::upstream {
struct SwingGraphicsMeta {
 uint32_t initial_frame=0,frame_width=0,frame_height=0,source_columns=0,source_rows=0,atlas_columns=0;
};
struct SwingGraphicsLayer {
 uint32_t role=0,width=0,height=0,parent=0,flags=0,bytes=0,crc=0;
 Vec2 position{},offset{};std::string path,source;
 std::array<uint8_t,32>sha256{},source_sha256{};
};
struct SwingGraphicsFrame {uint32_t source_frame=0;uint16_t x=0,y=0,width=0,height=0;};
// Independent metadata for the separately converted textures. GPU import and
// the source material remain backend-owned. No implicit fallback atlas exists.
class FieldSwingGraphicsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool bind(const FieldSwingData&,std::string&)const;
 bool valid()const{return valid_;}const SwingGraphicsMeta&meta()const{return meta_;}
 const std::array<SwingGraphicsLayer,2>&layers()const{return layers_;}
 const std::vector<SwingGraphicsFrame>&frames()const{return frames_;}
 const SwingGraphicsFrame*frame(uint32_t)const;
 // Checks actual texture bytes against the metadata's size and CRC. This is
 // an integrity check, not a T3X parser or a cryptographic authentication claim.
 bool texture_matches(uint32_t role,const uint8_t*,size_t,std::string&)const;
private:
 bool valid_=false;SwingGraphicsMeta meta_;std::array<SwingGraphicsLayer,2>layers_;
 std::vector<SwingGraphicsFrame>frames_;
};
}
