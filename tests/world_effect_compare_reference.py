#!/usr/bin/env python3
"""Verify production kernel against retained numeric Godot source-render data."""
import argparse,collections,hashlib,json,subprocess,tempfile,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(raw):return hashlib.sha256(raw).hexdigest()
def compare(probe,reference):
 manifest=json.loads((reference/'reference.json').read_text());raw=zlib.decompress((reference/'native-rgba.zlib').read_bytes())
 assert sha(raw)==manifest['numeric_rgba_sha256']
 records=[];offset=0;total=0
 with tempfile.TemporaryDirectory() as temp:
  for i,s in enumerate(manifest['samples']):
   count=s['width']*s['height']*4;actual=raw[offset:offset+count];offset+=count;out=Path(temp)/'native.rgba'
   subprocess.run([str(probe),'--compose',str(ROOT/'romfs/data/melody.encfx'),str(s['width']),str(s['height']),str(s['time']),*[str(v)for v in s['color']],str(s['alpha']),str(out)],check=True)
   cpu=out.read_bytes()
   # GLES2 transparent target stores source-alpha blended RGB. The C2D source
   # texture is straight RGBA8; apply its ordinary blend over transparent black.
   expected=bytearray(cpu)
   for p in range(0,len(cpu),4):
    for c in range(3):expected[p+c]=(cpu[p+c]*cpu[p+3]+127)//255
   mismatch=sum(actual[p:p+4]!=expected[p:p+4]for p in range(0,count,4))
   delta=max(abs(a-b)for a,b in zip(actual,expected));total+=count//4
   record={'sample':i,'width':s['width'],'height':s['height'],'time':s['time'],'mismatched_pixels':mismatch,'maximum_channel_delta':delta,'native_sha256':sha(actual),'expected_sha256':sha(expected)}
   records.append(record)
   if mismatch:raise AssertionError(record)
 assert offset==len(raw)
 return {'passed':True,'samples':records,'compared_pixels':total,'scope':'Source shader320x180, separately marked centered400x240 extension, and RGBA8 tint/fade blend samples. Numerical offscreen reference; not full UI or PICA hardware pixel equivalence.'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--reference',type=Path,default=ROOT/'reports/world-effect-reference');p.add_argument('--report',type=Path);a=p.parse_args();r=compare(a.probe.resolve(),a.reference)
 if a.report:a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(r,indent=2)+'\n')
 print(f"World effect native numeric reference: {r['compared_pixels']} pixels, {len(r['samples'])} samples, exact bytes")
