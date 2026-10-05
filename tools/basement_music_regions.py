#!/usr/bin/env python3
"""Pinned House MusicArea/MusicArea2 shared ownership and diary explicit calls.

This bounded scope supplies the existing owned-player MusicRegion consumer;
The root and basement same-song areas share this owner. Other House regions
remain outside this new scene capability.
"""
from __future__ import annotations
import argparse,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require,node
from tools.drawer_program import canonical,digest,read_json,write_json
HOUSE='Maps/podunk/Nintens House.tscn';CHANGER='Nodes/Overworld/MusicChanger.tscn'
MANAGER='Scripts/global/audioManager.gd'
IR='content/basement-music-regions.json';REVIEW='reports/basement-music/source-review.json'
PACK='romfs/sound/banks/house.encmusic'

def build(root=ROOT,ex=None):
    root=Path(root);ex=ex or Extractor(root);scene=ex.text(HOUSE);changer=ex.text(CHANGER);manager=ex.text(MANAGER)
    old=read_json(root/'content/podunk-music.json')
    require(ex.sources[CHANGER]==old['sources'][CHANGER] and ex.sources[MANAGER]==old['sources'][MANAGER],'Unreviewed MusicChanger/manager semantics')
    match=re.search(r'^script/source = ("(?:[^"\\]|\\.)*")$',changer,re.M);require(match is not None,'MusicChanger embedded source missing');script=json.loads(match[1],strict=False)
    defaults={}
    for key in ('music','loop','appear_flag','disappear_flag','diegetic','volume_db','fadein_length','fadeout_length','disabled'):
        m=re.search(r'^export(?: \([^)]*\))? var '+key+r' = (.+)$',script,re.M);require(m is not None,'Unknown music default '+key);defaults[key]=json.loads(m[1])
    stop=re.search(r'^func stop_music\(fadeoutLength = ([0-9.]+)\):',script,re.M);require(stop is not None,'Music source default stop missing')
    require('audioManager.get_latest_audio_player_index()' in script and 'attached_player' in script and 'player_inside = true' in script and 'func play_music():' in script,'Music owned-player source changed')
    for connection in ('[connection signal="body_entered" from="." to="." method="_on_Area2D_body_entered"]','[connection signal="body_exited" from="." to="." method="_on_MusicArea_body_exited"]','[connection signal="tree_exiting" from="." to="." method="_on_MusicArea_tree_exiting"]'):require(connection in changer,'Music source Area signal wiring changed')
    area=node(changer,'.');require(area['collision_layer']==4096 and area['collision_mask']==0 and area['monitorable'] is False,'Music source Area query policy changed')
    require('get_child(0).disabled = disabled' in script,'Music source ready disabled policy changed')
    authored=read_json(root/'content/phone-linker-bindings.json')['audio']+read_json(root/'content/basement-audio-binding.json')['music_assets']
    rows=[]
    for source_node in ('MusicArea2','MusicArea','MusicArea3','Poltergeist/MusicArea'):
        props=node(scene,source_node);require(set(props)<=set(defaults)|{'visible','position'} and props.get('visible') is False,'Unknown House music node property')
        values=defaults.copy();values.update({k:v for k,v in props.items() if k in values});require(values['music']=='' and values['loop'] and values['diegetic'] is False,'Unknown House music track policy')
        source='Audio/Music/'+values['loop'];assets=[a for a in authored if a['source']==source];require(len(assets)==1 and assets[0]['identity']['kind']=='stable','House music author identity missing');asset=assets[0]
        ex.data(source);ex.data(source+'.import');track=dict(stable_id=asset['identity']['value'],source_path='res://'+source,source_sha256=ex.sources[source],import_sha256=ex.sources[source+'.import'])
        collision=node(scene,source_node+'/CollisionShape2D');require(set(collision)=={'position','shape'} and set(collision['shape'])=={'SubResource'},'Unknown House music collision binding')
        shape=re.search(r'^\[sub_resource type="RectangleShape2D" id='+str(collision['shape']['SubResource'])+r'\]\nextents = Vector2\( ([^)]+) \)',scene,re.M);require(shape is not None,'House music shape missing');extents=[float(v)for v in shape[1].split(',')]
        parent_flag='';parent_position=[0,0]
        if '/'in source_node:
            parent=node(scene,source_node.rsplit('/',1)[0]);require(set(parent)=={'disappear_flag'},'Unknown House music parent lifecycle');parent_flag=parent['disappear_flag']
        geometry=[props['position'][i]+collision['position'][i]+parent_position[i]for i in(0,1)]+extents
        region=dict(id=zlib.crc32((HOUSE+':'+source_node).encode())&0xffffffff,source_path=source_node,track_id=track['stable_id'],volume_db=values['volume_db'],fadein_seconds=values['fadein_length'],fadeout_seconds=values['fadeout_length'],appear_flag=values['appear_flag'],disappear_flag=values['disappear_flag'],disabled=values['disabled'],shape_paths=[source_node+'/CollisionShape2D'])
        # Actual sibling traversal order, not our diary-first compatibility view.
        parent_path,name=source_node.rsplit('/',1)if '/'in source_node else('.',source_node)
        header='[node name="'+name+'" parent="'+parent_path+'"'
        ordinal=scene.index(header);rows.append(dict(region=region,geometry=geometry,track=track,parent_disappear_flag=parent_flag,source_ordinal=ordinal))
    silence=float(re.search(r'^const SILENT_SOUND_THRESHOLD = (.+)$',manager,re.M)[1]);fadeto=float(re.search(r'^func music_fadeto\(index, volume = 0, duration = ([0-9.]+)\):',manager,re.M)[1])
    primary=rows[0]
    return dict(schema=1,commit=PIN,scene=HOUSE,sources=dict(sorted(ex.sources.items())),silence_db=silence,fade_to_seconds=fadeto,default_stop_seconds=float(stop[1]),geometry=primary['geometry'],region=primary['region'],additional_regions=rows[1:],track=primary['track'],parent_disappear_flag=primary['parent_disappear_flag'],source_ordinal=primary['source_ordinal'])

