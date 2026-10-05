"""Manual negative admission and source-order cases for family followup."""
import copy, unittest
from unittest.mock import patch
from tools import link_family_followup as family
from tools.native_content import OPCODES

class FamilyAdmission(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.data=family.load()

    def test_unknown_source_field_rejected(self):
        from tools.extract_battle_entry import Extractor
        original=Extractor.yaml
        def changed(ex,path):
            value=original(ex,path)
            if path.endswith('/carol_key.yaml'): value['0']['unknown_effect']=True
            return value
        with patch.object(Extractor,'yaml',changed), self.assertRaises(ValueError): family.build()

    def test_changed_graph_rejected(self):
        from tools.extract_battle_entry import Extractor
        original=Extractor.yaml
        def changed(ex,path):
            value=original(ex,path)
            if path.endswith('/mimmie_key.yaml'): value['0']['goto']='0'
            return value
        with patch.object(Extractor,'yaml',changed), self.assertRaises(ValueError): family.build()

    def fake(self,missing=False):
        class Ex:
            root=family.ROOT
            strings=['carol_ask_key','mimmie_ask_key']
            sections={'Flag':[{'name_string':0},{'name_string':1}],
                      'Program':[{} for _ in range(16)],'Command':[]}
            def string(self,value): self.strings.append(value); return len(self.strings)-1
            def file(self,value): pass
            def record_map(self,*args): pass
        ex=Ex(); ex.strings=list(ex.strings); ex.sections=copy.deepcopy(ex.sections)
        if missing: ex.sections['Flag'].pop()
        return ex

    def test_missing_flag_is_transactional(self):
        ex=self.fake(True); old=copy.deepcopy(ex.sections)
        with patch.object(family,'load',return_value=self.data),patch.object(family,'adopt_sources'), self.assertRaises(ValueError):
            family.append_room(ex)
        self.assertEqual(old,ex.sections)

    def test_flag_precedes_final_input_gate_and_ids_append(self):
        ex=self.fake()
        with patch.object(family,'load',return_value=self.data),patch.object(family,'adopt_sources'):
            family.append_room(ex)
        self.assertEqual([p['stable_id'] for p in ex.sections['Program'][16:]],[17,18])
        for p in ex.sections['Program'][16:]:
            commands=ex.sections['Command'][p['first_command']:p['first_command']+p['command_count']]
            at=next(i for i,c in enumerate(commands) if OPCODES[c['opcode']]=='SetFlag')
            self.assertEqual([OPCODES[c['opcode']] for c in commands[at-1:at+2]],['ShowDialogue','SetFlag','AwaitDialogue'])
            self.assertEqual(commands[-1]['duration'],self.data['end_duration'])

if __name__=='__main__': unittest.main()
