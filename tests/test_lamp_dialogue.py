import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from tools.lamp_dialogue import ROOT,REVIEW,RECEIPT,SOURCE_FILES,compile_receipt,compile_phrase,verify_sources
from tools.reference_lamp_dialogue import command_block,fixture

class LampDialogueTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.review=json.loads(REVIEW.read_text())
        cls.data=json.loads(RECEIPT.read_text())
        cls.trace=json.loads((ROOT/'reports/m5-lamp-dialogue-reference/scheduler.json').read_text())

    def test_native_parser_receipt_matches_external_commands(self):
        program=compile_receipt(self.data,self.review)
        self.assertEqual(len(program),70)
        from tools.native_content import compile_ir,parse_pack,OPCODES
        sections=parse_pack(compile_ir(json.loads((ROOT/'content/native-opening.json').read_text()))[0])['sections']
        # Room v3 appends another independently bounded program. The original
        # Lamp receipt must still match its own complete stable-ID1 span.
        descriptors=[p for p in sections['Program']if p['stable_id']==1]
        self.assertEqual(len(descriptors),1)
        descriptor=descriptors[0]
        self.assertEqual(descriptor['phrase_count'],13)
        self.assertEqual(descriptor['command_count'],70)
        wire=sections['Command'][descriptor['first_command']:descriptor['first_command']+descriptor['command_count']]
        self.assertEqual([a['kind'] for a in program],[OPCODES[a['opcode']] for a in wire])
        self.assertEqual([a['phrase'] for a in program],[a['phrase'] for a in wire])
        self.assertEqual([a['duration'] for a in program],[a['duration'] for a in wire])
        self.assertEqual([a['vector'] for a in program],[a['vector'] for a in wire])
        self.assertEqual(fixture(self.trace,self.review),(ROOT/'tests/fixtures/lamp_dialogue_v0410.hpp').read_text())

    def test_native_source_gate_when_checkout_present(self):
        source=ROOT/'upstream/MOTHER-Encore'
        if not source.exists():self.skipTest('Checkout omitted; checked native receipts remain available')
        verify_sources(source,self.review)
        raw=(source/'Scripts/UI/DialogueBox.gd').read_text()
        self.assertEqual({k:hashlib.sha256(command_block(raw,k).encode()).hexdigest() for k in self.review['command_blocks']},self.review['command_blocks'])

    def test_changed_command_handler_parser_body_or_object_body_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            review=copy.deepcopy(self.review)
            for name in SOURCE_FILES:
                path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(name)
                review['sources'][name]=hashlib.sha256(path.read_bytes()).hexdigest()
            verify_sources(root,review)
            for name in SOURCE_FILES:
                path=root/name;old=path.read_bytes();path.write_bytes(old+b'changed body')
                with self.subTest(name=name),self.assertRaises(ValueError):verify_sources(root,review)
                path.write_bytes(old)

    def test_review_scope_version_and_missing_source_gate_rejected(self):
        for key,value in [('schema',2),('commit','unknown'),('game_version','0.5'),('whole_handler_approved',True),('sources',{})]:
            review=copy.deepcopy(self.review);review[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):compile_receipt(self.data,review)

    def test_receipt_wrong_engine_provenance_unknown_body_rejected(self):
        for key,value in [('schema',2),('commit','other'),('sources',{}),('extra_body','pass')]:
            data=copy.deepcopy(self.data);data[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):compile_receipt(data,self.review)
        for key,value in [('major',4),('patch',3),('hash','custom'),('build','custom')]:
            data=copy.deepcopy(self.data);data['godot'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):compile_receipt(data,self.review)

    def test_mutated_original_dialogue_and_missing_phrases_rejected(self):
        for modify in [lambda d:d['dialogue'].pop('7'),lambda d:d['dialogue']['4']['actorsmove']['lamp'].update(speed=501),lambda d:d['dialogue']['0'].update(text='Unreviewed')]:
            data=copy.deepcopy(self.data);modify(data)
            with self.assertRaises(ValueError):compile_receipt(data,self.review)

    def test_unsupported_commands_control_flow_and_wait_modes(self):
        for patch in [{'commands':['queue_free()']},{'text':'Hello'},{'goto':'0'},{'wait':0},{'wait':True},{'wait':float('nan')},{'caninput':True},{'autoadvance':1},{'autowait':1}]:
            phrase=copy.deepcopy(self.data['dialogue']['0']);phrase.update(patch)
            with self.subTest(patch=patch),self.assertRaises(ValueError):compile_phrase(phrase,0)
        for index in [-1,13,False]:
            with self.assertRaises(ValueError):compile_phrase(self.data['dialogue']['0'],index)

    def test_unknown_actor_target_method_and_body_fail_closed(self):
        mutations=[(0,'actors',{'ninten':'leader','lamp':'Objects/doll'}),(0,'talker','ninten'),
            (0,'actorsanim',{'lamp':{'anim':'Broken'}}),(0,'music','new.ogg'),
            (1,'actorsturn',{'ninten':{'x':True,'y':0}}),(1,'changecam','ninten'),
            (1,'movecam',{'x':474,'y':392,'length':1}),
            (1,'movecam',{'x':474,'y':392,'time':2}),
            (1,'actorsshake',{'lamp':{'x':2,'length':1,'queue':True}}),
            (2,'actorsemote',{'ninten':'angry'}),(2,'objectsfunction',{'Poltergeist/MusicArea':'play_music()'}),
            (5,'soundeffect','other.wav'),(5,'shakecam',{'length':.2,'size':'big'}),
            (5,'actorsjump',{'ninten':{'height':3,'length':.2,'shadow':False}}),
            (12,'ovbattlemusic',False),(12,'startbattle',{'battlers':[{'doll':'lamp'}],'winflag':'poltergeist'})]
        for idx,key,value in mutations:
            phrase=copy.deepcopy(self.data['dialogue'][str(idx)]);phrase[key]=value
            with self.subTest(idx=idx,key=key),self.assertRaises(ValueError):compile_phrase(phrase,idx)

    def test_unknown_movement_modes_body_and_nonfinite_numbers_rejected(self):
        for mutate in [lambda m:m.update(type='step'),lambda m:m.update(queue=True),lambda m:m.update(movement=[]),lambda m:m['movement'][0].update(teleport=True),lambda m:m['movement'][0].update(x=float('inf')),lambda m:m.update(speed=-1)]:
            phrase=copy.deepcopy(self.data['dialogue']['4']);mutate(phrase['actorsmove']['lamp'])
            with self.assertRaises(ValueError):compile_phrase(phrase,4)

    def test_dispatch_order_is_handler_order_not_yaml_order(self):
        doc=self.data['dialogue']
        for index in range(13):
            p=doc[str(index)];reordered=dict(reversed(list(p.items())))
            self.assertEqual(compile_phrase(p,index),compile_phrase(reordered,index))
        final=[a['kind'] for a in compile_phrase(doc['12'],12)]
        self.assertEqual(final,['StartWait','CallObjectDeferred','OverworldBattleMusic','MoveActor','JumpActor','QueueBattle','AwaitTimer'])
        phrase1=[a['kind'] for a in compile_phrase(doc['1'],1)]
        self.assertEqual(phrase1,['StartWait','TurnActor','ShakeActor','AnimateActor','ChangeCamera','YieldIdle','MoveCamera','AwaitTimer'])

    def test_final_handoff_is_after_wait_and_ordered_signals(self):
        program=compile_receipt(self.data,self.review)
        self.assertEqual([a['kind'] for a in program[-8:]],['AwaitTimer','StopInteraction','SetTalker','RestoreActor','ReleaseBattleActor','CutsceneEnded','DialogueDone','RequestBattle'])
        requests=[e for e in self.trace['events'] if e['kind']=='RequestBattle']
        self.assertEqual(len(requests),1);self.assertEqual(requests[0]['frame'],384)

    def test_changed_reference_timing_payload_or_scope_rejected(self):
        changes=[lambda d:d.update(schema=2),lambda d:d.update(delta='0.01666666666666667'),lambda d:d.update(scope='full game'),lambda d:d.update(overrides={}),lambda d:d.update(command_blocks={}),lambda d:d['events'].pop(),lambda d:d['events'][-1].update(frame=363),lambda d:d['events'][3].update(actor='Lamp')]
        for mutate in changes:
            data=copy.deepcopy(self.trace);mutate(data)
            with self.assertRaises(ValueError):fixture(data,self.review)

    def test_source_block_extraction_rejects_missing_or_ambiguous(self):
        raw='func test():\n\tif _curr_phrase.has("wait"):\n\t\tx()\n\ty()\n'
        self.assertEqual(command_block(raw,'wait'),'\tif _curr_phrase.has("wait"):\n\t\tx()\n')
        for text,key in [(raw,'other'),(raw+raw,'wait')]:
            with self.assertRaises(ValueError):command_block(text,key)

if __name__=='__main__':unittest.main()
