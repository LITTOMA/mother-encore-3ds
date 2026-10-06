// Resource publication uses the actual shared typed loaders and bindings.
// This does not enter scenes, execute gameplay, render or advance RNG.
#include "encore/catalog_resource_admission.hpp"
#include "encore/field_item_definitions.hpp"
#include "encore/global_yaml_caches.hpp"
#include "encore/global_yaml_file.hpp"
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
  if (argc!=2 && argc!=4 && argc!=11 && argc!=15) return 2;
  if (argc>=4 && std::string(argv[2])!="--global-items") return 2;
  if (argc>=11 && std::string(argv[4])!="--global-caches") return 2;
  if(argc==15 && (std::string(argv[11])!="--global-directory" || std::string(argv[13])!="--global-yaml-file")) return 2;
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
    if(argc==15) {
      GlobalPackedDirectoryData directory;
      GlobalYamlFileData files;
      if(!directory.load_file(argv[12],caches,registry,error)||
         !files.load_file(argv[14],caches,error)) {
        std::cerr<<"Global source Directory/File resource: "<<error<<'\n';return 1;
      }
      std::cout<<"Global source Directory/File formats admitted: "
               <<directory.files().size()<<" PCK files / "<<directory.directories().size()
               <<" directories / "<<files.records().size()<<" YAML documents\n";
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
