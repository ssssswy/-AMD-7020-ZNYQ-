"""Verify ELF memory ownership, frozen hardware contracts and required software."""
from pathlib import Path
import hashlib,importlib.util,re
R=Path(__file__).resolve().parents[1];S=R/'software_v5_1'
RESERVED={'.capture_history':(0x06000000,0x06800000),'.trigger_history':(0x06800000,0x07000000),
 '.gpu_commands':(0x07200000,0x07300000),'.vector_history':(0x09000000,0x0B000000),
 '.wave_lod':(0x0B000000,0x0B800000),'.error_history':(0x0B800000,0x0B900000),'.error_wave':(0x0B900000,0x0C100000)}
def verify(elf,mapfile):
 spec=importlib.util.spec_from_file_location('elf_parser',R/'hardware_inputs/elf_memory_parser_v35.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 v=m.parse_elf(Path(elf).read_bytes());allocated=[s for s in v['sections'] if s['flags']&2 and s['size']];loads=v['loads']
 m.nonoverlap([(s['name'],s['base'],s['base']+s['size']) for s in allocated]);m.nonoverlap([(str(i),s['base'],s['base']+s['size']) for i,s in enumerate(loads)])
 for name,(lo,hi) in RESERVED.items():
  parts=[s for s in allocated if s['name']==name];assert len(parts)==1,name
  s=parts[0];assert s['type']==8 and s['base']==lo and s['base']+s['size']<=hi and s['align']>=64,name
  if name=='.vector_history':assert s['size']==32*1024*1024
  if name=='.wave_lod':assert s['size']==7*1024*1024
  if name=='.error_wave':assert s['size']==8*1024*1024
 for s in allocated:
  if s['name'] not in RESERVED:assert 0x100000<=s['base'] and s['base']+s['size']<=0x800000,s['name']
 # A zero-file-byte LOAD can span adjacent explicitly owned NOLOAD regions.
 owned=[]
 for lo,hi in sorted(RESERVED.values()):
  if owned and owned[-1][1]==lo:owned[-1]=(owned[-1][0],hi)
  else:owned.append((lo,hi))
 for s in loads:
  if 0x100000<=s['base'] and s['base']+s['size']<=0x800000:continue
  assert s['file_bytes']==0 and any(lo<=s['base'] and s['base']+s['size']<=hi for lo,hi in owned),s
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 assert sha(S/'src/uart_capture_sg.c')==sha(R/'software_v4_0/src/uart_capture_sg.c')
 before=dict(re.findall(r'^#define (\w+) (.+)$',(R/'software_v4_0/include/board_config.h').read_text(),re.M));after=dict(re.findall(r'^#define (\w+) (.+)$',(S/'include/board_config.h').read_text(),re.M))
 assert all(after[n]==value for n,value in before.items() if n not in ('TRACEGPU_WAVE_LOD_BASE','TRACEGPU_WAVE_LOD_LIMIT'))
 for name in ('tg_ai_monitor_configure','tg_ai_monitor_read_event','tg_ai_monitor_snapshot','tg_ai_trigger_agreement','tg_ai_evidence_read','tg_ai_ui_q11','tg_errors_ai','tg_errors_page','tg_snapshots_step','tg_time_format','tg_ui_error_draw'):
  assert name in v['symbols'],name
 for name in ('tg_ai_global_step','tg_ai_bus_features','tg_ai_snapshot_begin','tg_ai_pl_submit','tg_ai_edge_step'):
  assert name not in v['symbols'],name
 program_end=max(s['base']+s['size'] for s in allocated if s['name'] not in RESERVED)
 return dict(passed=True,allocated_sections=allocated,loads=loads,program_end=program_end,margin_to_framebuffer=0x800000-program_end,reserved_regions={n:[lo,hi] for n,(lo,hi) in RESERVED.items()},map_sha256=sha(Path(mapfile)),limits='ELF/link ownership; physical DDR and runtime pressure remain board tests')
