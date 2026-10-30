"""Compare cooperative planning with the immutable 49015 source; no board."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,zipfile
R=Path(__file__).resolve().parents[1];S=R/'software_v4_0'
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);a=p.parse_args();assert a.attempt.isalnum()
 out=R/f'output/v49_sliced_planner_{a.attempt}';out.mkdir(exist_ok=False)
 backup=R/'backups/v4.9_before_sliced_planner_20261030/software_before.zip'
 with zipfile.ZipFile(backup) as z:
  name=next(n for n in z.namelist() if n.endswith('src/wave_render.c'));original=z.read(name)
 header=(S/'include/wave_render.h').read_text();names=set(re.findall(r'\b(tracegpu_\w+)\s*\(',header));names.add('wave_lod_storage')
 prefix=''.join(f'#define {n} reference_{n}\n' for n in sorted(names))
 prefix+='#define reference_tracegpu_wave_build reference_wave_build\n'
 ref=out/'reference.c';ref.write_text(prefix+original.decode(),encoding='utf8')
 members=[R/n for n in json.loads((R/'tests/ui_sources.json').read_text()) if not n.endswith('/stream_mocks.c')]
 members.extend(S/'src'/f'{n}.c' for n in ('ai_monitor_alert','ai_evidence_report','ui_ai_evidence'))
 tb=R/'tests/test_v49_sliced_planner.c'
 inputs={p.relative_to(R).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in [*members,tb,backup,Path(__file__),*[p for p in (S/'include').glob('*.h')]]}
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','')
 cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(S/'include'),'-I'+str(S/'tests'),'-I'+str(S/'tests/mocks'),'-Wa,-I'+str(S),*map(str,members),str(ref),str(tb),'-lm','-o',str(out/'slice.exe')]
 r=subprocess.run(cmd,env=env,capture_output=True);(out/'compile.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr[-6000:]
 r=subprocess.run([str(out/'slice.exe')],env=env,capture_output=True,timeout=180);(out/'run.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
 assert b'SLICED_PLANNER_PASS' in r.stdout and b'MENU_ANIMATION_PASS' in r.stdout
 assert all(hashlib.sha256((R/n).read_bytes()).hexdigest()==v for n,v in inputs.items())
 (out/'result.json').write_text(json.dumps(dict(passed=True,source_inputs=inputs,reference_sha256=hashlib.sha256(original).hexdigest(),scope='Exact legacy reference plans, snapshot append/overwrite/reset, cold/warm LOD, vector-byte bypass and menu animation; no board timing/FPS',programmed=False),indent=2))
 print(r.stdout.decode(errors='replace'))
if __name__=='__main__':main()
