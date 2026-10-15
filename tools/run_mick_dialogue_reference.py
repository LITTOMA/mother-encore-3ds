#!/usr/bin/env python3
"""Run original bounded typewriter methods on admitted Mick text in Godot 3.6.2."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, require
from tools.run_battle_victory_reference import function, sha
from tools.run_segment_reference import ENGINE_SHA256
from tools.mick_treats import load

DRIVER='''extends SceneTree
func _init():call_deferred("run")
func run():
 var file=File.new()
 assert(file.open("res://fixture.json",File.READ)==OK)
 var fixtures=JSON.parse(file.get_as_text()).result
 file.close()
 var cases=[]
 for fixture in fixtures:
  var box=load("res://box.gd").new()
  get_root().add_child(box)
  box._dialogue_label=RichTextLabel.new()
  box.add_child(box._dialogue_label)
  box._cursor_down_sprite=Control.new()
  box.add_child(box._cursor_down_sprite)
  var audio=AudioStreamPlayer.new()
  audio.name="AudioStreamPlayer"
  box.add_child(audio)
  box._curr_phrase={"text":fixture.text}
  box._finished=false
  box._print_dialogue_segment(true)
  box._dialogue_label.visible_characters=0
  var gates=0
  var ticks=0
  while not box._finished and ticks<20000:
   box._action_press(false,true)
   box._advance_printing(1.0/60)
   ticks+=1
   if not box._stopped:continue
   gates+=1
   var visible=box._dialogue_label.visible_characters
   var segment=box._segment_num
   for _i in range(600):box._advance_printing(1.0/60)
   assert(box._stopped and not box._finished)
   assert(box._dialogue_label.visible_characters==visible and box._segment_num==segment)
   assert(box._speed_multiplier_from_input==1)
   box._action_press(true,false)
   assert(not box._stopped)
  assert(box._finished and gates==fixture.gates)
  cases.append({"key":fixture.key,"locale":fixture.locale,"waits":gates,"idle_stable":true,"finished":true})
  box.free()
 assert(file.open("res://reference.json",File.WRITE)==OK)
 file.store_string(JSON.print({"engine":Engine.get_version_info(),"cases":cases}))
 file.close()
 print("MICK_DIALOGUE_REFERENCE_COMPLETE")
 quit()
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();out=args.out.resolve()
    require(not out.exists() and out.is_relative_to(ROOT/'build'),'Fresh owned build output required')
    require(sha(args.godot)==ENGINE_SHA256,'Unreviewed reference engine')
    ex=Extractor(ROOT);ir=load();source=ex.text('Scripts/UI/AbstractDialogueBox.gd')
    text_tools=ex.text('Scripts/global/text_tools.gd');methods={}
    names=['_action_press','_print_dialogue_segment','_advance_printing','_get_no_br_dialog_content','_get_last_visible_char','_get_text_speed','_stop_phrase','_has_remaining_segments']
    constants=re.findall(r'^const [A-Z_]+ := [^\n]+',source,re.M)
    text_constants=re.findall(r'^const CHAR_[A-Z_]+ := [^\n]+',text_tools,re.M)
    require(len(constants)==5 and len(text_constants)>=5,'Source text constants changed')
    require('result = CHAR_WAIT + "\\n" + CHAR_BULLET' in text_tools,'Source WAIT@ lowering changed')
    script='extends Node\n'+'\n'.join(constants)+'\nclass TextTools:\n'+'\n'.join(' '+s for s in text_constants)+'\n'
    script+='''class Settings:
 var text_speed=0.02
var globaldata=Settings.new()
var _dialogue_label
var _cursor_down_sprite
var _curr_phrase={}
var _segment_num=0
var _stopped=false
var _finished=false
var _t=0.0
var _speed_multiplier_from_input=1.0
var _speed_multiplier_from_tags=1.0
func _update_bullets(_line,_bullet):pass
func _finish_phrase():_finished=true
func _next_phrase():assert(false)
'''
    for name in names:
        code=function(source,name);methods[name]=hashlib.sha256(code.encode()).hexdigest();script+='\n'+code
    fixtures=[]
    for row in ir['texts']:
        for locale,text in row['localized'].items():
            if '\u2063' not in text:continue
            fixtures.append(dict(key=row['translation_key'],locale=locale,text=text,gates=text.count('\u2063')))
    require(fixtures and sum(f['gates'] for f in fixtures)==26,'Reviewed Mick WAIT coverage changed')
    out.mkdir(parents=True)
    for name,body in {'box.gd':script,'probe.gd':DRIVER,'fixture.json':json.dumps(fixtures,ensure_ascii=False),'project.godot':'config_version=4\n[application]\nconfig/name="Mick timing reference"\n[logging]\nfile_logging/enable_logging=false\n'}.items():
        (out/name).write_text(body,encoding='utf8')
    with (out/'godot.log').open('wb') as log:
        result=subprocess.run([str(args.godot.resolve()),'--no-window','--path',str(out),'--script','probe.gd'],stdout=log,stderr=subprocess.STDOUT,timeout=45)
    log=(out/'godot.log').read_text(encoding='utf8',errors='replace')
    require(result.returncode==0 and 'SCRIPT ERROR' not in log and 'ERROR:' not in log and 'MICK_DIALOGUE_REFERENCE_COMPLETE' in log,'Original typewriter reference failed; inspect retained log')
    data=json.loads((out/'reference.json').read_text(encoding='utf8'))
    require(data['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8' and len(data['cases'])==len(fixtures),'Reference identity/count rejected')
    receipt=dict(commit=ex.lock['commit'],sources=ex.sources,methods_sha256=methods,engine_sha256=sha(args.godot),tool_sha256=sha(__file__),reference_sha256=sha(out/'reference.json'),
                 adapters=['Admitted Mick EN/ZH text uses reviewed WAIT@ lowering; no full TextTools execution, glyph/color/bullet/layout comparison or game scene',
                           'Native RichTextLabel and AudioStreamPlayer; no audio stream, bullet drawing or next-phrase execution; completion records finished only',
                           'Settings adapter uses 0.02 seconds per character, physics steps are explicit 1/60; no exact frame expectations'],
                 scope='Original AbstractDialogueBox methods: 26 Mick input waits, 600 idle steps at each, reset acceleration, fresh accept resumes. Not a full original-game run.')
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
    print('Original Godot methods:',len(data['cases']),'phrases;',sum(c['waits'] for c in data['cases']),'stable source waits')

if __name__=='__main__':main()
