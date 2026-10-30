"""One frozen evaluation; no threshold fitting or model selection."""
from common import *
from integer_model import BatchRom
from rules import edge_rules,strong_rules
import struct,time,csv
def wilson(k,n):
 if not n:return [None,None]
 p=k/n;z=1.959963984540054;d=1+z*z/n;center=(p+z*z/(2*n))/d;half=z*((p*(1-p)/n+z*z/(4*n*n))**.5)/d
 return [max(0.,center-half),min(1.,center+half)]
def stats(y,p):
 y=np.asarray(y,bool);p=np.asarray(p,bool);tp=int(np.sum(y&p));fn=int(np.sum(y&~p));fp=int(np.sum(~y&p));tn=int(np.sum(~y&~p))
 return dict(tp=tp,fn=fn,fp=fp,tn=tn,anomalies=tp+fn,normal=fp+tn,recall=tp/(tp+fn) if tp+fn else None,fpr=fp/(fp+tn) if fp+tn else None,
  recall_ci95=wilson(tp,tp+fn),fpr_ci95=wilson(fp,fp+tn))
def paired(a,b):
 # a-b paired bootstrap, exact empirical {-1,0,+1} distribution.
 d=np.asarray(a,np.int8)-np.asarray(b,np.int8);n=len(d)
 if not n:return None
 counts=np.array([(d==-1).sum(),(d==0).sum(),(d==1).sum()]);rng=np.random.default_rng(202610270462);boot=rng.multinomial(n,counts/n,size=10000);ci=np.quantile((boot[:,2]-boot[:,0])/n,[.025,.975])
 discord=int(counts[0]+counts[2]);k=int(min(counts[0],counts[2]));p=min(1.,2*sum(math.exp(math.lgamma(discord+1)-math.lgamma(j+1)-math.lgamma(discord-j+1)-discord*math.log(2)) for j in range(k+1))) if discord else 1.
 return dict(gain_pp=float(d.mean()*100),ci95_pp=(ci*100).tolist(),a_only=int(counts[2]),b_only=int(counts[0]),mcnemar_exact_p=p)
