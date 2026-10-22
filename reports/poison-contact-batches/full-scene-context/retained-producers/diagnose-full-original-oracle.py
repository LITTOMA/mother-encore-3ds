from pathlib import Path
import argparse,json,os,subprocess,time
ap=argparse.ArgumentParser();ap.add_argument('--name',default='diagnostic-original-boot-3');ap.add_argument('--warm',action='store_true');args=ap.parse_args()
r=Path.cwd();out=r/'build/full-port/full-original-oracle-1';p=out/'project';source=r/'upstream/MOTHER-Encore';adapter=p/'oracle-diagnostic';adapter.mkdir(exist_ok=True)
wrapper='''extends "res://Scripts/global/global.gd"
func _init_player():
 print("ORACLE before _init_player")
 ._init_player()
 print("ORACLE after _init_player")
func _load_default_save():
 print("ORACLE before _load_default_save")
 ._load_default_save()
 print("ORACLE after _load_default_save")
func _load_dict_to_game(save_data: Dictionary, goto_game := true):
 print("ORACLE before _load_dict_to_game ", goto_game)
 var result = ._load_dict_to_game(save_data, goto_game)
 print("ORACLE after _load_dict_to_game")
 return result
'''
(adapter/'observed-global.gd').write_text(wrapper,encoding='utf8')
scene=(source/'Scripts/global/global.tscn').read_text('utf8').replace('res://Scripts/global/global.gd','res://oracle-diagnostic/observed-global.gd');(adapter/'observed-global.tscn').write_text(scene,encoding='utf8')
(adapter/'observer.gd').write_text('''extends Node
var frames := 0
func _ready():
 print("ORACLE all autoloads Ready")
func _process(_delta):
 frames += 1
 if frames == 10:
  print("ORACLE main ten idle frames ", get_tree().current_scene.name)
  get_tree().quit()
''',encoding='utf8')
if args.warm:
 (adapter/'observer.gd').write_text('''extends Node
var frames := 0
func _ready():
 print("ORACLE all autoloads Ready")
func _process(_delta):
 frames += 1
 if frames in [1, 10, 60, 120, 300, 600]:
  print("ORACLE warm frame ", frames, " caches ", Cache.cache_.size(), " materials ", Cache.materials.size())
 if frames >= 300 and Cache.cache_.size() == 2 and Cache.materials.empty():
  print("ORACLE warm original before clean quit ", get_tree().current_scene.name)
  get_tree().quit()
''',encoding='utf8')
config=(source/'project.godot').read_text('utf8').replace('global="*res://Scripts/global/global.tscn"','global="*res://oracle-diagnostic/observed-global.tscn"').replace('[debug]','OracleDiagnostic="*res://oracle-diagnostic/observer.gd"\n\n[debug]');assert config.count('OracleDiagnostic=')==1;(p/'project.godot').write_text(config,encoding='utf8')
env=dict(os.environ)
for key,child in [('APPDATA','user-roaming'),('LOCALAPPDATA','user-local'),('XDG_DATA_HOME','user-data'),('XDG_CONFIG_HOME','user-config')]:env[key]=str(out/child)
cmd=[str(r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe'),'--path',str(p),'--no-window','--audio-driver','Dummy','--video-driver','GLES2'];start=time.monotonic();code=None;timed_out=False
assert not(out/(args.name+'.log')).exists()
with (out/(args.name+'.log')).open('wb')as log:
 try:result=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=45,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));code=result.returncode
 except subprocess.TimeoutExpired:timed_out=True
(out/(args.name+'.json')).write_text(json.dumps(dict(command=cmd,return_code=code,timed_out=timed_out,elapsed_seconds=time.monotonic()-start,scope='Diagnostic wrappers only;preserve source methods;not accepted game boot.'),indent=2)+'\n');print('Diagnostic result',code,'timed out',timed_out)
