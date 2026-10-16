#!/usr/bin/env python3
"""Run the pinned original Camarea script in an isolated Godot 3.6.2 layout."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools import podunk_field as pf


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--godot',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();out=a.out.resolve()
    if out.exists() or not out.is_relative_to(pf.ROOT/'build'):
        raise ValueError('Fresh owned build output required')
    if not a.godot.is_file():raise ValueError('Explicit Godot executable required')
    sources=pf.Sources();scene=pf.Scene(sources,pf.load_recipe())
    prefix='Camareas/CamareaRect';rect=scene.by_path[prefix];area=scene.by_path[prefix+'/camarea'];collider=scene.by_path[prefix+'/camarea/CollisionShape2D']
    shape=pf.owner_of(collider,'shape').sub_resource(collider.props['shape'])
    data=dict(margins=[rect.props.get(k,0) for k in ('margin_left','margin_top','margin_right','margin_bottom')],area=list((area.props['position'].x,area.props['position'].y)),collider=list((collider.props['position'].x,collider.props['position'].y)),extents=[shape['extents'].x,shape['extents'].y])
    original=sources.data('Scripts/Main/camarea.gd');sources.data('Scripts/Main/Camera2D.gd')
    out.mkdir(parents=True)
    def write(name,value):(out/name).write_bytes(value.encode('utf8'))
    write('input.json',json.dumps(data,indent=2)+'\n')
    (out/'camarea.gd').write_bytes(original)
    write('project.godot',"config_version=4\n[application]\nconfig/name=\"Podunk camera reference\"\n[autoload]\nglobal=\"*res://global.gd\"\n[display]\nwindow/size/width=400\nwindow/size/height=240\n[rendering]\nquality/driver/driver_name=\"GLES2\"\n")
    write('global.gd',"extends Node\nvar currentCamera\nvar player\nfunc get_player():\n return player\n")
    # Adapter only exposes the original set_camarea_offset position assignment.
    write('camera.gd',"extends Camera2D\nvar camareas=0\nfunc set_camarea_offset(off):\n position=off\n")
    write('probe.gd',"extends SceneTree\nfunc _init():\n call_deferred(\"run\")\nfunc run():\n var input_file=File.new()\n input_file.open(\"res://input.json\",File.READ)\n var data=JSON.parse(input_file.get_as_text()).result\n input_file.close()\n if Engine.get_version_info().hash != \"3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8\":\n  quit(1)\n  return\n var world=Node2D.new()\n get_root().add_child(world)\n var rect=ReferenceRect.new()\n rect.margin_left=data.margins[0]\n rect.margin_top=data.margins[1]\n rect.margin_right=data.margins[2]\n rect.margin_bottom=data.margins[3]\n world.add_child(rect)\n var area=Area2D.new()\n area.set_script(load(\"res://camarea.gd\"))\n area.name=\"camarea\"\n area.position=Vector2(data.area[0],data.area[1])\n area.cam_rect_path=NodePath(\"..\")\n var collider=CollisionShape2D.new()\n collider.name=\"CollisionShape2D\"\n collider.position=Vector2(data.collider[0],data.collider[1])\n var shape=RectangleShape2D.new()\n shape.extents=Vector2(data.extents[0],data.extents[1])\n collider.shape=shape\n area.add_child(collider)\n rect.add_child(area)\n var player=Node2D.new()\n world.add_child(player)\n var camera=Camera2D.new()\n camera.set_script(load(\"res://camera.gd\"))\n camera.current=true\n player.add_child(camera)\n var g=get_root().get_node(\"global\")\n g.currentCamera=camera\n g.player=player\n yield(self,\"idle_frame\")\n var point=collider.global_position\n var outside=[]\n for p in [Vector2(314,279),Vector2(351,274)]:\n  outside.append(abs(p.x-point.x)>shape.extents.x or abs(p.y-point.y)>shape.extents.y)\n area._on_enter()\n yield(self,\"idle_frame\")\n yield(self,\"idle_frame\")\n var result={\"center\":[point.x,point.y],\"size\":[area.get_size().x,area.get_size().y],\"limits\":[camera.limit_left,camera.limit_top,camera.limit_right,camera.limit_bottom],\"reported_points_outside\":outside}\n var file=File.new()\n file.open(\"res://result.json\",File.WRITE)\n file.store_string(JSON.print(result,\"  \")+\"\\n\")\n file.close()\n print(JSON.print(result))\n quit()\n")
    result=subprocess.run([str(a.godot.resolve()),'--no-window','--path',str(out),'--script','probe.gd'],capture_output=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
    (out/'godot.log').write_bytes(result.stdout+result.stderr)
    result.check_returncode();actual=json.loads((out/'result.json').read_text())
    expected=dict(center=[2390,1308],size=[520,456],limits=[1952,1064,2472,1520],reported_points_outside=[True,True])
    if actual!=expected:raise ValueError('Original camera behavior changed: '+str(actual))
    report=dict(schema=1,upstream_commit=pf.PIN,engine='Godot 3.6.2 3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8',godot_sha256=pf.sha(a.godot.read_bytes()),inputs=data,result=actual,sources=sources.digests(),scope='Unmodified original camarea.gd and source scene properties; isolated layout, original global/camera host adapters. No whole-map original gameplay or hardware claim.')
    write('comparison.json',json.dumps(report,indent=2)+'\n')
    print('Original camera reference passed:',actual)
    return 0


if __name__=='__main__':
    raise SystemExit(main())
