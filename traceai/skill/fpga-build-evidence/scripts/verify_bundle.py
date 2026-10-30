"""Verify explicit local artifact evidence. Never builds, programs, or runs a target."""
from pathlib import Path
import argparse,hashlib,json,math,re,sys,zipfile

def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def verify(root,manifest=None,bit=None,xsa=None,status=None,minimum_wns=0.0,minimum_whs=0.0):
 root=Path(root).resolve();checks=[]
 if not root.is_dir():raise ValueError('Artifact root is not a directory')
 def record(name,passed,detail):checks.append({'check':name,'passed':bool(passed),'detail':detail})
 if manifest:
  data=json.loads(Path(manifest).read_text(encoding='utf-8-sig'));items=data.get('artifacts')
  if not isinstance(items,dict) or not items:raise ValueError('Manifest needs a nonempty artifacts map')
  for name,want in items.items():
   target=(root/name).resolve()
   if Path(name).is_absolute() or not target.is_relative_to(root):
    record('manifest_path',False,name);continue
   if not isinstance(want,str) or not re.fullmatch(r'[0-9a-fA-F]{64}',want):
    record('manifest_digest',False,name);continue
   actual=digest(target) if target.is_file() else None
   record('artifact_sha256',actual==want.lower(),{'path':name,'expected':want.lower(),'actual':actual})
 if bool(bit)!=bool(xsa):raise ValueError('Supply both BIT and XSA or neither')
 if bit:
  with zipfile.ZipFile(xsa) as z:
   members=[n for n in z.namelist() if n.lower().endswith('.bit')]
   if len(members)!=1:record('xsa_bit_count',False,len(members))
   else:
    inside=hashlib.sha256(z.read(members[0])).hexdigest();outside=digest(Path(bit))
    record('xsa_bit_pair',inside==outside,{'member':members[0],'embedded':inside,'external':outside})
 if status:
  data=json.loads(Path(status).read_text(encoding='utf-8-sig'))
  for name,bound in [('wns_ns',minimum_wns),('whs_ns',minimum_whs)]:
   value=data.get(name);valid=isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value)
   record(name,valid and value>=bound,{'value':value,'minimum':bound})
  for name in ('drc_errors','drc_critical'):
   value=data.get(name);record(name,isinstance(value,int) and not isinstance(value,bool) and value==0,{'value':value,'required':0})
 if not checks:raise ValueError('No evidence requested')
 return {'passed':all(x['passed'] for x in checks),'scope':'Supplied file identity, XSA/BIT byte pairing and supplied timing/DRC metrics only',
  'inputs':{'root':str(root),'manifest':str(manifest) if manifest else None,'bit':str(bit) if bit else None,'xsa':str(xsa) if xsa else None,'status':str(status) if status else None},
  'checks':checks,'programmed':False,'board_accepted':False,'limits':['Does not verify source-to-bit provenance without a separately reviewed source manifest.',
   'Does not validate external timing constraints, CDC, RTL behavior, platform/BSP/ELF ownership, or physical hardware.']}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--manifest',type=Path);p.add_argument('--bit',type=Path);p.add_argument('--xsa',type=Path);p.add_argument('--status',type=Path);p.add_argument('--minimum-wns',type=float,default=0.0);p.add_argument('--minimum-whs',type=float,default=0.0);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 try:result=verify(a.root,a.manifest,a.bit,a.xsa,a.status,a.minimum_wns,a.minimum_whs)
 except (ValueError,OSError,KeyError,zipfile.BadZipFile) as e:result={'passed':False,'error':str(e),'scope':'Evidence parse failed','programmed':False,'board_accepted':False}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print('EVIDENCE_CHECK_'+('PASS' if result['passed'] else 'FAIL')+'; NOT BOARD ACCEPTANCE');return 0 if result['passed'] else 1
if __name__=='__main__':sys.exit(main())
