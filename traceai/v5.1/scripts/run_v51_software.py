"""Guarded fresh Vitis platform/BSP/application build; never downloads/runs."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
R=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text(encoding='utf8'))
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);p.add_argument('--hardware-attempt',required=True);p.add_argument('--execute',action='store_true');a=p.parse_args();assert a.attempt.isdigit() and a.hardware_attempt.isdigit()
 hw=R/f'output/ai_soc_{a.hardware_attempt}';logs=R/f'logs/ai_software_{a.attempt}';out=R/f'output/ai_software_{a.attempt}';ws=R/f'vitis_ws_ai_{a.attempt}'
 assert all(not p.exists() for p in [logs,out,ws]),'Use a fresh attempt'
 verified=load(R/f'logs/ai_soc_{a.hardware_attempt}/verified_result.json');assert verified['passed'] and verified['abi']=='0x00040801'
 assert all(sha(R/n)==h for n,h in verified['source_inputs'].items())
 bit,xsa=hw/'tracegpu_v4_9.bit',hw/'tracegpu_v4_9.xsa'
 assert sha(bit)==verified['bit_sha256'] and sha(xsa)==verified['xsa_sha256']
 sdk=Path('E:/AMD/2025.2/Vitis');assert 'XILINX_VERSION_VITIS=2025.2' in (sdk/'data/version.bat').read_text() and 'release-2025.2' in (sdk/'rigel_version.dat').read_text()
 tracked=[p for p in (R/'software_v5_1').rglob('*') if p.is_file()]+[R/'scripts/build_v51_software.py',R/'scripts/run_v51_software.py',R/'hardware_inputs/elf_memory_parser_v35.py',R/'scripts/verify_v51_memory.py']
 inputs={p.relative_to(R).as_posix():sha(p) for p in tracked}
 print(json.dumps(dict(execute=a.execute,workspace=str(ws),hardware_attempt=a.hardware_attempt,version='5.1',tool='Vitis 2025.2',display_baseline='v4.7',programmed=False)),flush=True)
 if not a.execute:return
 logs.mkdir();(logs/'source_inputs.json').write_text(json.dumps(inputs,indent=2))
 env=os.environ.copy();env.update(PYTHONUTF8='1',PYTHONDONTWRITEBYTECODE='1',XILINX_LOCAL_USER_DATA='no',TG_AI_SW_ATTEMPT=a.attempt,TG_AI_HW_ATTEMPT=a.hardware_attempt)
 cmd=['C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe','-NoProfile','-NonInteractive','-Command',"& 'E:/AMD/2025.2/Vitis/bin/vitis.bat' --source '"+(R/'scripts/build_v51_software.py').as_posix()+"'; exit $LASTEXITCODE"]
 report=dict(passed=False,command=cmd,version='5.1',programmed=False,board_accepted=False)
 (logs/'command.json').write_text(json.dumps(cmd,indent=2))
 try:
  with (logs/'stdout.txt').open('xb') as f:c=subprocess.run(cmd,cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT)
  text=(logs/'stdout.txt').read_text(errors='replace');print(text[-4500:],flush=True)
  report['exit_code']=c.returncode;b=load(out/'build_report.json');report['build_report']=b
  assert c.returncode==0 and 'TG_V51_SOFTWARE_BUILD_MEMORY_PASS_NOT_BOARD' in text and b['passed'] and b['source_members']==len([p for p in (R/'software_v5_1/src').iterdir() if p.suffix in ('.c','.S')])
  assert b['version']=='5.1' and b['ai_abi']=='0x00040801' and b['workspace_report']['passed']
  assert b['bit_sha256']==sha(bit) and b['xsa_sha256']==sha(xsa)
  assert b['elf_sha256']==sha(out/'tracegpu_v5_1.elf') and b['map_sha256']==sha(out/'tracegpu_v5_1.map')
  warnings=[line for line in text.splitlines() if 'warning:' in line and '/tracegpu_app/src/' in line.replace('\\','/')];assert not warnings,warnings
  report.update(passed=True,artifact_sha256={f.name:sha(f) for f in out.glob('tracegpu_v5_1*') if f.is_file()})
 except BaseException as e:report['failure']=str(e);print(type(e).__name__,str(e),flush=True)
 finally:
  changes=[n for n,h in inputs.items() if sha(R/n)!=h];report['changed_inputs']=changes;report['maintained_inputs_unchanged']=not changes
  if changes:report['passed']=False
  (logs/'result.json').write_text(json.dumps(report,indent=2))
 return 0 if report['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
