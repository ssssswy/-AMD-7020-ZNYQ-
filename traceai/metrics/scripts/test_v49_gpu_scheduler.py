"""Noncoherent CPU/GPU, scanout, fallback and fault regression of real scheduler."""
from pathlib import Path
import argparse,json,hashlib,subprocess,os
R=Path(__file__).resolve().parents[1];S=R/'software_v4_0'
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);a=p.parse_args();assert a.attempt.isalnum()
 out=R/f'output/v49_gpu_scheduler_{a.attempt}';out.mkdir(exist_ok=False)
 original=S/'tests/test_render_gpu_partial.c';tb=S/'tests/test_render_scheduler_v3_4.c'
 (out/'test_render_gpu_partial_embedded.c').write_text(original.read_text().replace('int main(int argc,char **argv){','int gpu_suite_main(int argc,char **argv){'))
 harness=out/'harness.c';harness.write_text('#include "'+tb.as_posix()+'"\n')
 members=[R/n for n in json.loads((R/'tests/ui_sources.json').read_text()) if not n.endswith('/stream_mocks.c')]
 members.append(S/'src/render_policy.c')
 members.extend(S/'src'/f'{n}.c' for n in ('ai_monitor_alert','ai_evidence_report','ui_ai_evidence'))
 tracked=[*members,tb,original,S/'src/render_scheduler.c',S/'include/render_scheduler.h',Path(__file__)]
 hashes={p.relative_to(R).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
 cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(S/'include'),'-I'+str(S/'tests'),'-I'+str(S/'tests/mocks'),'-I'+str(out),'-Wa,-I'+str(S),*map(str,members),str(harness),'-lm','-o',str(out/'scheduler.exe')]
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','')
 r=subprocess.run(cmd,env=env,capture_output=True);(out/'compile.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr[-6000:]
 r=subprocess.run([str(out/'scheduler.exe'),str(out)],env=env,capture_output=True,timeout=180);(out/'run.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stdout[-5000:]+r.stderr
 assert b'SCHEDULER_INTEGRATION_PASS' in r.stdout
 assert all(hashlib.sha256((R/n).read_bytes()).hexdigest()==h for n,h in hashes.items())
 (out/'result.json').write_text(json.dumps(dict(passed=True,source_inputs=hashes,scope='Noncoherent CPU/DDR model, actual scheduler, flip barrier, fault quarantine, fallback; not board FPS',programmed=False),indent=2))
 print(r.stdout.decode(errors='replace')[-2500:])
if __name__=='__main__':main()
