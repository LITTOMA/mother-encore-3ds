#!/usr/bin/env python3
"""Generate a checked field map candidate under build; never a release scene."""
from __future__ import annotations
import argparse,hashlib,json,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f

def target(root,path):
 f.require(type(path)is str and path and chr(92)not in path and ':'not in path and not Path(path).is_absolute()and all(p not in ('','.','..')for p in Path(path).parts),'Invalid candidate resource path')
 out=(root/path).resolve();f.require(out.is_relative_to(root.resolve()),'Candidate path escapes owner');return out

def compile_map(recipe_path,out,tex3ds,command=None):
 out=Path(out).resolve();f.require(out.is_relative_to((ROOT/'build').resolve()),'Candidate root must stay in build')
 recipe=f.load_recipe(recipe_path);f.require(recipe['schema']==2,'Candidate tool expects general field recipe')
 model=f.Model(recipe);atlas=f.Atlases(model);paths=[recipe['atlas_directory']+'/atlas-%02d.t3x'%i for i in range(len(atlas.pages))]
 tool=Path(tex3ds).resolve();f.require(tool.is_file(),'Real tex3ds required for candidate graphics')
 root=out/'romfs';pages=[];out.mkdir(parents=True,exist_ok=True)
 for i,page in enumerate(atlas.pages):
  png=out/('atlas-%02d.png'%i);dest=target(root,paths[i]);dest.parent.mkdir(parents=True,exist_ok=True);temp=dest.with_suffix('.t3x.candidate')
  try:
   page.save(png,optimize=False)
   if command:
    rel=lambda p:p.resolve().relative_to(ROOT).as_posix()
    subprocess.run(shlex.split(command)+['-f','rgba8','-z','none','-o',rel(temp),rel(png)],cwd=ROOT,check=True)
   else:subprocess.run([str(tool),'-f','rgba8','-z','none','-o',str(temp),str(png)],cwd=ROOT,check=True)
   f.require(temp.is_file()and temp.stat().st_size>0,'Real texture compiler produced no data');temp.replace(dest)
   pages.append({'path':paths[i],'width':page.width,'height':page.height,'rgba_sha256':atlas.digest(i),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'bytes':dest.stat().st_size})
  finally:png.unlink(missing_ok=True);temp.unlink(missing_ok=True)
 blob=f.encode_map(model,atlas,paths);scene,_=f.unpack_container(blob,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
 dest=target(root,recipe['outputs']['map']);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(blob)
 proof={'scope':'Candidate static map/graphics only; Room/NPC/story/music and playable scene are not staged','commit':f.PIN,'scene':scene,'recipe_sha256':hashlib.sha256(Path(recipe_path).read_bytes()).hexdigest(),'producer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'map_compiler_sha256':hashlib.sha256(Path(f.__file__).read_bytes()).hexdigest(),'tex3ds_sha256':hashlib.sha256(tool.read_bytes()).hexdigest(),'map':{'path':recipe['outputs']['map'],'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest()},'pages':pages,'counts':{name:len(getattr(model,name))for name in ('layers','sprites','bodies','shape_rows','doors','cameras','items','groups')},'source_actor_indices':model.npc_bindings,'sources':model.sources.digests(),'undefined_tile_cells':model.undefined_cells}
 (out/'candidate.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2)+chr(10),encoding='utf-8');return proof

def main():
 p=argparse.ArgumentParser();p.add_argument('--recipe',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--tex3ds',type=Path,required=True);p.add_argument('--tex3ds-command');a=p.parse_args();print(json.dumps(compile_map(a.recipe,a.out,a.tex3ds,a.tex3ds_command),ensure_ascii=False,indent=2))
if __name__=='__main__':main()
