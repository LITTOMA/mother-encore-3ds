"""Keep playable shared-core sources in the genuine console build as well."""
from pathlib import Path
import re,unittest
ROOT=Path(__file__).resolve().parents[1]
FIXTURE_ONLY={'runtime/content.cpp','runtime/game.cpp','runtime/save.cpp','runtime/story.cpp'}
def checked(cmake,make):
 block=re.search(r'add_library\(encore_core\s+(.*?)\)',cmake,re.S)
 if not block:raise ValueError('Missing explicit desktop core source list')
 core=block[1].split();rows=re.findall(r'^SOURCES\s*[:+]?=\s*(.*)$',make,re.M);console=[v for row in rows for v in row.split()]
 if not rows or any(not re.fullmatch(r'(?:runtime|platform/ctr)/[A-Za-z0-9_-]+\.cpp',v)for v in core+console)or len(set(core))!=len(core)or len(set(console))!=len(console):raise ValueError('Unknown or duplicate build source expression')
 if set(core)-set(console)!=FIXTURE_ONLY:raise ValueError('Playable shared-core source omitted from console build')
 if not FIXTURE_ONLY.isdisjoint(console):raise ValueError('Fixture-only execution added to console build')
 return set(core),set(console)
class ConsoleCoreSourcesTests(unittest.TestCase):
 def setUp(self):
  self.cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8');self.make=(ROOT/'platform/ctr/Makefile').read_text(encoding='utf-8')
 def test_playable_sources_registered_in_both_builds(self):
  _,console=checked(self.cmake,self.make)
  self.assertIn('platform/ctr/music_bank_transports.cpp',console)
 def test_missing_core_and_fixture_substitutions_reject(self):
  for name in ['runtime/music_player_choice.cpp','runtime/music_players.cpp','runtime/music_scene_graph.cpp','runtime/music_bank_routes.cpp']:
   with self.assertRaises(ValueError):checked(self.cmake,self.make.replace(name,''))
  with self.assertRaises(ValueError):checked(self.cmake,self.make+'\nSOURCES += runtime/game.cpp\n')
 def test_unknown_and_duplicate_source_expressions_reject(self):
  for name in ['$(UNREVIEWED_SOURCE)','runtime/../escape.cpp','runtime/music_players.cpp']:
   with self.assertRaises(ValueError):checked(self.cmake,self.make+'\nSOURCES += '+name+'\n')
if __name__=='__main__':unittest.main()
