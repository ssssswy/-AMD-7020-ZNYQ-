"""Test actual AI diagnostic formatting and the production application call site."""
from pathlib import Path
import argparse,os,subprocess,json,hashlib
R=Path(__file__).resolve().parents[1];S=R/'software_v4_0'
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);a=p.parse_args();assert a.attempt.isalnum()
 out=R/f'output/v49_ai_diagnostics_{a.attempt}';out.mkdir(exist_ok=False)
 files=[S/'include/ai_monitor.h',S/'src/ai_monitor.c',S/'src/ai_monitor_main.inc',R/'tests/test_v49_ai_diagnostics.c',Path(__file__)]
 inputs={p.relative_to(R).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','')
 cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(S/'include'),str(S/'src/ai_monitor.c'),str(R/'tests/test_v49_ai_diagnostics.c'),'-o',str(out/'diagnostics.exe')]
 r=subprocess.run(cmd,env=env,capture_output=True);(out/'compile.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 r=subprocess.run([str(out/'diagnostics.exe')],env=env,capture_output=True);(out/'run.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
 assert b'V49_AI_DIAGNOSTICS_PASS' in r.stdout
 # Exercise the application/MMIO snapshot path and capture its actual xil_printf.
 template=R/'output/v49_and_ps_diag001/app.c';assert template.exists(),'Run test_v49_and_ps.py --attempt diag001 first'
 t=template.read_text();old='static void xil_printf(const char *format,...){(void)format;}'
 new='''static char diag_log[32768];static unsigned diag_used,diag_lines;
static void xil_printf(const char *format,...){
 va_list args;va_start(args,format);int n=vsnprintf(diag_log+diag_used,sizeof diag_log-diag_used,format,args);va_end(args);
 assert(n>=0&&(unsigned)n<sizeof diag_log-diag_used);diag_used+=(unsigned)n;
 if(strstr(diag_log,"AI49 coverage"))diag_lines++;
}'''
 assert old in t;t=t.replace(old,new)
 marker='assert(ack_count==1&&ai_alert.count==1&&ai_alert.view.live&&ai_alert.view.status==TG_AI_UI_BUSY);'
 assert marker in t
 t=t.replace(marker,marker+'''assert(strstr(diag_log,"checked=3 skip=0 failed=0"));
 /* Snapshot skip-only counts through the real application, not a formatter-only call. */
 regs[0x90/4]=17;regs[0x88/4]=UINT32_MAX;regs[0x8c/4]=1;raw_status|=16;
 shell.now=1100;ai_background(&u);
 assert(strstr(diag_log,"checked=3 skip=17 failed=0"));
 assert(strstr(diag_log,"sample_drop=8589934591")&&strstr(diag_log,"partial=1"));
 /* Restore the lifecycle test's time and diagnostic epoch for its remaining cases. */
 memset(&ai_diag,0,sizeof ai_diag);ai_last_publish_ms=0;raw_status&=~16U;shell.now=100;
''')
 (out/'app_logging.c').write_text(t)
 cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(S/'include'),'-I'+str(S/'tests'),'-I'+str(S/'tests/mocks'),*[str(S/'src'/f'{n}.c') for n in ('ai_evidence_report','ai_monitor','ai_monitor_alert','ai_alert','ui_ai','ui_ai_notice')],str(out/'app_logging.c'),'-lm','-o',str(out/'app_logging.exe')]
 r=subprocess.run(cmd,env=env,capture_output=True);(out/'app_compile.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 r=subprocess.run([str(out/'app_logging.exe')],env=env,capture_output=True);(out/'app_run.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr
 for p in [template,*[S/'src'/f'{n}.c' for n in ('ai_evidence_report','ai_monitor_alert','ai_alert','ui_ai','ui_ai_notice')]]:inputs[p.relative_to(R).as_posix()]=hashlib.sha256(p.read_bytes()).hexdigest()
 assert all(hashlib.sha256((R/n).read_bytes()).hexdigest()==v for n,v in inputs.items())
 (out/'result.json').write_text(json.dumps(dict(passed=True,source_inputs=inputs,scope='Production formatter and production app/MMIO skip-only log regression; not board timing/FPS',programmed=False),indent=2))
 print('V49_AI_DIAGNOSTICS_AND_APP_PASS')
if __name__=='__main__':main()
