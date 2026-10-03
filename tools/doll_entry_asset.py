#!/usr/bin/env python3
"""Extract and compile the bounded pinned Doll battle entrance.

UI and transition resources reuse the checked opening bundle. Doll enemy/Baby
art are exact upstream images. ENCBTL01 v2 retains every active Baby parameter,
including the source's omitted (zero) palette divisor. It never invents combat,
win flags, palette periods, or shader output for undefined source arithmetic.
"""
from __future__ import annotations
import argparse, copy, csv, hashlib, io, json, re, struct, subprocess, sys
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools.extract_battle_entry import Extractor, require, properties, one
from tools.battle_assets import encode_indexed, decode_indexed, verify as verify_common_assets
from tools.upstream import read_json, write_json
from tools import native_battle
IR_PATH=ROOT/'content/doll-entry.json'
OUT=ROOT/'romfs/doll-preview'
PACK=ROOT/'romfs/data/doll-entry.encbattle'
RECEIPT=OUT/'source.json'
PIN='7d9246600fffe518408f5830d4848635019005a3'
EXTRA_ART=[('background','Graphics/Battle BGS/baby.png','doll-background.bpx','indexed'),('enemy','Graphics/Battle Sprites/doll.png','doll-enemy.t3x','texture'),('background-palette','Graphics/Battle BGS/baby_pal.png','doll-palette.bpx','indexed')]

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def extract():
 ex=Extractor(ROOT);ir=ex.build()
 doll=ex.yaml('Data/Battlers/doll.yaml');scene=ex.yaml('Data/Dialogue/Podunk/cutscenes/doll_attack.yaml');last=scene['6']
 require(doll['boss'] is True and doll['music']=='' and doll['bg']=='baby' and 'battlescript' not in doll,'Unknown Doll encounter behavior')
 require(last['startbattle']=={'battlers':[{'doll':'doll'}],'actorskeep':{'doll':True},'wincutscene':'Podunk/cutscenes/doll_defeated'},'Changed Doll battle boundary')
 ir['binary_version']=2;ir['scope'].update(battle_id='doll',completion='source Doll entrance and first command menu; explicit reviewed GLES2 fixed-palette-row policy; no selected action execution')
 ir['entry'].update(win_flag='',encountered_key='doll',first_encounter_flag='doll_fought',post_battle_cutscenes={'win':'Podunk/cutscenes/doll_defeated'},can_run=False)
 ir['entry']['source_can_run_argument']=False
 ir['entry']['can_run_explanation']='DialogueBox._end_dialogue explicitly calls uiManager.start_battle(0,false,...) for queued encounters'
 size=ex.png_size('Graphics/Battle Sprites/doll.png');vp=ir['viewport']
 ir['enemy'].update(id='doll',data=doll,pool_exp=doll['exp'],pool_cash=doll['cash'],sprite_size=size,sprite_center=[vp['width']/2,147/2],sprite_position=[vp['width']/2-size[0]/2,147/2-size[1]/2])
 ir['presentation'].update(asset_recipe_path='content/doll-entry.json',asset_receipt_path='romfs/doll-preview/source.json')
 for row in csv.reader(io.StringIO(ex.text('Translations/TranslatedText/battlers - sheet.csv'))):
  if row and row[0] in ['DOLL_NAME','DOLL_DESC','DOLL_ART']:ir['presentation']['translations_en'][row[0]]=row[1]
 for name,path,output,kind in EXTRA_ART:
  imsize=ex.png_size(path);ex.data(path+'.import')
  ir['presentation']['assets'][name]={'source':path,'texture_size':imsize,'grid':[1,1],'frame_size':imsize,'output':'doll-preview/'+output,'kind':kind}
 text=ex.text('Graphics/Battle BGS/baby.bbg')
 layers=[{'index':int(m[1]),'properties':properties(m[2])} for m in re.finditer(r'^\[Layer (\d+)\]\s*\n(.*?)(?=^\[|\Z)',text,re.M|re.S)]
 require([l['index'] for l in layers]==[0,1],'Unknown Baby layer count')
 expected={'shader','texture','texture_stretch','opacity','screen_size','move','ping_pong_speed','oscillation_amplitude','oscillation_frequency','oscillation_speed','osc_amp_ping_pong','osc_trans_ping_pong','compression_amplitude','compression_frequency','compression_speed','comp_amp_ping_pong','comp_trans_ping_pong','interlaced_amplitude','interlaced_frequency','interlaced_speed','inter_amp_ping_pong','inter_trans_ping_pong','palette_shifting_speed','palette','palette_shifting','barrel','effect','effect_scale','barrelxy'}
 for layer in layers:
  p=layer['properties'];require(set(p)==expected and p['shader']=='[DEFAULT]' and p['texture_stretch']=='STRETCH_TILE' and p['palette_shifting'] is True,'Unreviewed Baby profile')
 shader=ex.text('addons/distortionator_integration/default_shader.tres');importer=ex.text('addons/distortionator_integration/scene_importer.gd')
 require('uniform int palette_anim_frame_count;' in shader and 'palette_anim_frame_count' not in importer,'Palette divisor source changed')
 review=read_json(ROOT/'reports/doll-background-reference/review.json')
 require(review['source_default_divisor']==0 and review['compatibility_policy']['row']==0,'Unsupported native palette finding')
 ir['background'].update(source='Graphics/Battle BGS/baby.bbg',layers=layers,texture_size=ir['presentation']['assets']['background']['texture_size'],native_layer_size=ir['presentation']['assets']['background']['texture_size'],native_control_size=[320,180],uv_domain='STRETCH_TILE native source texture176x172 divisor, control320x180 reference /400x240 expanded; distorted coordinates repeat',palette_divisor={'uniform':'palette_anim_frame_count','source_value':None,'default_value':0,'runtime_assignment_found':False,'status':'undefined GLSL division by zero; render must fail closed unless separately reviewed backend result is supplied'})
 system=ex.text('Scripts/UI/Battle/BattleSystem.gd');boss='Audio/Music/'+one(r'"bossencounter": "([^"]+)"',system,'boss audio')[1]
 ex.data(boss);ex.data(boss+'.import')
 ir['background']['compatibility_policy']={'kind':'fixed_palette_row_for_undefined_divisor','fixed_palette_row':0,'review':'reports/doll-background-reference/review.json','review_sha256':sha(ROOT/'reports/doll-background-reference/review.json'),'scope':'Matches pinned original Godot3.6.2 GLES2 llvmpipe observations; undefined arithmetic on other backends is not portable'}
 ir['audio'].update(encounter_audio_id=1002,encounter_source=boss)
 ir['audio']['overworld_battle_music_note']='Doll music field empty; existing Poltergeist overworld audio remains; Boss encounter jingle is separate'
 receipt=read_json(ROOT/'romfs/house-preview/source.json');doll_resource=next(r for r in receipt['resources'] if r['role']=='doll')
 actor=read_json(ROOT/'romfs/actor-preview/source.json');texture=actor['recipe']
 for src in ['Graphics/Character Sprites/Npcs/1dir/doll.png','Graphics/Character Sprites/Ninten/main.png','LICENSE']:ex.data(src)
 def resource(r):return {k:r[k] for k in ['path','width','height','columns','rows','sha256']}
 ir['binding']={'stable_id':2,'player_instance':0,'enemy_instance':2,'world_resources':{'world_player':{'path':'actor-preview/ninten-main.t3x','width':texture['size'][0],'height':texture['size'][1],'columns':texture['grid'][0],'rows':texture['grid'][1],'sha256':actor['outputs']['ninten-main.t3x']['sha256']},'world_enemy':resource(doll_resource)}}
 ir['party']['runtime_state']='The caller supplies surviving party HP/PP/EXP from the current world; these baseline rows do not heal/reset after Lamp'
 ir['selectors']['background']='baby.bbg all fields, exact default shader ordering, source omitted palette count0; no inferred palette cycling period'
 ir['licence_review']='Pinned LICENSE permits assets in game-related forks/modifications/translations; original art retained only in Mother: Encore port with original conditions'
 ir['sources']=ex.sources
 return ir

