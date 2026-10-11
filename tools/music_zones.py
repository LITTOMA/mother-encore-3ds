"""Compile actual pinned MusicChanger collision polygons, in scene order."""
import json, math, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.podunk_field import Sources, Scene, load_recipe, owner_of, Vec2, decompose_polygon_in_convex, f32, require
from tools.podunk_music import checked
OUT=Path('sound/banks/podunk.enczones')
def compile_zones(root=ROOT):
    root=Path(root);recipe=json.loads((root/'content/podunk-music.json').read_text());checked(recipe,root/'upstream/MOTHER-Encore')
    scene=Scene(Sources(root),load_recipe(root/'content/podunk-field.json'))
    payload=bytearray()
    for region in recipe['regions']:
        path=region['source_path'];parts=[]
        area=scene.by_path[path]
        require(area.props.get('collision_layer',1)==4096 and area.props.get('collision_mask',1)==0 and (scene.recipe['player']['collision_mask'] & area.props['collision_layer']) and area.props.get('monitoring',True),'Unreviewed music collision filtering')
        for shape in region['shape_paths']:
            node=scene.by_path[shape];xf=scene.global_xf[shape]
            require(not node.props.get('disabled',False) and not node.props.get('one_way_collision',False),'Disabled/one-way music shape')
            if node.type=='CollisionShape2D':
                doc=owner_of(node,'shape');ref=node.props['shape'];require(doc.sub_type(ref)=='RectangleShape2D','Unsupported music shape')
                e=doc.sub_resource(ref)['extents'];polygons=[[( -e.x,-e.y),(e.x,-e.y),(e.x,e.y),(-e.x,e.y)]]
            elif node.type=='CollisionPolygon2D':
                require(node.props.get('build_mode',0)==0,'Unsupported music polygon mode')
                polygons=decompose_polygon_in_convex([(p.x,p.y) for p in node.props['polygon']])
            else:raise ValueError('Unsupported music shape node')
            for points in polygons:
                polygon=[xf.apply(Vec2(x,y)) for x,y in points]
                require(3<=len(polygon)<=64,'Music polygon vertex count')
                points=[(f32(p.x),f32(p.y)) for p in polygon]
                require(all(math.isfinite(v) and abs(v)<=1e6 for p in points for v in p),"Invalid music polygon coordinate")
                turns=[]
                for i,a in enumerate(points):
                    b,c=points[(i+1)%len(points)],points[(i+2)%len(points)]
                    turns.append((b[0]-a[0])*(c[1]-b[1])-(b[1]-a[1])*(c[0]-b[0]))
                require(all(v>0 for v in turns) or all(v<0 for v in turns),"Music polygon is not strictly convex")
                parts.append(points)
        require(1<=len(parts)<=128,'Music polygon count')
        name=path.encode('ascii');payload+=struct.pack('<I',len(name))+name+struct.pack('<I',len(parts))
        for polygon in parts:
            payload+=struct.pack('<I',len(polygon))
            for p in polygon:payload+=struct.pack('<2f',*p)
    return struct.pack('<8s4I',b'ENCZON01',1,24+len(payload),zlib.crc32(payload),len(recipe['regions']))+payload

def stage_files(source):
    blob=compile_zones();require((Path(source)/OUT).read_bytes()==blob,'Stale music geometry');return {OUT:blob}
if __name__=='__main__':
    blob=compile_zones();out=ROOT/'romfs'/OUT;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob);print('Compiled music region geometry:',len(blob),'bytes')
