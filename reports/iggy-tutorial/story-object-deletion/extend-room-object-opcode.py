from pathlib import Path
r=Path(__file__).resolve().parents[2]
def replace(name,old,new):
 p=r/name;s=p.read_bytes().decode('utf8');new=new.replace('\r\n','\r\n'if'\r\n'in s else'\n')
 if old not in s and s.count(new)==1:return
 assert s.count(old)==1,(name,old,s.count(old));p.write_bytes(s.replace(old,new).encode('utf8'))
replace('runtime/world.cpp','content_=content;story_key_items_=nullptr;','content_=content;story_objects_=nullptr;story_key_items_=nullptr;')
replace('include/encore/dialogue.hpp','EraseActor, GrantKeyItem','EraseActor, GrantKeyItem, QueueFreeStoryObject')
replace('runtime/dialogue.cpp','"EraseActor","GrantKeyItem"};','"EraseActor","GrantKeyItem","QueueFreeStoryObject"};')
replace('tools/native_content.py',"'EraseActor','GrantKeyItem']","'EraseActor','GrantKeyItem','QueueFreeStoryObject']")
replace('runtime/room_data.cpp','u32(bytes+36)==12||u32(bytes+36)==13','u32(bytes+36)==12||u32(bytes+36)==13||u32(bytes+36)==14')
replace('runtime/room_data.cpp','u32(bytes+36)>=13?48:','u32(bytes+36)>=14?49:u32(bytes+36)>=13?48:')
replace('runtime/room_data.cpp','    if(u32(bytes+36)==13){','    if(u32(bytes+36)==14){bool used=false;for(uint32_t i=0;i<v.command_count();++i)used|=v.command(i).opcode==49;if(!used)return fail("Room: capability 14 requires a typed object deletion");}\r\n    if(u32(bytes+36)==13){')
replace('runtime/room_data.cpp','            case 46:if(!ref(c.target_index', '            case 49:{if(!ref(c.target_index,v.string_count()))return fail("Room: missing story object path");const auto path=v.string(c.target_index);if(path.size()>512||path.find("..")!=path.npos||!relative_path(path,false))return fail("Room: invalid story object path");for(unsigned char ch:path)if(ch>=127)return fail("Room: non-ASCII story object path");break;}\r\n            case 46:if(!ref(c.target_index')
replace('tools/native_content.py','len(OPCODES)if capabilities>=13 else 48','len(OPCODES)if capabilities>=14 else 49 if capabilities>=13 else 48')
replace('tools/native_content.py',"elif op in(45,48):","elif op==49:\r\n            path=string(c['target_index'],'Command.object_path',False)\r\n            check(len(path)<=512 and all(32<=ord(ch)<127 and ch not in ':\\\\'for ch in path)and '..'not in path and all(part not in('','.','..')for part in path.split('/')),'Invalid story object path')\r\n        elif op in(45,48):")
replace('tools/native_content.py',"    check(capabilities!=13 or any(c['opcode']==48", "    check(capabilities!=14 or any(c['opcode']==49 for c in sections['Command']),'Capability 14 requires a typed object deletion')\r\n    check(capabilities!=13 or any(c['opcode']==48")
replace('tools/native_content.py',"in(8,9,10,11,12,13)","in(8,9,10,11,12,13,14)")
print('Room14 typed deletion codec and independent World host extension applied; no shipped resource change')
