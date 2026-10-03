#!/usr/bin/env python3
"""Run isolated source entry AnimationPlayer/SceneTreeTimer/Tween methods in Godot3.6.2.

No original game autoloads, combat, audio playback, or GPU output are involved.
The original source methods are copied unchanged except leading event logging;
minimal nodes replace only services outside this timing probe's stated scope.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, require


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def function(text,name):
    matches=list(re.finditer(r'^func '+re.escape(name)+r'\([^\n]*\n.*?(?=^func |\Z)',text,re.M|re.S))
    require(len(matches)==1,'Missing/ambiguous original method '+name)
    return matches[0][0].rstrip()+'\n'
def instrument(source,event):
    first,body=source.split('\n',1)
    return first+'\n\trunner.event("'+event+'")\n'+body

def prepare(root,work):
    ex=Extractor(root);ir=ex.build()
    require(not work.exists() and work.is_relative_to(root/'build'),'Use fresh build directory')
    work.mkdir(parents=True)
    src=ex.text('Scripts/UI/Battle/BattleSystem.gd')
    methods=['_jump_to_battle','_jump_player_to_partyinfo','_enemy_to_position','_show_action_menu','_show_enemy_sprites','_remove_enemy_transitions']
    prefix='''extends Node2D
var runner
var _show_intro_outro = false
var _enemies_shaking = false
var _advantage = 0
enum Advantage { ENEMY=-1, NEUTRAL, PLAYER }
const SPRITE_FRAMES = {"jump_down":Vector2(0,18)}
var _party_info
func _jump_npc_to_side(_index):
\tassert(false) # No NPCs are admitted by this bounded fixture.
func _battle_start(_anim):
\trunner.event("battle_start_menu_active")
\trunner.menu_active=true
'''
    for method in methods:
        extracted=function(src,method)
        if method=='_jump_to_battle':
            lines=extracted.splitlines();out=[];timer_number=0
            for line in lines:
                out.append(line)
                if 'yield(get_tree().create_timer(' in line:
                    timer_number+=1
                    indent=line[:len(line)-len(line.lstrip())]
                    out.append(indent+'runner.event(\"jump_wait_'+str(timer_number)+'_done\")')
            extracted='\n'.join(out)+'\n'
        prefix+='\n'+instrument(extracted,method)
    (work/'battle.gd').write_text(prefix)
    plate=ex.text('Scripts/UI/Battle/PartyInfoPlate.gd')
    (work/'plate.gd').write_text('extends Control\nvar runner\nvar _init_rect_pos_y=20.0\nvar _tween\n'+instrument(function(plate,'quake'),'plate_quake'))
    (work/'enemy.gd').write_text('extends TextureRect\nvar runner\nfunc appear():\n\trunner.event("enemy_appear")\n')
    cursor=ex.text('Scripts/UI/cursor.gd')
    cm=['set_cursor_from_index','get_menu_item_at_index','_idx_or_next_valid_idx','_idx_or_prev_valid_idx','_should_skip_item']
    cp='''extends Node2D
var runner
var menu_parent
var cursor_offset=Vector2(12,2)
var _cursor_size=Vector2(24,12)
var cursor_index=0
var _tween
var frames=null
var skip_hidden_items=false
var skip_empty_labels=false
const TWEEN_LENGTH=0.1
func _unhighlight():pass
func _turn_on_highlight():pass
func play_sfx(_name):pass
'''
    for name in cm:cp+='\n'+function(cursor,name)
    (work/'cursor.gd').write_text(cp)
    # Source clips are parsed by the same checked adapter, then passed to native Animation.
    (work/'clips.json').write_text(json.dumps(ir['animations']))
    (work/'metadata.json').write_text(json.dumps({'commit':ir['commit'],'scope':'isolated original entry methods and native animation/timer/tween timing; no combat/render reference','methods':methods+cm+['PartyInfoPlate.quake'],'sources':ex.sources}))
    (work/'probe.gd').write_bytes((ROOT/'tools/godot_battle_reference.gd').read_bytes())
    (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Encore isolated battle entry reference"\n[logging]\nfile_logging/enable_logging=false\n[display]\nwindow/size/width=320\nwindow/size/height=180\n[physics]\ncommon/physics_fps=60\n')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--godot',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--reports',type=Path,required=True);a=p.parse_args()
    work=a.work.resolve();reports=a.reports.resolve()
    require(not reports.exists() and reports.is_relative_to(ROOT/'reports'),'Use fresh reports directory')
    prepare(ROOT,work);reports.mkdir(parents=True)
    env=dict(os.environ)
    for key in ['XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME']:
        directory=work/key.lower();directory.mkdir();env[key]=str(directory)
    cmd=[str(a.godot.resolve()),'--path',str(work),'--fixed-fps','60','--script',str(work/'probe.gd'),'--encore-out='+str(reports/'reference.json')]
    with (reports/'godot.txt').open('wb') as out:run=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT,env=env,timeout=45)
    log=(reports/'godot.txt').read_text()
    require(run.returncode==0 and 'ERROR:' not in log and 'WARNING:' not in log and 'BATTLE_ENTRY_REFERENCE_COMPLETE' in log,'Native probe failed; retain log')
    ref=json.loads((reports/'reference.json').read_text())
    require(ref['godot']['string']=='3.6.2-stable (official)','Wrong reference engine')
    require(len(ref['frames'])==150 and ref['menu_active'] is True,'Incomplete timing domain')
    names=[e['event'] for e in ref['events']]
    for key in ['_enemy_to_position','_jump_to_battle','_jump_player_to_partyinfo','_show_action_menu','enemy_appear','_remove_enemy_transitions','battle_start_menu_active']:
        require(names.count(key)==1,'Event missing/repeated '+key)
    receipt={'schema':1,'scope':ref['scope'],'commit':ref['commit'],'engine_sha256':sha(a.godot),'reference_sha256':sha(reports/'reference.json'),'tools_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [Path(__file__).resolve(),ROOT/'tools/godot_battle_reference.gd',ROOT/'tools/extract_battle_entry.py']},'events':ref['events'],'gpu_reference':'not run','full_source_game':'not run','hardware':'not run','emulator':'not run'}
    (reports/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(ref['events'],indent=2))
    print('Native battle entry timing reference complete:',reports)
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,subprocess.SubprocessError) as error:print('BATTLE ENTRY REFERENCE ERROR: '+str(error),file=sys.stderr);sys.exit(1)
