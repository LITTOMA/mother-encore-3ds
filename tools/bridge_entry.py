#!/usr/bin/env python3
"""Source-bound entrance for the original bridge encounter, without combat claims."""
from __future__ import annotations
import argparse,copy,csv,hashlib,io,json,re,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools.extract_battle_entry import Extractor,require,properties,PIN
from tools import native_battle,field_npcs
from tools.battle_assets import encode_indexed,decode_indexed
from PIL import Image
RECIPE=ROOT/'content/bridge-encounter.json'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):
    def unique(pairs):
        value={}
        for key,item in pairs:require(key not in value,'Duplicate bridge JSON field '+key);value[key]=item
        return value
    def invalid(value):raise ValueError('Nonfinite bridge JSON number')
    return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique,parse_constant=invalid)
def write(path,value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes((json.dumps(value,indent=2,ensure_ascii=False)+'\n').encode('utf-8'))
def policy():
    value=read(RECIPE)
    require(set(value)=={'schema','kind','commit','scene','actor','programme','battle_id','battler','ir','receipt','pack','graphics','license_review'} and type(value['schema'])is int and value['schema']==1 and value['kind']=='encore.field-encounter.recipe' and value['commit']==PIN,'Unreviewed bridge recipe')
    for key in ('scene','actor','programme','ir','receipt','pack','graphics'):
        path=value[key];require(type(path)is str and path and ':'not in path and '\\'not in path and all(p not in ('','.','..')for p in path.split('/')),'Unsafe encounter binding')
    require(type(value['battle_id'])is int and value['battle_id']>0 and value['pack'].endswith('.encbattle') and value['graphics'].startswith('graphics/battle/') and value['license_review'],'Encounter identity/permission')
    return value

def extract():
    recipe=policy();ex=Extractor(ROOT);ir=ex.build()
    source='Data/Battlers/'+recipe['battler']+'.yaml';enemy=ex.yaml(source);doc=ex.yaml(recipe['programme'])
    start=[p['startbattle']for p in doc.values()if 'startbattle'in p]
    require(len(start)==1 and set(start[0])=={'battlers','winflag'} and start[0]['battlers']==[{recipe['battler']:'dog'}] and doc['0']['actors']['dog']==recipe['actor'],'Bridge source actor/battle binding changed')
    require(enemy['boss']is False and enemy['music'] and enemy['bg'] and set(enemy['skills'][0])=={'skill','weight'} and not enemy.get('battlescript'),'Unreviewed field encounter ownership')
    _,owners,npc_sources=field_npcs.build();matches=[o for o in owners if o['binding']['path']==recipe['actor']]
    require(len(matches)==1,'Encounter actor requires a checked field motion owner');owner=matches[0]
    t=owner['ir']['texture'];world_texture=ROOT/'romfs'/t['output'];require(world_texture.is_file(),'Missing original field actor texture')
    ir['binary_version']=2;ir['scope'].update(battle_id=recipe['battler'],completion='Original bridge encounter entrance and command UI only; combat, loot and story continuation require their own admitted execution resources')
    ir['entry'].update(win_flag=start[0]['winflag'],encountered_key=recipe['battler'],first_encounter_flag=recipe['battler']+'_fought',post_battle_cutscenes={},can_run=False,source_can_run_argument=False,can_run_explanation='Queued DialogueBox cutscene battles pass can_run=false',show_intro_outro=bool(enemy.get('show_intro_outro',False)))
    sprite='Graphics/Battle Sprites/'+enemy['sprite']+'.png';size=ex.png_size(sprite);ex.data(sprite+'.import');center=ir['enemy']['sprite_center']
    ir['enemy'].update(id=recipe['battler'],data=enemy,pool_exp=enemy['exp'],pool_cash=enemy['cash'],sprite_size=size,sprite_position=[center[i]-size[i]/2 for i in range(2)])
    background='Graphics/Battle BGS/'+enemy['bg']+'.bbg';raw=ex.text(background)
    layers=[dict(index=int(m[1]),properties=properties(m[2]))for m in re.finditer(r'^\[Layer (\d+)\]\s*\n(.*?)(?=^\[|\Z)',raw,re.M|re.S)]
    require([l['index']for l in layers]==[0,1] and len({l['properties']['texture']for l in layers})==1,'Unreviewed bridge background layers')
    image=layers[0]['properties']['texture'].lstrip('/');bsize=ex.png_size(image);ex.data(image+'.import')
    require(all(l['properties']['texture_stretch']=='STRETCH_TILE' and not l['properties']['palette_shifting']for l in layers),'Unreviewed bridge background mode')
    ir['background'].update(source=background,layers=layers,texture_size=bsize,native_layer_size=bsize,native_control_size=[320,180],uv_domain='Original STRETCH_TILE texture; checked default shader projected into expanded400x240 display')
    for name,path,filename,kind in [('background',image,'bridge-background.bpx','indexed'),('enemy',sprite,'bridge-enemy.t3x','texture')]:
        size=ex.png_size(path);ir['presentation']['assets'][name]=dict(source=path,texture_size=size,grid=[1,1],frame_size=size,output=recipe['graphics']+'/'+filename,kind=kind)
    ir['presentation'].update(asset_recipe_path=recipe['ir'],asset_receipt_path=recipe['receipt'])
    for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/battlers - sheet.csv'))):
        if row['key']in (enemy['name'],enemy['description'],enemy['article']):ir['presentation']['translations_en'][row['key']]=row['en']
    music='Audio/Music/Battle Themes/'+enemy['music'];ex.data(music);ex.data(music+'.import')
    ir['audio'].update(battle_music_source=music,continue_overworld_source=None,overworld_battle_music=False,pause_overworld_music=True,overworld_battle_music_note='Enemy has an explicit source battle theme; this entrance resource records ownership but does not itself implement audio playback')
    actor=read(ROOT/'content/asset-receipts/graphics/actors/source.json');texture=actor['recipe']
    ir['binding']=dict(stable_id=recipe['battle_id'],player_instance=0,enemy_instance=owner['binding']['id']-1,world_resources={
        'world_player':dict(path='graphics/actors/ninten-main.t3x',width=texture['size'][0],height=texture['size'][1],columns=texture['grid'][0],rows=texture['grid'][1],sha256=actor['outputs']['ninten-main.t3x']['sha256']),
        'world_enemy':dict(path=t['output'],width=t['size'][0],height=t['size'][1],columns=t['columns'],rows=t['rows'],sha256=sha(world_texture))})
    ir['licence_review']=recipe['license_review'];ex.data('LICENSE');ir['sources']={**ex.sources,**npc_sources}
    return ir

def validate(ir):
    expected=extract();require(ir==expected,'Bridge entrance differs from pinned source extraction')
    native_battle.verify_sources(ir)

def assets(ir,tex3ds,command):
    validate(ir);recipe=policy();require(tex3ds and tex3ds.is_file(),'Real tex3ds required')
    receipt=copy.deepcopy(read(ROOT/'content/asset-receipts/graphics/battle/lamp/source.json'))
    for name in ('background','enemy'):
        spec=ir['presentation']['assets'][name];source=ROOT/'upstream/MOTHER-Encore'/spec['source'];target=ROOT/'romfs'/spec['output'];target.parent.mkdir(parents=True,exist_ok=True)
        with Image.open(source)as image:
            if spec['kind']=='indexed':target.write_bytes(encode_indexed(image,(1,1)));decode_indexed(target.read_bytes())
            else:
                rel=lambda p:p.resolve().relative_to(ROOT).as_posix()
                args=shlex.split(command)+['-f','rgba8','-z','none','-o',rel(target),rel(source)]if command else[str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)]
                subprocess.run(args,cwd=ROOT,check=True)
            index=next(i for i,r in enumerate(receipt['resources'])if r['name']==name)
            receipt['resources'][index]=dict(id=index,name=name,source=spec['source'],kind=spec['kind'],size=list(image.size),grid=[1,1],output=spec['output'],fix_alpha_edges=False,width=image.width,height=image.height,frames=1)
    receipt['outputs']={Path(r['output']).name:dict(sha256=sha(ROOT/'romfs'/r['output']),bytes=(ROOT/'romfs'/r['output']).stat().st_size)for r in receipt['resources']}
    receipt.update(recipe=dict(schema=1,commit=PIN,source_ir_sha256=sha(ROOT/recipe['ir']),sources=ir['sources'],licence_review=recipe['license_review']),tex3ds_sha256=sha(tex3ds),producer_sha256=sha(__file__),policy_sha256=sha(RECIPE),scope='Original source bridge dog sprite/background, shared checked UI; entrance only')
    write(ROOT/recipe['receipt'],receipt)

