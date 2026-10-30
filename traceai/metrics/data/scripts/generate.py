"""Fresh paired tests. Transmitter truth is stored separately, never an input.
Known UART injection code is reused and disclosed; novel bus/shape rendering
below is independent. New seeds, parent groups and raw hashes are enforced.
"""
from common import *
from dataclasses import asdict
import importlib.util,math,sys
sys.path.insert(0,str(TRAIN/'uart_frame_identity_r11_20261024'))
from uai import data as ud
N=768;PREFIX=512
def runs(v):
 e=np.r_[0,np.flatnonzero(v[1:]!=v[:-1])+1,len(v)];return list(zip(e[:-1],e[1:]))
def digest(raw):return hashlib.sha256(np.asarray(raw,np.uint8).tobytes()).hexdigest()
def history():
 blocked=set();sources=[]
 for folder in TRAIN.iterdir():
  if not folder.is_dir() or folder.name.startswith('.'):continue
  for f in folder.rglob('*.jsonl'):
   sources.append(dict(path=str(f),sha256=sha(f)))
   for line in f.read_text(encoding='utf-8').splitlines():
    row=json.loads(line)
    for key in ('raw_sha256','clean_sha256'):
     if row.get(key):blocked.add(row[key])
 for prior in CFG.get('prior_test_datasets',[]):
  f=Path(prior)/'truth.jsonl';sources.append(dict(path=str(f),sha256=sha(f)))
  for line in f.read_text(encoding='utf-8').splitlines():
   row=json.loads(line);blocked.add(row['raw_sha256'])
 save(ROOT/'evidence/historical_exclusion.json',dict(files=sources,unique_hashes=len(blocked)))
 return blocked
def config(protocol,split,rng):
 ood=split=='parameter_ood'
 if protocol=='uart':
  rates=[6720,14400,74880,153600,307200,512000] if ood else [4800,9600,19200,115200,230400,921600]
  desired=float(rng.uniform(21.3,21.7) if ood else rng.uniform(10,23.8))
 else:
  rates=([333333,750000,1750000] if protocol=='spi' else [137000,271000,333000]) if ood else ([100000,250000,1000000,4000000] if protocol=='spi' else [100000,400000])
  desired=float(rng.uniform(25.3,25.7) if ood else rng.uniform(10,24))
 rate=int(rng.choice(rates));period=max(1,round(1e8/(rate*desired)));u=1e8/(period*rate)
 if not (4.5<=u<=28 if protocol=='uart' else 8<=u<=28):return None
 # The actual hardware samples at an integer number of 100MHz ticks.
 return dict(protocol=protocol,period_ticks=period,rate_hz=rate,sample_rate_hz=1e8/period,u=u,
  data_bits=int(rng.choice([7,8])),parity=str(rng.choice(['N','E','O'])),stop_bits=int(rng.choice([1,2])),
  cpol=int(rng.integers(2)),cpha=int(rng.integers(2)),word_bits=int(rng.choice([8,16])),
  setup=int(rng.choice([1,2])),hold=int(rng.choice([0,1])),glitch=max(2,int(.12*u)),clock=max(3,int(.30*u)),timeout=int(rng.choice([0,96,128])))
