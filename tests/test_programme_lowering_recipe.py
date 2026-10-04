import copy,io,json,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
from tools import programme_lowering_recipe as r,phone_dialogue,native_content
from tools.extract_native_content import Extractor

ROOT=Path(__file__).resolve().parents[1]

def prepare_fixtures(root=ROOT):
    ir,_=Extractor(root).run()
    blob,_=native_content.compile_ir(ir)
    out=Path(root)/'build/programme-fixtures/regenerated.encroom'
    out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob)
    # The existing native Record consumer reads these checked companions beside
    # its Room input. Only explicit fixture preparation copies their bytes.
    for name in ['opening.encsession','doll-entry.encround','opening.encitems']:
        (out.parent/name).write_bytes((Path(root)/'romfs/data'/name).read_bytes())
    return out

class ProgrammeLoweringRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe=r.load(ROOT)
        cls.ir=json.loads((ROOT/'content/native-opening.json').read_text(encoding='utf-8'))

    def reject(self,mutation):
        value=copy.deepcopy(self.recipe);mutation(value)
        with self.assertRaises((ValueError,KeyError,TypeError)):r.checked(value,ROOT)

    def test_original_complete_room_and_phone_data_equivalence(self):
        room,_=Extractor(ROOT).run()
        self.assertEqual(room['strings'],self.ir['strings'])
        self.assertEqual(room['sections'],self.ir['sections'])
        old,_=native_content.compile_ir(self.ir);new,_=native_content.compile_ir(room)
        self.assertEqual(new[128:],old[128:])
        # Reviewed input hashes change when an adapter is moved to data. The
        # executable resources retain every wire table byte and stable ID.
        phone=phone_dialogue.build(ROOT)
        expected=(ROOT/'content/phone-stage/dialogue.json').read_text(encoding='utf-8')
        self.assertEqual(json.dumps(phone,indent=2,ensure_ascii=False)+'\n',expected)

    def test_schema_missing_unknown_fields_opcodes_and_reference_tags(self):
        for key in self.recipe:self.reject(lambda v,k=key:v.pop(k))
        self.reject(lambda v:v.update(extra=1))
        self.reject(lambda v:v.update(schema=2))
        self.reject(lambda v:v.update(schema=True))
        self.reject(lambda v:v['programmes']['doll'].update(extra=1))
        self.reject(lambda v:v['programmes']['doll']['commands'][0].update(op='evaluate'))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(kind='IgnoreUnknown'))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(unknown=1))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(actor={'$eval':'script'}))
        self.reject(lambda v:v['normal_metadata'].update(extra=1))
        self.reject(lambda v:v['normal_metadata']['nodes'][0].update(extra=1))

    def test_source_expressions_docs_identities_and_numeric_types(self):
        self.reject(lambda v:v['facts']['turn-default'].update(value=.09))
        self.reject(lambda v:v['facts']['music-fade'].update(value=True))
        self.reject(lambda v:v['facts']['camera-trans'].update(value='linear'))
        self.reject(lambda v:v['facts']['voice-root'].update(value='res://other/'))
        self.reject(lambda v:v['identities']['phone/player-token'].update(value='[Ana]'))
        self.reject(lambda v:v['facts']['shake-direction'].update(value=[0,1]))
        self.reject(lambda v:v['facts']['turn-default'].update(pattern='var missing = (?P<value>[0-9.]+)'))
        self.reject(lambda v:v['documents'][v['programmes']['doll']['source']]['0'].update(wait=.7))
        self.reject(lambda v:v['identities']['doll/actor/Doll'].update(value='Lamp'))
        self.reject(lambda v:v['identities']['doll/text/2'].update(value=True))
        self.reject(lambda v:v['identities']['doll/actor/Doll'].update(path=['0','actors','missing']))
        self.reject(lambda v:v['normal_metadata']['nodes'][1]['branch'].update(value='ana'))

    def test_missing_and_malformed_source_references_never_fall_back(self):
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(value={'$source':['0','unknown']}))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(value={'$fact':'unknown'}))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(actor={'$identity':'unknown'}))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(duration={'$argument':'unknown'}))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(value={'$source':['0'],'extra':1}))
        self.reject(lambda v:v['programmes']['doll']['commands'][0]['template'].update(duration=.123))
        self.reject(lambda v:v['programmes']['lamp']['boundary'].update(prefix_count=True))

    def test_provenance_unsafe_paths_json_duplicate_and_nonfinite(self):
        self.reject(lambda v:v.update(commit='0'*40))
        self.reject(lambda v:v['sources'].update({v['programmes']['lamp']['source']:'0'*64}))
        self.reject(lambda v:v['sources'].update({'../escape':'0'*64}))
        with tempfile.TemporaryDirectory()as td:
            path=Path(td)/'recipe.json'
            for text in ['{"schema":1,"schema":1}','{"n":NaN}','{"n":Infinity}','{']:
                path.write_text(text,encoding='utf-8')
                with mock.patch.object(r,'PATH',path),self.assertRaises(ValueError):r.read(ROOT)

    def test_source_phase_order_and_actor_binding_order(self):
        value=self.recipe;row=value['programmes']['doll'];doc=copy.deepcopy(value['documents'][row['source']])
        reordered={k:dict(reversed(list(p.items())))for k,p in doc.items()}
        self.assertEqual(r.execute('doll',doc,root=ROOT),r.execute('doll',reordered,root=ROOT))
        reordered['0']['actors']=dict(reversed(list(reordered['0']['actors'].items())))
        with self.assertRaisesRegex(ValueError,'binding order'):r.execute('doll',reordered,root=ROOT)
        with self.assertRaises(ValueError):r.execute('doll',dict(reversed(list(doc.items()))),root=ROOT)

    def test_ordinary_phone_extract_rejects_before_changing_stage(self):
        stage=ROOT/'content/phone-stage/dialogue.json';before=stage.read_bytes()
        invalid=copy.deepcopy(self.recipe);invalid['facts']['turn-default']['value']=.09
        with mock.patch.object(r,'read',return_value=invalid),mock.patch.object(sys,'argv',['phone_dialogue.py','extract','--root',str(ROOT)]),mock.patch.object(sys,'stderr',io.StringIO()):
            self.assertEqual(phone_dialogue.main(),1)
        self.assertEqual(stage.read_bytes(),before)

    def test_stale_valid_identity_is_rejected_by_room_compile_gate(self):
        changed=copy.deepcopy(self.recipe);changed['identities']['doll/text/2']['value']+=1
        r.checked(changed,ROOT)
        # This is the gate used after source provenance in normal reviewed Room
        # compilation. A valid authoring identity change requires a fresh IR.
        with mock.patch.object(r,'read',return_value=changed),self.assertRaisesRegex(ValueError,'Stale programme'):
            r.verify_room(self.ir,ROOT)

if __name__=='__main__':
    if sys.argv[1:]==['--prepare-only']:print(prepare_fixtures())
    else:unittest.main()
