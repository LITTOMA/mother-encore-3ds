#!/usr/bin/env python3
"""Retain all checked House Control properties; loading never enters a scene."""
from __future__ import annotations
import argparse, hashlib, json, math, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools import house_return_canvas as canvas,house_node_tree as tree,house_geometry
IR=ROOT/'content/native-house-return-controls.json'
REVIEW=ROOT/'reports/house-return-controls/source-review.json'
PACK=ROOT/'romfs/data/house-return.enccontrols'
# These are native schema names, not game content or property defaults.
BASE={
 '_import_path':'path','pause_mode':'int','physics_interpolation_mode':'int',
 'unique_name_in_owner':'bool','process_priority':'int','visible':'bool',
 'modulate':'color','self_modulate':'color','show_behind_parent':'bool',
 'light_mask':'int','material':'nil','use_parent_material':'bool',
 **{k:'real' for k in ('anchor_left','anchor_top','anchor_right','anchor_bottom','margin_left','margin_top','margin_right','margin_bottom','rect_rotation','size_flags_stretch_ratio')},
 **{k:'int' for k in ('grow_horizontal','grow_vertical','focus_mode','mouse_filter','mouse_default_cursor_shape','size_flags_horizontal','size_flags_vertical')},
 **{k:'vec' for k in ('rect_min_size','rect_scale','rect_pivot_offset')},
 'rect_clip_content':'bool','hint_tooltip':'text',
 **{k:'path' for k in ('focus_neighbour_left','focus_neighbour_top','focus_neighbour_right','focus_neighbour_bottom','focus_next','focus_previous')},
 'input_pass_on_modal_close_click':'bool','theme':'nil','theme_type_variation':'text','script':'nil'}
LABEL={'custom_fonts/font':'ref','text':'text','align':'int','valign':'int',
 'autowrap':'bool','clip_text':'bool','uppercase':'bool','percent_visible':'real',
 'lines_skipped':'int','max_lines_visible':'int'}
TEXTURE={'texture':'ref','expand':'bool','stretch_mode':'int','flip_h':'bool','flip_v':'bool'}
TYPES=['nil','bool','int','real','text','path','vec','color','ref','meta']
ENGINE={
 'scene/gui/control.cpp':'48b5ead083050c90441cc532506279852cab9cda86473fcc63090d3476ba7ec3',
 'scene/gui/box_container.cpp':'574a21b17e19c739dc9df95b276bf444bc24bdb85aa3d13a9445e1b86cb1ebe6',
 'scene/gui/container.cpp':'af470a5f41e05b2cdce804e2f33c2163abe7ceb5ae29c00407852e66bab0b9b8',
 'scene/gui/label.cpp':'054562ca81216ddca8e95e218cf20a7cc5ba838530f20a6037c9573f67ee4284',
 'scene/2d/canvas_item.h':'bf6c713f0f0af990b4d2564b30b5dc68a1a02e9a2e37bd03efca2c3a071db8ea',
 'scene/2d/node_2d.h':'061b2f77387359be4f0c4109f307b59c7c78458fbd515b30d8b1b91370e7c2d1',
 'scene/2d/node_2d.cpp':'b23b4474baa0f015e560ce072df745861e6dc9bb7dd388cbe974deb02cc8fd45',
 'scene/main/viewport.cpp':'6eb44227474694932b3ad8f093b032f47f95f1c1ed364c742a3662bd33044ef7',
 'core/math/rect2.h':'3eab30b289e5293c559707bf50ae984451f3b2cb71280e5204a23fea112039cf'}
def canonical(v):return json.dumps(v,sort_keys=True,separators=(',',':'),ensure_ascii=False)
def property_checked(v,t):
 if t=='nil':return v is None
 if t=='bool':return type(v)is bool
 if t=='int':return type(v)is int and -(2**31)<=v<2**31
 if t=='real':return type(v)in(int,float)and math.isfinite(v)and abs(v)<=3.402823466e38
 if t=='text':return isinstance(v,str)and '\0'not in v
 if t=='path':return isinstance(v,dict)and set(v)=={'type','value'}and v['type']=='NodePath'and isinstance(v['value'],str)and not v['value']
 if t=='vec':return isinstance(v,list)and len(v)==2 and all(property_checked(x,'real')for x in v)
 if t=='color':return isinstance(v,dict)and set(v)=={'type','r','g','b','a'}and v['type']=='Color'and all(property_checked(v[k],'real')for k in ('r','g','b','a'))
 if t=='ref':return isinstance(v,dict)and set(v)=={'type','id'}and v['type']=='ResourceReference'and type(v['id'])is int and 0<v['id']<2**31
 if t=='meta':return v=={'type':'Dictionary','pairs':[['_edit_lock_',True]]}
 return False
