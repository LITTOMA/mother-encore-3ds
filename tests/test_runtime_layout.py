import json, tempfile, unittest
from pathlib import Path
from tools.runtime_layout import load, runtime_path
from tools.gameplay_audio import encode, ROLES
from tools.romfs_layout import check_layout
class RuntimeLayoutTests(unittest.TestCase):
    def test_projection_and_unknown(self):
        for old,new in load().items():
            self.assertEqual(runtime_path(old),new)
            check_layout([new])
        with self.assertRaises(ValueError):runtime_path('data/unknown.encroom')
    def test_legacy_and_wrong_category_rejected(self):
        for path in ('data/opening.encroom','ui/opening.encroom','world/opening.encaudio','sound/music/gameplay.enccues','sound/banks/../podunk.enczones'):
            with self.subTest(path=path),self.assertRaises(ValueError):check_layout([path])
    def test_invalid_mapping_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'content').mkdir()
            for value in ({'schema':2,'kind':'encore.runtime-layout','paths':{}},
                          {'schema':1,'kind':'encore.runtime-layout','paths':{'data/a.encroom':'world/../a.encroom'}},
                          {'schema':1,'kind':'encore.runtime-layout','paths':{'data/a.encroom':'ui/a.encroom'}}):
                (root/'content/runtime-layout.json').write_text(json.dumps(value))
                with self.assertRaises(ValueError):load(root)
    def test_cue_encoder_rejects_unbound_and_unsafe(self):
        with self.assertRaises(ValueError):encode([])
        for path in ('../escape','Audio/../escape','Audio//bad','C:/sound'):
            with self.assertRaises(ValueError):encode([path]*len(ROLES))
    def test_texture_producer_migration_rejects_semantic_changes(self):
        from tools.migrate_layout_dependencies import projected_producer
        old=b"PACK='romfs/data/a.encroom'\nVALUE=1\n"
        new=b"PACK='romfs/world/a.encroom'\nVALUE=1\n"
        paths={'data/a.encroom':'world/a.encroom'}
        projected_producer(old,new,paths)
        with self.assertRaises(ValueError):projected_producer(old,new.replace(b'VALUE=1',b'VALUE=2'),paths)
if __name__=='__main__':unittest.main()
