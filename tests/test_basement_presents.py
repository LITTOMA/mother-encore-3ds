"""Source contracts and negative cases for the two basement interactions."""
import copy,hashlib,json,unittest
from pathlib import Path
from tools import diary_story,field_equipment,house_presents,native_session,native_house,native_content
from tools.reuse_ui_textures import candidate
ROOT=Path(__file__).resolve().parents[1]

class BasementSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.diary=diary_story.load(ROOT)
        cls.presents=house_presents.load(ROOT)
        cls.session=native_session.read(native_session.IR)
        cls.room=json.loads((ROOT/'content/native-opening.json').read_text(encoding='utf-8'))

    def test_original_graph_learning_and_identity(self):
        source=self.diary['document'];self.assertEqual(source['9']['learnskills'],{'ninten':'lifeUpA'})
        self.assertEqual([source[str(n)]['wait'] for n in (2,3,4,5,6,7,9)],[1,7,2,2,2,3,.6])
        self.assertEqual(self.session['skill_acquisitions'],[{'skill_id':'lifeUpA','flag_id':'got_diary'}])
        self.assertEqual(self.session['skill_order'],['strike','splitShot','telepathy','lifeUpA'])
        diary=next(o for o in self.presents['objects'] if o['item']=='GGFDiary')
        commands=self.presents['programs'][diary['program']]['commands']
        self.assertEqual([c['op'] for c in commands[:6]],['BranchFlag','PlayClip','GrantItem','SetFlag','StartStory','End'])
        self.assertEqual(commands[4]['a'],diary_story.PATH)

    def test_unknown_diary_keys_edges_actors_and_skills_fail_closed(self):
        mutations=[lambda d:d['9'].update(ignored=True),lambda d:d['0']['actors'].update(ana='ana'),
            lambda d:d['1'].update(goto='10'),lambda d:d['2']['actorsmove']['ninten'].update(speed=99),
            lambda d:d['3']['actorsanim']['ninten'].update(type=0),lambda d:d['9']['learnskills'].update(ninten='unknown'),
            lambda d:d['4'].update(wait=float('nan')),lambda d:d['4'].update(wait=-1),lambda d:d['0'].pop('text')]
        for mutation in mutations:
            doc=copy.deepcopy(self.diary['document']);mutation(doc)
            with self.subTest(mutation=mutation),self.assertRaises(ValueError):diary_story.validate_document(doc)

    def test_source_frame_mapping_and_original_menu_prompt(self):
        sleep=self.diary['clips']['NintenDiarySleep'];wake=self.diary['clips']['NintenDiaryWakeUp']
        self.assertEqual((sleep['keys'][0][1],sleep['keys'][-1][1],wake['keys'][0][1],wake['keys'][-1][1]),(0,34,35,52))
        self.assertGreater(sleep['length'],0);self.assertLess(sleep['length'],7);self.assertLess(wake['length'],3)
        text=next(t for t in self.diary['texts'] if t['label']=='10')
        self.assertIn('START',text['raw']);self.assertNotIn('[ui_select]',text['raw'])
        self.assertEqual(self.diary['menu_button'],dict(action='ui_select',label='START',mask=8))

    def test_bat_capacity_warning_queues_original_animation_completion(self):
        bat=next(o for o in self.presents['objects'] if o['item']=='BatPlastic')
        commands=self.presents['programs'][bat['program']]['commands']
        branch=next(c for c in commands if c['op']=='BranchSpace');full=commands[branch['a']:]
        self.assertEqual([c['op'] for c in full[:5]],['QueueClip','PreviewItem','ShowText','AwaitText','End'])
        self.assertEqual(full[0]['a'],'Wrapped')
        self.assertFalse(self.presents['templates'][self.presents['programs'][bat['program']]['template']]['key_item'])

    def test_new_room_operands_fail_closed_and_old_prefix_is_unchanged(self):
        op=native_content.OPCODES;commands=self.room['sections']['Command']
        for kind in ('SetActorProfile','LearnSkill','ScreenOverlay'):
            index=next(i for i,c in enumerate(commands) if c['opcode']==op.index(kind))
            for field,value in [('target_index',0xffffffff),('flags',1),('auxiliary_index',0)]:
                changed=copy.deepcopy(self.room);changed['sections']['Command'][index][field]=value
                with self.subTest(kind=kind,field=field),self.assertRaises(ValueError):native_content.compile_ir(changed)
        legacy=native_content.parse_pack((ROOT/'tests/fixtures/pre-basement-opening.encroom').read_bytes())
        current=native_content.parse_pack(native_content.compile_ir(self.room)[0])
        self.assertEqual(current['strings'][:len(legacy['strings'])],legacy['strings'])
        for name,rows in legacy['sections'].items():self.assertEqual(current['sections'][name][:len(rows)],rows,name)

    def test_healing_skill_payloads_and_versions_fail_closed(self):
        ir=field_equipment.load(ROOT);tables=field_equipment.lower(ir,field_equipment.verify_receipt(ir))
        self.assertEqual([r[5:9]+[r[14]] for r in tables['Skills']],[[1,0,0,0,0],[2,5,65,5,5]])
        for column,value in [(0,0),(5,3),(6,65536),(7,0),(9,0),(10,0),(14,0)]:
            changed=copy.deepcopy(tables);changed['Skills'][1][column]=value
            with self.subTest(column=column),self.assertRaises(ValueError):field_equipment.encode(changed)

    def test_current_house_capacity_keeps_behavioral_rule_seven(self):
        ir=json.loads((ROOT/'content/native-house.json').read_text(encoding='utf-8'));tables=native_house.lower(ir)
        self.assertEqual(ir['schema'],8);self.assertGreater(len(tables['Tokens']),256)
        native_house.parse_pack(native_house.encode(tables,version=8))
        with self.assertRaises(ValueError):native_house.parse_pack(native_house.encode(tables,version=7))

    def test_frozen_legacy_resource_fingerprints(self):
        provenance=json.loads((ROOT/'tests/fixtures/basement-legacy-provenance.json').read_text())
        for path,record in provenance['files'].items():
            raw=(ROOT/path).read_bytes();self.assertEqual(len(raw),record['bytes']);self.assertEqual(hashlib.sha256(raw).hexdigest(),record['sha256'])

    def test_texture_reuse_rejects_recipe_converter_and_receipt_changes(self):
        old={'commit':'pin','resources':[{'source':'same.png'}],'sources':{'same.png':'original'}}
        raw=json.dumps(old).encode();producer=b'def compile_assets():\n return 1\n'
        sha=lambda x:hashlib.sha256(x).hexdigest()
        receipt=dict(schema=1,commit='pin',ir_sha256=sha(raw),producer_sha256=sha(producer),tex3ds_sha256='a'*64,outputs={})
        self.assertEqual(candidate(raw,producer,receipt,old,producer,raw),receipt)
        for changed,code,proof in [(dict(old,resources=[]),producer,receipt),(dict(old,sources={}),producer,receipt),
            (old,producer.replace(b'1',b'2'),receipt),(old,producer,dict(receipt,producer_sha256='b'*64))]:
            with self.assertRaises(ValueError):candidate(raw,producer,proof,changed,code,raw)

    def test_naming_dependency_extension_keeps_defaults_and_rejects_mutation(self):
        from tools import new_game_assets as n
        current=copy.deepcopy(self.session)
        current['schema']=8;current.pop('field_encounters')
        current['acquisitions']=[x for x in current['acquisitions']if x['item_id']!='FleaBag']
        current['storage_policies']=[x for x in current['storage_policies']if x['item_id']!='FleaBag']
        current['mutable_flags']=[x for x in current['mutable_flags']if x not in ('beat_dog_bridge','pippis_mom_call')]
        previous=copy.deepcopy(current)
        previous['schema']=7
        previous.pop('skill_acquisitions');previous.pop('skill_order')
        previous['acquisitions']=previous['acquisitions'][:-1]
        previous['storage_policies']=previous['storage_policies'][:-1]
        previous['key_acquisitions']=[k for k in previous['key_acquisitions'] if k['item_id']!='GGFDiary']
        binding,_,_=n.load_presentation(ROOT);recipe={'dependencies':{binding['session_ir']:'a'*64}}
        self.assertEqual(n.rebound_session_dependency(recipe,previous,current,'b'*64)['dependencies'][binding['session_ir']],'b'*64)
        for mutate in (lambda r:r['defaults'].update(position_x=999),lambda r:r['levels'][0]['stats'].__setitem__(0,999),
                       lambda r:r['skill_order'].reverse(),lambda r:r['skill_acquisitions'][0].update(skill_id='unknown'),
                       lambda r:r['acquisitions'][0].update(doses=99),lambda r:r.update(unreviewed=True)):
            bad=copy.deepcopy(current);mutate(bad)
            with self.subTest(mutate=mutate),self.assertRaises(ValueError):n.rebound_session_dependency(recipe,previous,bad,'b'*64)

if __name__=='__main__':unittest.main()
