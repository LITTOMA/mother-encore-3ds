#!/usr/bin/env python3
"""Focused official-Godot oracle and disposable House fixtures for tokens 8/9.

Source method bodies stay unchanged; the delay helper receives its global-data service explicitly. Audio is a counted service boundary,
phrase preparation is a bounded tag adapter, and there is no game/GUI claim.
"""
import argparse, copy, hashlib, json, os, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,require
from tools.run_battle_victory_reference import function
from tools.native_house import lower,encode
from tools.dad_record_dialogue import build as dad_build

def main():
 p=argparse.ArgumentParser();p.add_argument('--godot',type=Path);p.add_argument('--replay',action='store_true');p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);a=p.parse_args()
 work=a.work.resolve();reports=a.reports.resolve();require(work.is_relative_to(ROOT/'build') and reports.is_relative_to(ROOT/'reports'),'Fixture paths must stay under build/reports');work.mkdir(parents=True,exist_ok=True);reports.mkdir(parents=True,exist_ok=True)
 ex=Extractor(ROOT);abstract=ex.text('Scripts/UI/AbstractDialogueBox.gd');text=ex.text('Scripts/global/text_tools.gd');dialogue=ex.text('Scripts/UI/DialogueBox.gd')
 # Match exact W@ preprocessing that trims one literal newline first.
 require('str_before = str_before.trim_suffix("\\n")\n\t\t\t\tresult = CHAR_WAIT + "\\n" + CHAR_BULLET' in text,'W@ source changed')
 require('return CHAR_DELAY.repeat(int(amount / (globaldata.text_speed * 16)))' in text,'Delay source changed')
 methods=['add_line_breaks','strip_bbcode','get_text_delay','_tr']
 constants='\n'.join(line for line in text.splitlines() if line.startswith('const CHAR_'))
 (work/'text.gd').write_text('extends Reference\n'+constants+'\n\n'+'\n'.join(function(text,k).replace('get_text_delay(amount: float)', 'get_text_delay(amount: float, globaldata)') for k in methods))
 (work/'abstract.gd').write_text(abstract.replace('class_name AbstractDialogueBox\n','').replace('extends CanvasLayer','extends CanvasLayer\nvar globaldata\nconst TextTools=preload("res://text.gd")'))
 (work/'dialogue.gd').write_text('extends "res://abstract.gd"\nfunc _finish_phrase():\n\t_finished=true\n')
 (work/'service.gd').write_text('extends Node\nvar text_speed=0.02\n')
 (work/'audio.gd').write_text('extends Node\nvar stream=true\nvar calls=0\nvar last_pitch=0.0\nfunc set_pitch_scale(value):last_pitch=value\nfunc play():calls+=1\nfunc stop():pass\n')
 (work/'project.godot').write_text('config_version=4\n[autoload]\nglobaldata="*res://service.gd"\n[logging]\nfile_logging/enable_logging=false\n')
 (work/'Fonts').mkdir(exist_ok=True)
 for name in ['EBMain.ttf','EBMain_la.tres','EBMain_ko.ttf','EBMain_fw9.ttf','EBMain_jaM3.ttf','EBMain_zh_cn.ttf']:(work/'Fonts'/name).write_bytes(ex.data('Fonts/'+name))
 graph=dad_build(ROOT)
 require(graph==json.loads((ROOT/'reports/dad-record/command-graph.json').read_text()),'Stale Dad-normal graph fixture')
 cases=[]
 for row in graph['texts'][1:5]:cases.append(dict(name=row['source_label'],source=row['text_en'],segments=row['segments'],speed=.02,fast=30))
 def custom(name,tokens,source,speed=.02,fast=10000):cases.append(dict(name=name,source=source,segments=[dict(house_tokens=tokens,wait_for_input=False)],speed=speed,fast=fast))
 custom('interior_delay',[{'kind':1,'text':'A'},{'kind':8,'text':'0.5'},{'kind':1,'text':' B'}],'A[D:0.5] B')
 custom('trailing_delay',[{'kind':1,'text':'A'},{'kind':8,'text':'4'}],'A[D:4]')
 custom('only_delay',[{'kind':8,'text':'4'}],'[D:4]')
 custom('forced_newline',[{'kind':1,'text':'A'},{'kind':9,'text':''},{'kind':1,'text':'B'}],'A\nB')
 custom('delay_live_speed',[{'kind':1,'text':'A'},{'kind':8,'text':'4'},{'kind':1,'text':'B'}],'A[D:4]B',.04)
 custom('colored_delay',[{'kind':1,'text':'A'},{'kind':8,'text':'4'},{'kind':3,'text':'ea8b2c'},{'kind':1,'text':'B'},{'kind':4,'text':''},{'kind':1,'text':'C'}],'A[D:4][color=#ea8b2c]B[/color]C')
 custom('empty_delay',[{'kind':8,'text':'0.01'}],'[D:0.01]')
 base=json.loads((ROOT/'content/native-house.json').read_text());presentation=json.loads((ROOT/'content/native-house-presentation.json').read_text())
 firsts={count:next(d['first_segment'] for d in base['dialogues'] if d['segment_count']==count)for count in [1,2]}
 metadata=[]
 for case in cases:
  ir=copy.deepcopy(base);first=firsts[len(case['segments'])];ir['interaction']['text_seconds']=case['speed']
  for i,seg in enumerate(case['segments']):
   dst=ir['segments'][first+i];dst['speaker']='Carol';dst['voice']=base['segments'][0]['voice'];dst['tokens']=seg['house_tokens'];dst['flags']=3 if seg['wait_for_input'] else 5
  (work/(case['name']+'.enchouse')).write_bytes(encode(lower(ir,presentation,verify_assets=False),version=6))
  metadata.append(f"{case['name']} {first} {len(case['segments'])} {case['fast']}")
 ir=copy.deepcopy(base);ir['interaction']['text_seconds']=1e-12;ir['segments'][firsts[1]]['tokens']=[dict(kind=8,text='4')]
 (work/'excessive-delay.enchouse').write_bytes(encode(lower(ir,presentation,verify_assets=False),version=6))
 (work/'cases.json').write_text(json.dumps(cases))
 case_metadata='\n'.join(metadata)+'\n'
 case_hash=hashlib.sha256(json.dumps(cases,sort_keys=True).encode()).hexdigest()
 if a.replay:
  manifest=json.loads((reports/'source-manifest.json').read_text())
  require(manifest['commit']==ex.lock['commit'] and manifest['sources']==ex.sources,'Recorded source oracle manifest changed')
  require(manifest['fixture_cases_sha256']==case_hash and (reports/'cases.tsv').read_text()==case_metadata,'Recorded source oracle cases changed')
  for name in ['cases.tsv','frames.tsv','finals.tsv']:require(hashlib.sha256((reports/name).read_bytes()).hexdigest()==manifest['reference_sha256'][name],'Recorded source oracle bytes changed: '+name)
  print('Checked source oracle replay: '+str(len(cases))+' case packs generated')
  return
 require(a.godot is not None,'Pass --godot for oracle generation or --replay for checked recorded oracle')
 (reports/'cases.tsv').write_text(case_metadata)
 (work/'probe.gd').write_text(PROBE)
 run=subprocess.run([str(a.godot.resolve()),'--path',str(work),'-s','probe.gd'],capture_output=True,text=True,env=dict(os.environ,HOUSE_RECORD_OUT=str(reports),XDG_DATA_HOME=str(work/'userdata')),timeout=30)
 (reports/'reference.log').write_text(run.stdout+run.stderr)
 require(run.returncode==0 and 'SCRIPT ERROR' not in run.stderr,'Native fixture failed: '+run.stderr)
 (reports/'source-manifest.json').write_text(json.dumps(dict(commit=ex.lock['commit'],sources=ex.sources,fixture_cases_sha256=case_hash,reference_sha256={name:hashlib.sha256((reports/name).read_bytes()).hexdigest() for name in ['cases.tsv','frames.tsv','finals.tsv']},methods={k:hashlib.sha256(function(text,k).encode()).hexdigest() for k in methods},scope='Native Godot3.6.2 printing, source line wrapping, live delay counts, voice RNG; explicit phrase-tag/audio adapters; no GUI or hardware'),indent=2)+'\n')
 print('Source fixture generated: '+str(len(cases))+' cases')