def validate(ir):
 native_battle.verify_sources(ir)
 require(ir['commit']==PIN and ir.get('binary_version')==2 and ir['scope']['battle_id']=='doll','Unreviewed Doll identity/version')
 require(ir['binding']['stable_id']==2 and ir['binding']['enemy_instance']==2 and ir['binding']['player_instance']==0,'Doll Room binding changed')
 require(not ir['scope']['whole_battle_approved'] and not ir['scope']['combat_implemented'],'Doll combat not reviewed')
 require(ir['enemy']['id']=='doll' and ir['enemy']['data']['boss'] is True,'Doll roster changed')
 require(ir['background']['source']=='Graphics/Battle BGS/baby.bbg','Doll background source changed')
 require(ir['background']['palette_divisor']['default_value']==0,'Never guess source palette divisor')
 require(ir.get('licence_review'),'Missing original asset review')
 policy=ir['background'].get('compatibility_policy',{});require(policy.get('kind')=='fixed_palette_row_for_undefined_divisor' and policy.get('fixed_palette_row')==0 and policy.get('review')=='reports/doll-background-reference/review.json','Unreviewed Baby backend policy')
 require(sha(ROOT/policy['review'])==policy['review_sha256'],'Changed Baby backend evidence')
 review=read_json(ROOT/policy['review'])
 for path,digest in review['source_sha256'].items():require(ir['sources'].get(path)==digest,'Baby policy source changed')
 for filename,digest in review['evidence'].items():require(sha(ROOT/'reports/doll-background-reference'/filename)==digest,'Changed Baby native evidence')


