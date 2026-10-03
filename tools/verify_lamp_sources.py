#!/usr/bin/env python3
"""Build-time fingerprints for reviewed lamp command/action source boundaries."""
from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    root=ROOT/'upstream/MOTHER-Encore';lock=json.loads((ROOT/'upstream.lock').read_text())
    def git(*args):return subprocess.check_output(['git','-C',str(root),*args],text=True,stderr=subprocess.PIPE).strip()
    if git('rev-parse','HEAD')!=lock['commit'] or git('status','--porcelain'):raise ValueError('Requires pristine pinned upstream')
    sources={}
    for name in ('lamp-dialogue-v0410.json','actor-actions-v0410.json','lamp-world-v0410.json','cutscene-camera-v0410.json'):
        review=json.loads((ROOT/'compatibility/reviews'/name).read_text())
        if review.get('schema')!=1 or review.get('commit')!=lock['commit'] or review.get('game_version')!=lock['game_version']:raise ValueError('Review differs from current source lock')
        for path,wanted in review['sources'].items():
            if path in sources and sources[path]!=wanted:raise ValueError('Conflicting source review')
            sources[path]=wanted
    for path,wanted in sources.items():
        p=Path(path)
        if p.is_absolute() or '..'in p.parts:raise ValueError('Unsafe source path')
        if hashlib.sha256((root/p).read_bytes()).hexdigest()!=wanted:raise ValueError('Changed source needs semantic review: '+path)
    print('Verified',len(sources),'lamp command/action source boundaries; no whole-game compatibility claim')
if __name__=='__main__':main()
