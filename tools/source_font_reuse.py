import argparse,collections,copy,hashlib,json,re,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def scalar_text(text):
    # Source CSV contains literal C escapes; actual importer unescapes them.
    text=re.sub(r'\\(?:n|r|t|b|f|v)', '', text)
    # RichTextLabel BBCode tags are layout/control, never glyph requirements.
    text=re.sub(r'\[/?(?:color|wave|shake|rainbow|tornado|fade|center|right|fill|b|i|u|s|url|img|font|code|table|cell|indent|list|lb|rb)(?:=[^\]]*)?\]', '', text)
    return {ord(c) for c in text if ord(c)>=32 and ord(c)!=127}

def glyph_requirements(catalog):
    groups=collections.defaultdict(set)
    names=''.join(l['name'] for l in catalog['locales'])
    for locale in catalog['locales']:
        path=locale['font'];groups[path].update(range(32,127));groups[path].update(scalar_text(names))
        for row in catalog['records']:
            groups[path].update(scalar_text(row['values'].get(locale['code'],'') or row['values'].get('en','')))
    return dict(groups)

def catalogue_reuse(previous,current,manifest,new_sha):
    if previous['commit']!=current['commit'] or previous['locales']!=current['locales'] or glyph_requirements(previous)!=glyph_requirements(current):
        raise ValueError('Font locale/required glyph set changed; regenerate assets')
    if not re.fullmatch('[0-9a-f]{64}',new_sha):raise ValueError('Invalid font catalogue fingerprint')
    result=copy.deepcopy(manifest);result['catalog_sha256']=new_sha
    return result

def reuse_catalogue(base):
    from tools.source_fonts import stage_files
    import ast
    scalar_ast=lambda text:next(n for n in ast.parse(text).body if isinstance(n,ast.FunctionDef) and n.name=='scalar_text')
    if ast.dump(scalar_ast((ROOT/'tools/source_fonts.py').read_text()))!=ast.dump(scalar_ast(Path(__file__).read_text())):
        raise ValueError('Font scalar extraction differs from original producer')
    if not re.fullmatch('[0-9a-f]{40}',base or ''):raise ValueError('Font reuse needs exact baseline commit')
    old=lambda name:subprocess.check_output(['git','show',base+':'+name],cwd=ROOT)
    path='content/asset-receipts/fonts/source.json';prior=old(path);manifest=json.loads(prior)
    previous=old('content/native-localization.json')
    if manifest['catalog_sha256']!=hashlib.sha256(previous).hexdigest() or manifest['generator_sha256']!=hashlib.sha256(old('tools/source_fonts.py')).hexdigest():
        raise ValueError('Font baseline source identity mismatch')
    receipt=ROOT/path
    if json.loads(receipt.read_bytes())!=manifest:raise ValueError('Font baseline receipt differs; refuse unrelated edits')
    current=ROOT/'content/native-localization.json'
    candidate=catalogue_reuse(json.loads(previous),json.loads(current.read_bytes()),manifest,sha(current))
    candidate['generator_sha256']=sha(ROOT/'tools/source_fonts.py')
    candidate['catalogue_reuse']={'baseline_commit':base,'baseline_catalogue_sha256':manifest['catalog_sha256'],'required_glyph_sets_equal':True,'retained_original_atlas_priority':True,'admission_tool_sha256':sha(__file__)}
    # Existing glyph/table/page bytes must match the admitted baseline exactly.
    for row in [candidate['binary']]+candidate['pages']:
        raw=(ROOT/'romfs/fonts'/row['path']).read_bytes()
        expected_bytes=row.get('bytes',row.get('file_bytes'))
        if len(raw)!=expected_bytes or hashlib.sha256(raw).hexdigest()!=row['sha256']:
            raise ValueError('Font reuse binary/page differs from baseline')
        if 'crc32' in row and zlib.crc32(raw)!=row['crc32']:
            raise ValueError('Font reuse page CRC differs from baseline')
    receipt.write_bytes((json.dumps(candidate,ensure_ascii=False,indent=2)+'\n').encode('utf8'))
    try:stage_files(ROOT/'romfs') # unchanged original producer validates all source/header/page records
    except Exception:
        receipt.write_bytes(prior)
        raise
    print('Font catalogue re-admitted: identical required glyphs; checked font binary/pages retained')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--base',required=True);reuse_catalogue(p.parse_args().base)
