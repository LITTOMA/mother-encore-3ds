#!/usr/bin/env python3
"""Observe checked external house changes through one unchanged host executable."""
import argparse,copy,hashlib,json,subprocess
from pathlib import Path
import native_house as h

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,required=True);p.add_argument('--dir',type=Path,default=h.ROOT/'reports/sister-door-data');a=p.parse_args();a.dir.mkdir(parents=True,exist_ok=True)
 ir=json.loads((h.ROOT/'content/native-house.json').read_text());pres=json.loads((h.ROOT/'content/native-house-presentation.json').read_text());h.verify_sources(ir,pres)
 changed=copy.deepcopy(ir);presentation=copy.deepcopy(pres)
 changed['doors'][0]['destination'][0]=222;changed['doors'][0]['fade_in_speed']=2;changed['interaction']['ray_length']=18;changed['npcs'][0]['view_radius']=46;changed['segments'][0]['tokens'][0]['text']='Externally changed greeting, '
 changed['openable_doors'][1]['close_delay']=.45;changed['openable_doors'][1]['ram_required_y']=0;changed['openable_doors'][1]['ram_strength']=2
 changed['story_triggers'][0]['center'][0]=129;changed['story_conditions'][0]['value']=1;changed['segments'][4]['tokens'][0]['text']='External blocked-door text';presentation['profiles'][1]['sprite_offset'][1]=-4
 before=digest(a.probe);observed={};hashes={}
 for key,source,pr in [('a',ir,pres),('b',changed,presentation)]:
  blob=h.encode(h.lower(source,pr));h.parse_pack(blob);path=a.dir/('house-'+key+'.enchouse');path.write_bytes(blob);hashes[key]=digest(path);observed[key]=subprocess.check_output([str(a.probe.resolve()),str(path.resolve())],text=True).strip()
 expected_a="destination=220,385 fade_speed=1.5 ray_length=16 view_radius=44 text=Are you okay, \nclose_delay=0.3 ram_y=-1 ram_strength=1 sprite_y=-3 story_center=128,176 condition_value=0 blocked=The door won't open!"
 expected_b="destination=222,385 fade_speed=2 ray_length=18 view_radius=46 text=Externally changed greeting, \nclose_delay=0.45 ram_y=0 ram_strength=2 sprite_y=-4 story_center=129,176 condition_value=1 blocked=External blocked-door text"
 h.require(observed=={'a':expected_a,'b':expected_b},'House data-only observations differ');after=digest(a.probe);h.require(before==after,'Probe executable changed')
 report=dict(schema=2,binary_before=before,binary_after=after,same_binary=True,pack_sha256=hashes,observed=observed,scope='Host checked loader observation; no emulator/hardware claim',changes=['warp destination/fade','ray/view geometry','NPC text','blocked-door text','door Timer/ram condition/strength','per-NPC presentation offset','story geometry/condition'])
 (a.dir/'data-only-proof.json').write_text(json.dumps(report,indent=2)+'\n');print('House data-only proof: unchanged executable observed all twelve external content changes')
if __name__=='__main__':main()