def uart_pair(c,family,rng,seed):
 if family=='parity_flip' and c['parity']=='N':c['parity']=str(rng.choice(['E','O']))
 cfg=ud.Config(expected_baud_hz=c['rate_hz'],sample_rate_hz=c['sample_rate_hz'],data_bits=c['data_bits'],parity=c['parity'],stop_bits=c['stop_bits'],
  tx_error=float(rng.uniform(-.015,.015)),jitter=float(rng.uniform(0,.01)),severity=float(rng.uniform(.35,1)),seed=seed,renderer='analytical')
 if family in ('glitch','mandatory_stop_low','parity_flip','baud_mismatch','edge_skew'):
  clean,raw,meta=ud.generate_pair(cfg,family);return clean[None],raw[None],dict(family=family,physical_audit=meta.get('audit_target_violation_samples',[]),injection=[meta['anomaly_start']+512,meta['anomaly_end']+512],kind=1 if family=='glitch' else 3)
 clean,frames=ud.base(cfg);raw=clean.copy();u=c['u']
 if family=='burst_chatter':
  sites=[(a,b) for a,b in runs(clean) if b-a>=12 and max(a+2,514)+8<min(b-1,766)]
  if not sites:raise ValueError('site')
  a,b=sites[int(rng.integers(len(sites)))];at=int(rng.integers(max(a+2,514),min(b-9,756)+1))
  for off in (0,3,6):raw[at+off:at+off+1]^=1
  span=[at,at+7];kind=1;audit=[at,at+3,at+6]
 elif family=='break_train':
  choices=[f for f in frames if 514<=f['start']<650 and any(role=='stop' and 512<=math.ceil((a+b)*.5)<768 for role,a,b in zip(f['roles'],f['bounds'][:-1],f['bounds'][1:]))]
  if not choices:raise ValueError('site')
  f=choices[int(rng.integers(len(choices)))];at=int(math.ceil(f['start']+.7*u));raw[at:]=0
  audit=[math.ceil((a+b)*.5) for role,a,b in zip(f['roles'],f['bounds'][:-1],f['bounds'][1:]) if role=='stop' and 512<=math.ceil((a+b)*.5)<768]
  if not audit or not all(raw[t]==0 for t in audit):raise ValueError('audit')
  span=[at,N];kind=3
 elif family in ('correlated_edge_jitter','mark_space_bias'):
  choices=[f for f in frames if 513<=f['start'] and f['end']<766]
  if not choices:raise ValueError('site')
  f=choices[int(rng.integers(len(choices)))];bounds=np.array(f['bounds']);new=bounds.copy();bits=f['bits']
  if family=='correlated_edge_jitter':new+=.5*u*np.sin(np.linspace(0,2*np.pi,len(bounds)))
  else:
   for j in range(1,len(bounds)-1):new[j]+=(.23*u if bits[j-1]==0 and bits[j]==1 else -.23*u if bits[j-1]==1 and bits[j]==0 else 0)
  if np.any(np.diff(new)<=0):raise ValueError('site')
  for j,bit in enumerate(bits):raw[np.arange(N)[(np.arange(N)>=new[j])&(np.arange(N)<new[j+1])]]=bit
  # Physical label audit uses TX anchor, never provided to a detector.
  violations=ud.observable_frame_violations(raw,cfg,f)
  if not violations:raise ValueError('unobservable')
  delta=np.flatnonzero(raw!=clean)
  if not len(delta):raise ValueError('unobservable')
  span=[int(delta[0]),int(delta[-1]+1)];kind=3;audit=violations
 else:raise ValueError(family)
 return clean[None],raw[None],dict(family=family,physical_audit=audit,injection=span,kind=kind)
def raster(events,initial):
 raw=np.empty((len(initial),N),np.uint8)
 for ch,(items,level) in enumerate(zip(events,initial)):
  items=sorted(items,key=lambda q:q[0]);times=np.array([v[0] for v in items]);values=np.array([level]+[v[1] for v in items],np.uint8)
  raw[ch]=values[np.searchsorted(times,np.arange(N),side='right')]
 return raw
def bus_normal(c,rng):
 p=c['u'];spi=c['protocol']=='spi';ev=[[] for _ in range(4 if spi else 2)];initial=[c['cpol'],1,0,1] if spi else [1,1]
 transactions=[];cells=[];t=-float(rng.uniform(0,30))*p
 while t<1000:
  start=t;events=[]
  if spi:
   ev[1].append((t,0));bs=t+.35*p;n=c['word_bits'];payload=rng.integers(0,2,(2,n))
   if rng.random()<.25:payload[:]=int(rng.integers(2))
   for j in range(n):
    period=p*float(rng.uniform(.99,1.01));duty=float(rng.uniform(.4,.6));lead=bs+duty*period;trail=bs+period
    ev[0].extend(((lead,1-c['cpol']),(trail,c['cpol'])))
    change=bs+(.05 if c['cpha']==0 else .55)*period
    for line in range(2):ev[line+2].append((change,int(payload[line,j])))
    sample=trail if c['cpha'] else lead;cells.append(dict(sample=sample,bit=j,start=start))
    events.append(sample);bs=trail
    if rng.random()<.08:bs+=p*float(rng.uniform(1,4)) # legal selected SPI pause
   t=bs+.35*p;ev[1].append((t,1));transactions.append((start,t,events));gap=p*float(rng.uniform(.8,5))
   if gap>2*p and rng.random()<.5:ev[0].extend(((t+.4*p,1-c['cpol']),(t+.7*gap,c['cpol'])))
   t+=gap
  else:
   ev[1].append((t,0));t+=.6*p;ev[0].append((t,0));events=[]
   # Full address/data byte followed by ACK/NACK; neither NACK value is a fault.
   count=int(rng.choice([1,2]));j=0
   for byte in range(count):
    value=int(rng.integers(256));bits=[(value>>k)&1 for k in range(7,-1,-1)]+[int(rng.integers(2))]
    for bit in bits:
     stretch=float(rng.uniform(.3,2))*p if rng.random()<.10 else 0.
     if c['timeout']:stretch=min(stretch,max(0,c['timeout']*.4-.5*p))
     rise=t+.5*p+stretch;fall=rise+.5*p;ev[1].append((t+.15*p,bit));ev[0].extend(((rise,1),(fall,0)))
     cells.append(dict(sample=rise,bit=j%9,start=start));events.append(rise);j+=1;t=fall
    if byte<count-1 and rng.random()<.35:
     ev[1].append((t+.2*p,1));ev[0].append((t+.6*p,1));ev[1].append((t+1.1*p,0));t+=1.7*p;ev[0].append((t,0));j=0
   ev[1].append((t+.2*p,0));ev[0].append((t+.6*p,1));ev[1].append((t+1.1*p,1));t+=1.1*p
   transactions.append((start,t,events));t+=p*float(rng.uniform(2,5))
 return raster(ev,initial),transactions,cells