def compile_art(ir,tex3ds):
 validate(ir);verify_common_assets(ROOT/'upstream/MOTHER-Encore')
 base=read_json(ROOT/'romfs/battle-preview/source.json');receipt=copy.deepcopy(base);OUT.mkdir(parents=True,exist_ok=True)
 for name,path,filename,kind in EXTRA_ART:
  spec=ir['presentation']['assets'][name];source=ROOT/'upstream/MOTHER-Encore'/path;target=OUT/filename
  require(spec['source']==path and spec['kind']==kind and spec['output']=='doll-preview/'+filename,'Unreviewed asset output')
  with Image.open(source) as im:
   image=im.convert('RGBA');require(list(image.size)==spec['texture_size'],'Changed Doll dimensions')
  if kind=='indexed':target.write_bytes(encode_indexed(image,(1,1)));decode_indexed(target.read_bytes())
  else:subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)],check=True)
  old=next((i for i,r in enumerate(receipt['resources']) if r['name']==name),len(receipt['resources']))
  record={'id':old,'name':name,'source':path,'kind':kind,'size':list(image.size),'grid':[1,1],'output':spec['output'],'fix_alpha_edges':False,'width':image.width,'height':image.height,'frames':1}
  if old==len(receipt['resources']):receipt['resources'].append(record)
  else:receipt['resources'][old]=record
 receipt['outputs']={Path(r['output']).name:{'sha256':sha(ROOT/'romfs'/r['output']),'bytes':(ROOT/'romfs'/r['output']).stat().st_size} for r in receipt['resources']}
 receipt.update(recipe={'schema':1,'commit':ir['commit'],'source_ir_sha256':sha(IR_PATH),'sources':{p:ir['sources'][p] for _,p,_,_ in EXTRA_ART},'licence_review':ir['licence_review']},tex3ds_sha256=sha(tex3ds),scope='Shared checked entry/font assets; exact Doll battle sprite; lossless original Baby red-channel/palette images. Source palette divisor0 retained with independently reviewed GLES2 fixedrow0 behavior; no full-scene GPU pixel-equivalence claim.')
 write_json(RECEIPT,receipt)


def compile_pack(ir):
 validate(ir);receipt=read_json(RECEIPT)
 require(receipt['recipe']['source_ir_sha256']==sha(IR_PATH),'Stale Doll asset receipt')
 tables=native_battle.lower(ir,receipt);blob=native_battle.encode(tables,ir['commit']);native_battle.parse_sections(blob)
 return blob,tables


def main():
 ap=argparse.ArgumentParser();ap.add_argument('action',choices=['extract','assets','compile','verify']);ap.add_argument('--tex3ds',type=Path);a=ap.parse_args()
 if a.action=='extract':write_json(IR_PATH,extract());print('Extracted source Doll entrance, roster and complete Baby parameters');return
 ir=read_json(IR_PATH)
 if a.action=='assets':
  require(a.tex3ds is not None,'Need tex3ds');compile_art(ir,a.tex3ds);print('Compiled exact Doll/Baby art and reused common entry assets');return
 blob,tables=compile_pack(ir)
 if a.action=='compile':
  PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(blob)
  out=ROOT/'reports/doll-entry';out.mkdir(parents=True,exist_ok=True)
  write_json(out/'data-build.json',{'schema':1,'commit':ir['commit'],'ir_sha256':sha(IR_PATH),'pack_sha256':sha(PACK),'pack_bytes':len(blob),'binary_version':2,'source_palette_divisor':0,'sources':ir['sources'],'resources':len(tables['resources']),'scope':ir['scope'],'gpu_reference':'not run by this compiler','combat':'unimplemented; must reject actions','reward_flags_written':[]})
 else:require(PACK.read_bytes()==blob,'Stale Doll entry pack');native_battle.stage_files(ROOT/'romfs',PACK.relative_to(ROOT/'romfs'))
 print('Doll entry '+a.action+': '+str(len(blob))+' bytes; no combat or fabricated palette period')
if __name__=='__main__':
 try:main()
 except (ValueError,OSError,KeyError,TypeError,struct.error,subprocess.SubprocessError) as e:print('DOLL ENTRY ERROR:',e,file=sys.stderr);raise SystemExit(1)
