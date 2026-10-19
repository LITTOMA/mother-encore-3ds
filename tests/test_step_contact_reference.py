import copy,json,unittest
from tools import run_step_contact_reference as r
class StepContactReferenceTests(unittest.TestCase):
 def test_source_geometry_and_methods_review(self):self.assertEqual(r.source()[-1],json.loads((r.ROOT/'compatibility/reviews/step-contact-v0410.json').read_bytes()))
 def test_native_outcomes_and_fixture(self):
  doc=json.loads((r.ROOT/'reports/mayor-office-import/step-contact-native.json').read_bytes());self.assertEqual(r.fixture(doc),(r.ROOT/'tests/fixtures/step-contacts.tsv').read_text(encoding='utf-8'));self.assertEqual(doc['rows'][15]['inside'],[False,True]);self.assertEqual(doc['rows'][9]['wanted'],'wood')
 def test_reject_invalid_or_changed_result(self):
  doc=json.loads((r.ROOT/'reports/mayor-office-import/step-contact-native.json').read_bytes())
  for mutate in [lambda p:p.update(schema=True),lambda p:p['rows'].pop(),lambda p:p['rows'][0].update(wanted='unknown'),lambda p:p['rows'][0].update(inside=[0,False]),lambda p:p['rows'][0].update(inside=[]),lambda p:p['rows'][0].update(shadow='unreviewed'),lambda p:p['rows'][0]['case'].update(mask=0),lambda p:p['rows'][0]['case'].update(enabled=[1,True])]:
   p=copy.deepcopy(doc);mutate(p)
   with self.assertRaises(ValueError):r.validate_result(p)
if __name__=='__main__':unittest.main()