PROBE=r'''extends SceneTree
const TextTools=preload("res://text.gd")
func _init():call_deferred("run")
func node(type,name,parent):
 var n=ClassDB.instance(type)
 n.name=name
 parent.add_child(n)
 return n
func run():
 assert(Engine.get_version_info().hash=="3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 var globaldata=load("res://service.gd").new()
 var tr=Translation.new()
 tr.locale="en"
 tr.add_message("WORD_SEPARATOR"," ")
 TranslationServer.add_translation(tr)
 TranslationServer.set_locale("en")
 var f=File.new()
 assert(f.open("res://cases.json",File.READ)==OK)
 var cases=JSON.parse(f.get_as_text()).result
 f.close()
 var out=File.new()
 assert(out.open(OS.get_environment("HOUSE_RECORD_OUT")+"/frames.tsv",File.WRITE)==OK)
 var finals=File.new()
 assert(finals.open(OS.get_environment("HOUSE_RECORD_OUT")+"/finals.tsv",File.WRITE)==OK)
 for c in cases:
  globaldata.text_speed=c.speed
  var d=load("res://dialogue.gd").new()
  d.globaldata=globaldata
  get_root().add_child(d)
  d.set_physics_process(false)
  d.set_process_input(false)
  var sound=load("res://audio.gd").new()
  sound.name="AudioStreamPlayer"
  d.add_child(sound)
  d._dialogue_label=node("RichTextLabel","Text",d)
  d._bullet_label=node("RichTextLabel","Bullet",d)
  d._cursor_down_sprite=node("AnimatedSprite","Cursor",d)
  for label in [d._dialogue_label,d._bullet_label]:
   label.bbcode_enabled=true
   label.rect_size=Vector2(235,60)
   label.scroll_active=false
   label.scroll_following=true
   label.add_constant_override("line_separation",3)
   label.add_font_override("normal_font",load("res://Fonts/EBMain_la.tres"))
  var text=c.source.replace("[@]",TextTools.CHAR_BULLET)
  text=text.replace("\n[W@]", "[W@]").replace("[W@]",TextTools.CHAR_WAIT+"\n"+TextTools.CHAR_BULLET)
  var re=RegEx.new()
  re.compile("\\[D:([0-9.]+)\\]")
  var m=re.search(text)
  while m:
   text=text.substr(0,m.get_start())+TextTools.get_text_delay(m.get_string(1).to_float(),globaldata)+text.substr(m.get_end())
   m=re.search(text)
  d._curr_phrase={"text":TextTools.add_line_breaks(text,d._dialogue_label)}
  d._dialogue_label.visible_characters=0
  d._finished=false
  d._print_dialogue_segment(true)
  seed(123)
  for tick in range(2000):
   if d._stopped:d._action_press(true,false)
   if tick>=c.fast:d._action_press(true,true)
   d._advance_printing(1.0/60.0)
   var shown=d._get_no_br_dialog_content().substr(0,d._dialogue_label.visible_characters).replace(TextTools.CHAR_DELAY,"").replace(TextTools.CHAR_WAIT,"")
   out.store_line("%s %s %s %s %s %s %s"%[c.name,tick,d._dialogue_label.visible_characters,int(d._stopped),int(d._finished),sound.calls,shown.to_utf8().hex_encode() if shown!="" else "-"])
   if d._finished:break
  assert(d._finished)
  finals.store_line("%s %s %s"%[c.name,randi(),d._dialogue_label.text.count("\n")])
  yield(self,"idle_frame")
  d.free()
 globaldata.free()
 TranslationServer.remove_translation(tr)
 yield(self,"idle_frame")
 out.close()
 finals.close()
 quit(0)
'''
if __name__=='__main__':main()
