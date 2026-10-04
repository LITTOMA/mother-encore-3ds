import copy,io,json,sys,unittest,tempfile
from pathlib import Path
from unittest import mock
ROOT=Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0,str(ROOT))
from tools import phone_presentation_bindings as b,phone_assets,native_phone
from tools.extract_battle_entry import Extractor

def prepare_fixtures(root=ROOT):
    # Only the explicit fixture target writes. Ordinary parallel test runs read
    # the existing source/pack and exercise the compiler in memory.
    blob=native_phone.compile_pack(root)
    out=Path(root)/'build/phone-presentation-fixtures/regenerated.encphone'
    out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob)
    return out

class PhonePresentationBindingsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.value=b.load(ROOT);cls.ex=Extractor(ROOT)

    def rejected(self,mutate):
        value=copy.deepcopy(self.value);mutate(value)
        with self.assertRaises((ValueError,KeyError,TypeError)):
            b.checked(value,self.ex)

    def test_default_complete_ir_and_binary_identity(self):
        expected=(ROOT/'content/phone-stage/presentation.json').read_text(encoding='utf-8')
        actual=phone_assets.build(ROOT)
        self.assertEqual(json.loads(expected),actual)
        self.assertEqual(json.dumps(actual,indent=2,ensure_ascii=False)+'\n',expected)
        self.assertEqual(native_phone.compile_pack(ROOT),(ROOT/'romfs/data/opening.encphone').read_bytes())

    def test_missing_unknown_schema_and_nested_fields(self):
        for key in self.value:self.rejected(lambda v,k=key:v.pop(k))
        self.rejected(lambda v:v.update(extra=True))
        self.rejected(lambda v:v.update(schema=2))
        self.rejected(lambda v:v.update(schema=True))
        for group in ['sprite','audio','player','callbacks','sounds','ring_binding']:
            self.rejected(lambda v,g=group:v[g].update(extra=1))
        self.rejected(lambda v:v['clips'][0].update(extra=1))
        self.rejected(lambda v:v['clips'][1]['events'][0].update(extra=1))

    def test_unknown_events_roles_refs_and_time_mechanisms(self):
        self.rejected(lambda v:v['clips'][0].update(role='other'))
        self.rejected(lambda v:v['clips'][0].update(native_kind=2))
        self.rejected(lambda v:v['clips'][1]['events'][0].update(kind='Ignore'))
        self.rejected(lambda v:v['clips'][1]['events'][0].update(track=999))
        self.rejected(lambda v:v['clips'][1]['events'][1].update(sound='unknown'))
        self.rejected(lambda v:v['clips'][1]['events'][0].update(time_encoding='eval'))
        self.rejected(lambda v:v['clips'][1]['events'].pop())
        self.rejected(lambda v:v['clips'][1]['events'][1].update(track=0))
        self.rejected(lambda v:v['clips'][0]['animation'].update(source_resource_id=88))
        self.rejected(lambda v:v['clips'][0]['animation'].update(name='RESET'))

    def test_source_track_numeric_facts_are_checked_not_just_hashes(self):
        self.rejected(lambda v:v['clips'][1]['animation'].update(length=1.5))
        self.rejected(lambda v:v['clips'][1]['animation']['tracks'][0]['keys']['values'].__setitem__(0,0))
        self.rejected(lambda v:v['clips'][1]['animation']['tracks'][1]['keys']['times'].__setitem__(0,.1))
        self.rejected(lambda v:v['clips'][1]['animation']['tracks'][1].update(path='Other:playing'))
        self.rejected(lambda v:v['clips'][1]['animation']['tracks'][1]['keys']['values'].__setitem__(0,False))
        self.rejected(lambda v:v['sprite'].update(size=[80,22]))

    def test_source_expressions_resource_declarations_and_story_binding(self):
        self.rejected(lambda v:v['callbacks']['ring'].update(review=v['callbacks']['ring']['review'].replace('Ring','Idle')))
        self.rejected(lambda v:v['callbacks']['use'].update(method='_ring'))
        self.rejected(lambda v:v['declarations'].__setitem__(0,'onready var _anim_player = $Other'))
        self.rejected(lambda v:v['sounds'].update(hangup=v['sounds']['ring']))
        self.rejected(lambda v:v['sprite']['review'].update(texture={'ExtResource':3}))
        self.rejected(lambda v:v['ring_binding'].update(flag='talked_to_dad'))
        self.rejected(lambda v:v['ring_binding'].update(object='Objects/npc'))
        self.rejected(lambda v:v['ring_binding']['review']['5'].update(autowait=2))

    def test_source_fingerprints_pin_and_unsafe_paths(self):
        self.rejected(lambda v:v.update(commit='0'*40))
        self.rejected(lambda v:v['sources'].update({v['scene']:'0'*64}))
        self.rejected(lambda v:v['sources'].pop(v['scene']))
        self.rejected(lambda v:v['sources'].update({'LICENSE':'0'*64}))
        self.rejected(lambda v:v['sprite'].update(path='../phone.t3x'))
        self.rejected(lambda v:v['sprite'].update(path='C:/phone.t3x'))

    def test_json_duplicate_fields_nonfinite_and_wrong_numeric_types(self):
        with tempfile.TemporaryDirectory()as td:
            path=Path(td)/'bindings.json'
            for text in ['{"schema":1,"schema":1}','{"number":NaN}','{"number":Infinity}','{']:
                path.write_text(text,encoding='utf-8')
                with mock.patch.object(b,'PATH',path),self.assertRaises(ValueError):b.read(ROOT)
        self.rejected(lambda v:v['clips'][0]['animation'].update(step=True))
        self.rejected(lambda v:v['clips'][0]['animation']['tracks'][0].update(interp=True))
        self.rejected(lambda v:v['clips'][0]['animation']['tracks'][0]['keys'].update(update=True))
        self.rejected(lambda v:v['clips'][0]['animation']['tracks'][0]['keys'].update(transitions=[True]))
        self.rejected(lambda v:v['clips'][0]['animation']['tracks'][0]['keys'].update(times=[False]))

    def test_event_emitter_reads_source_keys_and_track_order(self):
        clip=copy.deepcopy(next(c for c in self.value['clips']if c['role']=='ring'))
        base=b.events(clip,self.value)
        clip['animation']['tracks'][0]['keys']['values'][0]=0
        changed=b.events(clip,self.value)
        self.assertNotEqual(base,changed)
        self.assertEqual(changed[0]['frame'],0)
        self.assertEqual(len(changed),sum(len(t['keys']['times'])for t in clip['animation']['tracks']))
        for event in changed:
            self.assertEqual(event['kind'],'Frame'if event['track']==0 else'PlaySound')
        # This helper demonstrates data consumption, not a reviewed alternate
        # source. The ordinary checked frontend must reject the changed facts.
        self.rejected(lambda v:v['clips'][1]['animation']['tracks'][0]['keys']['values'].__setitem__(0,0))

    def test_ordinary_compile_gate_rejects_and_preserves_existing_outputs(self):
        paths=[ROOT/'romfs/data/opening.encphone',ROOT/'compatibility/reviews/phone-pack.json']
        before=[p.read_bytes()for p in paths]
        for mutation in [lambda v:v.update(extra=1),lambda v:v['sprite'].update(path='phone-preview/changed.t3x')]:
            value=copy.deepcopy(self.value);mutation(value)
            with mock.patch.object(b,'read',return_value=value),mock.patch.object(sys,'argv',['native_phone.py','compile','--root',str(ROOT)]),mock.patch.object(sys,'stderr',io.StringIO()):
                self.assertEqual(native_phone.main(),1)
            self.assertEqual([p.read_bytes()for p in paths],before)

if __name__=='__main__':
    if sys.argv[1:]==['--prepare-only']:print(prepare_fixtures())
    else:unittest.main()
