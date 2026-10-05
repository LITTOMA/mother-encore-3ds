#!/usr/bin/env python3
"""Independent ENCSTG01 checked binary compiler; no JSON console consumer."""
import argparse,hashlib,math,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.drawer_program import fields,safe_path,write_json,digest
from tools.extract_battle_entry import PIN,require
from tools.storage_assets import IR,ROLES,PARAMETERS,BINDINGS,load,verify_receipt
PACK='romfs/data/opening.encstorage'
NIL=0xffffffff
NAMES=['Strings','Policies','Parameters','Bindings','Resources','Layouts','Equipment']
FORMATS=[None,'<6I','<If','<2I','<7I32s','<7I10f4I','<I7i']
STRIDES=[1]+[struct.calcsize(f) for f in FORMATS[1:]]
HEADER=64+16*len(NAMES)
KINDS=['Container','Sprite','Rectangle','NinePatch','Text']

def lower(ir,receipt):
    fields(ir,('schema','kind','commit','scope','sources','dependencies','parameters','bindings','policies','equipment','translations','resources','layouts'),'Storage IR')
    require(type(ir['schema']) is int and ir['schema']==1 and ir['kind']=='encore.native-storage.source-ir' and ir['commit']==PIN,'Storage schema/pin')
    fields(ir['parameters'],PARAMETERS,'Storage parameters');fields(ir['bindings'],BINDINGS,'Storage bindings')
    pool=bytearray(b'\0');offsets={'':0};out={n:[] for n in NAMES}
    def string(v):
        require(type(v) is str and '\0' not in v and len(v.encode())<=4096,'Storage string')
        if v not in offsets:offsets[v]=len(pool);pool.extend(v.encode()+b'\0')
        return offsets[v]
    for p in ir['policies']:
        fields(p,('definition_id','source_item','doses','max_count','sort_rank_en','sort_rank_zh'),'Storage policy')
        # IR retains stable definition identity; the pack addresses the checked
        # immutable native Items prefix (id1/id2 -> index0/index1).
        require(p['definition_id'] in (1,2),'Storage stable Items identity')
        out['Policies'].append([p['definition_id']-1,string(p['source_item']),p['doses'],p['max_count'],p['sort_rank_en'],p['sort_rank_zh']])
    out['Parameters']=[[i+1,ir['parameters'][name]] for i,name in enumerate(PARAMETERS)]
    out['Bindings']=[[i+1,string(ir['bindings'][name])] for i,name in enumerate(BINDINGS)]
    for r in ir['resources']:
        fields(r,('id','name','source','size','crop','grid','output'),'Storage asset')
        a=receipt['outputs'][r['output']];size=a['size']
        out['Resources'].append([r['id'],string(r['output']),1,*size,*r['grid'],bytes.fromhex(a['sha256'])])
    for l in ir['layouts']:
        fields(l,('id','role','parent','kind','resource','frame','flags','anchor','rect','color','patch'),'Storage layout')
        require(l['role'] in ROLES and l['kind'] in KINDS,'Storage layout enum')
        out['Layouts'].append([l['id'],ROLES.index(l['role'])+1,NIL if l['parent'] is None else l['parent'],KINDS.index(l['kind'])+1,NIL if l['resource'] is None else l['resource'],l['frame'],l['flags'],*l['anchor'],*l['rect'],*l['color'],*l['patch']])
    for e in ir['equipment']:
        fields(e,('definition_id','boosts'),'Storage equipment');require(len(e['boosts'])==7,'Storage equipment stats')
        require(e['definition_id'] in (1,2),'Storage equipment stable identity');out['Equipment'].append([e['definition_id']-1,*e['boosts']])
    out['Strings']=bytes(pool);validate(out);return out

def encode(t,pin=PIN):
    validate(t);require(pin==PIN,'Unreviewed Storage pin')
    blob=bytearray(HEADER);struct.pack_into('<8s6I20s12x',blob,0,b'ENCSTG01',1,1,0,1,1,7,bytes.fromhex(pin))
    for i,n in enumerate(NAMES):
        while len(blob)%4:blob.append(0)
        data=t[n] if i==0 else b''.join(struct.pack(FORMATS[i],*r) for r in t[n])
        struct.pack_into('<4I',blob,64+i*16,i+1,len(blob),len(t[n]),STRIDES[i]);blob.extend(data)
    struct.pack_into('<I',blob,12,len(blob));struct.pack_into('<I',blob,16,zlib.crc32(blob)&NIL);return bytes(blob)

