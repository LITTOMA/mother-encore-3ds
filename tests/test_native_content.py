import copy
import hashlib
import json
import os
import shutil
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from tools.native_content import (
    ROOT, SCHEMAS, ContentError, compile_ir, parse_pack, file_crc,
    verify_provenance, encode_record, decode_record, NONE, NO_ACTOR,
)


class NativeContentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir=json.loads((ROOT/'content/native-opening.json').read_text())
        cls.blob,cls.manifest=compile_ir(cls.ir)

    def changed_ir(self,action):
        data=copy.deepcopy(self.ir);action(data);return data

    def rejects_ir(self,action):
        with self.assertRaises((ContentError,ValueError,OverflowError,struct.error)):
            compile_ir(self.changed_ir(action))

    def mutation(self,offset,fmt,value):
        data=bytearray(self.blob);struct.pack_into('<'+fmt,data,offset,value)
        struct.pack_into('<I',data,52,file_crc(data));return bytes(data)

    def offset(self,name,index=0,field=0):
        s=self.manifest['sections'][name];return s['offset']+index*s['stride']+field

    def test_roundtrip_determinism_all_schema_widths(self):
        self.assertEqual(compile_ir(self.ir)[0],self.blob)
        parsed=parse_pack(self.blob)
        self.assertEqual(len(parsed['sections']),26)
        self.assertEqual(len(parsed['strings']),len(self.ir['strings']))
        for name,rows in self.ir['sections'].items():
            self.assertEqual(len(rows),len(parsed['sections'][name]))
            for a,b in zip(rows,parsed['sections'][name]):
                self.assertEqual(encode_record(name,a),encode_record(name,b))
        self.assertFalse(self.manifest['cpp_compiler_invoked'])
        self.assertEqual(len(self.blob),self.manifest['bytes'])

    def test_every_truncation_and_trailing_byte_rejected(self):
        for size in range(len(self.blob)):
            with self.assertRaises(ContentError):parse_pack(self.blob[:size])
        with self.assertRaises(ContentError):parse_pack(self.blob+b'\0')

    def test_header_versions_capabilities_size_reserved_rejected(self):
        changes=[(0,'I',0),(8,'H',2),(10,'H',1),(12,'I',64),(16,'I',0xffffffff),
                 (20,'I',0x04030201),(24,'I',2),(28,'I',0),(32,'I',1),(36,'I',1),(36,'I',8),
                 (40,'I',0),(44,'I',0xfffffff0),(48,'H',25),(50,'H',28),
                 (108,'I',0),(112,'I',0),(116,'I',1),(120,'I',1)]
        for offset,fmt,value in changes:
            with self.subTest(offset=offset),self.assertRaises(ContentError):parse_pack(self.mutation(offset,fmt,value))

    def test_checksum_and_unknown_directory_values(self):
        corrupt=bytearray(self.blob);corrupt[-1]^=1
        with self.assertRaises(ContentError):parse_pack(corrupt)
        changes=[(128,'H',99),(130,'H',2),(132,'I',0xfffffffc),(136,'I',0xffffffff),(140,'I',0),
                 (144,'I',1),(148,'I',0),(128+24,'H',1)]
        for o,f,v in changes:
            with self.subTest(offset=o),self.assertRaises(ContentError):parse_pack(self.mutation(o,f,v))
        swapped=bytearray(self.blob);swapped[128:152],swapped[152:176]=swapped[152:176],swapped[128:152]
        struct.pack_into('<I',swapped,52,file_crc(swapped))
        with self.assertRaises(ContentError):parse_pack(swapped)

    def test_overlap_unaligned_section_and_nonzero_padding(self):
        first=self.manifest['sections']['StringRef']['offset']
        for o in (first,first+1):
            with self.assertRaises(ContentError):parse_pack(self.mutation(128+24+4,'I',o))
        # Authored string lengths may now align naturally. Create a valid
        # synthetic variant with deliberate StringBytes alignment padding.
        variant=copy.deepcopy(self.ir);variant['strings'][-1]+='x'
        padded,manifest=compile_ir(variant)
        spans=sorted((v['offset'],v['offset']+v['bytes'])for v in manifest['sections'].values()if v['count'])
        for (_,stop),(start,_)in zip(spans,spans[1:]):
            if stop<start:
                changed=bytearray(padded);changed[stop]=1;struct.pack_into('<I',changed,52,file_crc(changed))
                with self.assertRaises(ContentError):parse_pack(changed)
                break
        else:self.fail('Expected at least one alignment pad in baseline')

    def test_string_bounds_nul_and_utf8_rejected(self):
        pos=self.offset('StringRef')
        for field,value in ((0,0xffffffff),(4,0xffffffff)):
            with self.assertRaises(ContentError):parse_pack(self.mutation(pos+field,'I',value))
        pool=self.manifest['sections']['StringBytes']['offset']
        with self.assertRaises(ContentError):parse_pack(self.mutation(pool,'B',65))
        # The first nonempty string starts immediately after empty string's NUL.
        with self.assertRaises(ContentError):parse_pack(self.mutation(pool+1,'B',0xff))
        self.rejects_ir(lambda d:d['strings'].append(d['strings'][1]))
        self.rejects_ir(lambda d:d['strings'].__setitem__(1,'embedded\0nul'))
        self.rejects_ir(lambda d:d['strings'].__setitem__(1,'x'*4097))

    def test_nonfinite_and_signed_widths_rejected(self):
        for name,field in [('Vec2',0),('Clip',4),('Command',24)]:
            if name=='Command':fmt='Q';bits=0x7ff8000000000000
            else:fmt='I';bits=0x7fc00000
            with self.subTest(name=name),self.assertRaises(ContentError):parse_pack(self.mutation(self.offset(name,field=field),fmt,bits))
        self.rejects_ir(lambda d:d['sections']['Scene'][0]['spawn'].__setitem__(0,float('nan')))
        self.rejects_ir(lambda d:d['sections']['Battle'][0].update(advantage=1<<40))
        self.rejects_ir(lambda d:d['sections']['Rule'][0].update(value=True))

    def test_reserved_record_bytes_rejected(self):
        positions=[('Resource',20),('BodyRule',10),('Overlay',30),('Key',6),('ActorProfile',30),
                   ('ActorInstance',36),('Flag',10),('InitialFlag',5),('Condition',6),
                   ('Trigger',28),('Command',44),('MovementPath',14),('MovementEntry',2),('Rule',4)]
        for name,field in positions:
            with self.subTest(name=name),self.assertRaises(ContentError):parse_pack(self.mutation(self.offset(name,field=field),'B',1))

    def test_reference_limits_duplicate_ids_and_range_overflow(self):
        edits=[lambda d:d['sections']['Overlay'][0].update(resource_index=NONE),
               lambda d:d['sections']['Polygon'][0].update(first_vertex=NONE),
               lambda d:d['sections']['Polygon'][0].update(vertex_count=65535),
               lambda d:d['sections']['Clip'][0].update(first_key=NONE),
               lambda d:d['sections']['Clip'][0].update(key_count=17),
               lambda d:d['sections']['BodyRule'][0].update(body_id=0),
               lambda d:d['sections']['ActorProfile'][0].update(execution_kind=3),
               lambda d:d['sections']['ActorInstance'][0].update(binding_kind=3),
               lambda d:d['sections']['Scene'][0].update(rule_profile_id=2),
               lambda d:d['sections']['Flag'][1].update(stable_id=d['sections']['Flag'][0]['stable_id']),
               lambda d:d['sections']['Program'][0].update(command_count=NONE),
               lambda d:d['sections']['AnimationBinding'][0].update(direction=8)]
        for edit in edits:
            with self.subTest(edit=edit):self.rejects_ir(edit)

    def test_geometry_bounds_degeneracy_and_convexity(self):
        self.rejects_ir(lambda d:d['sections']['Polygon'][0]['minimum'].__setitem__(0,999))
        def duplicate(d):
            i=d['sections']['Polygon'][0]['first_vertex'];d['sections']['Vec2'][i+1]=dict(d['sections']['Vec2'][i])
        self.rejects_ir(duplicate)
        self.rejects_ir(lambda d:d['sections']['CameraArea'][0]['extents'].__setitem__(0,0))

    def test_texture_grid_resource_paths_and_rects(self):
        self.rejects_ir(lambda d:d['sections']['Resource'][0].update(columns=3))
        self.rejects_ir(lambda d:d['sections']['Overlay'][0].update(w=65535))
        self.rejects_ir(lambda d:d['sections']['Resource'][0].update(kind=3))
        self.rejects_ir(lambda d:d['sections']['Resource'][0].update(sha256='0'*64))
        self.rejects_ir(lambda d:d.update(upstream_commit='0'*40))
        self.rejects_ir(lambda d:d['strings'].__setitem__(d['sections']['Resource'][0]['path_string'],'../secret.t3x'))
        self.rejects_ir(lambda d:d['strings'].__setitem__(d['sections']['Resource'][0]['path_string'],'/root/secret.t3x'))

    def test_rule_table_and_xp_contract(self):
        self.rejects_ir(lambda d:d['sections']['Rule'].pop())
        self.rejects_ir(lambda d:d['sections']['Rule'][0].update(key=99))
        self.rejects_ir(lambda d:d['sections']['Rule'][0].update(scalar_type=2))
        self.rejects_ir(lambda d:d['sections']['Rule'][5].update(value=.75))
        self.rejects_ir(lambda d:d['sections']['Experience'][0].update(required_total_exp=1))
        self.rejects_ir(lambda d:d['sections']['Experience'][2].update(required_total_exp=1))

    def test_periodic_room_shake_source_contract(self):
        self.assertEqual((self.ir['rules'],self.ir['capabilities']),(7,7))
        binding=self.ir['sections']['Binding'][1]
        self.assertEqual((binding['kind'],binding['value'],binding['duration']),(3,4,5))
        resource=self.ir['sections']['Resource'][binding['target_index']]
        self.assertEqual(resource['kind'],2)
        self.assertEqual(self.ir['strings'][resource['path_string']],'res://Audio/Sound effects/M3/PK_Thunder_a_b_y_O_hit.wav')
        self.assertFalse(any(b['kind']==2 for b in self.ir['sections']['Binding']))
        rules={r['key']:r for r in self.ir['sections']['Rule']}
        self.assertEqual([rules[k]['value']for k in range(21,26)],[4,.5,.8,1,0])
        self.assertTrue(all(rules[k]['scalar_type']==2 for k in range(21,26)))

    def test_periodic_room_shake_negative_bindings_and_rules(self):
        for field,value in [('kind',4),('target_index',NONE),('target_index',0),('value',0),('value',-1),('value',1000001),('duration',0),('duration',3601),('flags',1),('auxiliary_index',0)]:
            with self.subTest(field=field,value=value):self.rejects_ir(lambda d:d['sections']['Binding'][1].update({field:value}))
        for key,value in [(21,0),(21,3601),(22,-.1),(22,4),(23,0),(23,3601),(24,2),(24,0),(25,-2)]:
            with self.subTest(key=key,value=value):self.rejects_ir(lambda d:next(r for r in d['sections']['Rule']if r['key']==key).update(value=value))
        self.rejects_ir(lambda d:next(r for r in d['sections']['Rule']if r['key']==25).update(scalar_type=1))
        # Valid scalar zeros and signed direction data are retained exactly.
        data=self.changed_ir(lambda d:next(r for r in d['sections']['Rule']if r['key']==24).update(value=-1))
        self.assertEqual(parse_pack(compile_ir(data)[0])['sections']['Rule'][23]['value'],-1)

    def test_animation_modes_visibility_order_and_frame_range(self):
        self.rejects_ir(lambda d:d['sections']['Clip'][0].update(flags=16))
        self.rejects_ir(lambda d:d['sections']['Clip'][0].update(flags=3))
        self.rejects_ir(lambda d:d['sections']['Clip'][0].update(visibility=3))
        self.rejects_ir(lambda d:d['sections']['Key'][0].update(frame=65535))
        self.rejects_ir(lambda d:d['sections']['Key'][0].update(time=.1))
        # Source oddity retained rather than normalized away.
        c=self.ir['sections']['Clip'][2]
        keys=self.ir['sections']['Key'][c['first_key']:c['first_key']+c['key_count']]
        self.assertTrue(any(k['time']>=c['length']for k in keys))
        self.assertEqual(self.ir['sections']['Clip'][32]['flags'],4)
        self.assertEqual(self.ir['sections']['Clip'][33]['flags'],2)
        self.assertEqual(self.ir['sections']['Clip'][34]['flags'],8)

    def test_commands_unknown_target_extra_fields_and_terminal(self):
        self.rejects_ir(lambda d:d['sections']['Command'][0].update(opcode=27))
        self.rejects_ir(lambda d:d['sections']['Command'][0].update(value=1))
        self.rejects_ir(lambda d:d['sections']['Command'][0].update(actor_index=0))
        self.rejects_ir(lambda d:d['sections']['Command'][0].update(auxiliary_index=0))
        self.rejects_ir(lambda d:d['sections']['Command'][0].update(phrase=999))
        self.rejects_ir(lambda d:d['sections']['Command'][-1].update(opcode=22,actor_index=NO_ACTOR,target_index=NONE,duration=0))
        i=next(i for i,c in enumerate(self.ir['sections']['Command'])if c['opcode']==3)
        self.rejects_ir(lambda d:d['sections']['Command'][i].update(duration=0))
        self.rejects_ir(lambda d:d['sections']['Command'][i].update(unrecognized_data=0))

    def test_flags_conditions_and_actor_profile_bounds(self):
        self.rejects_ir(lambda d:d['sections']['Condition'][0].update(domain=1))
        self.rejects_ir(lambda d:d['sections']['InitialFlag'][0].update(value=2))
        self.rejects_ir(lambda d:d['sections']['Flag'][0].update(default_value=2))
        self.rejects_ir(lambda d:d['sections']['ActorProfile'][0].update(initial_frame=65535))
        self.rejects_ir(lambda d:d['sections']['ActorInstance'][0].update(direction=[0,0]))
        self.rejects_ir(lambda d:d['sections']['ActorInstance'][0].update(flags=2))

    def test_revision6_branch_targets_and_operand_domains(self):
        dad=next(p for p in self.ir['sections']['Program']if self.ir['strings'][p['source_path_string']]=='Reusable/dad_normal')
        first=dad['first_command'];count=dad['command_count']
        self.assertEqual(self.ir['strings'][dad['source_path_string']],'Reusable/dad_normal')
        self.assertEqual(count,48)
        def command(pc,**change):return lambda d:d['sections']['Command'][first+pc].update(change)
        edits=[command(3,target_index=3),command(3,target_index=count),command(3,target_index=13),
               command(24,auxiliary_index=NONE),command(24,auxiliary_index=count),
               command(24,target_index=NONE),command(24,value=2),command(4,auxiliary_index=4),
               command(4,target_index=0),command(4,target_index=NONE),command(30,target_index=32),
               command(30,auxiliary_index=0),command(33,opcode=5),command(34,opcode=5),
               command(33,target_index=0),command(34,value=1),command(33,flags=1),
               command(47,duration=0),command(30,opcode=42),command(2,opcode=3,duration=.25)]
        for edit in edits:
            with self.subTest(edit=edit):self.rejects_ir(edit)
        leader=self.ir['sections']['Command'][first+4]['target_index']
        for value in ['Lloyd','lloyd/actor',' lloyd','lloyd-actor','éclair','9lloyd','a'*65]:
            with self.subTest(leader=value):self.rejects_ir(lambda d:d['strings'].__setitem__(leader,value))
        # Boolean tests use numeric 0/1; they never accept arbitrary scalar predicates.
        changed=self.changed_ir(command(24,value=0))
        parse_pack(compile_ir(changed)[0])

    def test_revision6_terminal_branches_and_legacy_compatibility(self):
        first=next(p for p in self.ir['sections']['Program']if self.ir['strings'][p['source_path_string']]=='Reusable/dad_normal')['first_command']
        data=self.changed_ir(lambda d:d['sections']['Command'][first+3].update(opcode=23,target_index=NONE,duration=.5))
        parse_pack(compile_ir(data)[0])
        data['sections']['Command'][first+3]['duration']=0
        with self.assertRaises(ContentError):compile_ir(data)
        for rules,programs,bindings in [(6,12,9),(5,11,9),(4,5,7)]:
            old=copy.deepcopy(self.ir);old.update(rules=rules,capabilities=rules)
            old['sections']['Program']=old['sections']['Program'][:programs]
            end=old['sections']['Program'][-1]['first_command']+old['sections']['Program'][-1]['command_count']
            old['sections']['Command']=old['sections']['Command'][:end]
            old['sections']['Binding']=old['sections']['Binding'][:bindings]
            old['sections']['Rule']=old['sections']['Rule'][:25]
            # Remove only appended Pillow movement records for the historical rules fixture.
            first_pillow=next(p for p in self.ir['sections']['Program']if self.ir['strings'][p['source_path_string']]=='Podunk/cutscenes/pillow_attack')['first_command']
            path_end=min(c['target_index']for c in self.ir['sections']['Command'][first_pillow:]if c['opcode']==29)
            old['sections']['MovementPath']=old['sections']['MovementPath'][:path_end]
            last=old['sections']['MovementPath'][-1]
            old['sections']['MovementEntry']=old['sections']['MovementEntry'][:last['first_entry']+last['entry_count']]
            with self.subTest(rules=rules):self.assertEqual(parse_pack(compile_ir(old)[0])['rules'],rules)
            # New movement bits remain invalid in each accepted historical
            # version, even after removing every appended Pillow-only record.
            for flags in (4,8):
                invalid=copy.deepcopy(old);invalid['sections']['MovementPath'][0]['flags']=flags
                with self.subTest(rules=rules,flags=flags),self.assertRaises(ContentError):compile_ir(invalid)
            self.rejects_ir(lambda d:d.update(rules=rules,capabilities=rules))

    def test_revision7_actor_loop_repeat_and_stop_operands(self):
        self.assertEqual((self.ir['rules'], self.ir['capabilities']), (7, 7))
        paths=self.ir['sections']['MovementPath']
        loop=next(i for i,p in enumerate(paths)if p['flags']&4)
        self.assertEqual(paths[loop]['entry_count'],14)
        for change in [dict(flags=16),dict(entry_count=17),dict(speed=0)]:
            self.rejects_ir(lambda d: d['sections']['MovementPath'][loop].update(change))
        stop=next(i for i,c in enumerate(self.ir['sections']['Command'])if c['opcode']==42)
        for change in [dict(opcode=43),dict(actor_index=NO_ACTOR),dict(target_index=0),dict(flags=1),dict(duration=.1),dict(value=1)]:
            self.rejects_ir(lambda d: d['sections']['Command'][stop].update(change))
        jump=next(i for i,c in enumerate(self.ir['sections']['Command'])if c['opcode']==12 and c['flags']==1)
        self.rejects_ir(lambda d:d['sections']['Command'][jump].update(flags=16))
        shake=next(i for i,c in enumerate(self.ir['sections']['Command'])if c['opcode']==11 and c['duration']==-1)
        for value in [-2,0,11]:
            self.rejects_ir(lambda d:d['sections']['Command'][shake].update(duration=value))
        for key in (26,27):
            self.rejects_ir(lambda d:next(r for r in d['sections']['Rule']if r['key']==key).update(value=0))
        self.rejects_ir(lambda d:d.update(rules=6,capabilities=6))

    def test_provenance_wrong_hash_path_and_source_change(self):
        verify_provenance(self.ir)
        self.rejects_ir(lambda d:d['provenance']['sources'].update({'../escape':'0'*64}))
        with tempfile.TemporaryDirectory()as tmp:
            root=Path(tmp);f=root/'source.txt';f.write_text('original')
            data=copy.deepcopy(self.ir);data['provenance']={'sources':{'source.txt':hashlib.sha256(f.read_bytes()).hexdigest()}}
            # rules7 always requires the complete reviewed recipe. This tiny
            # file-only fixture exercises the accepted historical hash gate.
            with self.assertRaisesRegex(ContentError,'Missing Pillow source binding provenance'):
                verify_provenance(data,root)
            data['rules']=data['capabilities']=6
            verify_provenance(data,root);f.write_text('changed')
            with self.assertRaises(ContentError):verify_provenance(data,root)

    def test_data_only_variants_and_no_cpp_toolchain(self):
        data=copy.deepcopy(self.ir)
        data['sections']['Scene'][0]['spawn'][0]+=1
        data['sections']['ActorProfile'][0]['sprite_offset'][0]+=2
        data['sections']['Key'][0]['frame']+=1
        wait=next(c for c in data['sections']['Command']if c['opcode']==3);wait['duration']+=.125
        data['sections']['Overlay'][0]['sort_y']+=3
        changed,_=compile_ir(data);self.assertNotEqual(changed,self.blob)
        parsed=parse_pack(changed)
        self.assertEqual(parsed['sections']['Scene'][0]['spawn'],data['sections']['Scene'][0]['spawn'])
        self.assertEqual(parsed['sections']['ActorProfile'][0]['sprite_offset'],data['sections']['ActorProfile'][0]['sprite_offset'])
        with tempfile.TemporaryDirectory()as tmp:
            p=Path(tmp);source=p/'variant.json';source.write_text(json.dumps(data))
            git=shutil.which('git');self.assertIsNotNone(git)
            if os.name=='nt':
                tools_path=str(Path(git).parent)
            else:
                tools_path=str(p/'no-toolchain');Path(tools_path).mkdir();(Path(tools_path)/'git').symlink_to(Path(git).resolve())
            for compiler in ('cc','c++','gcc','g++','clang','clang++','cl','arm-none-eabi-g++'):
                self.assertIsNone(shutil.which(compiler,path=tools_path),compiler)
            env=dict(os.environ,PATH=tools_path,CXX='/bin/false',CC='/bin/false',DEVKITPRO='',DEVKITARM='')
            command=[sys.executable,str(ROOT/'tools/native_content.py'),'compile','--input',str(source),'--out',str(p/'variant.encroom'),'--manifest',str(p/'manifest.json')]
            # Schema-valid changes above prove compile_ir is data-driven, but
            # the production CLI must reject content absent from the source IR.
            run=subprocess.run(command,env=env,cwd=ROOT,text=True,capture_output=True)
            self.assertNotEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('Stale programme recipe/Room content',run.stderr)
            self.assertFalse((p/'variant.encroom').exists());self.assertFalse((p/'manifest.json').exists())
            source.write_text(json.dumps(self.ir))
            run=subprocess.run(command,env=env,cwd=ROOT,text=True,capture_output=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertEqual((p/'variant.encroom').read_bytes(),self.blob)
            self.assertFalse(json.loads((p/'manifest.json').read_text())['cpp_compiler_invoked'])

    def test_production_exporters_no_longer_emit_content_headers(self):
        for name in ('world_geometry','character_animation','lamp_dialogue','house_layers'):
            text=(ROOT/'tools'/f'{name}.py').read_text()
            self.assertNotIn('include/encore/generated',text)
            self.assertNotIn('def header(',text)
            self.assertNotIn('header_sha256',text.replace("'header_sha256' in r",''))

if __name__=='__main__':unittest.main()
