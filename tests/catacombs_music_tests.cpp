#include "music_scene_graph_reference.hpp"
#include "encore/music_zones.hpp"
#include <string>
int main(int argc,char**argv){
 CHECK(argc==2);const std::string root=argv[1];MusicRegionData data;MusicZones zones;AudioBank bank;
 CHECK(data.load_file((root+"/source.encmusic").c_str(),error));CHECK(zones.load_file((root+"/source.enczones").c_str(),error));CHECK(bank.load_file((root+"/source.encaudio").c_str(),error));CHECK(data.matches(bank,error)&&zones.matches(data,error));CHECK(data.regions().size()==5&&data.tracks().size()==2&&bank.count()==2);
 std::ifstream geometry(root+"/geometry.tsv");CHECK(bool(geometry));size_t i=0;std::string path,appear,disappear;float gain,fadein,fadeout;unsigned shapes;
 while(geometry>>path>>gain>>fadein>>fadeout>>appear>>disappear>>shapes){
  CHECK(i<data.regions().size());const auto&r=data.regions()[i];const auto&z=zones.zones()[i];CHECK(r.source_path==path&&z.path==path&&r.volume_db==gain&&r.fadein_seconds==fadein&&r.fadeout_seconds==fadeout&&r.appear_flag==(appear=="-"?"":appear)&&r.disappear_flag==(disappear=="-"?"":disappear)&&!r.disabled&&z.polygons.size()==shapes);
  for(unsigned j=0;j<shapes;++j){unsigned points;CHECK(bool(geometry>>points)&&points==z.polygons[j].size());for(unsigned k=0;k<points;++k){float x,y;CHECK(bool(geometry>>x>>y)&&z.polygons[j][k].x==x&&z.polygons[j][k].y==y);}}++i;
 }
 CHECK(i==5);for(unsigned j=0;j<bank.count();++j){const auto a=bank.asset(j);CHECK(a.loops()&&a.loop_start>0&&a.loop_start<a.frames&&a.gain_db==0);AudioFrameCursor cursor;CHECK(cursor.reset(a.frames,a.loop_start,true));uint32_t first=0;CHECK(cursor.take(a.frames,first)==a.frames&&first==0);CHECK(cursor.take(1,first)==1&&first==a.loop_start);}
 reference(data,data,(root+"/scenes.tsv").c_str());
 std::printf("Catacombs music:%u checks;92 original callback observations,5 original geometry/flag/gain records,actual converted audio metadata and loop cursors;no NDSP/audible/full scene/hardware claim\n",checks);
}