import math
def main():
 freeze();out=ROOT/'results';out.mkdir(exist_ok=False)
 for f in ('integer_batch_selftest.json','freeze.json'):assert (ROOT/'evidence'/f).exists()
 observed=[json.loads(line) for line in (ROOT/'datasets/observed_config.jsonl').read_text().splitlines()];raw=np.load(ROOT/'datasets/raw.npz')['raw'];n=len(raw)
 # Freeze evaluated code before any scores are computed.
 save(out/'evaluation_inputs.json',dict(scripts={str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'scripts').iterdir() if p.is_file()},dataset=json.loads((ROOT/'datasets/manifest.json').read_text()),no_retuning=True))
 model=BatchRom();pred=np.zeros(n,bool);cnn_kind=np.zeros(n,np.int8);cnn_source=np.zeros(n,np.int8);cnn_score=np.zeros(n,np.int64);cnn_cut=np.zeros(n,np.int64);cnn_position=np.full(n,np.nan);clocal=np.zeros(n,np.int32)
 d=np.load(ROOT/'features/uart.npz');ids=d['global_ids'];start=time.perf_counter();gz=model.infer(0,d['global_x']);save(out/'uart_global_logits.json',dict(count=len(gz),model=0))
 inputfile=out/'global_logits.bin';inputfile.write_bytes(struct.pack('<I',len(gz))+gz.astype('<i4').tobytes());run([ROOT/'features/reference.exe','global',inputfile,out/'global_decisions.bin'],out/'global_decision.log')
 gb=(out/'global_decisions.bin').read_bytes();assert struct.unpack_from('<I',gb)[0]==len(ids);gd=np.frombuffer(gb[4:],'<i4').reshape(-1,2)
 pred[ids]=gd[:,0]!=0;cnn_source[ids]=2;cnn_kind[ids]=np.where(gd[:,0]==1,3,np.where(gd[:,0]==3,4,0));cnn_score[ids]=gd[:,1];cnn_cut[ids]=607
 az=model.infer(2,d['local_x']);am=az[:,1].astype(np.int64)-az[:,0].astype(np.int64);ti=d['token_ids'];roles=d['role'];clocal[ids]=np.bincount(ti,minlength=n)[ids]
 margins=np.full(len(ti),-2**31,np.int64);cuts=np.where(roles==1,1920,9856);heads=np.where(roles==1,1,2)
 ix=np.flatnonzero(roles==1)
 if len(ix):
  z=model.infer(1,d['local_x'][ix]).astype(np.int64);margins[ix]=(z[:,1]-np.max(z[:,[0,2,3,4]],axis=1))*8+np.minimum(am[ix],0)
 ix=np.flatnonzero(roles==2)
 if len(ix):
  sx=d['stop_x'][ix].copy().reshape(-1,6,64);sx[:,3,:]=((np.clip(am[ix],-16384,16384)+16384+256)//512)[:,None]
  z=model.infer(3,sx).astype(np.int64);margins[ix]=(z[:,1]-z[:,0])*2+np.minimum(am[ix],0)
 global_hit=pred.copy();best=np.full(n,-1,np.int64)
 for j,w in enumerate(ti):
  if margins[j]>cuts[j] and (best[w]<0 or margins[j]>margins[best[w]]):best[w]=j
 for w in ids:
  if not global_hit[w] and best[w]>=0:
   j=best[w];pred[w]=1;cnn_kind[w]=heads[j];cnn_source[w]=1;cnn_score[w]=margins[j];cnn_cut[w]=cuts[j]
   if 512<=d['centers'][j]<=767:cnn_position[w]=d['centers'][j]
 np.savez_compressed(out/'uart_token_diagnostics.npz',ids=ti,roles=roles,primary=d['primary'],center=d['centers'],anchor_margin=am,score=margins,cut=cuts)
 timings={'uart_cnn_host_seconds':time.perf_counter()-start};print('CNN_UART_DONE',len(ids),flush=True)
 for mid,name,cut in ((4,'spi',-773),(5,'i2c',-589)):
  start=time.perf_counter();d=np.load(ROOT/f'features/{name}.npz');ids=d['ids'];z=model.infer(mid,d['x']);head=z[:,1:4].argmax(1)+1;score=z[np.arange(len(z)),head];hit=score>cut
  pred[ids]=hit;cnn_kind[ids]=np.where(hit,head,0);cnn_source[ids]=3;cnn_score[ids]=score;cnn_cut[ids]=cut;cnn_position[ids]=514+4*z[:,4:].argmax(1);timings[name+'_cnn_host_seconds']=time.perf_counter()-start
  np.savez_compressed(out/f'{name}_logits.npz',ids=ids,logits=z);print('CNN_BUS_DONE',name,len(ids),flush=True)
 edge_hit=np.zeros(n,bool);rule_hit=np.zeros(n,bool);evidence=[];start=time.perf_counter()
 for i,(w,obs) in enumerate(zip(raw,observed)):
  c=obs['config'];e=edge_rules(w,c);r=strong_rules(w,c);edge_hit[i]=bool(e);rule_hit[i]=bool(r);evidence.append(dict(index=i,edge=e,strong=r))
 timings['rules_host_seconds']=time.perf_counter()-start
 (out/'rule_evidence.jsonl').write_text('\n'.join(json.dumps(r,default=json_default) for r in evidence)+'\n')
 # Truth is first opened only AFTER all detector outputs are fixed.
 truth=[json.loads(line) for line in (ROOT/'datasets/truth.jsonl').read_text(encoding='utf-8').splitlines()];y=np.array([r['label'] for r in truth],bool);methods={'traditional_rules':rule_hit,'current_cnn':pred,'rules_or_cnn':rule_hit|pred,'existing_edge_only':edge_hit}
 np.savez_compressed(out/'predictions.npz',truth=y,**methods,cnn_kind=cnn_kind,cnn_source=cnn_source,cnn_score=cnn_score,cnn_cut=cnn_cut,cnn_position=cnn_position,tokens=clocal)
 summary={};masks={'overall':np.array([r['split']!='unobservable_control' for r in truth])}
 for split in CFG['splits']:masks[split]=np.array([r['split']==split for r in truth])
 for proto in CFG['protocols']:
  masks[proto]=np.array([r['protocol']==proto and r['split']!='unobservable_control' for r in truth])
  for split in CFG['splits']:masks[proto+'_'+split]=np.array([r['protocol']==proto and r['split']==split for r in truth])
 masks['unobservable_control']=np.array([r['split']=='unobservable_control' for r in truth])
 for name,mask in masks.items():
  summary[name]={method:stats(y[mask],value[mask]) for method,value in methods.items()}
  pos=mask&y;neg=mask&~y
  summary[name]['cnn_vs_rules']=dict(recall=paired(pred[pos],rule_hit[pos]),fpr=paired(pred[neg],rule_hit[neg]))
  summary[name]['complementarity']=dict(cnn_only_anomalies=int(np.sum(pos&pred&~rule_hit)),cnn_only_normals=int(np.sum(neg&pred&~rule_hit)),rules_only_anomalies=int(np.sum(pos&rule_hit&~pred)),both_missed=int(np.sum(pos&~rule_hit&~pred)))
 family={}
 for proto in CFG['protocols']:
  for split in CFG['splits']:
   for name in sorted({r['paired_fault'] for r in truth if r['protocol']==proto and r['split']==split}):
    mask=np.array([r['protocol']==proto and r['split']==split and r['paired_fault']==name for r in truth]);family[proto+'/'+split+'/'+name]={method:stats(y[mask],v[mask]) for method,v in methods.items()}
 save(out/'metrics.json',dict(summary=summary,families=family,host_timing_not_fpga=timings,thresholds_frozen=True,fpga_inputs_unchanged=verify_unchanged(),model_hash=model.ref.digest,window_level_metrics=True,unseen_definition='injection mechanisms withheld from original single-fault generator; synthetic structured combinations, not open-set proof'))
 with (out/'case_results.csv').open('w',newline='',encoding='utf-8-sig') as f:
  writer=csv.writer(f);writer.writerow(['index','protocol','split','group','family','truth','traditional_rules','current_cnn','rules_or_cnn','existing_edge_only','cnn_kind','cnn_source','cnn_score','cnn_cut','candidate_sample'])
  for i,r in enumerate(truth):writer.writerow([i,r['protocol'],r['split'],r['group'],r['family'],int(y[i]),int(rule_hit[i]),int(pred[i]),int((rule_hit|pred)[i]),int(edge_hit[i]),int(cnn_kind[i]),int(cnn_source[i]),int(cnn_score[i]),int(cnn_cut[i]),float(cnn_position[i]) if np.isfinite(cnn_position[i]) else ''])
 print('ABLATION_COMPLETE',json.dumps(summary['overall'],default=json_default),flush=True)
if __name__=='__main__':main()
