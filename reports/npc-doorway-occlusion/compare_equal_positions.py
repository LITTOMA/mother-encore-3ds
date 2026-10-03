from pathlib import Path
import json,math,hashlib
from PIL import Image
R=Path(__file__).resolve().parents[2]
r=json.loads((R/'content/native-opening.json').read_text());rows=json.loads((R/'romfs/house-layers/source.json').read_text())['tiles'];up=R/'upstream/MOTHER-Encore';mask=Image.open(up/'Graphics/Tilesets/TilPodunkInteriors.png').convert('RGBA')
result=[]
for name,idx,path in [('Ninten',0,'Graphics/Character Sprites/Ninten/main.png'),('Minnie',4,'Graphics/Character Sprites/Npcs/4dir/minnie.png')]:
 p=r['sections']['ActorProfile'][idx];res=r['sections']['Resource'][p['primary_resource']];tex=Image.open(up/path).convert('RGBA');w=res['width']//res['columns'];h=res['height']//res['rows'];frame=1
 for y in (160,176,184,192,200):
  x0=math.floor(464+p['sprite_position'][0]+p['sprite_offset'][0]-w/2+.5);y0=math.floor(y+p['sprite_position'][1]+p['sprite_offset'][1]-h/2+.5)
  opaque=covered=below=withbars=0
  for sy in range(h):
   for sx in range(w):
    if not tex.getpixel((frame%res['columns']*w+sx,frame//res['columns']*h+sy))[3]:continue
    opaque+=1;x=x0+sx;wy=y0+sy;masked=False
    for t in rows:
     if t['flags']==1 and t['x']<=x<t['x']+t['width'] and t['y']<=wy<t['y']+t['height'] and mask.getpixel((t['u']+x-t['x'],t['v']+wy-t['y']))[3]:masked=True;break
    covered+=masked
    if wy>=192 and not masked:
     below+=1
     # Original 28-pixel source bottom bar, bottom-anchored on native viewport.
     withbars+=not (212<=wy+24<240)
  result.append(dict(actor=name,root=[464,y],source_pixels=opaque,covered_by_original_above=covered,uncovered_below_above=below,uncovered_below_above_with_source_bars=withbars))
report=dict(scope='Equal-position source sprite alpha + exact original Above atlas. Common native pose transform/floor(+0.5). No runtime geometry or source data changed. This is differential pixel evidence, not an emulator result.',results=result)
(R/'reports/npc-doorway-occlusion/equal-position-pixels.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
