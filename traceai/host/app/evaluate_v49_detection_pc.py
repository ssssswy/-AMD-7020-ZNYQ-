"""Read-only current-model replay over sealed observed/label corpora."""
from pathlib import Path
import sys,json,hashlib,subprocess,os,collections,csv
import numpy as np
R=Path(__file__).resolve().parents[1];A=R.parent/'v4.6_ablation_20261027';O=R/'output/v49_detection_pc_20261030_002';O.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
sys.path.insert(0,str(A/'scripts'));import common
common.PROJECT=R
from common import save,run
from integer_model import BatchRom
raw=np.load(A/'datasets/raw.npz')['raw'];observed=[json.loads(s) for s in (A/'datasets/observed_config.jsonl').read_text().splitlines()];n=len(raw)
# Cached feature inputs are accepted only after exact production encoder identity checks.
fm=json.loads((A/'features/manifest.json').read_text())
for name in ('ai_global.c','ai_slots.c'):
 old=next(h for p,h in fm['source_inputs'].items() if p.endswith(name));assert sha(R/'software_v4_0/src'/name)==old
out=O
source=(A/'scripts/evaluate.py').read_text();code=source[source.index(' model=BatchRom();'):source.index(" timings={'uart_cnn_host_seconds'")]
code='\n'.join(s[1:] if s.startswith(' ') else s for s in code.splitlines())
# Rebind feature/compiled global-decision paths to sealed corpus; model uses current ROMs.
code=code.replace("ROOT/'features/uart.npz'","A/'features/uart.npz'").replace("ROOT/'features/reference.exe'","A/'features/reference.exe'")
import struct,time
ROOT=A
exec(code,globals())
# Execute maintained production edge detector with physical clock ticks.
s= (R/'tests/v46_edge_reference.c').read_text().replace('unsigned mode,map[4],cpol,cpha,cs,gl,ph,su,ho,n;','unsigned mode,map[4],cpol,cpha,cs,gl,ph,su,ho,n,period;')
s=s.replace('%u%llu",&mode','%u%llu%u",&mode').replace('&n,&base)==14','&n,&base,&period)==15').replace('put(base+i,level','put(base+(uint64_t)i*period,level').replace('put(base+n,previous','put(base+(uint64_t)n*period,previous')
(O/'edge_reference.c').write_text(s)
env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','')
cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-I'+str(R/'software_v4_0/include'),str(O/'edge_reference.c'),*[str(R/'software_v4_0/src'/f'{name}.c') for name in ('logic_trace_view','capture_vector_stream','ai_edge_review')],'-o',str(O/'edge.exe')]
v=subprocess.run(cmd,env=env,capture_output=True);(O/'compile.log').write_bytes(v.stdout+v.stderr);assert v.returncode==0,v.stderr
with (O/'edge_inputs.txt').open('w') as f:
 f.write(str(n)+'\n')
 for obs,w in zip(observed,raw):
  c=obs['config'];proto=c['protocol'];period=c['period_ticks'];mode={'uart':0,'spi':1,'i2c':2}[proto]
  budgets=[(100000000//c['rate_hz'])//8,0,0,0] if not mode else [c['glitch']*period,c['clock']*period,c['setup']*period if mode==1 else 0,c['hold']*period if mode==1 else 0]
  f.write(' '.join(map(str,[mode,0,1,2,3,c['cpol'],c['cpha'],0,*budgets,768,1000000000,period]))+'\n')
  packed=np.sum(w.astype(np.uint32)*(1<<np.arange(4))[:,None],axis=0);f.write(' '.join(map(str,packed))+'\n')
v=subprocess.run([str(O/'edge.exe'),str(O/'edge_inputs.txt')],env=env,capture_output=True);(O/'edge_events.txt').write_bytes(v.stdout);assert v.returncode==0,v.stderr
rule=np.zeros(n,bool);kinds=[set() for _ in range(n)]
for line in v.stdout.decode().splitlines():
 i,ch,kind,begin,end,limit=map(int,line.split());period=observed[i]['config']['period_ticks']
 if begin>=1000000000+512*period:rule[i]=1;kinds[i].add(kind)
truth=[json.loads(s) for s in (A/'datasets/truth.jsonl').read_text().splitlines()]
def metric(ids,p):
 pos=[i for i in ids if truth[i]['label']];neg=[i for i in ids if not truth[i]['label']];tp=sum(bool(p[i]) for i in pos);fp=sum(bool(p[i]) for i in neg)
 return dict(tp=tp,fn=len(pos)-tp,fp=fp,tn=len(neg)-fp,positive=len(pos),negative=len(neg),recall=tp/len(pos) if pos else None,fpr=fp/len(neg) if neg else None)
methods={'PL_rules':rule,'UART_CNN':pred,'OR':rule|pred,'AND_window':rule&pred};summary={};families=[]
for proto in ('uart','spi','i2c'):
 ids=[i for i,t in enumerate(truth) if t['protocol']==proto and t['split']!='unobservable_control'];ms=methods if proto=='uart' else {'PL_rules':rule}
 summary[proto]={name:metric(ids,p) for name,p in ms.items()}
 for family in sorted({truth[i]['paired_fault'] for i in ids}):
  ix=[i for i in ids if truth[i]['paired_fault']==family]
  for name,p in ms.items():families.append(dict(protocol=proto,family=family,method=name,**metric(ix,p)))
 summary[proto]['splits']={split:{name:metric([i for i in ids if truth[i]['split']==split],p) for name,p in ms.items()} for split in ('known','parameter_ood','novel_shape')}
np.savez_compressed(O/'predictions.npz',cnn=pred,cnn_kind=cnn_kind,rules=rule)
with (O/'per_type.csv').open('w',newline='',encoding='utf-8-sig') as f:
 writer=csv.DictWriter(f,fieldnames=list(families[0]));writer.writeheader();writer.writerows(families)
inputs={p.relative_to(R).as_posix():sha(p) for folder in ('model/v46_uart','model/r11','src/ai/v49_compare','src/ai/v46') for p in (R/folder).rglob('*') if p.is_file()}
inputs.update({f'software_v4_0/src/{name}.c':sha(R/'software_v4_0/src'/f'{name}.c') for name in ('ai_global','ai_slots','ai_edge_review')})
result=dict(passed=True,scope='PC INT8 ROM replay and maintained edge-rule C reference; sealed synthetic 20261027 corpus replay, not fresh independent data, not live DMA/coverage or board',summary=summary,per_type=families,source_inputs=inputs,corpus_sha256=sha(A/'datasets/raw.npz'),normal_definition='paired lawful controls per fault family',recall_definition='any alarm in target samples 512..767, not necessarily correct fault type',AND_definition='window-level intersection, not hardware same-window temporal-evidence trigger matching',programmed=False)
(O/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(summary,indent=2),flush=True)
