"""Append source-admitted media without re-decoding existing checked PCM.

The pure merger rejects removals, retuning, changed old assets/receipts and mixed
reference decoders. Publication is explicit; no source or audio bytes are patched.
"""
import argparse,copy,hashlib,json,math,os,re,shutil,sys,tempfile,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools import audio_asset as a
from tools.extract_battle_entry import require,Extractor


def canonical(v):return json.dumps(v,sort_keys=True,separators=(',',':')).encode()
def file_record(path,raw):return dict(path=path,size=len(raw),sha256=a.sha(raw))

def checked_previous(recipe,manifest,blob,bank_path):
    require(type(recipe)is dict and set(recipe)==set('schema upstream_commit bus_source bus_sha256 manager_source manager_sha256 assets'.split())and type(manifest)is dict and set(manifest)==set('schema upstream_commit recipe_sha256 ffmpeg_version ffmpeg_sha256 assets files scope'.split()),'Audio append prior unknown fields')
    require(type(recipe.get('schema'))is int and type(manifest.get('schema'))is int and manifest.get('schema')==recipe.get('schema')==1 and manifest['upstream_commit']==recipe['upstream_commit']and manifest['recipe_sha256']==a.sha(canonical(recipe)),'Audio append prior recipe/receipt mismatch')
    require(type(recipe['assets'])is list and type(manifest['assets'])is list and type(manifest['files'])is list and type(manifest['ffmpeg_version'])is str and manifest['ffmpeg_version']and type(manifest['ffmpeg_sha256'])is str and re.fullmatch('[0-9a-f]{64}',manifest['ffmpeg_sha256'])and int(manifest['ffmpeg_sha256'],16),'Audio append prior decoder/list fields')
    bank=a.parse_bank(blob);require(len(bank['assets'])==len(recipe['assets'])==len(manifest['assets']),'Audio append prior asset count')
    files={f['path']:f for f in manifest['files']};require(len(files)==len(manifest['files'])and files.get(bank_path)==file_record(bank_path,blob),'Audio append prior bank fingerprint')
    require(set(files)=={bank_path}|{s['pcm_path']for s in recipe['assets']},'Audio append prior file coverage')
    for declaration,receipt,record in zip(recipe['assets'],manifest['assets'],bank['assets']):
        require(set(receipt)==set(record)|set('source_sample_rate import_sha256 loop_offset_seconds pcm_sha256 command'.split()),'Audio append prior receipt fields')
        require(set(declaration)in (set('stable_id source_path source_sha256 import_sha256 pcm_path gain_db'.split()),set('stable_id source_path source_sha256 import_sha256 pcm_path gain_db output_sample_rate'.split())),'Audio append prior declaration fields')
        require(type(declaration['stable_id'])is int and type(declaration['gain_db'])in(int,float)and ("output_sample_rate"not in declaration or type(declaration['output_sample_rate'])is int),'Audio append prior declaration types')
        require(all((type(receipt.get(k))in(int,float)if type(v)is float else type(receipt.get(k))is type(v))for k,v in record.items()),'Audio append prior decoded types')
        require(all(receipt.get(k)==v for k,v in record.items())and all(receipt.get('sample_rate'if k=='output_sample_rate'else k)==v for k,v in declaration.items()),'Audio append prior declaration/decoded record changed')
        require(type(receipt['source_sample_rate'])is int and receipt['source_sample_rate']>0 and type(receipt['command'])is list and receipt['command']and all(type(v)is str for v in receipt['command']),'Audio append prior decoder command/rate')
        require(all(type(receipt[k])is str and re.fullmatch('[0-9a-f]{64}',receipt[k])and int(receipt[k],16)for k in ('import_sha256','pcm_sha256')),'Audio append prior receipt digest')
        offset=receipt['loop_offset_seconds'];require(type(offset)in(int,float)and math.isfinite(offset)and offset>=0 and int(offset*record['sample_rate'])==record['loop_start']and(record['loop']or offset==0),'Audio append prior loop settings')
        require(files[record['pcm_path']]==dict(path=record['pcm_path'],size=record['pcm_bytes'],sha256=receipt['pcm_sha256']),'Audio append prior PCM fingerprint')
    return bank

def merge(old_recipe,old_manifest,old_blob,new_recipe,added_manifest,added_blob,*,bank_path='sound/banks/opening.encaudio'):
    old=checked_previous(old_recipe,old_manifest,old_blob,bank_path)
    require(set(old_recipe)==set(new_recipe)and all(old_recipe[k]==new_recipe[k]for k in old_recipe if k!='assets'),'Audio append source envelope changed')
    count=len(old_recipe['assets']);require(new_recipe['assets'][:count]==old_recipe['assets']and count<len(new_recipe['assets'])<=64,'Audio append must preserve all existing declarations in order')
    tail=copy.deepcopy(new_recipe);tail['assets']=new_recipe['assets'][count:];added=checked_previous(tail,added_manifest,added_blob,'sound/banks/opening.encaudio')
    require(all(old[k]==added[k]for k in ('master_db','silence_db')),'Audio append changes global gain')
    require(old_manifest['ffmpeg_sha256']==added_manifest['ffmpeg_sha256']and old_manifest['ffmpeg_version']==added_manifest['ffmpeg_version'],'Audio append requires the same reviewed decoder for an unambiguous receipt')
    blob=a.build_bank(old['assets']+added['assets'],old['master_db'],old['silence_db']);manifest=copy.deepcopy(old_manifest);manifest['recipe_sha256']=a.sha(canonical(new_recipe));manifest['assets']+=copy.deepcopy(added_manifest['assets'])
    pcm_files=[copy.deepcopy(f)for f in old_manifest['files']if f['path']!=bank_path]+[copy.deepcopy(f)for f in added_manifest['files']if f['path']!='sound/banks/opening.encaudio'];manifest['files']=sorted(pcm_files+[file_record(bank_path,blob)],key=lambda f:f['path'])
    checked_previous(new_recipe,manifest,blob,bank_path);return blob,manifest