def parse_pack(blob):
    require(isinstance(blob,(bytes,bytearray)) and HEADER<=len(blob)<=8*1024*1024,'Storage binary size')
    magic,schema,size,crc,caps,rules,count,pin=struct.unpack_from('<8s6I20s',blob)
    require(magic==b'ENCSTG01' and schema==caps==rules==1 and count==7 and size==len(blob) and pin.hex()==PIN and not any(blob[52:64]),'Storage header/version/pin')
    copy=bytearray(blob);copy[16:20]=b'\0'*4;require(zlib.crc32(copy)&NIL==crc,'Storage CRC')
    out={};end=HEADER
    for i,n in enumerate(NAMES):
        kind,start,num,stride=struct.unpack_from('<4I',blob,64+i*16)
        require(kind==i+1 and stride==STRIDES[i] and start==(end+3)//4*4 and not any(blob[end:start]) and start+num*stride<=len(blob),'Storage section directory')
        data=blob[start:start+num*stride];out[n]=bytes(data) if i==0 else [list(struct.unpack_from(FORMATS[i],data,j*stride)) for j in range(num)];end=start+num*stride
    require(end==len(blob),'Storage trailing bytes');validate(out);return out

def validate(t):
    fields(t,NAMES,'Storage sections');pool=t['Strings'];require(isinstance(pool,bytes) and 0<len(pool)<=1024*1024 and pool[0]==pool[-1]==0,'Storage string pool')
    starts=set();off=0
    while off<len(pool):
        starts.add(off);end=pool.find(b'\0',off);require(end>=off,'Storage unterminated string');pool[off:end].decode('utf-8');off=end+1
    def string(index):
        require(type(index) is int and index in starts,'Storage string reference');return pool[index:pool.index(0,index)].decode('utf-8')
    def integer(v,low=0,high=NIL):return type(v) is int and low<=v<=high
    def finite(v):return type(v) in (int,float) and math.isfinite(v)
    require(0<len(t['Policies'])<=16 and len(t['Equipment'])==len(t['Policies']) and len(t['Resources'])<=64 and len(t['Layouts'])<=128,'Storage section capacity')
    ids=set();sources=set();scores=[set(),set()]
    for p in t['Policies']:
        require(len(p)==6 and integer(p[0],0) and p[0] not in ids and safe_path(string(p[1])) and string(p[1]) not in sources and integer(p[2],1,65535) and p[3]==1 and integer(p[4],1,2000000) and integer(p[5],1,2000000),'Storage policy')
        ids.add(p[0]);sources.add(string(p[1]))
        for i in range(2):require(p[4+i] not in scores[i],'Storage unsupported equal-score sort');scores[i].add(p[4+i])
    params={}
    for p in t['Parameters']:
        require(len(p)==2 and integer(p[0],1,len(PARAMETERS)) and p[0] not in params and finite(p[1]) and abs(p[1])<=2000000,'Storage parameter');params[p[0]]=p[1]
    require(set(params)==set(range(1,len(PARAMETERS)+1)),'Storage missing parameter')
    require(1<=params[1]<=64 and params[1]==int(params[1]) and 1<=params[2]<=64 and params[2]==int(params[2]) and 1<=params[3]<=16 and params[3]==int(params[3]) and 1<=params[4]<=64 and 0<params[5]<=30 and -2000000<=params[6]<0 and params[7] in (0,1) and 0<params[8]<=10 and all(0<params[i]<=8192 for i in (9,10,11,12,15,16)) and all(abs(params[i])<=128 for i in (13,14)),'Storage parameter ranges')
    require(0<params[17]<=120 and all(0<=params[i]<3 and params[i]==int(params[i]) for i in range(18,22)) and 0<params[22]<=10 and all(0<params[i]<=100 for i in range(23,30)) and 0<params[30]<=128,'Storage cursor/stat/font parameter ranges')
    require(-128<=params[31]<=128 and params[31]==int(params[31]),'Storage counter character spacing')
    bindings={}
    for b in t['Bindings']:
        require(len(b)==2 and integer(b[0],1,len(BINDINGS)) and b[0] not in bindings and string(b[1]),'Storage binding');bindings[b[0]]=string(b[1])
    require(set(bindings)==set(range(1,len(BINDINGS)+1)),'Storage missing bindings')
    for i,s in bindings.items():
        if i in (15,16,17,18,19,20,22,23):require(safe_path(s),'Storage source path binding')
        elif i==21:require(s.count('%s')==2 and s.replace('%s','').find('%')<0,'Storage counter template')
        else:require('[' not in s and ']' not in s and ('{' not in s.replace('{item}','')) and '}' not in s.replace('{item}',''),'Storage text controls')
    seen=set()
    for r in t['Resources']:
        require(len(r)==8 and integer(r[0],1) and r[0] not in seen and safe_path(string(r[1])) and string(r[1]).startswith('graphics/') and string(r[1]).endswith('.t3x') and r[2]==1 and all(integer(v,1,8192) for v in r[3:7]) and r[3]%r[5]==r[4]%r[6]==0 and isinstance(r[7],bytes) and len(r[7])==32 and any(r[7]),'Storage resource');seen.add(r[0])
    roles=set();seen=set()
    for i,l in enumerate(t['Layouts']):
        require(len(l)==21 and integer(l[0],1) and l[0] not in seen and integer(l[1],1,len(ROLES)) and (l[2]==NIL or integer(l[2],0,i-1)) and integer(l[3],1,5) and l[6]==0 and all(finite(v) and abs(v)<=8192 for v in l[7:17]) and all(0<=v<=1 for v in l[7:9]+l[13:17]) and l[11]>0 and l[12]>0 and all(integer(v,0,8192) for v in l[17:21]),'Storage layout');seen.add(l[0]);roles.add(l[1])
        if l[3] in (2,4):
            require(integer(l[4],0,len(t['Resources'])-1),'Storage layout resource');r=t['Resources'][l[4]]
            require(integer(l[5],0,r[5]*r[6]-1),'Storage layout frame')
            if l[3]==4:require(l[17]+l[19]<=r[3]//r[5] and l[18]+l[20]<=r[4]//r[6],'Storage ninepatch')
        else:require(l[4]==NIL and l[5]==0,'Storage unused layout resource')
        require(l[3]==4 or not any(l[17:21]),'Storage unused patch')
        if l[1] in (8,9,15,16,20,21):require(l[2]!=NIL and t['Layouts'][l[2]][1] in (3,4),'Storage pane ownership')
    require(all(i in roles for i in (2,3,4,5,6,7,8,9,10,11,12,13,15,17,18,19,20,21)),'Storage required layout roles')
    seen=set()
    for e in t['Equipment']:
        require(len(e)==8 and e[0] in ids and e[0] not in seen and all(integer(v,0,65535) for v in e[1:]),'Storage equipment');seen.add(e[0])

def stage_files(source):
    source=Path(source);blob=(source/'data/opening.encstorage').read_bytes();t=parse_pack(blob);pool=t['Strings'];out={Path('data/opening.encstorage'):blob}
    for r in t['Resources']:
        path=pool[r[1]:pool.index(0,r[1])].decode();data=(source/path).read_bytes();require(hashlib.sha256(data).digest()==r[7],'Storage staged texture hash');out[Path(path)]=data
    return out

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['compile','verify']);args=p.parse_args()
    try:
        ir=load(ROOT);blob=encode(lower(ir,verify_receipt(ROOT)));parse_pack(blob)
        if args.action=='compile':target=ROOT/PACK;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
        else:require((ROOT/PACK).read_bytes()==blob,'Stale Storage binary')
        print('Storage binary:',len(blob),'bytes; UID transfer and equipment data')
    except (ValueError,KeyError,TypeError,OSError,struct.error,OverflowError) as error:print('Storage binary rejected:',error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
