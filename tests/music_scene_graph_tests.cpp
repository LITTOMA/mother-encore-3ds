#include "music_scene_graph_reference.hpp"
namespace {
std::string digest(const MusicSceneGraph&g){
 std::ostringstream out;out<<g.scene_epoch()<<g.active()<<g.players().ready()<<g.players().cleanup_waiting()<<g.players().deferred_finished_count()<<std::hexfloat;
 for(const auto&n:g.players().nodes())out<<' '<<n.identity<<' '<<n.generation<<' '<<n.stream<<' '<<n.finished_loop<<' '<<n.finished_index<<' '<<n.remove_on_finished<<' '<<n.gain_db<<' '<<n.playing<<' '<<n.pending_delete<<' '<<n.ensure_empty_on_delete<<' '<<n.tween.from<<' '<<n.tween.to<<' '<<n.tween.duration<<' '<<n.tween.elapsed<<' '<<n.tween.active;
 for(const auto&a:g.areas())out<<' '<<a.player<<a.inside<<a.registered<<a.pending_exit<<a.removed;
 for(auto id:g.registered())out<<' '<<id;
 return out.str();
}
void negative(const MusicRegionData&data){
 MusicSceneGraph g;MusicRegionContext context;context.flag=[](std::string_view){return false;};MusicRegionData invalid;const auto path=data.regions()[0].source_path;
 auto before=digest(g);auto reject=[&](bool result){CHECK(!result&&digest(g)==before);};
 reject(g.attach_scene(1,data,error));reject(g.enter(1,path,context,error));reject(g.exit(1,path,context,error));reject(g.idle_frame(1,error));reject(g.tree_exit(1,path,error));reject(g.stop_area(1,path,1,error));reject(g.stop_on_door(1,1,error));reject(g.finish_scene(1,error));reject(g.advance(0,false,error));reject(g.flush_deletions(error));
 CHECK(g.initialize({data.silence_db(),0,data.fade_to_seconds(),2},error));before=digest(g);reject(g.initialize({data.silence_db(),0,data.fade_to_seconds(),2},error));reject(g.attach_scene(0,data,error));reject(g.attach_scene(1,invalid,error));
 {auto retained=data;CHECK(g.attach_scene(1,retained,error));}before=digest(g);reject(g.attach_scene(2,data,error));reject(g.enter(1,"Unbound",context,error));reject(g.exit(1,"Unbound",context,error));reject(g.tree_exit(1,"Unbound",error));reject(g.stop_area(1,path,-1,error));reject(g.stop_on_door(1,NAN,error));reject(g.finish_scene(1,error));
 CHECK(g.enter(1,path,context,error));CHECK(g.data().regions()[0].source_path==path);before=digest(g);
 CHECK(g.enter(0,"Unbound",context,error)&&digest(g)==before);CHECK(g.exit(9,"Unbound",context,error)&&digest(g)==before);CHECK(g.tree_exit(9,"Unbound",error)&&digest(g)==before);CHECK(g.stop_area(9,"Unbound",NAN,error)&&digest(g)==before);CHECK(g.stop_on_door(9,NAN,error)&&digest(g)==before);CHECK(g.idle_frame(9,error)&&digest(g)==before);CHECK(g.finish_scene(9,error)&&digest(g)==before);
 for(const auto&r:data.regions())if(!r.appear_flag.empty()||!r.disappear_flag.empty()){auto absent=context;absent.flag={};reject(g.enter(1,r.source_path,absent,error));break;}
 // A fully occupied global player list must preserve scene registrations and
 // all previous nodes when the source decision needs another player.
 uint64_t id;CHECK(g.players().add_player(id,error));CHECK(g.players().play_loop(id,"res://fixture/other",error));before=digest(g);for(const auto&r:data.regions())if(r.track_id!=data.regions()[0].track_id&&r.appear_flag.empty()&&r.disappear_flag.empty()){reject(g.enter(1,r.source_path,context,error));break;}
 auto copy=g;CHECK(copy.players().nodes()[0].stream==g.players().nodes()[0].stream);CHECK(copy.stop_area(1,path,0,error));CHECK(digest(g)==before);
 for(const auto&r:data.regions())CHECK(g.tree_exit(1,r.source_path,error));CHECK(g.finish_scene(1,error));before=digest(g);reject(g.enter(1,path,context,error));reject(g.exit(1,path,context,error));reject(g.idle_frame(1,error));reject(g.tree_exit(1,path,error));reject(g.finish_scene(1,error));reject(g.stop_on_door(1,0,error));reject(g.attach_scene(1,data,error));
 CHECK(g.attach_scene(2,data,error));CHECK(g.enter(1,"Unbound",context,error));CHECK(g.players().nodes().size()==2);
 MusicSceneGraph wrong;CHECK(wrong.initialize({data.silence_db(),0,data.fade_to_seconds()+1,16},error));CHECK(!wrong.attach_scene(1,data,error));
 // The callback itself does not check a disabled CollisionShape: that belongs
 // to the geometry emitter. Negative contexts are checked above the flags.
 CHECK(g.players().stop(1,error));CHECK(g.enter(2,path,context,error));CHECK(g.exit(2,path,context,error));for(unsigned i=0;i<1023;++i)CHECK(g.exit(2,path,context,error));before=digest(g);reject(g.exit(2,path,context,error));CHECK(g.idle_frame(2,error));
}
}
int main(int argc,char**argv){CHECK(argc==4);MusicRegionData podunk,mayor;CHECK(podunk.load_file(argv[1],error));CHECK(mayor.load_file(argv[2],error));reference(podunk,mayor,argv[3]);negative(podunk);std::printf("Music scene graph: %u checks; 10 original scene callback sequences, global ordered loop nodes, deferred exits, source flags/UI and negative ownership/epoch/capacity rollback; no physical bank/DSP integration claim\n",checks);}