def publish(metadata,payloads,validate):
    saved={path:path.read_bytes()for path in metadata};created=[]
    try:
        for path,raw in payloads.items():
            path.parent.mkdir(parents=True,exist_ok=True)
            with path.open('xb')as stream:
                created.append(path)
                stream.write(raw)
        for path,raw in metadata.items():path.write_bytes(raw)
        validate()
    except BaseException:
        for path,raw in saved.items():path.write_bytes(raw)
        for path in created:path.unlink()
        raise

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--stage',action='store_true');ap.add_argument('--scope',choices=('run-loop','prop-nodes','item-consume'),default='run-loop');ap.add_argument('--base',type=Path,help='Read-only prior generated inputs under build; candidate-only reproduction');ap.add_argument('--out',type=Path,required=True);ap.add_argument('--ffmpeg',default='ffmpeg');ap.add_argument('--ffprobe',default='ffprobe');o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Audio append candidate must stay under build');out.mkdir(parents=True,exist_ok=True)
    from tools.phone_linker_bindings import audio,verify_audio
    from tools.footstep_audio import bindings
    recipe_path=ROOT/'content/native-audio.json';manifest_path=ROOT/'content/asset-receipts/audio/opening.json';bank_path=ROOT/'romfs/sound/banks/opening.encaudio';prior=o.base.resolve()if o.base else None
    require(prior is None or prior.is_relative_to(ROOT/'build')and prior!=ROOT/'build'and not o.stage,'Audio append prior snapshots stay under build and cannot publish')
    old_recipe=json.loads((prior/'native-audio.json'if prior else recipe_path).read_bytes());old_manifest=json.loads((prior/'opening.json'if prior else manifest_path).read_bytes());old_blob=(prior/'opening.encaudio'if prior else bank_path).read_bytes();new=audio(ROOT,json.loads((ROOT/'content/native-opening.json').read_bytes()));verify_audio(new,ROOT)
    if new==old_recipe:
        from tools.restore_audio import validated_plan
        plan,*_=validated_plan(ROOT,ROOT/'romfs');require(all(not p['missing']for p in plan),'Audio current metadata has missing PCM');print('Audio append: already current; no files changed');return
    base=checked_previous(old_recipe,old_manifest,old_blob,'sound/banks/opening.encaudio');tail=copy.deepcopy(new);tail['assets']=new['assets'][len(old_recipe['assets']):]
    if o.scope=='prop-nodes':
        from tools.field_prop_audio import bindings
    elif o.scope=='item-consume':
        from tools.item_consume_audio import bindings
    expected=bindings(ROOT);require([v['stable_id']for v in tail['assets']]==[v['identity']['value']for v in expected],'Audio append scope must be exactly reviewed '+o.scope+' bindings')
    from tools.restore_audio import safe_target
    for record in old_manifest['files']:
        if record['path']=='sound/banks/opening.encaudio':continue
        target=safe_target(ROOT/'romfs',record['path']);require(target.is_file(),'Audio append existing PCM missing');raw=target.read_bytes();decoded=next(v for v in base['assets']if v['pcm_path']==record['path']);require(len(raw)==record['size']and a.sha(raw)==record['sha256']and zlib.crc32(raw)==decoded['pcm_crc32'],'Audio append existing PCM changed')
    with tempfile.TemporaryDirectory(prefix='run-audio-append-',dir=out)as temporary:
        temporary=Path(temporary);ex=Extractor(ROOT);added=a.compile_assets(tail,ex.upstream,temporary,ffmpeg=o.ffmpeg,ffprobe=o.ffprobe);added_blob=(temporary/'sound/banks/opening.encaudio').read_bytes();blob,manifest=merge(old_recipe,old_manifest,old_blob,new,added,added_blob)
        for row in added['files']:
            if row['path']=='sound/banks/opening.encaudio':continue
            dest=out/row['path'];dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(temporary/row['path'],dest)
    files={'native-audio.json':(json.dumps(new,indent=2)+'\n').encode(),'opening.json':(json.dumps(manifest,indent=2)+'\n').encode(),'opening.encaudio':blob}
    for name,raw in files.items():(out/name).write_bytes(raw)
    proof=dict(scope='Append exact reviewed '+o.scope+' bindings only',old_asset_count=len(base['assets']),new_asset_count=len(new['assets']),old_records_unchanged=a.parse_bank(blob)['assets'][:len(base['assets'])]==base['assets'],unchanged_pcm={f['path']:f['sha256']for f in old_manifest['files']if f['path']!='sound/banks/opening.encaudio'},source_producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),new_files={name:file_record(name,raw)for name,raw in files.items()},staged=False)
    if o.stage:
        metadata={recipe_path:files['native-audio.json'],manifest_path:files['opening.json'],bank_path:blob};payloads={safe_target(ROOT/'romfs',row['path']):(out/row['path']).read_bytes()for row in added['files']if row['path']!='sound/banks/opening.encaudio'}
        def validate_publication():
            from tools.restore_audio import validated_plan
            plan,*_=validated_plan(ROOT,ROOT/'romfs');require(all(not p['missing']for p in plan),'Published audio PCM missing')
        publish(metadata,payloads,validate_publication);proof['staged']=True
    (out/'source.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Audio append:',len(base['assets']),'->',len(new['assets']),'assets; existing PCM/records retained; staged=',o.stage)
if __name__=='__main__':main()