def bus_pair(c,family,rng):
 clean,tx,cells=bus_normal(c,rng);raw=clean.copy();spi=c['protocol']=='spi';p=c['u'];active=np.zeros(N,bool)
 for a,b,_ in tx:active[(np.arange(N)>=a)&(np.arange(N)<b)]=1
 def choose_site(lines,minimum=5):
  sites=[]
  for line in lines:
   for a,b in runs(clean[line]):
    lo=max(a+2,514);hi=min(b-minimum-1,760-minimum)
    if lo<=hi and active[lo:hi+minimum+1].all():sites.append((line,lo,hi))
  if not sites:raise ValueError('site')
  line,lo,hi=sites[int(rng.integers(len(sites)))];return line,int(rng.integers(lo,hi+1))
 if family=='glitch':
  line,at=choose_site([0,2,3] if spi else [0]);width=max(1,c['glitch']-1);raw[line,at:at+width]^=1;span=[at,at+width];kind=1
 elif family=='short_clock_phase':
  sites=[(a,b) for a,b in runs(clean[0]) if a>=514 and b<766 and b-a>=c['clock']+1 and active[a-1:b+1].all()]
  if not sites:raise ValueError('site')
  a,b=sites[int(rng.integers(len(sites)))];width=max(c['glitch'],c['clock']-1)
  if width>=c['clock']:raise ValueError('site')
  raw[0,a+width:b]=1-clean[0,a];span=[a,a+width];kind=2
 elif family in ('incomplete_word','early_stop','cs_rebound','restart_bounce'):
  sites=[]
  for cell in cells:
   at=math.ceil(cell['sample']+.15*p)
   if not 514<=at<754 or cell['start']<0:continue
   if spi and (cell['bit']+1)%c['word_bits'] and clean[1,at]==0:sites.append((at,cell))
   if not spi and cell['bit'] not in (0,8) and clean[0,at-1] and clean[0,at] and not clean[1,at-1] and not clean[1,at]:sites.append((at,cell))
  if not sites:raise ValueError('site')
  at,cell=sites[int(rng.integers(len(sites)))];end=next(b for a,b,_ in tx if a==cell['start'])
  if family in ('cs_rebound','restart_bounce'):
   width=2;raw[1,at:at+width]=1;span=[at,at+width]
  else:raw[1,at:min(N,math.ceil(end))]=1;span=[at,min(N,math.ceil(end))]
  kind=3
 elif family in ('multiline_burst','clock_chatter'):
  line,at=choose_site([2,3] if spi and family=='multiline_burst' else [0],minimum=10)
  for off in (0,3,6):raw[line,at+off]^=1
  if family=='multiline_burst':
   second=3 if line==2 else 2
   if not spi:second=1
   for off in (0,3,6):raw[second,at+off]^=1
  span=[at,at+7];kind=1
 else:raise ValueError(family)
 if np.array_equal(raw,clean):raise ValueError('unobservable')
 return clean,raw,dict(family=family,kind=kind,injection=span,physical_audit='scheduled selected transaction + explicit closed runt or mid-word termination; truth not supplied to detectors')