def compile_pack(ir):
    validate(ir);recipe=policy();receipt=read(ROOT/recipe['receipt'])
    require(receipt['recipe']['source_ir_sha256']==sha(ROOT/recipe['ir']) and receipt['producer_sha256']==sha(__file__) and receipt['policy_sha256']==sha(RECIPE),'Stale bridge asset receipt')
    for r in receipt['resources']:
        path=ROOT/'romfs'/r['output'];fingerprint=receipt['outputs'][path.name]
        require(sha(path)==fingerprint['sha256'] and path.stat().st_size==fingerprint['bytes'],'Changed bridge/shared entrance texture')
    tables=native_battle.lower(ir,receipt);blob=native_battle.encode(tables,PIN);native_battle.parse_sections(blob);return blob

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('extract','assets','compile','verify'));p.add_argument('--tex3ds',type=Path);p.add_argument('--tex3ds-command');a=p.parse_args();recipe=policy()
    if a.action=='extract':write(ROOT/recipe['ir'],extract());print('Extracted source bridge entrance; no combat approval');return
    ir=read(ROOT/recipe['ir'])
    if a.action=='assets':assets(ir,a.tex3ds,a.tex3ds_command);print('Compiled original bridge entrance art');return
    blob=compile_pack(ir);target=ROOT/'romfs'/recipe['pack']
    if a.action=='verify':require(target.read_bytes()==blob,'Stale bridge entrance resource')
    else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
    print('Bridge entrance',a.action,len(blob),'bytes; battle/loot execution remains independent')

if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,TypeError,OSError,subprocess.SubprocessError)as e:print('BRIDGE ENTRANCE ERROR:',e,file=sys.stderr);raise SystemExit(1)
