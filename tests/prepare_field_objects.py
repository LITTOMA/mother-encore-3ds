"""Source-owned metadata fixture, not shipped object behavior or texture assets."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f
def build():
 folder=ROOT/'reports/iggy-tutorial/shovel-preparation';audit=json.loads((folder/'shovel-next-source-audit.json').read_bytes());visual=json.loads((folder/'visual-source.json').read_bytes())
 assert audit['commit']==visual['upstream']==f.PIN
 for name,digest in audit['sources'].items():assert hashlib.sha256((ROOT/'upstream/MOTHER-Encore'/name).read_bytes()).hexdigest()==digest,name
 assert visual['sources']==audit['sources']and visual['atlas']['width']==32 and visual['atlas']['height']==32
 pool=bytearray();seen={}
 def text(value):
  if not value:return(0,0)
  if value not in seen:seen[value]=(len(pool),len(value.encode('utf8')));pool.extend(value.encode('utf8'))
  return seen[value]
 texture=text('graphics/world/podunk/objects/atlas-00.t3x');path=text(audit['object_path']);flag=text(audit['ready']['flag']);sparkle=audit['sparkles'];position=audit['ready']['position']
 policy=struct.pack('<6I',32,32,0,0,*texture)+bytes.fromhex(visual['outputs']['atlas-00.t3x']['sha256'])+bytes(8)
 frames=visual['atlas']['frames'];indices=visual['sparkle_animation']['frame_indices'];assert len(frames)==6 and len(indices)==40
 obj=struct.pack('<8I4I9f3I',1,1,255,0,0,len(indices),sparkle['serialized_initial_frame'],0,*path,*flag,*position,0,0,*sparkle['position'],sparkle['speed'],*sparkle['random_range'],1,0,0)
 assert len(policy)==64 and len(obj)==96
 rectangles=b''.join(struct.pack('<4I',row['x'],row['y'],row['width'],row['height'])for row in frames)
 sequence=struct.pack('<'+'I'*len(indices),*indices)
 recipe=f.load_recipe();return f.pack_container(b'ENCFOB01',recipe['scene']['id'],[bytes(pool),policy,obj,rectangles,sequence],(1,64,96,16,4),1024*1024,capability=1)
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve();assert out!=ROOT/'build'and(ROOT/'build').resolve()in out.parents;out.mkdir(parents=True,exist_ok=True);blob=build();(out/'source.encobjects').write_bytes(blob);print('Source object metadata fixture:',len(blob),'bytes; no shipped behavior/texture')
if __name__=='__main__':main()
