#include "encore/world_links.hpp"
#include "encore/resource_catalog.hpp"
#include <cmath>
#include <cstdio>
int main(){encore::upstream::WorldLinksData links;encore::upstream::ResourceCatalog catalog;std::string e;if(!links.load_file("build/mayor-import/world.enclinks",e)||links.capability()!=2||links.scene_count()!=3||links.route_count()!=6||!catalog.load_file("romfs/system/native.encresources",e))return 1;if(links.bind_catalog(catalog,e))return 2;const auto a=links.route(4),b=links.route(5);if(a.from!=3||a.to!=3||a.destination.x!=8||a.destination.y!=-519||a.flags||b.from!=3||b.to!=3||std::abs(b.destination.x+72)>.0001f||b.destination.y!=-63||b.flags)return 3;std::printf("Candidate source routes PASS: 3 scenes, 6 routes, internal transform destinations, no internal music fade; existing catalog correctly rejects absent Mayor resources: %s\n",e.c_str());}
