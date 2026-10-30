"""Existing full UI/partial-pixel regression plus new AND menu/evidence scenes."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
from PIL import Image
R=Path(__file__).resolve().parents[1];S=R/'software_v4_0'
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);a=p.parse_args();assert a.attempt.isalnum();out=R/f'output/v49_and_ui_{a.attempt}';out.mkdir(exist_ok=False)
 original=(S/'tests/test_render_gpu_partial.c').read_text();(out/'test_render_gpu_partial_embedded.c').write_text(original.replace('int main(int argc,char **argv){','int unused_legacy_main(int argc,char **argv){'))
 code='#include "test_render_gpu_partial_embedded.c"\n'
 fixtures=[('test_ai_ui.c','v40_ai_main'),('test_v42.c','v42_all_main'),('test_v43_ui.c','v43_all_main'),('test_v44_ui.c','v44_all_main'),('test_v45_ui.c','v45_all_main'),('test_v46_ui.c','v46_all_main')]
 for name,alias in fixtures:code+='#define main '+alias+'\n#include "'+(R/'tests'/name).as_posix()+'"\n#undef main\n'
 old=(R/'tests/test_v47_ui.c').read_text().replace('(before+1)%4','(before+1)%5');(out/'legacy47.c').write_text(old)
 code+='#define main v47_all_main\n#include "legacy47.c"\n#undef main\n'
 code+='''
int main(int argc,char **argv){
 assert(!v47_all_main(argc,argv));
 static tracegpu_ui_state u;tracegpu_ui_shell s;memset(&u,0,sizeof u);
 u.logic_mode=u.quad_channel=u.dual_channel=1;u.logic_channel_count=8;u.logic_capture_mask=u.logic_visible=255;
 u.time_window_ticks=1000000;u.view_anchor_ticks=u.latest_event_ticks=2000000;u.logic_records=100;u.drain_state=CAPTURE_READY;
 for(unsigned j=0;j<8;j++)u.channel_ring[j]=&rings[j];tracegpu_ui_shell_init(&s,&u);s.motion=0;tg_workflow_enable(&s);s.workflow_applied=1;s.ai_autonomous=1;
 s.decode_config[0].protocol=TG_DECODE_UART;s.decode_slot=0;s.ai_trigger_mode=0;s.draining=0;
 for(unsigned j=0;j<10;j++){unsigned before=s.ai_trigger_mode;assert(!tracegpu_ui_shell_action(&s,UI_AI_TRIGGER_MODE,&u));assert(s.ai_trigger_mode==(before+1)%5);}
 s.ai_trigger_mode=4;u.running=1;assert(!tracegpu_ui_shell_action(&s,UI_AI_TRIGGER_MODE,&u)&&s.ai_trigger_mode==4);u.running=0;
 s.decode_config[0].protocol=TG_DECODE_SPI;assert(!tracegpu_ui_shell_action(&s,UI_AI_TRIGGER_SETTINGS,&u)&&s.ai_trigger_mode==1);
 s.decode_config[0].protocol=TG_DECODE_UART;s.ai_trigger_mode=4;reset_surfaces();memset(&touch,0,sizeof touch);
 for(unsigned step=0;step<3;step++){
  s.now=26000+step*300;s.drawer=3;s.position=s.target=800;s.menu_page=step?TG_MENU_AI_EVIDENCE:TG_MENU_AI_TRIGGER;
  u.ai_alert=(tg_ai_alert_view){.configured=1,.autonomous=1,.enabled=1,.status=TG_AI_UI_DONE,.revision=step+100,.count=1};
  u.ai_alert.event=(tg_ai_event){.id=0xf0000001,.model_id=11,.channel=3,.source=5,.head=2,.point=1,.hits=1,.begin=1000,.end=2000,.tick=1500,.pl_event=1,.observed_ticks=12,.limit_ticks=100,.rule_begin=1400,.rule_end=1412,.and_overwritten=step==2?3:0};
  tg_ai_evidence*e=&u.ai_alert.evidence;e->available=1;e->quality=TG_AI_EVIDENCE_OK;
  uint32_t*w=e->word;w[4]=23;w[6]=0x218ff;w[7]=54;w[10]=1200000;w[12]=1150000;w[16]=4096;w[17]=4000;w[18]=7;w[21]=2;w[23]=2500000;w[24]=6250000;w[25]=10U<<16|520;w[26]=7;w[27]=620U<<16|590;w[28]=5120;w[29]=0xfff;
  tracegpu_wave_build(&plan,&u,&rings[0]);render_frame(&u,&s,&rings[0],2200+step,(int)(step%3),0);
  memcpy(image.pixels,surface[front].cache,sizeof image.pixels);char f[1024];snprintf(f,sizeof f,"%s/and_%02u.bmp",argv[1],step);bmp(f);
 }
 puts("V49_AND_UI_PASS 105_legacy_scenes 3_AND_scenes 5_mode_cycle SPI_restriction pause_lock");return 0;
}
'''
 (out/'harness.c').write_text(code,encoding='utf8')
 members=[R/name.replace('software_v4_0/','software_v4_0/') for name in json.loads((R/'tests/ui_sources.json').read_text())]
 members.extend(S/'src'/f'{n}.c' for n in ('ai_monitor_alert','ai_evidence_report','ui_ai_evidence'))
 tracked=[*members,*[p for p in S.rglob('*') if p.is_file() and p.suffix in ('.h','.c','.tgf','.S','.inc')]];hashes={p.relative_to(R).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
 cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(S/'include'),'-I'+str(S/'tests'),'-I'+str(S/'tests/mocks'),'-I'+str(out),'-Wa,-I'+str(S),*map(str,members),str(out/'harness.c'),'-lm','-o',str(out/'ui.exe')]
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','')
 v=subprocess.run(cmd,env=env,capture_output=True,text=True,encoding='utf8',errors='replace');(out/'compile.log').write_text(v.stdout+v.stderr,encoding='utf8');assert v.returncode==0,v.stderr[-5000:]
 v=subprocess.run([str(out/'ui.exe'),str(out)],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180);(out/'run.log').write_text(v.stdout+v.stderr,encoding='utf8');assert v.returncode==0,v.stdout+v.stderr
 for p in out.glob('and_*.bmp'):Image.open(p).save(p.with_suffix('.png'))
 assert all(hashlib.sha256((R/n).read_bytes()).hexdigest()==h for n,h in hashes.items())
 (out/'result.json').write_text(json.dumps(dict(passed=True,source_inputs=hashes,legacy_scenes=105,new_and_scenes=3,scope='Actual renderer, host GPU mock and partial/full comparison. Not LCD board pixels or FPS.',programmed=False),indent=2))
 print(v.stdout[-1800:])
if __name__=='__main__':main()
