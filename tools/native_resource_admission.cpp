// Resource publication uses the actual shared typed loaders and bindings.
// This does not enter scenes, execute gameplay, render or advance RNG.
#include "encore/catalog_resource_admission.hpp"
#include "encore/field_item_definitions.hpp"
#include "encore/global_yaml_caches.hpp"
#include "encore/global_yaml_file.hpp"
#include "encore/global_data_constructor.hpp"
#include "encore/field_character_load.hpp"
#include "encore/global_load.hpp"
#include "encore/field_global_constructor.hpp"
#include "encore/player_initialization.hpp"
#include "encore/player_visual_scripts.hpp"
#include "encore/player_ready.hpp"
#include "encore/player_effects.hpp"
#include "encore/player_graphics.hpp"
#include "encore/player_motion.hpp"
#include "encore/global_ready.hpp"
#include "encore/player_fetcher.hpp"
#include "encore/player_resources.hpp"
#include "encore/house_global_bridge.hpp"
#include "encore/global_child_ready.hpp"
#include <limits>
#include <iostream>

namespace {
template<size_t N> bool hex(const char *text, std::array<uint8_t,N> &out) {
  const std::string s(text);
  if(s.size()!=2*N) return false;
  for(size_t n=0;n<N;++n) {
    auto digit=[](char c)->int {return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    int hi=digit(s[2*n]),lo=digit(s[2*n+1]);
    if(hi<0||lo<0) return false;
    out[n]=uint8_t(hi*16+lo);
  }
  return true;
}
bool number(const char *text,uint32_t &out) {
  uint64_t value=0;
  if(!*text) return false;
  for(auto p=text;*p;++p) {
    if(*p<'0'||*p>'9') return false;
    value=value*10+uint32_t(*p-'0');
    if(value>std::numeric_limits<uint32_t>::max()) return false;
  }
  out=uint32_t(value); return out!=0;
}
}
int main(int argc, char **argv) {
  using namespace encore::upstream;
  if (argc!=2 && argc!=4 && argc!=11 && argc!=15 && argc!=18 && argc!=23 && argc!=26 && argc!=30 && argc!=34 && argc!=36 && argc!=40 && argc!=42 && argc!=44 && argc!=46 && argc!=51 && argc!=53 && argc!=55) return 2;
  if (argc>=4 && std::string(argv[2])!="--global-items") return 2;
  if (argc>=11 && std::string(argv[4])!="--global-caches") return 2;
  if(argc>=15 && (std::string(argv[11])!="--global-directory" || std::string(argv[13])!="--global-yaml-file")) return 2;
  if(argc>=18 && std::string(argv[15])!="--global-constructor") return 2;
  if(argc>=23 && std::string(argv[18])!="--global-characters") return 2;
  if(argc>=26 && std::string(argv[23])!="--global-load") return 2;
  if(argc>=30 && std::string(argv[26])!="--global-node-constructor") return 2;
  if(argc>=34 && (std::string(argv[30])!="--player-initialization" || std::string(argv[32])!="--global-child-ready")) return 2;
  if(argc>=36 && std::string(argv[34])!="--player-visual-scripts") return 2;
  if(argc>=40 && (std::string(argv[36])!="--player-ready" || std::string(argv[38])!="--player-effects")) return 2;
  if(argc>=42 && std::string(argv[40])!="--player-graphics") return 2;
  if(argc>=44 && std::string(argv[42])!="--player-motion") return 2;
  if(argc>=46 && std::string(argv[44])!="--global-ready") return 2;
  if(argc>=51 && std::string(argv[46])!="--player-fetcher") return 2;
  if(argc>=53 && std::string(argv[51])!="--player-resources") return 2;
  if(argc==55 && std::string(argv[53])!="--house-global-bridge") return 2;
  const std::string root = std::string(argv[1]) + "/";
  std::string error;
  ResourceCatalog catalog;
  CatalogResourceAdmissionReport report;
  if (!catalog.load_file((root + "data/native.encresources").c_str(), error) ||
      !admit_catalog_resource_formats(catalog, root.c_str(), error, &report)) {
    std::cerr << error << '\n';
    return 1;
  }
  // The full Items constructor resource is a development dependency until
  // the complete globalData lifecycle is connected. Validate its real bytes
  // offline without adding it to the shipping catalog or startup work.
  if (argc >= 4) {
    FieldItemDefinitions global_items;
    if (!global_items.load_file(argv[3], error)) {
      std::cerr << "Global Items constructor resource: " << error << '\n';
      return 1;
    }
    if (!global_items.global_constructor_scope()) {
      error = "Expected global constructor capability";
      std::cerr << "Global Items constructor resource: " << error << '\n';
      return 1;
    }
    std::cout << "Global Items constructor format admitted: "
              << global_items.definitions().size() << " definitions\n";
  }
  if (argc>=11) {
    FieldIdentity identity;
    if(!number(argv[7],identity.scene_id)||!hex(argv[8],identity.upstream_commit)||
       !hex(argv[9],identity.source_sha256)) return 2;
    FieldGlobalRegistryData registry;
    if(!registry.load_file(argv[6],identity,error)) {
      std::cerr<<"Global registry constructor dependency: "<<error<<'\n';return 1;
    }
    const FieldGlobalAutoload *autoload=nullptr;
    for(const auto &a:registry.autoloads())
      if(a.path==argv[10] && a.script==argv[10]) {
        if(autoload) {std::cerr<<"Duplicate source cache autoload\n";return 1;}
        autoload=&a;
      }
    if(!autoload) {std::cerr<<"Actual source cache autoload absent\n";return 1;}
    FieldGlobalExternalSpec spec;
    spec.identity=registry.identity();spec.stable_id=autoload->id;spec.role=3;
    spec.name=autoload->name;spec.native_class=autoload->native_class;
    spec.source=autoload->path;spec.script=autoload->script;
    spec.source_sha=autoload->source_sha;spec.script_sha=autoload->script_sha;
    GlobalYamlCachesData caches;
    FieldItemDefinitions items;
    if(!caches.load_file(argv[5],spec,error)||!items.load_file(argv[3],error)) {
      std::cerr<<"Global source cache resource: "<<error<<'\n';return 1;
    }
    const auto expected=caches.expected_paths(4);
    if(expected.size()!=items.definitions().size()) {
      std::cerr<<"Global cache/Items independent source closure differs\n";return 1;
    }
    for(const auto &entry:expected) {
      std::array<uint8_t,32> digest{};
      const FieldItemDefinition *definition=nullptr;
      for(const auto &item:items.definitions()) if(item.source==entry.first) definition=&item;
      if(!definition||!items.source_hash(entry.first,digest)||digest!=entry.second) {
        std::cerr<<"Global cache/Items independent source entry differs\n";return 1;
      }
    }
    GlobalYamlFileData files;
    if(argc>=15) {
      GlobalPackedDirectoryData directory;
      if(!directory.load_file(argv[12],caches,registry,error)||
         !files.load_file(argv[14],caches,error)) {
        std::cerr<<"Global source Directory/File resource: "<<error<<'\n';return 1;
      }
      std::cout<<"Global source Directory/File formats admitted: "
               <<directory.files().size()<<" PCK files / "<<directory.directories().size()
               <<" directories / "<<files.records().size()<<" YAML documents\n";
    }
    FieldCharacterLoadData characters;
    GlobalLoadData load;
    GlobalDataConstructorData constructor;
    if(argc>=18) {
      FieldGlobalDataData members;
      if(!members.load_file(argv[17],identity,error)||
         !constructor.load_file(argv[16],members,caches,error)) {
        std::cerr<<"Global complete constructor format: "<<error<<'\n';return 1;
      }
      if(argc>=23) {
        FieldIdentity expected_characters;
        expected_characters.upstream_commit=identity.upstream_commit;

        if(!number(argv[20],expected_characters.scene_id)||
           !hex(argv[21],expected_characters.upstream_commit)||
           !hex(argv[22],expected_characters.source_sha256)) return 2;
        if(expected_characters.upstream_commit!=identity.upstream_commit ||
           !characters.load_file(argv[19],expected_characters,error)) {
          std::cerr<<"Global Character LOAD format: "<<error<<'\n';return 1;
        }
        if(argc>=26) {
          FieldGlobalFlagsData flags;
          if(!flags.load_file(argv[25],error) ||
             !load.load_file(argv[24],constructor,characters,flags,items,error) ||
             load.file_ir_sha256()!=files.ir_sha256()) {
            std::cerr<<"Global cold LOAD format: "<<error<<'\n';return 1;
          }
          std::cout<<"Global cold LOAD format admitted: "<<load.steps().size()
                   <<" source cursors / "<<load.documents().size()<<" documents\n";
        }
        std::cout<<"Global Character LOAD format admitted: "
                 <<characters.rows().size()<<" source owners\n";
      }
      std::cout<<"Global complete constructor format admitted: "
               <<constructor.declarations().size()<<" declarations / "
               <<constructor.objects().size()<<" owned member objects\n";
    }
    FieldGlobalConstructorData node_constructor;
    if(argc>=30) {
      FieldIdentity expected_node;
      expected_node.upstream_commit=identity.upstream_commit;
      if(!number(argv[28],expected_node.scene_id)||!hex(argv[29],expected_node.source_sha256))return 2;
      const FieldGlobalAutoload *source=nullptr;
      for(const auto &a:registry.autoloads())if(a.id==registry.global_autoload())source=&a;
      FieldGlobalExternalSpec node_spec;
      if(source){node_spec.identity=registry.identity();node_spec.stable_id=source->id;node_spec.role=3;node_spec.name=source->name;node_spec.native_class=source->native_class;node_spec.source=source->path;node_spec.script=source->script;node_spec.source_sha=source->source_sha;node_spec.script_sha=source->script_sha;}
      if(!source||!node_constructor.load_file(argv[27],expected_node,error)||!node_constructor.bind_registry(node_spec,error)){
        std::cerr<<"Global Node constructor format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Global Node constructor format admitted: "<<node_constructor.recipe().records().size()<<" source native nodes\n";
    }
    PlayerInitializationData player;
    if(argc>=34) {
      GlobalChildReadyData children;
      if(!player.load_file(argv[31],constructor,error) ||
         !children.load_file(argv[33],node_constructor,error)) {
        std::cerr<<"Player/global child format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Player/global child formats admitted: "<<player.recipe().records().size()
               <<" native Player nodes / "<<children.nodes().size()<<" global scripts\n";
    }
    PlayerVisualScriptsData visual;
    if(argc>=36) {
      if(!visual.load_file(argv[35],player,error)) {
        std::cerr<<"Player visual script format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Player visual script format admitted: "<<visual.shadow().animations.size()<<" original clips\n";
    }
    PlayerReadyData ready;
    if(argc>=40) {
      PlayerEffectsData effects;
      if(!ready.load_file(argv[37],player,error) || !effects.load_file(argv[39],player,error)) {
        std::cerr<<"Player Ready/effects format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Player Ready/effects formats admitted: "<<ready.states().size()<<" graph states / "<<effects.creators().size()<<" creators\n";
    }
    PlayerGraphicsData graphics;
    if(argc>=42) {
      if(!graphics.load_file(argv[41],visual,error)) {
        std::cerr<<"Player GPU image format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Player GPU image format admitted: "<<graphics.assets().size()<<" images\n";
    }
    if(argc>=44) {
      PlayerMotionData motion;
      if(!motion.load_file(argv[43],player,ready,error)) {
        std::cerr<<"Player motion format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Player motion format admitted\n";
    }
    if(argc>=46) {
      GlobalReadyData global_ready;
      if(!global_ready.load_file(argv[45],node_constructor,player,load,error)) {
        std::cerr<<"Global Ready format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Global Ready format admitted: "<<global_ready.policy().steps.size()<<" original cursors\n";
    }
    if(argc>=51) {
      FieldIdentity scene;scene.upstream_commit=identity.upstream_commit;
      FieldNodeTreeData scene_tree;PlayerFetcherData fetcher;
      if(!number(argv[49],scene.scene_id)||!hex(argv[50],scene.source_sha256))return 2;
      if(!scene_tree.load_file(argv[48],scene,error)||!fetcher.load_file(argv[47],player,scene_tree,node_constructor,error)) {
        std::cerr<<"Player SpriteDataFetcher format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Player SpriteDataFetcher format admitted: "<<fetcher.records().size()<<" actual script instances\n";
    }
    if(argc>=53) {
      PlayerResourcesData resources;
      if(!resources.load_file(argv[52],player,graphics,error)) {
        std::cerr<<"Player native Resource format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"Player native Resource format admitted: "<<resources.images().size()<<" textures / "<<resources.audios().size()<<" audio payloads\n";
    }
    if(argc==55) {
      NativeSessionData session;HouseGlobalBridgeData bridge;
      if(!session.load_file((root+catalog.path(ResourceRole::Session)).c_str(),error) ||
         !bridge.load_file(argv[54],characters,load,session,error)) {
        std::cerr<<"House continuation format/source: "<<error<<'\n';return 1;
      }
      std::cout<<"House continuation format admitted: "<<bridge.assignments().size()<<" source member mappings\n";
    }
    std::cout<<"Global YAML source formats admitted: "<<caches.records().size()
             <<" source records / "<<caches.getters().size()<<" getters\n";
  }
  std::cout << "Runtime binary admission: " << report.bindings << " bindings, "
            << report.singleton_formats << " startup singletons, "
            << report.encounter_battles + report.encounter_rounds
            << " encounter resources, " << report.room_effects
            << " Room effect resources; shared startup and music bindings\n";
}
