"""Actual Godot Shape2D capsule/rectangle queries with source node transforms.

This isolates geometry, not Area2D monitoring order or gameplay callbacks.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing,field_props
from tools.run_scene_reference import invoke
PROBE='''extends SceneTree
func _init():
    call_deferred("run")
func run():
    var f=File.new()
    assert(f.open("res://input.json",File.READ)==OK)
    var config=JSON.parse(f.get_as_text()).result
    f.close()
    var p=config.policy
    var player=Node2D.new()
    var pivot=Node2D.new()
    var hit=Node2D.new()
    var shape=CollisionShape2D.new()
    var capsule=CapsuleShape2D.new()
    capsule.radius=p[13]
    capsule.height=p[14]
    shape.shape=capsule
    shape.position=Vector2(p[10],p[11])
    shape.rotation=p[12]
    hit.position=Vector2(p[7],p[8])
    hit.rotation=p[9]
    pivot.position=Vector2(p[4],p[5])
    get_root().add_child(player)
    player.add_child(pivot)
    pivot.add_child(hit)
    hit.add_child(shape)
    var rectangle=RectangleShape2D.new()
    var rows=[]
    for q in config.queries:
        player.position=Vector2(q[0],q[1])
        pivot.rotation_degrees=q[2]
        rectangle.extents=Vector2(q[5],q[6])
        var result=capsule.collide(shape.global_transform,rectangle,Transform2D(0,Vector2(q[3],q[4])))
        rows.append([q,result])
    assert(f.open("res://native.json",File.WRITE)==OK)
    f.store_string(JSON.print({"engine":Engine.get_version_info(),"rows":rows},"  "))
    f.close()
    player.free()
    quit()
'''
def run(work,reports,engine):
 work=work.resolve();reports=reports.resolve();engine=engine.resolve()
 if work.exists()or reports.exists()or not work.is_relative_to(ROOT/'build')or not reports.is_relative_to(ROOT/'reports'):raise ValueError('Fresh owned contact reference directories required')
 swing=field_swing.source();props,_,proof=field_props.source();angles=sorted({k['value']for c in swing['clips']for k in c['keys']if k['kind']==2}|{swing['real'][6]*180/math.pi})
 # Test placement only: the actual tutorial exit and every source root shape.
 player=[1432,4719];queries=[]
 for angle in angles:
  for x in range(-45,46,3):
   for y in range(-45,46,3):queries.append([*player,angle,player[0]+x,player[1]+y,*props['props'][0]['hit_extents']])
  for row in props['props']:queries.append([*player,angle,*row['hit_center'],*row['hit_extents']])
 work.mkdir(parents=True);reports.mkdir(parents=True)
 (work/'project.godot').write_text('config_version=4\n[application]\nconfig/name="Source swing contact geometry"\n[logging]\nfile_logging/enable_logging=false\n',encoding='utf8')
 (work/'probe.gd').write_text(PROBE,encoding='utf8');(work/'input.json').write_text(json.dumps(dict(policy=swing['real'],queries=queries)),encoding='utf8')
 invoke(engine,work,['--script',str(work/'probe.gd')],reports/'godot.log');doc=json.loads((work/'native.json').read_bytes())
 # Godot's JSON printer rounds the decimal spelling. Shape transforms use
 # real_t (float32); require identical real_t bits, never an epsilon window.
 same=lambda a,b:len(a)==len(b)and all(struct.pack('<f',x)==struct.pack('<f',y)for x,y in zip(a,b))
 if doc['engine']['string']!='3.6.2-stable (official)'or len(doc['rows'])!=len(queries)or any(not same(q,row[0])or type(row[1])is not bool for q,row in zip(queries,doc['rows'])):raise ValueError('Contact reference engine/domain/coverage differs')
 (reports/'reference.tsv').write_text('\n'.join('\t'.join(map(str,[*q,int(v)]))for q,v in doc['rows'])+'\n',encoding='utf8')
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 receipt=dict(scope=__doc__,commit=field_swing.PIN,engine=doc['engine'],engine_sha256=sha(engine),sources={**swing['sources'],**proof['sources']},policy=swing['real'],angles=angles,queries=len(queries),overlaps=sum(row[1]for row in doc['rows']),numeric_admission='Exact float32 bits; native JSON decimal spelling may round without changing real_t',input_sha256=sha(work/'input.json'),probe_sha256=sha(work/'probe.gd'),producer_sha256=sha(Path(__file__).resolve()),outputs={p.name:sha(p)for p in reports.iterdir()if p.is_file()})
 (reports/'source.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Actual source swing contacts:',len(queries),'queries;',receipt['overlaps'],'overlaps; geometry only')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--work',type=Path,required=True);a.add_argument('--reports',type=Path,required=True);a.add_argument('--godot',type=Path,required=True);o=a.parse_args();run(o.work,o.reports,o.godot)