def schema(cls,p):
 s={**BASE,**(LABEL if cls=='Label'else {'alignment':'int'}if cls=='HBoxContainer'else TEXTURE if cls=='TextureRect'else {'color':'color'}if cls=='ColorRect'else {})}
 if p.get('material')is not None:s['material']='ref'
 if '__meta__'in p:s['__meta__']='meta'
 return s
def derive():
 c=canvas.load();t=tree.load();records=[];native=read(house_geometry.NATIVE)
 require(c['format']==2 and c['scene_id']==t['scene_id'] and c['tree_ir_sha256']==sha(tree.IR),'House Control checked Canvas/tree differs')
 byid={n['id']:n for n in t['records']}
 require(sha(house_geometry.NATIVE)==c['native_sha256'],'House Control complete native export differs')
 native_by={n['path']:n for n in native['nodes']};resources={r['id']:r for r in native['resources']}
 boundaries=list(c['control_boundaries'])
 for draw in c['records']:
  if draw['kind']not in(1,3):continue
  n=byid[draw['id']];p=decode(native_by[n['node']]['properties'])
  boundaries.append(dict(id=draw['id'],node=draw['node'],class_name=native_by[n['node']]['class'],flags=draw['flags'],owner_id=draw['owner_id'],owner_script=draw['owner_script'],owner_sha=draw['owner_sha'],native_properties=p,draw_kind=draw['kind'],texture=draw['texture']))
 for b in boundaries:
  cls=b['class_name'];require(cls in ('Control','HBoxContainer','Label','TextureRect','ColorRect'),'House Control native class unknown')
  props=b['native_properties'];s=schema(cls,props)
  require(set(props)==set(s),'House Control complete native property names differ')
  require(all(property_checked(props[k],s[k])for k in props),'House Control native property type rejected')
  n=byid[b['id']];require(n['node']==b['node'],'House Control stable tree node differs')
  if cls=='Control':require(t['classes'][byid[n['parent']]['class_index']]=='Node2D','House Control native parent anchor class unsupported')
  texture_source='';texture_sha='0'*64
  if cls=='TextureRect':
   tex=next(x for x in c['textures']if x['id']==b['texture']);resource=resources[props['texture']['id']]
   require(resource['class']=='StreamTexture'and resource['path']=='res://'+tex['source'],'House Control actual native texture resource differs')
   texture_source=tex['source'];texture_sha=tex['source_sha256']
  pose=1 if cls in('HBoxContainer','Label')or cls=='TextureRect'and b['owner_id']and next(x for x in c['records']if x['id']==b['id'])['owner']==6 else 2 if cls in('TextureRect','ColorRect')else 0
  records.append(dict(id=b['id'],parent=n['parent'],owner_id=b['owner_id'],node=b['node'],native_class=cls,flags=b['flags'],owner_script=b['owner_script'],owner_sha=b['owner_sha'],native_properties=props,native_properties_sha256=hashlib.sha256(canonical(props).encode()).hexdigest(),font_source=b.get('font_source',''),font_source_sha256=b.get('font_source_sha256','0'*64),draw_kind=b.get('draw_kind',0),texture=b.get('texture',0),texture_source=texture_source,texture_source_sha256=texture_sha,pose_owner=pose))
 require(len(records)==72 and sum(r['native_class']=='Label'for r in records)==23 and sum(r['native_class']=='HBoxContainer'for r in records)==23 and sum(r['native_class']=='TextureRect'for r in records)==24 and sum(r['native_class']=='ColorRect'for r in records)==1,'House Control exact native closure differs')
 for r in records:
  if r['native_class']=='Label':
   p=next((x for x in records if x['id']==r['parent']),None)
   require(p and p['native_class']=='HBoxContainer'and p['owner_id']==r['owner_id'],'House Label actual container/owner differs')
 return dict(schema=1,kind='encore.house-return-controls.source-ir',format=1,capabilities=1,rules=1,commit=PIN,scene=c['scene'],scene_id=c['scene_id'],scene_sha256=c['scene_sha256'],canvas_ir_sha256=sha(canvas.IR),tree_ir_sha256=sha(tree.IR),native_sha256=c['native_sha256'],source_receipt_sha256=c['source_receipt_sha256'],producer_sha256=sha(Path(__file__)),sources=c['sources'],engine_tag='3.6.2-stable',engine_sources=ENGINE,native_parent_anchor='CanvasItem::get_anchorable_rect returns Rect2(0,0,0,0); actual Node2D does not override',records=records,scene_admitted=False)
