from pathlib import Path
p=Path(__file__).resolve().parents[2]/'tools/native_content.py';s=p.read_bytes().decode('utf8');old='in(8,9,10,11,12,13)';assert s.count(old)==2;s=s.replace(old,'in(8,9,10,11,12,13,14)');p.write_bytes(s.encode('utf8'));print('Both IR and packed capability readers extended to14; older13 opcode49 still rejected')
