"""Run unchanged source HP/text methods in an isolated Godot3.6.2 wrapper.
External menu refresh and text sound are explicit inert boundaries.
"""
from pathlib import Path
import json,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor
from tools.reference_battle_entry import function
ex=Extractor(ROOT);work=ROOT/'build/round-presentation-native';work.mkdir(parents=True,exist_ok=True)
plate=ex.text('Scripts/UI/Battle/PartyInfoPlate.gd')
consts='\n'.join(re.findall(r'^const .+$',plate,re.M))
prefix='''extends Node
signal hp_scroll_done
var _target_hp=62
var _cur_hp=62
var _max_hp=62
var _dhp_frame=0
var _hp_timer:float=0
var _are_hp_scrolling=false
var _are_hp_increasing=false
var _frame_time=1.0/30.0
var base_scroll_speed:=1.0
var _ailment_scroll_multiplier=1.0
var user_fast_mode=false
var user_defending=false
var _ones_digit_hp
var _tens_digit_hp
var _huns_digit_hp
func refresh_menu_plate(): pass
func init():
	var exclamation=Control.new()
	exclamation.name="HPExclamation"
	add_child(exclamation)
	var ap=AnimationPlayer.new()
	ap.name="AnimationPlayer"
	exclamation.add_child(ap)
	var digits=[]
	for i in 3:
		var sprite=Sprite.new()
		sprite.hframes=8
		sprite.vframes=10
		add_child(sprite)
		digits.append(Digit.new(sprite,i==2))
	_ones_digit_hp=digits[0]
	_tens_digit_hp=digits[1]
	_huns_digit_hp=digits[2]
	set_instant_hp(62)
'''
prefix+='\n'+consts+'\n'+plate[plate.index('class Digit:'):plate.index('const TRANSITION_FRAMES')]
for name in ['set_instant_hp','set_target_hp','_process_hp','_get_scroll_speed','_hide_leading_zeros','_hide_exclamation','get_current_hp']:prefix+='\n'+function(plate,name)
(work/'plate.gd').write_text(prefix)
a=ex.text('Scripts/UI/AbstractDialogueBox.gd');battle=ex.text('Scripts/UI/Battle/BattleDialogueBox.gd');tools=ex.text('Scripts/global/text_tools.gd')
cs='\n'.join(re.findall(r'^const (?:SPEED_UP|NORMAL_SPEED|FASTER_SPEED|SLOWER_SPEED)[^\n]+',a,re.M))
chars='\n'.join('\t'+line for line in tools.splitlines()if line.startswith('const CHAR_'))
t='''extends Node
var _speed_multiplier_from_input=1.0
var _speed_multiplier_from_tags=1.0
var _t=0.0
var _finished=false
var _stopped=false
var _auto_advance=true
var _dialogue_label
var _cursor_down_sprite
var globaldata={"text_speed":0.02}
func _has_remaining_segments():return false
func _print_dialogue_segment(_a):assert(false)
func _next_phrase():_finished=true
func init():
	_dialogue_label=RichTextLabel.new()
	_dialogue_label.bbcode_enabled=true
	_dialogue_label.bbcode_text="\\nNinten attacks!"
	_dialogue_label.visible_characters=0
	add_child(_dialogue_label)
	_cursor_down_sprite=Sprite.new()
	add_child(_cursor_down_sprite)
	var audio=AudioStreamPlayer.new()
	audio.name="AudioStreamPlayer"
	add_child(audio)
	var timer=Timer.new()
	timer.name="Timer"
	add_child(timer)
'''+cs+'\nclass TextTools:\n'+chars+'\n'
for name in ['_advance_printing','_get_no_br_dialog_content','_get_last_visible_char','_get_text_speed','_action_press','_stop_phrase']:t+='\n'+function(a,name)
t+='\n'+function(battle,'_finish_phrase');(work/'text.gd').write_text(t)
(work/'project.godot').write_text('config_version=4\n[logging]\nfile_logging/enable_logging=false\n')
(work/'probe.gd').write_text('''extends SceneTree
func _init():
	var version=Engine.get_version_info()
	assert(version.major==3 and version.minor==6 and version.patch==2)
	var plate=load("res://plate.gd").new()
	get_root().add_child(plate)
	plate.init()
	plate.set_target_hp(60)
	var text=load("res://text.gd").new()
	get_root().add_child(text)
	text.init()
	var rows=[]
	for i in 100:
		plate._process_hp(1.0/60.0)
		text._advance_printing(1.0/60.0)
		rows.append([i+1,plate._cur_hp,plate._dhp_frame,plate.get_current_hp(),plate._ones_digit_hp._sprite.frame,plate._tens_digit_hp._sprite.frame,plate._huns_digit_hp._sprite.frame,plate._are_hp_scrolling,text._dialogue_label.visible_characters,text._finished])
	var file=File.new()
	file.open(OS.get_environment("ROUND_PROBE_OUT"),File.WRITE)
	file.store_string(JSON.print({"version":version,"rows":rows}))
	file.close()
	quit()
''')
import os
env=dict(os.environ,XDG_DATA_HOME=str(work/'userdata'),ROUND_PROBE_OUT=str(ROOT/'reports/battle-round-presentation/native-hp-text.json'))
try:
 r=subprocess.run(['/workspace/scratch/c6ba063dd54d/toolchain/bin/godot3','--path',str(work),'-s','probe.gd'],env=env,text=True,capture_output=True,timeout=8)
 output=r.stdout+r.stderr
except subprocess.TimeoutExpired as err:
 output=(err.stdout or b'').decode()+(err.stderr or b'').decode()
 (ROOT/'reports/battle-round-presentation/native-hp-text-failed.log').write_text(output)
 print(output)
 raise SystemExit(1)
(ROOT/'reports/battle-round-presentation/native-hp-text.log').write_text(output)
(ROOT/'reports/battle-round-presentation/native-hp-text-sources.json').write_text(json.dumps(ex.sources,indent=2)+'\n')
print(output)
if r.returncode or 'SCRIPT ERROR' in output:raise SystemExit(1)

record=json.loads((ROOT/"reports/battle-round-presentation/native-hp-text.json").read_text())
(ROOT/"reports/battle-round-presentation/native-hp-text.tsv").write_text("\n".join("\t".join(str(int(x)) for x in row) for row in record["rows"])+"\n")