def main():
 freeze();out=ROOT/'datasets';out.mkdir(exist_ok=False);blocked=history();used=set();rng=np.random.default_rng(CFG['seed']);raws=[];rows=[];rejected={};pairs=[]
 known={'uart':['glitch','mandatory_stop_low','parity_flip','baud_mismatch','edge_skew'],'spi':['glitch','short_clock_phase','incomplete_word'],'i2c':['glitch','short_clock_phase','early_stop']}
 novel={'uart':['burst_chatter','break_train','correlated_edge_jitter','mark_space_bias'],'spi':['multiline_burst','clock_chatter','cs_rebound'],'i2c':['multiline_burst','clock_chatter','restart_bounce']}
 for protocol in CFG['protocols']:
  for split in CFG['splits']:
   families=novel[protocol] if split=='novel_shape' else known[protocol]
   for i in range(CFG['pairs_per_protocol_split']):
    family=families[i%len(families)];success=False
    for trial in range(1000):
     c=config(protocol,split,rng)
     if c is None:continue
     seed=int(rng.integers(2**61))
     try:clean,bad,truth=uart_pair(c,family,rng,seed) if protocol=='uart' else bus_pair(c,family,rng)
     except ValueError as e:rejected[str(e)]=rejected.get(str(e),0)+1;continue
     h0,h1=digest(clean),digest(bad)
     if h0==h1 or h0 in blocked or h1 in blocked or h0 in used or h1 in used:rejected['duplicate_or_historical']=rejected.get('duplicate_or_historical',0)+1;continue
     group=f'{protocol}_{split}_{i:04d}';used.update((h0,h1));pair=[]
     for label,wave,h in ((0,clean,h0),(1,bad,h1)):
      packed=np.zeros((4,N),np.uint8);packed[:len(wave)]=wave;idx=len(rows);pair.append(idx);raws.append(packed)
      rows.append(dict(index=idx,group=group,protocol=protocol,split=split,label=label,family=family if label else 'legal_paired_control',paired_fault=family,
       config=c.copy(),seed=seed,raw_sha256=h,truth=truth if label else dict(normal_contract='legal transmitter tolerance, payload, gap/CS pause/ACK/NACK/stretch')))
     pairs.append(pair);success=True;break
    assert success,(protocol,split,family,'No eligible physical case')
   print('GENERATED',protocol,split,len(rows),flush=True)
 # Blinded semantics control: entire data bit flipped without parity; still a
 # legal UART payload. Both arms must see it as observable-normal, not missing.
 for i in range(150):
  for trial in range(1000):
   c=config('uart','known',rng)
   if c is None:continue
   c['parity']='N';cfg=ud.Config(expected_baud_hz=c['rate_hz'],sample_rate_hz=c['sample_rate_hz'],data_bits=c['data_bits'],parity='N',stop_bits=c['stop_bits'],seed=int(rng.integers(2**61)),renderer='analytical')
   clean,frames=ud.base(cfg);choices=[(f,j) for f in frames for j,role in enumerate(f['roles']) if role=='data' and 514<=f['bounds'][j]<f['bounds'][j+1]<766]
   if not choices:continue
   f,j=choices[int(rng.integers(len(choices)))];changed=clean.copy();a,b=f['bounds'][j:j+2];changed[(np.arange(N)>=a)&(np.arange(N)<b)]^=1;h=digest(changed[None])
   if h in used or h in blocked:continue
   used.add(h);packed=np.zeros((4,N),np.uint8);packed[0]=changed;raws.append(packed);rows.append(dict(index=len(rows),group=f'blind_{i}',protocol='uart',split='unobservable_control',label=0,family='legal_8N1_payload_change',paired_fault='none',config=c,seed=cfg.seed,raw_sha256=h,truth=dict(reason='no payload expectation and no parity; frame remains legal')));break
  else:raise AssertionError('blind case')
 np.savez_compressed(out/'raw.npz',raw=np.stack(raws));(out/'truth.jsonl').write_text('\n'.join(json.dumps(r,ensure_ascii=False,default=json_default) for r in rows)+'\n',encoding='utf-8')
 (out/'observed_config.jsonl').write_text('\n'.join(json.dumps(dict(index=r['index'],config=r['config'])) for r in rows)+'\n')
 save(out/'manifest.json',dict(windows=len(rows),pair_count=len(pairs),positives=sum(r['label'] for r in rows),raw_sha256=sha(out/'raw.npz'),truth_sha256=sha(out/'truth.jsonl'),config_sha256=sha(out/'observed_config.jsonl'),rejected_before_inference=rejected,pair_indices=pairs,all_test_hashes_new=True,seed=CFG['seed'],fpga_modified=False));print('DATASET_FROZEN',len(rows),flush=True)
if __name__=='__main__':main()