def extract(root=ROOT):
    root=Path(root);ir=build(root);write_json(root/IR,ir);write_json(root/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),producer_sha256=digest(root/'tools/basement_music_regions.py'),sources=ir['sources'],scope='All four actual House MusicChanger regions share one controller, including source parent lifecycle, traversal order and diary explicit calls'));return ir
def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR);require(canonical(ir)==canonical(build(root)),'Stale basement music IR');review=read_json(root/REVIEW);require(review['ir_sha256']==digest(root/IR) and review['producer_sha256']==digest(root/'tools/basement_music_regions.py') and review['sources']==ir['sources'] and review['commit']==PIN,'Unreviewed basement music metadata');return ir
def encode(ir):
    require(set(ir)=={'schema','commit','scene','sources','silence_db','fade_to_seconds','default_stop_seconds','geometry','region','additional_regions','track','parent_disappear_flag','source_ordinal'},'Music source fields')
    require(all(type(ir[k])in(int,float)and math.isfinite(ir[k])and 0<=ir[k]<=60 for k in('default_stop_seconds','fade_to_seconds'))and type(ir['silence_db'])in(int,float)and math.isfinite(ir['silence_db'])and -120<=ir['silence_db']<=-20,'Music source tuning')
    require(ir['schema']==1 and ir['commit']==PIN and len(ir['additional_regions'])==3,'House music source scope')
    rows=[dict(region=ir['region'],geometry=ir['geometry'],track=ir['track'])]+ir['additional_regions'];tracks={};ids=set();nodes=set()
    for row in rows:
        r=row['region'];t=row['track'];require(set(r)=={'id','source_path','track_id','volume_db','fadein_seconds','fadeout_seconds','appear_flag','disappear_flag','disabled','shape_paths'}and set(t)=={'stable_id','source_path','source_sha256','import_sha256'},'Music source record fields');require(r['track_id']==t['stable_id'] and r['id']not in ids and r['source_path']not in nodes and type(r['disabled'])is bool and r['shape_paths']==[r['source_path']+'/CollisionShape2D'],'House music identity');ids.add(r['id']);nodes.add(r['source_path'])
        require(len(row['geometry'])==4 and all(math.isfinite(v)and abs(v)<=8192 for v in row['geometry'])and all(v>0 for v in row['geometry'][2:]),'House music geometry')
        require(-120<=r['volume_db']<=24 and all(0<=r[k]<=60 for k in('fadein_seconds','fadeout_seconds')),'House music fade/gain')
        require(t['stable_id']not in tracks or tracks[t['stable_id']]==t,'Conflicting House song identity');tracks[t['stable_id']]=t
    b=bytearray(64)
    def u(v):b.extend(struct.pack('<I',v))
    def text(v):require(type(v)is str and len(v)<=1024 and all(32<=ord(c)<127 for c in v),'Music source text rejected');raw=v.encode('ascii');u(len(raw));b.extend(raw)
    for t in tracks.values():
        u(t['stable_id'])
        for k in('source_sha256','import_sha256'):require(re.fullmatch('[0-9a-f]{64}',t[k])and int(t[k],16),'Music source fingerprint');b.extend(bytes.fromhex(t[k]))
        text(t['source_path'])
    for row in rows:
        r=row['region'];u(r['id']);u(r['track_id']);u(int(r['disabled']));b.extend(struct.pack('<3f',r['volume_db'],r['fadein_seconds'],r['fadeout_seconds']));text(r['source_path']);text(r['appear_flag']);text(r['disappear_flag']);u(len(r['shape_paths']))
        for path in r['shape_paths']:text(path)
    struct.pack_into('<8s6I2f',b,0,b'ENCMUS01',1,len(b),0,len(rows),len(tracks),len(rows),ir['silence_db'],ir['fade_to_seconds']);struct.pack_into('<I',b,16,zlib.crc32(b));return bytes(b)

def stage_files(source):
    raw=encode(load());require((Path(source)/'sound/banks/house.encmusic').read_bytes()==raw,'Stale staged House music regions');return {Path('sound/banks/house.encmusic'):raw}
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
    try:
        if a.action=='extract':extract();print('Basement music source extracted');return 0
        b=encode(load())
        if a.action=='compile':(ROOT/PACK).parent.mkdir(parents=True,exist_ok=True);(ROOT/PACK).write_bytes(b)
        else:require((ROOT/PACK).read_bytes()==b,'Stale House music regions')
        print('Basement owned-player music regions:',len(b),'bytes')
    except (ValueError,KeyError,OSError,TypeError,struct.error) as e:print('Basement music rejected:',e,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
