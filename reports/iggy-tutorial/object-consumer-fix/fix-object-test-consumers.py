from pathlib import Path
r=Path(__file__).resolve().parents[2]
planned={};endings={}
def edit(name,old,new,count=1):
 p=r/name;raw=p.read_bytes();endings[name]='\r\n'if raw.count(b'\r\n')==raw.count(b'\n')else'\n';text=planned.get(name,raw.decode('utf8').replace('\r\n','\n'));assert text.count(old)==count,(name,old,text.count(old));planned[name]=text.replace(old,new)
edit('tests/prepare_swing_resources.py',"r['role']not in c.SWING_ROLES","r['role']not in c.SWING_ROLES|{'FieldObjects'}")
edit('tests/prepare_step_resources.py',"graph['scenes'][1]['assets'].pop('props')","graph['scenes'][1]['assets'].pop('props');graph['scenes'][1]['assets'].pop('objects')")
edit('tests/prepare_step_resources.py','model=f.Model(policy);','model=f.Model(policy,object_bindings={});')
edit('tests/resource_catalog_tests.cpp','get(blob,20)==12','get(blob,20)==13')
edit('tests/resource_catalog_tests.cpp','{20,11},{20,13}','{20,11},{20,12},{20,14}')
edit('tests/field_route_warp_tests.cpp','stride[]={1,24,84,36,4,8};','stride[]={1,24,84,36,4,8,8};')
edit('tests/field_route_warp_tests.cpp','count>=5&&count<=6','count>=5&&count<=7')
edit('tests/mayor_scene_resources_tests.cpp','links.capability()==4','links.capability()==5')
edit('tests/prop_scene_resources_tests.cpp','get(cat,20)==12&&get(links,20)==4','get(cat,20)==13&&get(links,20)==5')
edit('tests/prop_scene_resources_tests.cpp','{20,10},{20,11},{20,13}','{20,10},{20,11},{20,12},{20,14}')
edit('tests/prop_scene_resources_tests.cpp','{20,3},{20,5},{52,5}','{20,3},{20,4},{20,6},{52,5}')
edit('tests/prop_scene_resources_tests.cpp','world.capability()==4','world.capability()==5',2)
edit('tests/prop_scene_resources_tests.cpp','copy(ResourceRole::FieldProps,r.props);','copy(ResourceRole::FieldProps,r.props);copy(ResourceRole::FieldSteps,r.steps);copy(ResourceRole::FieldObjects,r.objects);\n const auto object_texture=owner.objects().texture();write(root/object_texture,read(source+"/"+object_texture));created.push_back(root/object_texture);')
edit('tests/prop_scene_resources_tests.cpp','catalog12/links4/props2','catalog13/links5/props2 with checked object companion')
edit('tests/field_prop_renderer_tests.cpp','CHECK(argc==2);','CHECK(argc==3);')
edit('tests/field_prop_renderer_tests.cpp','const auto map=resources.map();','FieldMapData legacy;CHECK(legacy.load_file(argv[2],error));const auto map=legacy.view();')
edit('tests/field_npc_motion_tests.cpp','argc==4,"Lindgren, dog and legacy Mick resources required"','argc==5,"Lindgren, dog, legacy Mick and legacy map resources required"')
edit('tests/field_npc_motion_tests.cpp','map.load_file("romfs/world/podunk/podunk.encmap",error)','map.load_file(argv[4],error)')
edit('tests/cemetery_roots_tests.cpp','int main(){','int main(int argc,char**argv){\n CHECK(argc==2);')
edit('tests/cemetery_roots_tests.cpp','map.load_file("romfs/world/podunk/podunk.encmap",error)','map.load_file(argv[1],error)')
edit('tests/field_interaction_tests.cpp','CHECK(argc==2||argc==3);','CHECK(argc==3||argc==4);')
edit('tests/field_interaction_tests.cpp','map.load_file(path("world/podunk/podunk.encmap").c_str(),error)','map.load_file(argv[2],error)')
edit('tests/field_interaction_tests.cpp','npc.load_file(argc==3?argv[2]:','npc.load_file(argc==4?argv[3]:')
edit('tests/field_interaction_tests.cpp','if(argc==3){','if(argc==4){')
p=r/'CMakeLists.txt';text=p.read_bytes().decode('utf8').replace('\r\n','\n');lines=text.splitlines();legacy='${CMAKE_BINARY_DIR}/legacy-field-map/legacy.encmap'
direct={'field_static_actors','field_route_warp','field_story'}
for i,line in enumerate(lines):
 if 'add_test(NAME 'not in line:continue
 name=line.split('add_test(NAME ',1)[1].split()[0]
 if name in direct:
  assert '${CMAKE_SOURCE_DIR}/romfs/world/podunk/podunk.encmap'in line,name
  lines[i]=line.replace('${CMAKE_SOURCE_DIR}/romfs/world/podunk/podunk.encmap',legacy)
 elif name in {'field_prop_renderer','field_npc_motion','cemetery_roots'}:lines[i]=line[:-1]+' '+legacy+')'
 elif name in {'field_interaction','field_interaction_policy'}:lines[i]=line.replace('${CMAKE_SOURCE_DIR}/romfs','${CMAKE_SOURCE_DIR}/romfs '+legacy,1)
names=sorted(direct|{'field_interaction','field_interaction_policy','field_prop_renderer','field_npc_motion','cemetery_roots'})
lines+=['','if(BUILD_TESTING AND Python3_Interpreter_FOUND)','    # Original pre-object scene behavior is still exercised byte-for-byte;','    # new object scene/load/art targets separately require the formal owner.','    set_tests_properties('+' '.join(names)+' PROPERTIES FIXTURES_REQUIRED legacy_field_map_fixture)','endif()']
raw=p.read_bytes();ending='\r\n'if raw.count(b'\r\n')==raw.count(b'\n')else'\n';p.write_bytes(('\n'.join(lines)+'\n').replace('\n',ending).encode('utf8'))
for name,text in planned.items():(r/name).write_bytes(text.replace('\n',endings[name]).encode('utf8'))
print('Updated legacy test consumers and complete typed companion fixtures; runtime admission unchanged.')
