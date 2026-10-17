import hashlib,json,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
from tools import test_region_background as tool
ROOT=Path(__file__).resolve().parents[1]
class RegionBaseline(unittest.TestCase):
 def test_review_binds_current_exact_normalized_source(self):
  review=json.loads((ROOT/'reports/bridge-battle/axis-baseline-review.json').read_text())
  source=(ROOT/review['baseline']).read_text().replace('    friend class RegionBackgroundKernel;\n','')
  digest=hashlib.sha256(source.encode()).hexdigest()
  self.assertEqual(review['normalized_sha256'],digest)
  self.assertIn(digest,tool.REVIEWED_BASELINE_SHA256)
 def test_unknown_header_rejected_before_compiler_even_optimized(self):
  with tempfile.TemporaryDirectory()as folder:
   root=Path(folder);(root/'tools').mkdir();(root/'include/encore').mkdir(parents=True)
   shutil.copyfile(ROOT/'tools/test_region_background.py',root/'tools/test_region_background.py')
   (root/'include/encore/background_kernel.hpp').write_text('class BackgroundKernel {}; // unreviewed\n')
   for options in ([],['-O']):
    result=subprocess.run([sys.executable,*options,str(root/'tools/test_region_background.py')],capture_output=True,text=True)
    self.assertNotEqual(result.returncode,0)
    self.assertIn('Baseline changed: review before generating overlay',result.stderr)
    self.assertFalse((root/'build/region-background-integration/overlay/encore/background_kernel.hpp').exists())
if __name__=='__main__':unittest.main()
