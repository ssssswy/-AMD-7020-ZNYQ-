"""Actual maintained PS decoders, integration/review and inherited error semantics. No board."""
from pathlib import Path
import os,subprocess,hashlib,json,argparse
R=Path(__file__).resolve().parents[1];S=R/'software_v5_1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--attempt',required=True);a=p.parse_args();assert a.attempt.isalnum()
 out=R/f'output/v51_regression_{a.attempt}';out.mkdir(exist_ok=False)
 env=os.environ.copy();env['PATH']='D:/cygwin/bin;'+env.get('PATH','')
 before={p.relative_to(R).as_posix():sha(p) for p in S.rglob('*') if p.is_file()}
 core='capture_vector_stream logic_trace_view logic_trace_index uart_vector_decoder protocol_decode_manager logic_review protocol_bus can_vector_decoder modbus_rtu_decoder rs485_vector_decoder i2c_analysis spi_analysis spi_stream_decoder decode_result_status protocol_search capture_export export_uart_transport uart_event_ring uart_dual_stream uart_burst_index trigger_store trigger_config logic_measure_task uart_capture_dma search_record_adapter'.split()
 cases={
 'contiguous':('test_uart_contiguous_v5_1.c',['uart_vector_decoder']),
 'boundary_controls':('test_uart_boundary_controls_v5_1.c',['uart_vector_decoder']),
 'contiguous_pipeline':('test_uart_contiguous_pipeline_v5_1.c',core),
 'rs485_vector':('test_rs485_vector_v3_7.c',['rs485_vector_decoder','modbus_rtu_decoder','uart_vector_decoder']),
 'modbus':('test_modbus_rtu_v3_7.c',['modbus_rtu_decoder']),
 'logic_complete':('test_logic_complete.c',core),
 'logic_review':('test_logic_review.c',core),
 'logic_eight':('test_logic_eight.c',core),
 'uart_readback':('test_uart_readback.c',['uart_dual_hw','capture_vector_hw']),
 }
 results=[]
 try:
  for name,(test,modules) in cases.items():
   files=[S/'tests'/test,*[S/'src'/f'{n}.c' for n in modules]]
   if test=='test_logic_complete.c':
    # Old optional SPI adapter fixture predates the VECTOR ABI identity fields.
    fixture=files[0].read_text();old='ring=(tracegpu_vector_ring){records,8192,n,n,n,0,0,0,0};'
    assert fixture.count(old)==1
    fixture=fixture.replace(old,'ring=(tracegpu_vector_ring){records,8192,n,n,n,0,0,0,0,TRACEGPU_VECTOR_VERSION,8,15};')
    files[0]=out/'logic_complete_fixture.c';files[0].write_text(fixture)
   if test=='test_uart_readback.c':
    # Supply the current capability registers in the old MMIO fixture.
    fixture='#include "capture_vector_abi.h"\n'+files[0].read_text()
    marker=' for(i=0;i<4;i++)if(a>=bases[i]'
    assert fixture.count(marker)==1
    fixture=fixture.replace(marker,' if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x80)return TRACEGPU_LOGIC_CHANNELS;\n if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x84)return TRACEGPU_UART_ENGINES;\n if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x88)return TRACEGPU_LOGIC_MASK;\n if(a==TRACEGPU_CAPTURE_CONTROL_BASE+0x8c)return TRACEGPU_VECTOR_VERSION;\n'+marker)
    files[0]=out/'uart_readback_fixture.c';files[0].write_text(fixture)
   cmd=['D:/cygwin/bin/gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wl,--stack,67108864','-I'+str(S/'include'),'-I'+str(S/'tests'),'-I'+str(S/'tests/mocks'),*map(str,files),'-lm','-o',str(out/(name+'.exe'))]
   (out/(name+'_command.json')).write_text(json.dumps(cmd,indent=2))
   q=subprocess.run(cmd,capture_output=True,text=True,env=env,encoding='utf8',errors='replace');(out/(name+'_compile.log')).write_text(q.stdout+q.stderr,encoding='utf8');assert q.returncode==0,q.stdout+q.stderr
   q=subprocess.run([str(out/(name+'.exe'))],capture_output=True,text=True,env=env,encoding='utf8',errors='replace',timeout=180);(out/(name+'_test.log')).write_text(q.stdout+q.stderr,encoding='utf8');assert q.returncode==0,q.stdout+q.stderr
   results.append(dict(name=name,passed=True,executable_sha256=sha(out/(name+'.exe')),stdout=q.stdout));print(name,q.stdout[-1800:],flush=True)
 finally:
  unchanged=all(sha(R/n)==h for n,h in before.items())
  report=dict(passed=len(results)==len(cases) and unchanged,version='5.1',tests=results,source_inputs=before,maintained_sources_unchanged=unchanged,programmed=False,scope='Host C tests and actual PS decode/review pipeline, no physical board/PL simulation')
  (out/'result.json').write_text(json.dumps(report,indent=2))
 assert report['passed']
if __name__=='__main__':main()