def review(d):return dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=d['producer_sha256'],canvas_ir_sha256=d['canvas_ir_sha256'],tree_ir_sha256=d['tree_ir_sha256'],native_sha256=d['native_sha256'],source_receipt_sha256=d['source_receipt_sha256'],sources=d['sources'],engine_tag=d['engine_tag'],engine_sources=d['engine_sources'],counts={'Control':1,'HBoxContainer':23,'Label':23,'TextureRect':24,'ColorRect':1},scene_admitted=False,semantics=['All72 complete original native Control properties retained; original47 canonical property digests match House Canvas boundaries, additional25 crossbind actual draw/native/texture resources','Unknown classes, property names, types, formats and mismatched original native/source closure reject','Prompt native owner is sole live HBox/Label/Arrow state and same SourceFontRenderer GPU owner; remaining TextureRect/ColorRect borrow actual House Canvas owner; no second layout clock or source Ready','Room Shaker Control retains exact anchors/margins/grow/pivot/native state; its original script lifecycle remains separately required','Reviewed native CanvasItem parent anchor rectangle is empty for Node2D; it is not replaced with viewport size','ColorRect actual editor _edit_lock_ metadata explicitly retained with closed Dictionary schema'])
def extract():
 d=derive();write(IR,d);write(REVIEW,review(d));return d
def load():
 d=read(IR);require(d==derive()and read(REVIEW)==review(d),'House Control source/review stale');return d
def encode(d):
 def u(v):return struct.pack('<I',v)
 def text(v):b=v.encode();return u(len(b))+b
 b=text(d['scene'])+bytes.fromhex(d['canvas_ir_sha256'])+bytes.fromhex(d['tree_ir_sha256'])+u(len(d['engine_sources']))
 for p,h in sorted(d['engine_sources'].items()):b+=text(p)+bytes.fromhex(h)
 b+=u(len(d['records']))
 for r in d['records']:
  b+=u(r['id'])+u(r['parent'])+u(r['owner_id'])+u(r['flags'])+text(r['node'])+text(r['native_class'])+text(r['owner_script'])+bytes.fromhex(r['owner_sha'])+bytes.fromhex(r['native_properties_sha256'])+text(r['font_source'])+bytes.fromhex(r['font_source_sha256'])
  b+=u(r['draw_kind'])+u(r['texture'])+text(r['texture_source'])+bytes.fromhex(r['texture_source_sha256'])+u(r['pose_owner'])
  p=r['native_properties'];s=schema(r['native_class'],p);b+=u(len(p))
  for k in sorted(p):b+=text(k)+u(TYPES.index(s[k]))+text(canonical(p[k]))
 b+=u(len(d['sources']))
 for p,h in sorted(d['sources'].items()):b+=text(p)+bytes.fromhex(h)
 return struct.pack('<8s8I20s32s32sI',b'ENCHCTL1',1,128,128+len(b),zlib.crc32(b),0x454e0081,1,1,d['scene_id'],bytes.fromhex(d['commit']),bytes.fromhex(d['scene_sha256']),bytes.fromhex(sha(IR)),0)+b
def compile():
 d=load();b=encode(d);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);return b
def stage_files(source):
 b=encode(load());require((Path(source)/'data/house-return.enccontrols').read_bytes()==b,'House Control staged pack differs');return {Path('data/house-return.enccontrols'):b}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args()
 if a.action=='extract':print('House Control source records:',len(extract()['records']),'; no Ready')
 else:print('House Control checked pack:',len(compile()),'bytes; no Ready')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE CONTROL ERROR: '+str(e))
