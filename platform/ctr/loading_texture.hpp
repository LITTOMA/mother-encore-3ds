#pragma once
#include <citro2d.h>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <new>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "encore/load_progress.hpp"

namespace encore::ctr {
namespace loading_texture_detail {
constexpr size_t read_chunk_bytes = 8192;
struct Input {
    FILE* file;
    uint64_t total;
    uint64_t completed = 0;
    bool failed = false;
};

// Tex3DS may request the entire uncompressed image in one callback. Fulfil that
// request directly in its destination, yielding between bounded file reads.
// This is synchronous: observers may draw the loading UI on the main thread,
// but must not access the texture currently being imported.
inline ssize_t read(void* context, void* destination, size_t size) {
    auto& input = *static_cast<Input*>(context);
    if (input.failed || size > size_t(std::numeric_limits<ssize_t>::max())) return -1;
    auto* bytes = static_cast<unsigned char*>(destination);
    size_t consumed = 0;
    while (consumed < size) {
        const size_t amount = std::min(read_chunk_bytes, size - consumed);
        const size_t received = std::fread(bytes + consumed, 1, amount, input.file);
        consumed += received;
        input.completed += received;
        if (received) report_load_progress(LoadPhase::Texture, input.completed, input.total);
        if (std::ferror(input.file)) {
            input.failed = true;
            return -1;
        }
        if (received != amount) {
            // A clean short read is valid decoder read-ahead, not failure:
            // libctru requests 16 KiB even for a complete 512-byte texture.
            // Return the useful bytes; Tex3DS rejects genuinely truncated data
            // if its decoder cannot produce the required output.
            break;
        }
    }
    return static_cast<ssize_t>(consumed);
}
}

// Own our stable texture and the public Tex3DS metadata handle. No dependency
// on Citro2D's private sprite-sheet layout, and no additional whole-file copy.
struct LoadingSpriteSheetData {
    C3D_Tex texture{};
    std::string source_path;
    size_t references=1;
    struct FlavorPixel {uint32_t offset,original;uint8_t slot;};
    std::vector<FlavorPixel> flavor_pixels;
    Tex3DS_Texture metadata = nullptr;
    std::vector<Tex3DS_SubTexture> prepared_subtextures;bool prepared_texture=false;
    LoadingSpriteSheetData() = default;
    LoadingSpriteSheetData(const LoadingSpriteSheetData&) = delete;
    LoadingSpriteSheetData& operator=(const LoadingSpriteSheetData&) = delete;
    ~LoadingSpriteSheetData() {
        if(metadata)Tex3DS_TextureFree(metadata);
        if(metadata||prepared_texture)C3D_TexDelete(&texture);
    }
};
using LoadingSpriteSheet = LoadingSpriteSheetData*;

namespace loading_menu_flavor_detail {
inline std::vector<LoadingSpriteSheet> sheets;
inline std::vector<std::string> paths;
inline std::array<uint32_t,8> source{};
inline std::vector<std::array<uint32_t,8>> palettes;
inline uint32_t selected=0,base_index=0;inline double threshold=0;
inline uint32_t reverse(uint32_t v){return ((v&0xff)<<24)|((v&0xff00)<<8)|((v>>8)&0xff00)|(v>>24);}
inline int slot(uint32_t rgba){for(size_t i=0;i<source.size();++i){unsigned distance=0;for(unsigned b=0;b<4;++b){const int d=int((rgba>>(8*b))&255)-int((source[i]>>(8*b))&255);distance+=unsigned(d*d);}if(double(distance)<threshold*threshold*255*255)return int(i);}return -1;}
inline bool prepare(LoadingSpriteSheet sheet){
 if(palettes.empty()||std::find(paths.begin(),paths.end(),sheet->source_path)==paths.end())return true;
 if(sheet->texture.fmt!=GPU_RGBA8||sheet->texture.size%4||!sheet->texture.data)return false;
 const auto*data=static_cast<const uint32_t*>(sheet->texture.data);sheet->flavor_pixels.clear();
 for(uint32_t i=0;i<sheet->texture.size/4;++i){const int index=slot(reverse(data[i]));if(index>=0)sheet->flavor_pixels.push_back({i,data[i],uint8_t(index)});}
 return !sheet->flavor_pixels.empty();
}
inline void apply(LoadingSpriteSheet sheet){if(sheet->flavor_pixels.empty())return;auto*p=static_cast<uint32_t*>(sheet->texture.data);for(const auto&cell:sheet->flavor_pixels)p[cell.offset]=selected!=base_index?reverse(palettes[selected][cell.slot]):cell.original;C3D_TexFlush(&sheet->texture);}
}
// Call only at a GPU-idle boundary. The allowlist is external reviewed UI
// identities, never a global replacement across actors, maps or backgrounds.
inline bool loading_menu_flavor_configure(const std::vector<std::string>&paths,const std::array<uint32_t,8>&source,const std::vector<std::array<uint32_t,8>>&palettes,double threshold,uint32_t base_index){
 using namespace loading_menu_flavor_detail;if(paths.empty()||palettes.empty()||base_index>=palettes.size()||threshold<=0||threshold>1||!loading_menu_flavor_detail::palettes.empty())return false;
 loading_menu_flavor_detail::paths=paths;loading_menu_flavor_detail::source=source;loading_menu_flavor_detail::palettes=palettes;loading_menu_flavor_detail::threshold=threshold;loading_menu_flavor_detail::base_index=base_index;selected=base_index;
 for(auto*sheet:sheets)if(!prepare(sheet))return false;
 return true;
}
// Register additional source-reviewed UI skins before loading their textures.
// The caller must bind these paths and the complete palette to its checked
// binary resource. GPU-idle ownership remains with the scene loader.
inline bool loading_menu_flavor_register_checked_paths(
 const std::vector<std::string>& additions,
 const std::array<uint32_t,8>& source,
 const std::vector<std::array<uint32_t,8>>& palettes,
 double threshold,uint32_t base_index) {
 using namespace loading_menu_flavor_detail;
 if(additions.empty()||loading_menu_flavor_detail::palettes.empty()||
    source!=loading_menu_flavor_detail::source||
    palettes!=loading_menu_flavor_detail::palettes||
    !std::isfinite(threshold)||threshold!=loading_menu_flavor_detail::threshold||
    base_index!=loading_menu_flavor_detail::base_index)return false;
 auto next=paths;
 for(size_t i=0;i<additions.size();++i){
  const auto&path=additions[i];
  if(path.compare(0,9,"graphics/")||path.size()<13||
     path.compare(path.size()-4,4,".t3x")||
     path.find("..")!=std::string::npos||path.find('\\')!=std::string::npos||
     std::find(additions.begin(),additions.begin()+i,path)!=additions.begin()+i)return false;
  if(std::find(next.begin(),next.end(),path)!=next.end())continue;
  for(const auto*sheet:sheets)if(sheet&&sheet->source_path==path)return false;
  next.push_back(path);
 }
 paths.swap(next);
 return true;
}
inline uint32_t loading_menu_flavor_selected(){return loading_menu_flavor_detail::selected;}
inline bool loading_menu_flavor_select(uint32_t index){using namespace loading_menu_flavor_detail;if(index>=palettes.size())return false;if(index==selected)return true;selected=index;for(auto*sheet:sheets)apply(sheet);return true;}
inline uint32_t loading_menu_flavor_color(uint32_t color){using namespace loading_menu_flavor_detail;if(selected==base_index||palettes.empty())return color;const auto alpha=color&0xff000000u;const int i=slot(color|0xff000000u);return i<0?color:(palettes[selected][size_t(i)]&0x00ffffffu)|alpha;}
inline void loading_sprite_sheet_free(LoadingSpriteSheet sheet) {if(!sheet||--sheet->references)return;auto&v=loading_menu_flavor_detail::sheets;v.erase(std::remove(v.begin(),v.end(),sheet),v.end());delete sheet;}

inline LoadingSpriteSheet loading_sprite_sheet_load(const char* filename, std::string* error = nullptr) {
    if (error) error->clear();
    const auto failed = [&](const std::string& reason) -> LoadingSpriteSheet {
        if (error) *error = reason + (filename ? std::string(": ") + filename : "");
        return nullptr;
    };
    if (!filename) return failed("Texture path is null");
    FILE* file = std::fopen(filename, "rb");
    if (!file) return failed("Texture file could not be opened");
    // Bound stdio read-ahead too; buffering does not require a second image.
    std::setvbuf(file, nullptr, _IOFBF, loading_texture_detail::read_chunk_bytes);
    const bool seek_end = std::fseek(file, 0, SEEK_END) == 0;
    const long length = seek_end ? std::ftell(file) : -1;
    if (length <= 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return failed("Texture file is empty or not seekable");
    }
    auto* sheet = new (std::nothrow) LoadingSpriteSheetData;
    if (!sheet) {
        std::fclose(file);
        return failed("Texture owner allocation failed");
    }
    loading_texture_detail::Input input{file, static_cast<uint64_t>(length)};
    report_load_progress(LoadPhase::Texture, 0, input.total);
    sheet->metadata = Tex3DS_TextureImportCallback(
        &sheet->texture, nullptr, false, loading_texture_detail::read, &input);
    const bool close_ok = std::fclose(file) == 0;
    if (!sheet->metadata || input.failed || !close_ok) {
        // Tex3DS 1.7.1 cleans its allocations on import failure. Only a
        // successful metadata handle transfers the GPU texture to this owner.
        std::string reason = input.failed ? "Texture file read failed" :
            (!close_ok ? "Texture file close failed" : "Texture decode/allocation failed");
#ifdef __3DS__
        if (!input.failed && close_ok) reason += " (linear free " + std::to_string(linearSpaceFree()) + " bytes)";
#endif
        delete sheet;
        return failed(reason);
    }
    sheet->source_path=filename;
    const std::string prefix="romfs:/";if(sheet->source_path.compare(0,prefix.size(),prefix)==0)sheet->source_path.erase(0,prefix.size());
    if(!loading_menu_flavor_detail::prepare(sheet)){delete sheet;return failed("Texture palette metadata rejected");}
    loading_menu_flavor_detail::sheets.push_back(sheet);loading_menu_flavor_detail::apply(sheet);
    // Match C2D_SpriteSheetLoad's transparent border and wrap configuration.
    sheet->texture.border = 0;
    C3D_TexSetWrap(&sheet->texture, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    return sheet;
}

// Explicit presentation sharing. Call only on the owner/main thread. Identical
// immutable resource paths share their exact decoded texture and flavor state.
inline std::string loading_texture_key(const char* filename){
    std::string key=filename?filename:"";const std::string prefix="romfs:/";
    if(key.compare(0,prefix.size(),prefix)==0)key.erase(0,prefix.size());
    return key;
}
inline LoadingSpriteSheet loading_sprite_sheet_resident(const char* filename){
    const auto key=loading_texture_key(filename);
    for(auto* sheet:loading_menu_flavor_detail::sheets)if(sheet->source_path==key)return sheet;
    return nullptr;
}
inline LoadingSpriteSheet loading_sprite_sheet_acquire(const char* filename,std::string* error=nullptr){
    if(error)error->clear();
    if(auto* sheet=loading_sprite_sheet_resident(filename)){++sheet->references;return sheet;}
    return loading_sprite_sheet_load(filename,error);
}
inline LoadingSpriteSheet loading_sprite_sheet_acquire_memory(const char* filename,const std::vector<uint8_t>& bytes,std::string& error){
    error.clear();
    if(auto* sheet=loading_sprite_sheet_resident(filename)){++sheet->references;return sheet;}
    if(bytes.empty()){error="Prewarmed texture owner expired: "+std::string(filename);return nullptr;}
    auto* sheet=new(std::nothrow) LoadingSpriteSheetData;
    if(!sheet){error="Prewarmed texture owner allocation failed";return nullptr;}
    sheet->metadata=Tex3DS_TextureImport(bytes.data(),bytes.size(),&sheet->texture,nullptr,false);
    if(!sheet->metadata){delete sheet;error="Prewarmed texture decode/allocation failed: "+std::string(filename);return nullptr;}
    sheet->source_path=loading_texture_key(filename);
    if(!loading_menu_flavor_detail::prepare(sheet)){delete sheet;error="Prewarmed texture palette rejected";return nullptr;}
    loading_menu_flavor_detail::sheets.push_back(sheet);loading_menu_flavor_detail::apply(sheet);
    sheet->texture.border=0;C3D_TexSetWrap(&sheet->texture,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);return sheet;
}

inline size_t loading_sprite_sheet_count(LoadingSpriteSheet sheet) {
    return sheet ? (sheet->prepared_texture?sheet->prepared_subtextures.size():Tex3DS_GetNumSubTextures(sheet->metadata)) : 0;
}

inline C2D_Image loading_sprite_sheet_get_image(LoadingSpriteSheet sheet, size_t index) {
    if (!sheet) return {nullptr, nullptr};
    return {&sheet->texture,sheet->prepared_texture?(index<sheet->prepared_subtextures.size()?&sheet->prepared_subtextures[index]:nullptr):Tex3DS_GetSubTexture(sheet->metadata,index)};
}
}
