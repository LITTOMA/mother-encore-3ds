"""Source-derived field MusicChanger candidate; keeps existing bank codecs.

Only loop-only non-diegetic areas with rectangular shapes are admitted here.
Candidates stay under build; this tool never stages an incomplete scene.
"""
import argparse,copy,hashlib,json,math,re,shutil,struct,sys,tempfile,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.podunk_field import Scene,Sources,load_recipe,owner_of,Vec2,f32
from tools.podunk_music import stable
CHANGER='Nodes/Overworld/MusicChanger.tscn';MANAGER='Scripts/global/audioManager.gd'


def recipe(path):
    def unique(pairs):
        out={}
        for k,v in pairs:require(k not in out,'Duplicate field music field');out[k]=v
        return out
    p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
    require(set(p)==set('schema kind commit scene field_recipe areas binding audio zones pcm_prefix'.split())and type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.field-music.candidate'and p['commit']==PIN,'Field music recipe schema/pin')
    for k in ('scene','field_recipe','binding','audio','zones'):
        v=p[k];require(type(v)is str and v and not any(c in v for c in ('\\',':'))and not v.startswith('/')and all(x not in ('','.','..')for x in v.split('/')),'Field music path')
    for k,ext in [('binding','.encmusic'),('audio','.encaudio'),('zones','.enczones')]:require(p[k].startswith('sound/banks/')and p[k].endswith(ext),'Field music output type')
    require(type(p['areas'])is list and 1<=len(p['areas'])<=16 and all(type(v)is str and v and all(x not in ('','.','..')for x in v.split('/'))for v in p['areas'])and len(set(p['areas']))==len(p['areas']),'Field music source areas')
    require(type(p['pcm_prefix'])is str and re.fullmatch('[a-z][a-z0-9-]{0,31}',p['pcm_prefix']),'Field music PCM owner')
    return p


def source(path=ROOT/'content/mayor-music.json'):
    p=recipe(path);files=Sources(ROOT);scene_recipe=load_recipe(ROOT/p['field_recipe']);require(scene_recipe['scene']['source']==p['scene'],'Field music scene differs');scene=Scene(files,scene_recipe)
    ex=files.extractor;manager=ex.text(MANAGER);floor=re.search(r'^const SILENT_SOUND_THRESHOLD = (-?[0-9.]+)$',manager,re.M);require(floor and 'func music_fadeto(index, volume = 0, duration = 1):'in manager,'Field music manager source policy changed')
    tracks=[];regions=[];polygons=[];seen=set()
    for path in p['areas']:
        require(path in scene.by_path,'Field MusicChanger is missing');n=scene.by_path[path]
        require(n.scene==CHANGER and n.type=='Area2D'and n.props.get('collision_layer',1)==4096 and n.props.get('collision_mask',1)==0 and n.props.get('monitoring',True)and (scene_recipe['player']['collision_mask']&4096),'Unreviewed MusicChanger filtering/class')
        script_doc=owner_of(n,'script');require(script_doc.source==CHANGER,'Unreviewed MusicChanger script override');script=script_doc.sub_resource(n.props['script'])['script/source'];defaults={}
        for name in ('music','loop','appear_flag','disappear_flag','diegetic','volume_db','fadein_length','fadeout_length','disabled'):
            found=re.search(r'^export(?: [(][^)]*[)])? var '+name+r' = (.+)$',script,re.M);require(found,'Unreviewed MusicChanger default '+name);defaults[name]=json.loads(found[1])
        require(set(n.props)<=set(defaults)|{'collision_layer','collision_mask','monitorable','monitoring','script','__meta__','visible','position','scale'},'Unreviewed MusicChanger override')
        values={k:n.props.get(k,v)for k,v in defaults.items()}
        require(values['music']==''and type(values['loop'])is str and values['loop']and values['diegetic']is False,'Field music intro/diegetic path is outside reviewed scope')
        require(all(type(values[k])is str for k in ('appear_flag','disappear_flag'))and type(values['disabled'])is bool,'Field music flags/disabled policy')
        for k,lo,hi in [('volume_db',-120,24),('fadein_length',0,60),('fadeout_length',0,60)]:require(type(values[k])in(int,float)and math.isfinite(values[k])and lo<=values[k]<=hi,'Field music numeric policy')
        source_path='res://Audio/Music/'+values['loop'];ident=stable(source_path);require(ident,'Field music zero track identity')
        if source_path not in seen:
            seen.add(source_path);raw=ex.data(source_path[6:]);imported=ex.data(source_path[6:]+'.import');tracks.append(dict(stable_id=ident,source_path=source_path,source_sha256=hashlib.sha256(raw).hexdigest(),import_sha256=hashlib.sha256(imported).hexdigest(),pcm_path='sound/music/'+p['pcm_prefix']+'-'+str(ident)+'.pcm',gain_db=0))
        shapes=[];parts=[]
        for child in scene.nodes:
            if child.path.rpartition('/')[0]!=path:continue
            require(child.type=='CollisionShape2D'and not child.props.get('disabled',False)and not child.props.get('one_way_collision',False),'Field music shape mechanism outside reviewed rectangle scope')
            require(set(child.props)<={'position','scale','shape','__meta__','disabled','one_way_collision','one_way_collision_margin'},'Unreviewed music rectangle transform')
            doc=owner_of(child,'shape');ref=child.props['shape'];require(doc.sub_type(ref)=='RectangleShape2D','Field music non-rectangle shape requires review');e=doc.sub_resource(ref)['extents'];require(isinstance(e,Vec2)and e.x>0 and e.y>0,'Field music rectangle extents')
            xf=scene.global_xf[child.path];points=[xf.apply(Vec2(x,y))for x,y in [(-e.x,-e.y),(e.x,-e.y),(e.x,e.y),(-e.x,e.y)]];parts.append([[f32(v.x),f32(v.y)]for v in points]);shapes.append(child.path)
        require(1<=len(shapes)<=16,'Field music shape count');regions.append(dict(id=stable(p['scene']+':'+path),source_path=path,track_id=ident,volume_db=values['volume_db'],fadein_seconds=values['fadein_length'],fadeout_seconds=values['fadeout_length'],appear_flag=values['appear_flag'],disappear_flag=values['disappear_flag'],disabled=values['disabled'],shape_paths=shapes));polygons.append(dict(source=path,polygons=parts))
    require(len({r['id']for r in regions})==len(regions)and len({t['stable_id']for t in tracks})==len(tracks),'Field music stable identity collision')
    metadata=dict(silence_db=float(floor[1]),fade_to_seconds=1,tracks=tracks,regions=regions)
    return p,metadata,polygons,dict(sorted(ex.sources.items()))


def validate_metadata(metadata,polygons):
    require(type(metadata)is dict and set(metadata)=={'silence_db','fade_to_seconds','tracks','regions'},'Field music metadata fields')
    def number(v,lo,hi):return type(v)in(int,float)and math.isfinite(v)and lo<=v<=hi
    def path(v):return type(v)is str and 0<len(v)<=1024 and all(32<=ord(c)<127 and c not in ('\\',':')for c in v)and all(x not in ('','.','..')for x in v.split('/'))
    require(number(metadata['silence_db'],-120,-20)and number(metadata['fade_to_seconds'],0,60),'Field music fade tuning')
    tracks=metadata['tracks'];regions=metadata['regions']
    require(type(tracks)is list and 1<=len(tracks)<=16 and type(regions)is list and 1<=len(regions)<=64,'Field music capacity')
    track_ids=set();track_paths=set();pcm_paths=set()
    for t in tracks:
        require(type(t)is dict and set(t)==set('stable_id source_path source_sha256 import_sha256 pcm_path gain_db'.split()),'Field music track fields')
        require(type(t['stable_id'])is int and 0<t['stable_id']<2**32 and t['stable_id']not in track_ids,'Field music track identity');track_ids.add(t['stable_id'])
        require(type(t['source_path'])is str and t['source_path'].startswith('res://Audio/Music/')and path(t['source_path'][6:])and t['source_path']not in track_paths,'Field music track source');track_paths.add(t['source_path'])
        require(path(t['pcm_path'])and t['pcm_path'].startswith('sound/music/')and t['pcm_path'].endswith('.pcm')and t['pcm_path']not in pcm_paths,'Field music PCM path');pcm_paths.add(t['pcm_path'])
        require(all(type(t[k])is str and re.fullmatch('[0-9a-f]{64}',t[k])and int(t[k],16)for k in ('source_sha256','import_sha256'))and number(t['gain_db'],-120,0),'Field music track fingerprint/gain')
    require(type(polygons)is list and len(polygons)==len(regions),'Field music geometry count')
    ids=set();paths=set();shapes=set()
    for r,geometry in zip(regions,polygons):
        require(type(r)is dict and set(r)==set('id source_path track_id volume_db fadein_seconds fadeout_seconds appear_flag disappear_flag disabled shape_paths'.split()),'Field music region fields')
        require(type(r['id'])is int and 0<r['id']<2**32 and r['id']not in ids and type(r['track_id'])is int and r['track_id']in track_ids,'Field music region identity');ids.add(r['id'])
        require(path(r['source_path'])and len(r['source_path'])<=256 and r['source_path']not in paths,'Field music area source');paths.add(r['source_path'])
        require(number(r['volume_db'],-120,24)and number(r['fadein_seconds'],0,60)and number(r['fadeout_seconds'],0,60)and type(r['disabled'])is bool,'Field music region policy')
        require(all(type(r[k])is str and re.fullmatch('[A-Za-z0-9_]*',r[k])for k in ('appear_flag','disappear_flag')),'Field music flag identity')
        require(type(r['shape_paths'])is list and 1<=len(r['shape_paths'])<=16,'Field music source shapes')
        for shape in r['shape_paths']:
            require(path(shape)and shape.startswith(r['source_path']+'/')and shape not in shapes,'Field music shape ownership');shapes.add(shape)
        require(type(geometry)is dict and set(geometry)=={'source','polygons'}and geometry['source']==r['source_path']and type(geometry['polygons'])is list and len(geometry['polygons'])==len(r['shape_paths']),'Field music geometry binding')
        for points in geometry['polygons']:
            require(type(points)is list and len(points)==4 and all(type(v)is list and len(v)==2 and all(number(x,-1e6,1e6)for x in v)for v in points),'Field music rectangle geometry')
            turns=[]
            for i,a in enumerate(points):
                b,c=points[(i+1)%4],points[(i+2)%4];turns.append((b[0]-a[0])*(c[1]-b[1])-(b[1]-a[1])*(c[0]-b[0]))
            require(all(v>0 for v in turns)or all(v<0 for v in turns),'Field music geometry must be strictly convex')
    return metadata


def encode(metadata,polygons):
    validate_metadata(metadata,polygons)
    # The existing ENCMUS01 / ENCZON01 codecs accept any checked source path.
    # Unlike the historical Podunk producer, a scene area need not be under Music/.
    blob=bytearray(64)
    def u(v):blob.extend(struct.pack('<I',v))
    def text(value):raw=value.encode('ascii');u(len(raw));blob.extend(raw)
    for t in metadata['tracks']:u(t['stable_id']);blob.extend(bytes.fromhex(t['source_sha256']));blob.extend(bytes.fromhex(t['import_sha256']));text(t['source_path'])
    for r in metadata['regions']:
        u(r['id']);u(r['track_id']);u(int(r['disabled']));blob.extend(struct.pack('<3f',r['volume_db'],r['fadein_seconds'],r['fadeout_seconds']));text(r['source_path']);text(r['appear_flag']);text(r['disappear_flag']);u(len(r['shape_paths']))
        for path in r['shape_paths']:text(path)
    struct.pack_into('<8s6I2f',blob,0,b'ENCMUS01',1,len(blob),0,len(metadata['regions']),len(metadata['tracks']),sum(len(r['shape_paths'])for r in metadata['regions']),metadata['silence_db'],metadata['fade_to_seconds']);struct.pack_into('<I',blob,16,zlib.crc32(blob))
    payload=bytearray()
    for region in polygons:
        raw=region['source'].encode('ascii');payload.extend(struct.pack('<I',len(raw))+raw+struct.pack('<I',len(region['polygons'])))
        for points in region['polygons']:
            payload.extend(struct.pack('<I',len(points)))
            for x,y in points:payload.extend(struct.pack('<2f',x,y))
    zones=struct.pack('<8s4I',b'ENCZON01',2 if any(not x['source'].startswith('Music/')for x in polygons)else 1,len(payload)+24,zlib.crc32(payload),len(polygons))+payload
    return bytes(blob),zones


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--recipe',type=Path,default=ROOT/'content/mayor-music.json');parser.add_argument('--out',type=Path,required=True);parser.add_argument('--audio',action='store_true');parser.add_argument('--ffmpeg',default='ffmpeg');parser.add_argument('--ffprobe',default='ffprobe');a=parser.parse_args();out=a.out.resolve()
    require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Field music candidates must remain under build');p,metadata,polygons,sources=source(a.recipe);bank,zones=encode(metadata,polygons);out.mkdir(parents=True,exist_ok=True)
    receipt_file=out/'source.json'
    if receipt_file.is_file():receipt_file.unlink()
    generated={p['binding'],p['zones']}
    for key,data in [('binding',bank),('zones',zones)]:file=npc_target(out,p[key]);file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data)
    receipt=dict(source_commit=PIN,dependencies={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in [a.recipe.resolve().relative_to(ROOT).as_posix(),p['field_recipe'],'tools/podunk_field.py','tools/audio_asset.py','content/native-audio.json']},producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),sources=sources,metadata=metadata,polygons=polygons,release_staged=False,audio=None)
    if a.audio:
        from tools import audio_asset
        audio_recipe=json.loads((ROOT/'content/native-audio.json').read_bytes());audio_recipe['assets']=copy.deepcopy(metadata['tracks'])
        with tempfile.TemporaryDirectory(prefix='scene-audio-',dir=out)as temporary:
            staging=Path(temporary);manifest=audio_asset.compile_assets(audio_recipe,ROOT/'upstream/MOTHER-Encore',staging,ffmpeg=a.ffmpeg,ffprobe=a.ffprobe)
            expected={t['pcm_path']for t in metadata['tracks']}|{'sound/banks/opening.encaudio'};require({r['path']for r in manifest['files']}==expected,'Field music compiler emitted unexpected files')
            for row in manifest['files']:
                original=row['path'];data=(staging/original).read_bytes();require(len(data)==row['size']and hashlib.sha256(data).hexdigest()==row['sha256'],'Field audio output fingerprint mismatch');row['path']=p['audio']if original=='sound/banks/opening.encaudio'else original
                generated.add(row['path']);file=npc_target(out,row['path']);file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data)
            receipt['audio']=manifest
    receipt['outputs']={file.relative_to(out).as_posix():dict(bytes=file.stat().st_size,sha256=hashlib.sha256(file.read_bytes()).hexdigest())for file in [npc_target(out,name)for name in sorted(generated)]}
    (out/'source.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Field music candidate:',len(metadata['regions']),'regions,',len(metadata['tracks']),'tracks;',sum(x['bytes']for x in receipt['outputs'].values()),'bytes; not staged')


def npc_target(out,relative):
    file=(out/relative).resolve();require(file.is_relative_to(out)and file!=out,'Field music output escaped owner');return file
if __name__=='__main__':main()
