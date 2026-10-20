import copy,hashlib,json,shutil,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import dungeon_mechanism_graphics as g,dungeon_mechanism_presentation as d,dungeon_mechanism_audio as a
from tests import prepare_dungeon_mechanism_presentation as fixture

class MechanismMedia(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.review=d.read();cls.model,cls.blob,cls.proof=d.extract();cls.greview,cls.ex=g.read();cls.areview=json.loads(a.RECIPE.read_bytes())
 def reject_review(self,module,review,edit,extract=False):
  p=copy.deepcopy(review);edit(p)
  with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='mechanism-media-negative-')as directory:
   file=Path(directory)/'review.json';file.write_text(json.dumps(p),encoding='utf8')
   with self.assertRaises(ValueError):(module.extract if extract else module.read)(file)
 def changed(self,at,value):
  b=bytearray(self.blob);struct.pack_into('<I',b,at,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return b
 def test_source_geometry_sprite_frames_and_independent_resource(self):
  self.assertEqual(d.parse(self.blob),self.model);self.assertEqual(len(self.blob),7356);self.assertEqual(len(self.model['owners']),14);self.assertEqual(len(self.model['shapes']),17)
  self.assertEqual([p['frames']for p in self.model['profiles']],[2,2,6,15]);self.assertFalse(self.proof['physics_admission']or self.proof['renderer_admission']or self.proof['whole_scene_approved'])
  self.assertEqual(sum(o['bits']for o in self.model['owners']),8)
 def test_presentation_review_unknown_schema_types_defaults_and_coverage_reject(self):
  for edit in [lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(commit='0'*40),lambda p:p.update(extra=1),lambda p:p.update(scope=''),lambda p:p.update(graphics_recipe='../escape'),lambda p:p.update(graphics_review_sha256='0'*64),lambda p:p['engine_defaults']['body'].update(safe_margin=0),lambda p:p['engine_defaults']['area'].update(layer=True),lambda p:p['engine_defaults']['sprite'].update(centered=1),lambda p:p['shape_profiles'][0]['shapes'][0].update(role='unknown'),lambda p:p['textures'][0].update(bytes=True),lambda p:p['textures'][0].update(sha256='z'*64),lambda p:p['textures'][0].update(path='../bad.t3x')]:
   with self.subTest(edit=edit):self.reject_review(d,self.review,edit)
 def test_source_profile_shape_and_texture_binding_mismatches_reject(self):
  for edit in [lambda p:p['shape_profiles'][0]['shapes'].pop(),lambda p:p['shape_profiles'][0]['shapes'][0].update(role='pressure'),lambda p:p['textures'][0].update(path='graphics/other.t3x'),lambda p:p['shape_profiles'][1].update(profile=55)]:
   with self.subTest(edit=edit):self.reject_review(d,self.review,edit,True)
 def test_every_truncation_schema_nested_version_and_section_spans_reject(self):
  for size in range(len(self.blob)):
   with self.assertRaises(ValueError):d.parse(self.blob[:size])
  sprites=struct.unpack_from('<I',self.blob,84)[0];owners=struct.unpack_from('<I',self.blob,100)[0];shapes=struct.unpack_from('<I',self.blob,116)[0];nested=struct.unpack_from('<I',self.blob,132)[0]
  for at,value in [(8,2),(20,2),(24,5),(28,1),(52,6),(56,1),(60,1),(68,0),(72,0),(76,2),(88,65),(92,111),(sprites,0),(sprites+20,0),(sprites+60,1025),(sprites+84,65535),(sprites+88,0x7fc00000),(sprites+104,32),(owners,0),(owners+32,2),(owners+36,1),(shapes,0),(shapes+4,4),(shapes+32,0),(shapes+40,0x100000),(shapes+48,16),(shapes+52,0x7f800000),(shapes+60,1),(nested+8,2),(nested+20,2)]:
   with self.subTest(at=at),self.assertRaises(ValueError):d.parse(self.changed(at,value))
 def test_duplicate_profile_and_string_alias_reject(self):
  table=struct.unpack_from('<I',self.blob,84)[0]
  with self.assertRaises(ValueError):d.parse(self.changed(table+112,self.model['profiles'][0]['id']))
  b=bytearray(self.blob);b[table+116:table+124]=b[table+4:table+12];struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
  with self.assertRaises(ValueError):d.parse(b)
 def test_actual_native_texture_import_keeps_visible_pixels_and_alpha(self):
  report=fixture.REPORT;first=(report/'texture-original-1/observations.json').read_bytes();self.assertEqual(first,(report/'texture-original-2/observations.json').read_bytes());p=json.loads(first)
  self.assertEqual(len(p['textures']),4)
  for row in p['textures']:self.assertEqual((row['flags'],row['format'],row['changed_visible_pixels'],row['changed_alpha_pixels']),(0,5,0,0));self.assertGreater(row['changed_transparent_rgb_pixels'],0)
 def test_graphics_review_schema_pin_source_and_owner_mismatch_reject(self):
  for edit in [lambda p:p.update(schema=2),lambda p:p.update(commit='0'*40),lambda p:p['sources'].update(LICENSE='0'*64),lambda p:p['profiles'][0].update(atlas_columns=True),lambda p:p['profiles'][0].update(output='../bad.t3x')]:
   with self.subTest(edit=edit):self.reject_review(g,self.greview,edit)
  p=copy.deepcopy(self.greview);p['profiles'][0].update(scene=p['profiles'][1]['scene'],texture=p['profiles'][1]['texture'],sprite=p['profiles'][1]['sprite'])
  with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='mechanism-graphics-owner-')as directory:
   file=Path(directory)/'review.json';file.write_text(json.dumps(p),encoding='utf8')
   with self.assertRaises(ValueError):g.build(file)
 def test_texture_import_unknown_fields_filters_loops_and_operations_reject(self):
  path=self.greview['profiles'][0]['texture'];raw=self.ex.data(path+'.import');g.texture_import(raw,path)
  for old,new in [(b'flags/filter=false',b'flags/filter=true'),(b'process/premult_alpha=false',b'process/premult_alpha=true'),(b'flags/repeat=0',b'flags/repeat=1'),(b'flags/mipmaps=false',b'flags/mipmaps=true'),(b'size_limit=0',b'size_limit=0\nunknown=1'),(b'process/fix_alpha_border=true',b'process/fix_alpha_border=false')]:
   with self.subTest(old=old),self.assertRaises(ValueError):g.texture_import(raw.replace(old,new),path)
 def test_actual_graphics_compiler_receipt_and_review_identities(self):
  p=json.loads((fixture.REPORT/'actual-graphics-2/source.json').read_bytes());self.assertEqual(p['review_sha256'],hashlib.sha256(g.RECIPE.read_bytes()).hexdigest());self.assertEqual(p['tex3ds_sha256'],'58cecde429b2c4d1b4e09ae85b1f3b6851bf78914117d0e7bf8f1ba1a0a47c9e')
  byid={r['profile']:r for r in self.review['textures']}
  for row in p['profiles']:self.assertEqual((row['t3x_bytes'],row['t3x_sha256']),(byid[row['id']]['bytes'],byid[row['id']]['sha256']))
 def test_audio_four_sources_six_independent_node_owners_and_denial_channel(self):
  recipe,proof=a.extract();self.assertEqual(len(recipe['assets']),4);self.assertEqual(len(proof['owners']),6);self.assertEqual(proof['denied']['channel'],'switch');self.assertFalse(proof['live_output_admission']);self.assertEqual(len({r['path']for r in proof['owners']}),6)
 def test_audio_review_schema_dependency_source_id_and_conversion_reject(self):
  for edit in [lambda p:p.update(schema=3),lambda p:p.update(extra=1),lambda p:p.update(commit='0'*40),lambda p:p.update(bindings_review_sha256='0'*64),lambda p:p['source_profiles'][0].update(node='unknown'),lambda p:p['source_profiles'][0].update(profile=55),lambda p:p['source_profiles'].pop(),lambda p:p['recipe']['assets'][0].update(stable_id=True),lambda p:p['recipe']['assets'][0].update(stable_id=1104),lambda p:p['recipe']['assets'][0].update(source_sha256='0'*64),lambda p:p['recipe']['assets'][0].update(gain_db=-5),lambda p:p['recipe']['assets'][0].update(pcm_path='../escape.pcm'),lambda p:p['recipe']['assets'].pop()]:
   with self.subTest(edit=edit):self.reject_review(a,self.areview,edit,True)
 def test_fixture_exact_projection_and_duplicate_json_reject(self):
  with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='mechanism-geometry-fixture-')as directory:
   fixture.build(Path(directory)/'valid');self.assertEqual(sum(l.startswith('O\t')for l in(Path(directory)/'valid/reference.tsv').read_text().splitlines()),28)
  for module in(d,g,a):
   with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='mechanism-json-')as directory:
    file=Path(directory)/'bad.json';file.write_text('{"schema":1,"schema":1}',encoding='utf8')
    with self.assertRaises(ValueError):(module.extract if module is a else module.read)(file)
 def test_fixture_changed_hash_producer_engine_or_missing_owner_reject(self):
  for kind in('hash','producer','engine','owner'):
   with self.subTest(kind=kind),tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='mechanism-frozen-negative-')as directory:
    base=Path(directory);report=base/'reference';report.mkdir();shutil.copyfile(fixture.REPORT/'verification.json',report/'verification.json')
    for i in(2,3):
     folder=report/('geometry-original-'+str(i));folder.mkdir()
     for name in('geometry.json','geometry-input.json','geometry-receipt.json'):shutil.copyfile(fixture.REPORT/folder.name/name,folder/name)
    name='geometry-receipt.json'if kind=='producer'else'geometry.json';target=report/'geometry-original-2'/name;p=json.loads(target.read_bytes())
    if kind=='producer':p['producer_sha256']='0'*64
    elif kind=='engine':p['engine']['patch']=1
    elif kind=='owner':p['records'].pop()
    target.write_text(json.dumps(p)+'\n',encoding='utf8')
    if kind!='hash':
     # Only a synthetic negative copy is re-sealed. Published observations and
     # their hashes are immutable; these cases test the semantic second gate.
     if kind in('engine','owner'):(report/'geometry-original-3'/name).write_bytes(target.read_bytes())
     audit=json.loads((report/'verification.json').read_bytes())
     for i in(2,3):
      f=report/('geometry-original-'+str(i))/name;audit['evidence_sha256'][f.relative_to(report).as_posix()]=hashlib.sha256(f.read_bytes()).hexdigest()
     (report/'verification.json').write_text(json.dumps(audit),encoding='utf8')
    with patch.object(fixture,'REPORT',report),self.assertRaises(ValueError):fixture.build(base/'candidate')
    self.assertFalse((base/'candidate').exists())

if __name__=='__main__':unittest.main()
