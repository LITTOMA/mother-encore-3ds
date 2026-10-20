"""Observe actual imported source textures; no full scene or pixel-match claim."""
import argparse,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_mechanism_graphics as graphics
from tools.run_scene_reference import invoke
from PIL import Image
import io

PROBE='''extends SceneTree
func _init():call_deferred("run")
func run():
    var config=File.new()
    assert(config.open("res://input.json",File.READ)==OK)
    var paths=JSON.parse(config.get_as_text()).result
    config.close()
    var result={"engine":Engine.get_version_info(),"textures":[]}
    for row in paths:
        var texture=load("res://"+row.path)
        assert(texture is StreamTexture)
        var image=texture.get_data()
        image.convert(Image.FORMAT_RGBA8)
        var file=File.new()
        assert(file.open("res://pixels-"+str(row.id)+".bin",File.WRITE)==OK)
        file.store_buffer(image.get_data())
        file.close()
        result.textures.append({"id":row.id,"path":row.path,"width":image.get_width(),"height":image.get_height(),"flags":texture.flags,"format":image.get_format()})
    var file=File.new()
    assert(file.open("res://observations.json",File.WRITE)==OK)
    file.store_string(JSON.print(result,"  "))
    file.close()
    print("DUNGEON_TEXTURE_REFERENCE_COMPLETE")
    quit()
'''

def run(out,engine):
 out=Path(out).resolve();engine=Path(engine).resolve();graphics.require(out.is_relative_to((ROOT/'build').resolve())and out!=ROOT/'build'and not out.exists()and engine.is_file(),'Fresh owned texture reference and actual engine required')
 p,ex=graphics.read();project=out/'project';project.mkdir(parents=True);(project/'.owned-mechanism-texture-reference').write_text('source-mechanism-texture-v1',encoding='utf8')
 rows=[dict(id=r['id'],path=r['texture'])for r in p['profiles']]
 for r in rows:
  for path in(r['path'],r['path']+'.import'):
   target=project/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(ex.data(path))
 (project/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Private texture reference"\n',encoding='utf8');(project/'input.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf8');(project/'probe.gd').write_text(PROBE,encoding='utf8')
 invoke(engine,project,['--editor','--quit'],out/'import.log');invoke(engine,project,['--script','res://probe.gd'],out/'engine.log')
 observed=json.loads((project/'observations.json').read_bytes());version=observed['engine'];graphics.require(tuple(version[k]for k in('major','minor','patch','hash'))==(3,6,2,'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'),'Texture engine changed')
 graphics.require([r['id']for r in observed['textures']]==[r['id']for r in rows],'Texture observation owner coverage')
 for row in observed['textures']:
  pixels=(project/('pixels-'+str(row['id'])+'.bin')).read_bytes()
  with Image.open(io.BytesIO(ex.data(row['path'])))as image:source=image.convert('RGBA');raw=source.tobytes();size=source.size
  graphics.require(size==(row['width'],row['height'])and len(raw)==len(pixels)and row['format']==5 and row['flags']==0,'Source texture size/format/filter/repeat/mipmap changed')
  visible=alpha=transparent=0
  for i in range(0,len(raw),4):
   if raw[i+3]!=pixels[i+3]:alpha+=1
   if raw[i:i+4]!=pixels[i:i+4]:
    if raw[i+3]:visible+=1
    else:transparent+=1
  graphics.require(not visible and not alpha,'Imported texture changes visible source pixels/alpha')
  row.update(source_rgba_sha256=hashlib.sha256(raw).hexdigest(),imported_rgba_sha256=hashlib.sha256(pixels).hexdigest(),changed_visible_pixels=visible,changed_alpha_pixels=alpha,changed_transparent_rgb_pixels=transparent)
 (out/'observations.json').write_text(json.dumps(observed,indent=2)+'\n',encoding='utf8');(out/'input.json').write_bytes((project/'input.json').read_bytes())
 proof=dict(commit=graphics.PIN,engine_sha256=hashlib.sha256(engine.read_bytes()).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),graphics_review_sha256=hashlib.sha256(graphics.RECIPE.read_bytes()).hexdigest(),sources=ex.sources,adapter_sha256=hashlib.sha256(PROBE.encode()).hexdigest(),observations_sha256=hashlib.sha256((out/'observations.json').read_bytes()).hexdigest(),scope=__doc__,whole_scene_approved=False)
 (out/'receipt.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Actual texture import:4 profiles; all visible RGBA and alpha unchanged; only alpha-zero RGB may differ')
 return proof

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--godot',type=Path,required=True);a=p.parse_args();run(a.out,a.godot)
