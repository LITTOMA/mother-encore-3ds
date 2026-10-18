import json,struct,unittest,zlib
from tools import field_scene_npcs as n,field_npc_motion as codec

class NpcPolicy(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.policy,cls.rows,cls.sources=n.build()
 def test_source_policy_resources(self):
  for alias,name,dialogue,directions in [('abbott_lobby','mayor-abbott-policy.encmick',True,4),('abbott_office','mayor-nodialog-policy.encmick',False,4),('pippi','mayor-pippi-policy.encmick',True,8)]:
   row=next(r for r in self.rows if r['binding']['alias']==alias);blob=n.interaction_blob(row)
   self.assertEqual(blob,(n.ROOT/'tests/fixtures'/name).read_bytes());codec.parse_policy(blob)
   self.assertEqual(struct.unpack_from('<2I',blob,56),(directions,1000));self.assertEqual(bool(struct.unpack_from('<I',blob,28)[0]&8),dialogue)
 def test_policy_version_duration_and_feature_negative(self):
  original=(n.ROOT/'tests/fixtures/mayor-pippi-policy.encmick').read_bytes()
  for at,value in [(20,4),(20,6),(20,8),(28,16),(28,1),(56,0),(56,3),(56,9),(60,0),(60,30001)]:
   bad=bytearray(original);struct.pack_into('<I',bad,at,value);struct.pack_into('<I',bad,16,zlib.crc32(bad[32:]))
   with self.assertRaises(ValueError):codec.parse_policy(bad)
  for size in (0,63,len(original)-1):
   with self.assertRaises(ValueError):codec.parse_policy(original[:size])
 def test_legacy_encoders_remain_distinct(self):
  row=self.rows[-1];legacy=codec.encode(row['ir'],row['binding']['id'],2)
  self.assertEqual(legacy,(n.ROOT/'tests/fixtures/mayor-pippi-motion.encmick').read_bytes())
  with self.assertRaises(ValueError):codec.parse_policy(legacy)
  with self.assertRaises(ValueError):codec.parse(n.interaction_blob(row))
 def test_native_tsv_is_exact_projection(self):
  result=json.loads((n.ROOT/'reports/mayor-office-import/npc-return-native.json').read_bytes())
  states=''.join('\t'.join(str(v).lower()if isinstance(v,bool)else str(v)for v in r)+'\n'for r in result['rows'])
  actions=''.join(str(i)+'\t'+str(a[0])+'\t'+a[1]+'\n'for i,c in enumerate(result['cases'])for a in c['actions'])
  self.assertEqual(states,(n.ROOT/'tests/fixtures/npc-return-states.tsv').read_text())
  self.assertEqual(actions,(n.ROOT/'tests/fixtures/npc-return-actions.tsv').read_text())
if __name__=='__main__':unittest.main()
