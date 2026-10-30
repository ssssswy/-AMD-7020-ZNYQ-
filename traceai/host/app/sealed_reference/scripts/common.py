from pathlib import Path
import hashlib,json,os,subprocess
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
CFG=json.loads((ROOT/'experiment.json').read_text())
PROJECT=Path(CFG['model_project']);TRAIN=Path(CFG['training_root'])
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def json_default(v):
 if isinstance(v,np.generic):return v.item()
 if isinstance(v,np.ndarray):return v.tolist()
 raise TypeError(type(v).__name__)
def save(p,v):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2,ensure_ascii=False,allow_nan=False,default=json_default),encoding='utf-8')
def run(cmd,log,timeout=1200):
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','');env['PYTHONDONTWRITEBYTECODE']='1'
 v=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout)
 Path(log).write_text(v.stdout+v.stderr,encoding='utf-8');assert v.returncode==0,(v.returncode,v.stdout[-1500:],v.stderr[-2000:]);return v.stdout
def protected():
 out={}
 for folder in ('src','software_v4_0','model','hardware_inputs','baseline_v3_9'):
  for p in (PROJECT/folder).rglob('*'):
   if p.is_file() and '__pycache__' not in p.parts:out[p.relative_to(PROJECT).as_posix()]=sha(p)
 return out
def freeze():
 p=ROOT/'evidence/freeze.json'
 inputs={str(f):sha(f) for f in [PROJECT/'model/r11/weights.mem',PROJECT/'model/r11/params.mem',PROJECT/'src/ai/r11_layers.vh',PROJECT/'model/r11/manifest.json',
  *[PROJECT/'software_v4_0/src'/n for n in ('ai_global.c','ai_slots.c','ai_bus_features.c')],
  *[PROJECT/'software_v4_0/include'/n for n in ('ai_global.h','ai_slots.h','ai_bus_features.h')]]}
 if p.exists():
  old=json.loads(p.read_text());assert old['model_inputs']==inputs;return old
 data=dict(configuration=CFG,model_inputs=inputs,project_inputs=protected(),created_before_test=True,fpga_modified=False,programmed=False)
 save(p,data);return data
def verify_unchanged():
 v=json.loads((ROOT/'evidence/freeze.json').read_text());assert v['project_inputs']==protected(),'FPGA project changed during independent experiment'
 for p,h in v['model_inputs'].items():assert sha(p)==h
 return True
