// Real uncompressed T3X pixel payloads, public SDK doubles, no GPU claim.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "platform/ctr/loading_texture.hpp"
#include "encore/startup_settings.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstring>
#include <iostream>
void Tex3DS_TextureFree(Tex3DS_Texture){}
void C3D_TexDelete(C3D_Tex*){}
int main(int argc,char**argv){
 assert(argc==3);using namespace encore;using namespace encore::ctr;
 upstream::StartupSettingsData d;std::string error;assert(d.load_file(argv[1],error));
 assert(loading_menu_flavor_configure(d.skin_paths,d.source_palette,d.palettes,d.palette_threshold,d.default_indices[1]));
 size_t checked=0,changed=0;
 for(const auto&path:d.skin_paths){
  std::ifstream stream(std::string(argv[2])+"/"+path,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(stream)),{});assert(b.size()>21&&b[0]==1&&b[1]==0&&b[3]==0&&b[17]==0);
  const size_t size=uint32_t(b[18])|uint32_t(b[19])<<8|uint32_t(b[20])<<16;assert(size+21==b.size()&&size%4==0);
  std::vector<uint32_t>original(size/4),pixels(size/4),world(size/4);std::memcpy(original.data(),b.data()+21,size);pixels=world=original;
  LoadingSpriteSheetData skin,unmanaged;skin.source_path=path;skin.texture.data=pixels.data();skin.texture.size=size;skin.texture.fmt=GPU_RGBA8;unmanaged.source_path="unlisted-world";unmanaged.texture.data=world.data();unmanaged.texture.size=size;unmanaged.texture.fmt=GPU_RGBA8;
  assert(loading_menu_flavor_detail::prepare(&skin)&&loading_menu_flavor_detail::prepare(&unmanaged));assert(!skin.flavor_pixels.empty()&&unmanaged.flavor_pixels.empty());
  loading_menu_flavor_detail::sheets={&skin,&unmanaged};
  for(uint32_t flavor=0;flavor<d.palettes.size();++flavor){assert(loading_menu_flavor_select(flavor));for(size_t i=0;i<original.size();++i){const auto raw=original[i];const uint32_t rgba=((raw&255)<<24)|((raw&0xff00)<<8)|((raw>>8)&0xff00)|(raw>>24);uint32_t expected=raw;
    for(size_t c=0;c<d.source_palette.size();++c){unsigned squared=0;for(unsigned channel=0;channel<4;++channel){const int diff=int((rgba>>(8*channel))&255)-int((d.source_palette[c]>>(8*channel))&255);squared+=unsigned(diff*diff);}if(squared<d.palette_threshold*d.palette_threshold*255*255){if(flavor!=d.default_indices[1]){const auto v=d.palettes[flavor][c];expected=((v&255)<<24)|((v&0xff00)<<8)|((v>>8)&0xff00)|(v>>24);}break;}}
    assert(pixels[i]==expected);++checked;changed+=pixels[i]!=original[i];
   }assert(world==original);}
  assert(loading_menu_flavor_select(d.default_indices[1]));assert(pixels==original);assert(!loading_menu_flavor_select(uint32_t(d.palettes.size())));loading_menu_flavor_detail::sheets.clear();
 }
 assert(changed);std::cout<<"settings_palette: "<<checked<<" checked real T3X pixel transforms, "<<changed<<" changed; "<<d.skin_paths.size()<<" source UI skins; default restore and unlisted world bytes unchanged\n";
}
