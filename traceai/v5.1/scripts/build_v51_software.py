"""Vitis 2025.2 clean app+platform build for the NEW AI XSA; never run/program."""
import argparse,hashlib,importlib.util,json,os,re,shutil,socket,subprocess,sys,zipfile
from pathlib import Path
import xml.etree.ElementTree as ET
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1];SW=ROOT/'software_v5_1'
BIN=Path('E:/AMD/2025.2/Vitis/gnu/aarch32/nt/gcc-arm-none-eabi/bin')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def save(p,v):
 with p.open('x',encoding='utf-8') as f:json.dump(v,f,indent=2)
def memory(elf,mapfile):
 path=ROOT/'scripts/verify_v51_memory.py';spec=importlib.util.spec_from_file_location('v51_memory',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module.verify(elf,mapfile)

def verify_workspace(ws: Path, xsa: Path) -> dict:
    app = ws / "tracegpu_app/vitis-comp.json"
    platform = ws / "tracegpu_platform/vitis-comp.json"
    ac, pc = json.loads(app.read_text()), json.loads(platform.read_text())
    require(ac.get("name") == "tracegpu_app" and ac.get("applicationFlow") == "EMBEDDED"
            and ac.get("platform") == "tracegpu_platform" and ac.get("domain") == "standalone_a9"
            and ac.get("cpuInstance") == "ps7_cortexa9_0", "Fresh application component metadata mismatch")
    require(pc.get("name") == "tracegpu_platform" and pc.get("type") == "PLATFORM", "Fresh platform component metadata mismatch")
    config = pc.get("configuration", {})
    require(Path(config.get("xsa", "")).resolve() == xsa.resolve(), "Platform does not reference actual candidate XSA")
    local_xsa = ws / config.get("xsaPathInPlatform", "")
    require(local_xsa.is_file() and sha(local_xsa) == sha(xsa), "Platform copied XSA identity mismatch")
    ini = ws / "_ide/version.ini"
    initialized = not ini.exists()
    if initialized:
        ini.parent.mkdir(exist_ok=True)
        ini.write_text("com.ide.version=2025.2\n", encoding="utf-8")
    require("com.ide.version=2025.2" in ini.read_text(encoding="utf-8"), "Workspace IDE version mismatch")
    return dict(passed=True, workspace=ws.as_posix(), metadata_initialized=initialized,
                version="2025.2", components={path.relative_to(ws).as_posix(): sha(path) for path in (app, platform, ini)},
                scope="Checked after genuine platform/BSP/app builds; metadata is not BSP build evidence", programmed=False)


def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',default=os.environ.get('TG_AI_SW_ATTEMPT','51001'));p.add_argument('--hardware-attempt',default=os.environ.get('TG_AI_HW_ATTEMPT','49011'));a=p.parse_args();assert a.attempt.isdigit() and a.hardware_attempt.isdigit()
 hw=ROOT/f'output/ai_soc_{a.hardware_attempt}';ws=ROOT/f'vitis_ws_ai_{a.attempt}';out=ROOT/f'output/ai_software_{a.attempt}'
 require(not ws.exists() and not out.exists(),'Preserve prior workspace/output');out.mkdir()
 report=dict(passed=False,workspace=str(ws),programmed=False,board_accepted=False,ui_ai_integrated=True,hardware_reused=True,version='5.1',ai_abi='0x00040801',display_baseline='v4.7 direct RGB/DE/clock',scope='v5.1 PS UART continuous-frame boundary repair; inherited 32MiB history/UI/AI; unchanged HW49011; not board accepted')
 client=None
 try:
  status=json.loads((hw/'hardware_status.json').read_text(encoding='utf-8'))
  require(json.loads((ROOT/f'logs/ai_soc_{a.hardware_attempt}/verified_result.json').read_text())['passed'],'Independent hardware audit required')
  require(status['wns_ns']>=0 and status['whs_ns']>=0 and status['drc_errors']==status['drc_critical']==0,'Hardware timing/DRC')
  xsa=hw/'tracegpu_v4_9.xsa';bit=hw/'tracegpu_v4_9.bit'
  with zipfile.ZipFile(xsa) as z:
   bits=[n for n in z.namelist() if n.endswith('.bit')];require(len(bits)==1 and hashlib.sha256(z.read(bits[0])).hexdigest()==sha(bit),'BIT/XSA pair')
   names=[n for n in z.namelist() if n.endswith('.hwh')];require(len(names)==1,'HWH count')
   hwh=ET.fromstring(z.read(names[0]));ranges=hwh.findall('.//MEMRANGE')
   ai=[n for n in ranges if n.get('INSTANCE')=='ai_inference'];require(ai,'AI missing from XSA')
   require(all(int(n.get('BASEVALUE'),0)==0x45400000 and int(n.get('HIGHVALUE'),0)==0x4540ffff for n in ai),'AI MMIO mismatch')
   dma=hwh.find(".//MODULE[@INSTANCE='uart_capture_dma']");params={p.get('NAME'):p.get('VALUE') for p in dma.findall('./PARAMETERS/PARAMETER')}
   require(params['C_INCLUDE_SG']=='1' and params['C_SG_INCLUDE_STSCNTRL_STRM']=='0','DMAFIX must be preserved')
  # Font blobs must belong to this version, not an older project's absolute path.
  for name in ('ui_regular_16','control_medium_20'):
   asm=SW/'src'/('embed_'+name+'.S');text=asm.read_text()
   match=re.search(r'\.incbin "([^"]+)"',text);require(match is not None,'Font inclusion')
   font=Path(match.group(1));require(font.is_file() and font.resolve().is_relative_to(SW.resolve()),'External font dependency')
  before={p.relative_to(SW).as_posix():sha(p) for p in SW.rglob('*') if p.is_file()};save(out/'source_inputs.json',before)
  import vitis
  # The vendor launcher can emit Windows-localized bytes before its JSON port
  # announcement. An explicit free port avoids its strict UTF-8 log parser.
  with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as probe:
   probe.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1);probe.bind(('0.0.0.0',0));server_port=probe.getsockname()[1]
  report['vitis_server_port']=server_port
  client=vitis.create_client(port=server_port,host="127.0.0.1",workspace=str(ws));require(Path(client.get_workspace()).resolve()==ws.resolve(),'Workspace')
  platform=client.create_platform_component(name='tracegpu_platform',hw_design=str(xsa),cpu='ps7_cortexa9_0',os='standalone',domain_name='standalone_a9',no_boot_bsp=True)
  domain=platform.get_domain(name='standalone_a9')
  for setting in ('standalone_stdin','standalone_stdout'):domain.set_config(option='os',param=setting,value='ps7_uart_0')
  require(platform.build()==0,'Platform build')
  exported=ws/'tracegpu_platform/export/tracegpu_platform';bsp=exported/'sw/standalone_a9';header=(bsp/'include/xparameters.h').read_text(encoding='utf-8')
  # If SDT does not list this custom module, do not bypass the binding check with a macro override.
  require(re.search(r'#define XPAR_AI_INFERENCE_BASEADDR\s+0x45400000',header,re.I),'AI BSP binding missing')
  require(re.search(r'#define XPAR_CPU_CORE_CLOCK_FREQ_HZ\s+666666687',header),'CPU clock changed')
  cfg=(ws/'tracegpu_platform/ps7_cortexa9_0/standalone_a9/bsp/libsrc/axidma/src/xaxidma_g.c').read_text(encoding='utf-8')
  require(re.search(r'0x0,\s*/\* xlnx,sg-include-stscntrl-strm \*/',cfg) and re.search(r'0x1,\s*/\* xlnx,include-sg \*/',cfg),'DMA BSP settings')
  app=client.create_app_component(name='tracegpu_app',platform=str(exported/'tracegpu_platform.xpfm'),domain='standalone_a9',template='empty_application')
  app.import_files(from_loc=str(SW/'src'),dest_dir_in_cmp='src',files_to_exclude=['lscript.ld'])
  app.set_app_config(key='USER_INCLUDE_DIRECTORIES',values=[str(SW/'include')])
  app.set_app_config(key='USER_COMPILE_DEFINITIONS',values=['SDT','PX_GPU_ENABLE','TRACEGPU_AI_BACKEND'])
  app.set_app_config(key='USER_COMPILE_OPTIMIZATION_LEVEL',values=['-O2'])
  app.set_app_config(key='USER_COMPILE_OTHER_FLAGS',values=['-ffunction-sections -fdata-sections'])
  app.set_app_config(key='USER_LINK_LIBRARIES',values=['m'])
  app.set_app_config(key='USER_LINKER_SCRIPT',values=[str(SW/'src/lscript.ld')])
  app_root=ws/'tracegpu_app';elf=app_root/'build/tracegpu_app.elf';mapfile=app_root/'build/tracegpu_app.map'
  app.set_app_config(key='USER_LINK_OTHER_FLAGS',values=['-Wl,--gc-sections -Wl,-Map='+mapfile.as_posix()])
  app.generate_build_files();require(app.clean()==0,'App clean');require(app.build()==0,'App build')
  ninja=(app_root/'build/build.ninja').read_text(encoding='utf-8-sig')
  members=[p for p in (SW/'src').iterdir() if p.suffix in ('.c','.S')]
  for p in members:require(sha(p)==sha(app_root/'src'/p.name) and 'tracegpu_app.elf.dir/'+p.name+'.obj' in ninja,'Member '+p.name)
  for name in ('ai_monitor_main.inc','error_history_main.inc'):
   require(sha(SW/'src'/name)==sha(app_root/'src'/name),'Application glue membership '+name)
  report['memory']=memory(elf,mapfile)
  require(b'TraceGPU v5.1' in elf.read_bytes(),'ELF version identity')
  report['workspace_report']=verify_workspace(ws,xsa)
  save(out/'workspace_report.json',report['workspace_report'])
  for arg,name in (('-lW','program_headers'),('-sW','symbols')):
   (out/('elf_'+name+'.txt')).write_text(subprocess.check_output([str(BIN/'arm-none-eabi-readelf.exe'),arg,str(elf)],text=True),encoding='utf-8')
  for p,name in ((elf,'tracegpu_v5_1.elf'),(mapfile,'tracegpu_v5_1.map')):shutil.copy2(p,out/name)
  require(before=={p.relative_to(SW).as_posix():sha(p) for p in SW.rglob('*') if p.is_file()},'Source changed during build')
  report.update(passed=True,tool='Vitis 2025.2',source_members=len(members),bit_sha256=sha(bit),xsa_sha256=sha(xsa),elf_sha256=sha(out/'tracegpu_v5_1.elf'),map_sha256=sha(out/'tracegpu_v5_1.map'))
  print('TG_V51_SOFTWARE_BUILD_MEMORY_PASS_NOT_BOARD',flush=True)
 finally:
  if client is not None:
   import vitis
   vitis.dispose()
  save(out/'build_report.json',report)
if __name__=='__main__':main()
