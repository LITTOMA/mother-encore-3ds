#!/usr/bin/env python3
"""Summarize a completed isolated ARM/PICA numeric run without image output."""
import argparse,hashlib,json,re,shutil,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--log',type=Path,required=True);p.add_argument('--out',type=Path,default=ROOT/'reports/world-effect-arm');a=p.parse_args();text=a.log.read_text();assert 'WORLD_EFFECT_ARM_REFERENCE_V1' in text and 'RESULT ' in text,'Run not complete'
 frames=[]
 for line in text.splitlines():
  if line.startswith('CASE '):frames.append({k:float(v)if any(c in v for c in '.e')else int(v)for k,v in re.findall(r'([a-z_]+)=([-+0-9.e]+)',line)})
 groups={}
 for width in [320,400]:
  f=[r for r in frames if r['w']==width];active=[r for r in f if r['alpha']>0]
  groups[str(width)]={'frames':len(f),'active_frames':len(active),'times_ms':{k:{'min':min(r[k]for r in active),'median':statistics.median(r[k]for r in active),'max':max(r[k]for r in active)}for k in ['standalone_ms','kernel_ms','upload_ms','submit_ms','finish_ms','wait_ms','gpu_queue_ms','readback_ms','invalidate_ms']}if active else {}}
 build=json.loads((a.out/'build.json').read_text());binary=Path(build['binary']);actual_sha=hashlib.sha256(binary.read_bytes()).hexdigest();assert actual_sha==build['binary_sha256'],'Launched artifact changed'
 passed='RESULT PASS ' in text and len(frames)==60 and len({r['index']for r in frames})==20 and all(r['cpu_mismatch']==r['gpu_mismatch']==r['max_channel_delta']==0 for r in frames)and text.count('CAL_RESULT exact_matches=1 ')==2
 result={'schema':1,'passed':passed,'binary_sha256':actual_sha,'observed_source_cases':len({r['index']for r in frames}),'observed_draws':len(frames),'expected_cpu_pixels':1459200,'expected_gpu_pixels':4377600,'cpu_mismatch_frames':sum(bool(r['cpu_mismatch'])for r in frames),'gpu_mismatch_frames':sum(bool(r['gpu_mismatch'])for r in frames),'maximum_channel_delta':max((r['max_channel_delta']for r in frames),default=0),'groups':groups,'frames':frames,'scope':'Actual ARM execution and actual RGBA8 C2D blend/readback under cloud Azahar. Same20 source cases,3 draws each. Numeric transfer only; no screenshot or image emission. Emulator timing is not physical3DS performance; no universal tint/time parity claim.'}
 a.out.mkdir(exist_ok=True,parents=True);shutil.copyfile(a.log,a.out/'native.log');(a.out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:result[k]for k in ['passed','observed_source_cases','observed_draws','cpu_mismatch_frames','gpu_mismatch_frames','maximum_channel_delta','groups']},indent=2))
if __name__=='__main__':main()
