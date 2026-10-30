"""Real new PS drivers/app/UI with host MMIO mocks; never executes on a board."""
from pathlib import Path
import argparse,json,hashlib,os,subprocess
R=Path(__file__).resolve().parents[1];S=R/'software_v4_0'
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);a=p.parse_args();assert a.attempt.isalnum();out=R/f'output/v49_and_ps_{a.attempt}';out.mkdir(exist_ok=False)
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','');records=[]
 def compile_run(name,files):
  cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(S/'include'),'-I'+str(S/'tests'),'-I'+str(S/'tests/mocks'),'-I'+str(out),'-Wa,-I'+str(S),*map(str,files),'-lm','-o',str(out/(name+'.exe'))]
  v=subprocess.run(cmd,env=env,capture_output=True,text=True,encoding='utf8',errors='replace');(out/(name+'_compile.log')).write_text(v.stdout+v.stderr,encoding='utf8');assert v.returncode==0,v.stderr[-6000:]
  v=subprocess.run([str(out/(name+'.exe')),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180);(out/(name+'_run.log')).write_text(v.stdout+v.stderr,encoding='utf8');assert v.returncode==0,v.stdout+v.stderr
  records.append(dict(name=name,passed=True));print(v.stdout[-1700:])
 s=(R/'tests/test_v46_monitor.c').read_text().replace('hw.reg[2]=0xfff','hw.reg[2]=0x3fff')
 s=s.replace(' init();assert(!tg_ai_monitor_init',' init();hw.reg[2]=0x1fff;assert(tg_ai_monitor_init(&d,io)<0&&!hw.writes);\n init();assert(!tg_ai_monitor_init')
 (out/'monitor.c').write_text(s);compile_run('monitor',[S/'src/ai_monitor.c',out/'monitor.c'])
 s=(R/'tests/test_v47_evidence_trigger.c').read_text().replace('reg[0x320/4]=0x40700','reg[0x320/4]=0x40801')
 needle='puts("V47_EVIDENCE_TRIGGER_DRIVER_PASS'
 extra='''
 d.configured=d.armed=1;d.config.mode=0;d.config.rules=1;
 reg[0x320/4]=0x40700;assert(tg_ai_trigger_bind(&t,&d,TG_AI_TRIGGER_BOTH)<0);
 reg[0x320/4]=0x40801;assert(!tg_ai_trigger_bind(&t,&d,TG_AI_TRIGGER_BOTH));
 assert(!tracegpu_trigger_hw_configure(&cfg,1,31,7)&&reg[0x324/4]==4);
 d.config.rules=0;assert(tracegpu_trigger_hw_configure(&cfg,1,31,7)<0);d.config.rules=1;
 d.config.mode=1;assert(tracegpu_trigger_hw_configure(&cfg,1,31,7)<0);d.config.mode=0;
 reg[0x33c/4]=8;p.source=5;p.protocol=1;p.frozen_trigger=1;p.begin=900;p.end=1200;
 uint32_t *r=&reg[0x3a0/4];r[0]=0x03010401;r[1]=12;r[2]=100;r[3]=990;r[4]=0;r[5]=1002;r[6]=0;r[7]=3;
 assert(!tg_ai_trigger_agreement(&t,&p)&&p.rule_width==12&&p.rule_begin==990&&p.rule_end==1002&&p.and_overwritten==3&&p.partial);
 assert(tg_ai_evidence_read(&d,&p,&e)==1); // source5 refers to unchanged CNN source1 evidence identity.
 r[0]^=1U<<24;assert(tg_ai_trigger_agreement(&t,&p)<0);r[0]^=1U<<24;
 r[3]=1300;r[5]=1312;assert(tg_ai_trigger_agreement(&t,&p)<0);r[3]=990;r[5]=1002;
 p.frozen_trigger=0;assert(tg_ai_trigger_agreement(&t,&p)<0);
 '''
 assert needle in s;s=s.replace(needle,extra+needle);(out/'driver.c').write_text(s)
 compile_run('driver',[*[S/'src'/f'{n}.c' for n in ('ai_evidence_report','ai_trigger_config','trigger_hw','trigger_config')],out/'driver.c'])
 s=(R/'tests/test_v47_app.c').read_text().replace('APP_GLUE_PATH',(S/'src/ai_monitor_main.inc').as_posix()).replace('regs[2]=0x1fff','regs[2]=0x3fff')
 s=s.replace('#include "'+(S/'src/ai_monitor_main.inc').as_posix()+'"', '''int tg_ai_trigger_agreement(tg_ai_trigger*t,tg_ai_monitor_event*p){(void)t;p->rule_identity=0x03010401;p->rule_width=2;p->rule_limit=5;p->rule_begin=1500;p->rule_end=1502;return p->source==5?0:-1;}
'''+ '#include "'+(S/'src/ai_monitor_main.inc').as_posix()+'"')
 needle='ai_publish(&u);assert(ai_alert.count==1);frozen_cause_valid=0;'
 extra='''
 shell.ai_trigger_mode=4;tg_ai_alert_reset(&ai_alert,ai_current_key());first_trigger_presented=0;
 inject(50);head[4]=(3U<<24)|(1U<<16)|(4U<<8)|1;shell.now+=100;ai_background(&u);
 assert(ai_alert.count==0); // Single-source queued event is drained but not presented in AND mode.
 frozen_cause[4]=0x03020501;frozen_cause_valid=1;ai_publish(&u);
 assert(ai_alert.count==1&&u.ai_alert.event.source==5&&u.ai_alert.event.observed_ticks==2&&u.ai_alert.event.limit_ticks==5);
 assert(strstr(tg_ai_event_source(&u.ai_alert.event),"且"));frozen_cause_valid=0;
 '''
 assert needle in s;s=s.replace(needle,needle+extra);(out/'app.c').write_text(s)
 compile_run('app',[*[S/'src'/f'{n}.c' for n in ('ai_evidence_report','ai_monitor','ai_monitor_alert','ai_alert','ui_ai','ui_ai_notice')],out/'app.c'])
 # Strict ARM compile catches all new production modules, not only linked host paths.
 cc='E:/AMD/2025.2/Vitis/gnu/aarch32/nt/gcc-arm-none-eabi/bin/arm-none-eabi-gcc.exe';bsp=R.parent/'v4.7_full/vitis_ws_ai_47002/tracegpu_platform/export/tracegpu_platform/sw/standalone_a9/include'
 for name in ('main','ai_monitor','ai_trigger_config','ai_monitor_alert','ai_alert','ai_evidence_report','ui_ai_evidence','ui_ai_notice','ui_workbench','ui_navigation'):
  cmd=[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard','-DSDT','-DPX_GPU_ENABLE','-DTRACEGPU_AI_BACKEND','-I'+str(bsp),'-I'+str(S/'include'),'-c',str(S/'src'/f'{name}.c'),'-o',str(out/(name+'.o'))]
  v=subprocess.run(cmd,capture_output=True,text=True,encoding='utf8',errors='replace');(out/(name+'_arm.log')).write_text(v.stdout+v.stderr,encoding='utf8');assert v.returncode==0,v.stderr
 records.append(dict(name='strict_ARM_compile',passed=True,count=10,bsp_scope='existing2025.2 BSP compile-only; new platform/link is a separate gate'))
 inputs={p.relative_to(R).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in S.rglob('*') if p.is_file()}
 (out/'result.json').write_text(json.dumps(dict(passed=True,tests=records,source_inputs=inputs,programmed=False,scope='Actual new drivers and app glue with mocked MMIO; ARM compile only is not new XSA link'),indent=2))
 print('V49_AND_PS_DRIVERS_APP_ARM_PASS')
if __name__=='__main__':main()
