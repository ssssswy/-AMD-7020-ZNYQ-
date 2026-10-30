from pathlib import Path
import json,numpy as np,hashlib,csv
R=Path(__file__).resolve().parents[1];O=R/'output/v49_detection_pc_20261030_002';A=R.parent/'v4.6_ablation_20261027'
truth=[json.loads(s) for s in (A/'datasets/truth.jsonl').read_text().splitlines()];d=np.load(O/'predictions.npz');events=[set() for t in truth]
for line in (O/'edge_events.txt').read_text().splitlines():
 i,ch,k,a,b,l=map(int,line.split());p=truth[i]['config']['period_ticks']
 if a>=1000000000+512*p:events[i].add(k)
types={}
for family,head in [('glitch',3),('burst_chatter',3),('parity_flip',1),('mandatory_stop_low',2),('baud_mismatch',4),('edge_skew',4),('correlated_edge_jitter',4),('mark_space_bias',4)]:
 ix=[i for i,t in enumerate(truth) if t['protocol']=='uart' and t['label'] and t['family']==family and t['split']!='unobservable_control'];types[family]=dict(n=len(ix),correct=int(sum((d['cnn'][i] and d['cnn_kind'][i]==head) or (head==3 and 1 in events[i]) for i in ix)))
(O/'uart_correct_type.json').write_text(json.dumps(types,indent=2));print(types)
# Confirm immutable signed implementation and current ROM/rule inputs.
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
signed=json.loads((R/'logs/ai_soc_49011/verified_result.json').read_text());assert all(sha(R/n)==h for n,h in signed['source_inputs'].items())
r=json.loads((O/'result.json').read_text());assert all(sha(R/n)==h for n,h in r['source_inputs'].items())
for name in ['v46_rules_pc20261030','v46_core_pc20261030']:
 t=json.loads((R/'output'/name/'result.json').read_text());assert t['passed'] and all(sha(R/n)==h for n,h in t['source_inputs'].items())
# Guard that extra setup/hold input corpus remains directly replayable.
import subprocess,os
env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','');v=subprocess.run([str(O/'edge.exe'),str(O/'setup_hold_inputs.txt')],env=env,capture_output=True);assert v.returncode==0 and v.stdout==(O/'setup_hold_events.txt').read_bytes()
(O/'verification.json').write_text(json.dumps(dict(passed=True,hardware_attempt='49011',software_attempt='49018',rules_rtl_cases=106,cnn_rom_rtl_cases=12,main_windows=5550,scored_windows=5400,unobservable_controls_excluded=150,extra_spi_setup_hold_windows=400,source_inputs=r['source_inputs'],extra_inputs_sha256=sha(O/'setup_hold_inputs.txt'),programmed=False),indent=2))
