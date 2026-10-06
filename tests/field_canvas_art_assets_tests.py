"""Manual-only source/asset negative cases. Never invoked automatically."""
import copy,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.field_canvas_art import load,stage_files,encode,read,RECEIPT,PACK
def main():
 d=load();a=read(RECEIPT);assert PACK.read_bytes()==encode(d,a);files=stage_files(ROOT/'romfs');assert len(files)==len(d['textures'])+2
 assert len({r['id']for r in d['records']})==len(d['records'])
 for r in d['records']:
  if r['shader']:assert r['owner']and r['shader_source']
  if r['kind']==1:assert r['stretch']in[2,3]and not r['centered']
 # Changed/missing actual converted bytes must fail even if a CRC-only file
 # would look structurally plausible. Temporary directory receives fixtures.
 import tempfile
 with tempfile.TemporaryDirectory()as name:
  root=Path(name)
  for p,raw in files.items():(root/p).parent.mkdir(parents=True,exist_ok=True);(root/p).write_bytes(raw)
  resource=Path(d['textures'][0]['path']);raw=(root/resource).read_bytes();(root/resource).write_bytes(bytes([raw[0]^1])+raw[1:])
  try:stage_files(root)
  except ValueError:pass
  else:raise AssertionError('Changed actual texture bytes accepted')
if __name__=='__main__':main()
