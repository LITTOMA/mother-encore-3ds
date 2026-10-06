"""Manual source/parser negative cases; no CI registration or test execution."""
import copy,unittest
from tools import field_shop,field_vending_machine
class ShopSourceCases(unittest.TestCase):
 def test_actual_offer_policy(self):
  d=field_shop.load();self.assertEqual([next(p['name']for p in d['policies']if p['id']==i)for i in d['offers']],['Hamburger','SportsDrink','EyeDrops']);self.assertTrue(d['can_sell'])
 def test_reject_unknown_and_partial_offer(self):
  for field,value in [('schema',2),('commit','00'*20),('offers',[1]),('lines',0),('cash_digits',0),('warning_seconds',-1)]:
   d=copy.deepcopy(field_shop.load());d[field]=value
   with self.assertRaises(ValueError):field_shop.validate(d)
 def test_reject_foreign_vending(self):
  for field,value in [('scene','Maps/unknown.tscn'),('id',0),('shop','invented'),('centered',False)]:
   d=copy.deepcopy(field_vending_machine.load());d[field]=value
   with self.assertRaises(ValueError):field_vending_machine.validate(d)
if __name__=='__main__':unittest.main()
