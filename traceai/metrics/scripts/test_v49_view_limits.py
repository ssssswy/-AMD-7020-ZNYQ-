from pathlib import Path
import argparse,subprocess,os,json,hashlib
R=Path(__file__).resolve().parents[1];S=R/'software_v4_0'
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);a=p.parse_args();assert a.attempt.isalnum()
 out=R/f'output/v49_view_limits_{a.attempt}';out.mkdir(exist_ok=False)
 files=[S/'src/gesture_recognizer.c',S/'src/capture_session.c',S/'src/main.c',S/'include/gesture_recognizer.h',S/'include/capture_session.h',R/'tests/test_v49_view_limits.c',Path(__file__)]
 inputs={p.relative_to(R).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','')
 tests=[('view',[S/'src/gesture_recognizer.c',S/'src/capture_session.c',R/'tests/test_v49_view_limits.c']),('session',[S/'src/capture_session.c',S/'tests/test_session.c'])]
 for name,sources in tests:
  cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(S/'include'),'-I'+str(S/'tests/mocks'),*map(str,sources),'-o',str(out/(name+'.exe'))]
  r=subprocess.run(cmd,env=env,capture_output=True);(out/(name+'_compile.log')).write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr
  r=subprocess.run([str(out/(name+'.exe'))],env=env,capture_output=True);(out/(name+'_run.log')).write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
  print(r.stdout.decode(errors='replace'))
 assert all(hashlib.sha256((R/n).read_bytes()).hexdigest()==v for n,v in inputs.items())
 (out/'result.json').write_text(json.dumps(dict(passed=True,source_inputs=inputs,scope='Real gesture/view bounds and drain state; no board input replay',programmed=False),indent=2))
if __name__=='__main__':main()
