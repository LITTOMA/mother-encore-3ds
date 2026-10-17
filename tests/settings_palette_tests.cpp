// Real uncompressed T3X pixel payloads, public SDK doubles, no GPU claim.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "platform/ctr/loading_texture.hpp"
#include "encore/startup_settings.hpp"
#include "encore/field_equipment_data.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstring>
#include <iostream>
#include <set>
void Tex3DS_TextureFree(Tex3DS_Texture){}
void C3D_TexDelete(C3D_Tex*){}
int main(int argc,char**argv){
 assert(argc==3);using namespace encore;using namespace encore::ctr;
 upstream::StartupSettingsData d;std::string error;assert(d.load_file(argv[1],error));
 assert(loading_menu_flavor_configure(d.skin_paths,d.source_palette,d.palettes,d.palette_threshold,d.default_indices[1]));
 size_t checked=0,changed=0;std::vector<std::pair<std::string,bool>>paths;std::set<std::string>seen;
 for(const auto&path:d.skin_paths){paths.push_back({path,false});seen.insert(path);}
 upstream::FieldEquipmentData field;assert(field.load_file((std::string(argv[2])+"/ui/opening.encfield").c_str(),error));const auto view=field.view();
 for(uint32_t i=0;i<view.count(upstream::FieldSection::Layouts);++i){const auto l=view.layout(i);if(l.kind==uint32_t(upstream::ItemDrawKind::NinePatch)){std::string path(view.string(view.resource(l.resource).image.path));if(seen.insert(path).second)paths.push_back({path,true});}}
 size_t bound_panels=0;
 for(const auto&entry:paths){const auto&path=entry.first;
  std::ifstream stream(std::string(argv[2])+"/"+path,std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(stream)),{});assert(b.size()>21&&b[0]==1&&b[1]==0&&b[3]==0&&b[17]==0);
  const size_t size=uint32_t(b[18])|uint32_t(b[19])<<8|uint32_t(b[20])<<16;assert(size+21==b.size()&&size%4==0);
  std::vector<uint32_t>original(size/4),pixels(size/4),world(size/4);std::memcpy(original.data(),b.data()+21,size);pixels=world=original;
  LoadingSpriteSheetData skin,unmanaged;skin.source_path=path;skin.texture.data=pixels.data();skin.texture.size=size;skin.texture.fmt=GPU_RGBA8;unmanaged.source_path="unlisted-world";unmanaged.texture.data=world.data();unmanaged.texture.size=size;unmanaged.texture.fmt=GPU_RGBA8;
  assert(!loading_menu_flavor_bind_panel(nullptr,path));assert(!loading_menu_flavor_bind_panel(&skin,path)); // Not a tracked live sheet.
  assert(loading_menu_flavor_detail::prepare(&skin)&&loading_menu_flavor_detail::prepare(&unmanaged));assert(unmanaged.flavor_pixels.empty());
  loading_menu_flavor_detail::sheets={&skin,&unmanaged};
  assert(!loading_menu_flavor_bind_panel(&skin,"wrong-checked-binding"));
  if(entry.second){assert(skin.flavor_pixels.empty());assert(loading_menu_flavor_bind_panel(&skin,path));++bound_panels;assert(!skin.flavor_pixels.empty());const auto original_cells=skin.flavor_pixels;
   assert(loading_menu_flavor_select(1));const auto recolored=pixels;assert(loading_menu_flavor_bind_panel(&skin,path));assert(pixels==recolored&&skin.flavor_pixels.size()==original_cells.size());for(size_t i=0;i<original_cells.size();++i)assert(skin.flavor_pixels[i].original==original_cells[i].original&&skin.flavor_pixels[i].offset==original_cells[i].offset);assert(loading_menu_flavor_select(d.default_indices[1]));assert(pixels==original);
  }else assert(!skin.flavor_pixels.empty());
  for(uint32_t flavor=0;flavor<d.palettes.size();++flavor){assert(loading_menu_flavor_select(flavor));for(size_t i=0;i<original.size();++i){const auto raw=original[i];const uint32_t rgba=((raw&255)<<24)|((raw&0xff00)<<8)|((raw>>8)&0xff00)|(raw>>24);uint32_t expected=raw;
    for(size_t c=0;c<d.source_palette.size();++c){unsigned squared=0;for(unsigned channel=0;channel<4;++channel){const int diff=int((rgba>>(8*channel))&255)-int((d.source_palette[c]>>(8*channel))&255);squared+=unsigned(diff*diff);}if(squared<d.palette_threshold*d.palette_threshold*255*255){if(flavor!=d.default_indices[1]){const auto v=d.palettes[flavor][c];expected=((v&255)<<24)|((v&0xff00)<<8)|((v>>8)&0xff00)|(v>>24);}break;}}
    assert(pixels[i]==expected);++checked;changed+=pixels[i]!=original[i];
   }assert(world==original);}
  assert(loading_menu_flavor_select(d.default_indices[1]));assert(pixels==original);assert(!loading_menu_flavor_select(uint32_t(d.palettes.size())));loading_menu_flavor_detail::sheets.clear();
 }
 assert(changed&&bound_panels);std::cout<<"settings_palette: "<<checked<<" checked real T3X pixel transforms, "<<changed<<" changed; "<<d.skin_paths.size()<<" startup skins and "<<bound_panels<<" checked field panels; default restore, repeated binding and unlisted world bytes unchanged\n";
}
