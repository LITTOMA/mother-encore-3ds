"""Execute the unchanged upstream emote scene in a small owned Godot project."""
import argparse,hashlib,json,re,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
PROBE='''extends SceneTree
class Owner:
    extends Node2D
    func get_direction():
        return Vector2.LEFT
func _init():
    call_deferred("_run")
func _run():
    var owner = Owner.new()
    get_root().add_child(owner)
    var emote = load("res://emotes.tscn").instance()
    owner.add_child(emote)
    var animation = emote.get_node("AnimationPlayer")
    animation.playback_process_mode = AnimationPlayer.ANIMATION_PROCESS_MANUAL
    animation.play("exclamation")
    for i in range(60):
        animation.advance(1.0 / 60.0)
        yield(self,"idle_frame")
    var result = {"godot":Engine.get_version_info(),"ended_playing":animation.is_playing(),"ended_frame":emote.frame,"ended_visible":emote.visible}
    animation.play("angry")
    animation.advance(1.0 / 60.0)
    result["replacement_frame"] = emote.frame
    result["replacement_playing"] = animation.is_playing()
    result["source_left_scale"] = emote.scale.x
    var file = File.new()
    file.open("res://result.json",File.WRITE)
    file.store_string(JSON.print(result))
    file.close()
    quit()
'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True);ex=Extractor(ROOT);source=ex.text('Nodes/Ui/emotes.tscn')
 temporary=Path(tempfile.mkdtemp(prefix='owned-emote-',dir=out));(temporary/'.owned-emote-reference').write_text('owned')
 try:
  (temporary/'project.godot').write_text('[application]\nconfig/name="Original emote reference"\n[rendering]\nquality/driver/driver_name="GLES2"\n');(temporary/'probe.gd').write_text(PROBE);(temporary/'emotes.tscn').write_text(source)
  for path in re.findall(r'^\[ext_resource path="res://([^"\n]+)"',source,re.M):
   target=temporary/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(path))
  startup=None
  if sys.platform=='win32':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
  for name,args in [('import',['--editor','--quit']),('run',['--script','res://probe.gd'])]:
   with(out/(name+'.log')).open('wb')as log:r=subprocess.run([str(a.godot.resolve()),'--path',str(temporary),*args],stdout=log,stderr=subprocess.STDOUT,startupinfo=startup,timeout=40)
   require(r.returncode==0,'Native emote '+name+' failed; actual log retained')
  result=json.loads((temporary/'result.json').read_text());result.update(sources=ex.sources,godot_sha256=hashlib.sha256(a.godot.read_bytes()).hexdigest(),probe_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),scope='Original unchanged emotes.tscn, manual native 60 Hz clock; final-frame retention and subsequent play replacement only; no full game/pixel comparison')
  (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
  require(result['godot']['major']==3 and result['godot']['minor']==6 and result['godot']['patch']==2,'Reference engine changed')
  require(not result['ended_playing']and result['ended_visible']and result['ended_frame']==64 and result['replacement_playing']and result['replacement_frame']==85 and result['source_left_scale']==-1,'Original emote retention/replacement differs')
  print('Original native emote: finished frame64 remains visible; angry replaces it with frame85')
 finally:
  require(temporary.parent==out and (temporary/'.owned-emote-reference').is_file(),'Emote temporary project ownership changed');shutil.rmtree(temporary)
if __name__=='__main__':main()
