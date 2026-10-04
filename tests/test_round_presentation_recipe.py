"""Real source oracle, fail-closed recipe checks and native consumer fixtures."""
import copy,json,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
from tools import round_assets as assets,round_presentation_recipe as recipe
from tools.extract_battle_entry import Extractor
import native_round

def report_to_ir(report):
    # Preserve established table ordering and independently extracted camera data.
    ir=json.loads((ROOT/'content/native-round.json').read_text(encoding='utf-8'))
    for section,current in ir['presentation'].items():
        if isinstance(current,dict):
            for name in current:
                if name in report[section]:current[name]=report[section][name]
        else:ir['presentation'][section]=report[section]
    return ir

def compile_report(report):return native_round.encode(native_round.lower(report_to_ir(report)))

def prepare_fixtures(original,resources,write=False):
    baseline=assets.build_presentation(Extractor(ROOT),resources,original)
    changed=copy.deepcopy(original)
    fact=next(f for f in changed['facts']if f['id']=='dialogue.text_rect')
    if fact['kind']!='native_policy':raise ValueError('Fixture cannot overwrite source expressions')
    fact['expected'][0]+=1
    modified=assets.build_presentation(Extractor(ROOT),resources,changed)
    if write:
        output=ROOT/'build/round-recipe-fixtures';output.mkdir(parents=True,exist_ok=True)
        (output/'baseline.encround').write_bytes(compile_report(baseline))
        (output/'changed.encround').write_bytes(compile_report(modified))
    return baseline,modified

class PresentationRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=recipe.read(ROOT/'content/round-presentation-recipe.json')
        cls.resources=json.loads((assets.OUT/'source.json').read_text(encoding='utf-8'))['resources']
    def rejected(self,value,resources=None):
        p=assets.Presentation(Extractor(ROOT),self.resources if resources is None else resources)
        with self.assertRaises(ValueError):recipe.apply(p,value)
        self.assertEqual((p.media,p.tracks,p.keys,p.events),([],[],[],[]))
    def test_real_source_report_ir_and_binary_oracles(self):
        fresh=assets.build_presentation(Extractor(ROOT),self.resources,self.original)
        expected=(assets.REPORT/'presentation.json').read_bytes()
        self.assertEqual((json.dumps(fresh,indent=2,sort_keys=True,ensure_ascii=False)+'\n').encode(),expected)
        self.assertEqual(report_to_ir(fresh),json.loads((ROOT/'content/native-round.json').read_text(encoding='utf-8')))
        self.assertEqual(compile_report(fresh),(ROOT/'romfs/data/opening.encround').read_bytes())
    def test_recipe_contract_rejects_unknown_missing_and_versions(self):
        for key in self.original:
            value=copy.deepcopy(self.original);del value[key]
            with self.subTest(missing=key):self.rejected(value)
        for key,value in [('extra',0),('schema',2),('schema',True),('kind','other'),('commit','unknown')]:
            modified=copy.deepcopy(self.original);modified[key]=value
            with self.subTest(key=key,value=value):self.rejected(modified)
    def test_source_hash_and_source_expression_are_both_required(self):
        value=copy.deepcopy(self.original);value['sources'][next(iter(value['sources']))]='0'*64;self.rejected(value)
        value=copy.deepcopy(self.original);fact=next(f for f in value['facts']if f['kind']=='numbers');fact['expected'][0]+=1;self.rejected(value)
        value=copy.deepcopy(self.original);fact=next(f for f in value['facts']if f['kind']=='numbers');fact['pattern']='does-not-match-upstream';self.rejected(value)
        value=copy.deepcopy(self.original);fact=next(f for f in value['facts']if f['kind']=='node');fact['properties'][0]['property']='unreviewed';self.rejected(value)
        value=copy.deepcopy(self.original);fact=next(f for f in value['facts']if f['kind']=='derive');fact['expected'][0]+=1;self.rejected(value)
    def test_facts_fail_closed(self):
        for modify in [lambda f:f.update(kind='unknown'),lambda f:f.update(extra=1),lambda f:f.update(expected=[float('nan')]),lambda f:f.update(expected=[True])]:
            value=copy.deepcopy(self.original);modify(value['facts'][0]);self.rejected(value)
        value=copy.deepcopy(self.original);value['facts'].append(copy.deepcopy(value['facts'][0]));self.rejected(value)
        for reference in [{'fact':'unknown','index':0},{'fact':'cell.party','index':100},{'fact':'cell.party','index':True},{'fact':'cell.party','index':0,'extra':1}]:
            value=copy.deepcopy(self.original);next(f for f in value['facts']if f['kind']=='derive')['inputs']=[reference];self.rejected(value)
        value=copy.deepcopy(self.original);fact=next(f for f in value['facts']if f['kind']=='derive');fact.update(operation='eval');self.rejected(value)
        value=copy.deepcopy(self.original);fact=next(f for f in value['facts']if f['kind']=='derive');fact.update(operation='ratio',inputs=[1,0]);self.rejected(value)
        resources=copy.deepcopy(self.resources);resources[0]['columns']=0;self.rejected(self.original,resources)
    def test_actions_bindings_and_context_fail_closed(self):
        for modify in [lambda a:a.update(op='execute'),lambda a:a.update(extra=1),lambda a:a.update(resource='missing'),lambda a:a.update(role=0),lambda a:a.update(rect=[1,2,3]),lambda a:a.update(anchor=[False,0])]:
            value=copy.deepcopy(self.original);modify(value['actions'][0]);self.rejected(value)
        value=copy.deepcopy(self.original);value['actions'][1]['id']=value['actions'][0]['id'];self.rejected(value)
        for op,field,bad in [('bind','slot','unknown'),('bind','media','not-created'),('track','property',19),('track','times',[2,1]),('track','values',[[[1]]]),('track','interpolation',9),('event','kind',0),('update','values',{'source':'unknown'})]:
            value=copy.deepcopy(self.original);next(a for a in value['actions']if a['op']==op)[field]=bad;self.rejected(value)
        value=copy.deepcopy(self.original);value['parameters']['Unreviewed']=[0,0,0,0];self.rejected(value)
        value=copy.deepcopy(self.original);del value['parameters']['TextTiming'];self.rejected(value)
        value=copy.deepcopy(self.original);value['context']['party']={'unknown':'test'};self.rejected(value)
        value=copy.deepcopy(self.original);value['context']['bash']={'media':'unknown'};self.rejected(value)
    def test_duplicate_json_keys_are_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build')as temp:
            path=Path(temp)/'duplicate.json';path.write_text('{"schema":1,"schema":1}',encoding='utf-8')
            with self.assertRaises(ValueError):recipe.read(path)
    def test_changed_external_layout_is_compiled_without_core_changes(self):
        baseline,changed=prepare_fixtures(self.original,self.resources)
        base=baseline['media'][baseline['bindings']['DialogueText']]
        modified=changed['media'][changed['bindings']['DialogueText']]
        self.assertEqual(modified['rect'][0]-base['rect'][0],1)
        self.assertEqual(changed['parameters']['DialogueTextLayout'][0]-baseline['parameters']['DialogueTextLayout'][0],1)
        self.assertEqual(baseline['tracks'],changed['tracks']);self.assertEqual(baseline['events'],changed['events'])
        self.assertNotEqual(compile_report(baseline),compile_report(changed))

if __name__=='__main__':
    if sys.argv[1:]==['--prepare-only']:
        resources=json.loads((assets.OUT/'source.json').read_text(encoding='utf-8'))['resources']
        prepare_fixtures(recipe.read(),resources,write=True)
    else:unittest.main()
