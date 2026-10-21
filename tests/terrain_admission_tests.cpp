#include "encore/dungeon_terrain.hpp"
#include "encore/field_scene.hpp"
#include <cstdio>
int main(int argc,char**argv){if(argc!=2)return 2;encore::upstream::DungeonTerrainData data;std::string error;if(!data.load_file(argv[1],error))return 3;encore::upstream::FieldScene scene;encore::upstream::WorldLinksData links;const bool admitted=scene.prepare({},data.graphics(),links,{}, {}, {}, {},error);if(admitted||error!="Detached terrain cannot create a playable field scene")return 4;std::puts("Actual FieldScene rejects the valid detached terrain before room/link/actor admission;no playable/root claim");}
