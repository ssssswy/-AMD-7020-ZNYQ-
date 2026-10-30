from common import *
import struct,time
def main():
 freeze();out=ROOT/'features';out.mkdir(exist_ok=False);observed=[json.loads(line) for line in (ROOT/'datasets/observed_config.jsonl').read_text().splitlines()];raw=np.load(ROOT/'datasets/raw.npz')['raw'];assert len(raw)==len(observed)
 source=[ROOT/'scripts/feature_reference.c',*[PROJECT/'software_v4_0/src'/n for n in ('ai_global.c','ai_slots.c','ai_bus_features.c')]]
 run(['D:/cygwin/bin/gcc.exe','-std=c11','-O3','-Wall','-Wextra','-Werror','-I'+str(PROJECT/'software_v4_0/include'),*source,'-lm','-o',out/'reference.exe'],out/'compile.log')
 with (out/'observed_input.bin').open('xb') as f:
  f.write(struct.pack('<I',len(raw)))
  for obs,w in zip(observed,raw):
   c=obs['config'];cfg=[{'uart':0,'spi':1,'i2c':2}[c['protocol']],c['period_ticks'],c['rate_hz'],c['data_bits'],{'N':0,'E':1,'O':2}[c['parity']],c['stop_bits'],c['cpol'],c['cpha'],c['word_bits'],c['setup'],c['hold'],c['glitch'],c['clock'],c['timeout']]
   f.write(struct.pack('<14I',*cfg));f.write(w.tobytes())
 start=time.perf_counter();run([out/'reference.exe','features',out/'observed_input.bin',out/'codes.bin'],out/'export.log');seconds=time.perf_counter()-start
 data=(out/'codes.bin').read_bytes();assert struct.unpack_from('<I',data)[0]==len(raw);offset=4
 global_x=[];global_ids=[];local_x=[];stop_x=[];token_ids=[];role=[];primary=[];centers=[];bx={1:[],2:[]};bid={1:[],2:[]}
 for i in range(len(raw)):
  proto=struct.unpack_from('<I',data,offset)[0];offset+=4
  if proto==0:
   global_x.append(np.frombuffer(data[offset:offset+6144],np.int8).copy());global_ids.append(i);offset+=6144;nt=struct.unpack_from('<I',data,offset)[0];offset+=4
   for _ in range(nt):
    r,p,center=struct.unpack_from('<IId',data,offset);offset+=16;token_ids.append(i);role.append(r);primary.append(p);centers.append(center)
    local_x.append(np.frombuffer(data[offset:offset+1280],np.int8).copy());offset+=1280;stop_x.append(np.frombuffer(data[offset:offset+384],np.int8).copy());offset+=384
  else:
   n=struct.unpack_from('<I',data,offset)[0];offset+=4;bx[proto].append(np.frombuffer(data[offset:offset+n],np.int8).copy());bid[proto].append(i);offset+=n
 assert offset==len(data)
 np.savez_compressed(out/'uart.npz',global_x=np.stack(global_x),global_ids=np.array(global_ids),local_x=np.stack(local_x),stop_x=np.stack(stop_x),token_ids=np.array(token_ids),role=np.array(role),primary=np.array(primary),centers=np.array(centers))
 for proto,name in ((1,'spi'),(2,'i2c')):np.savez_compressed(out/(name+'.npz'),x=np.stack(bx[proto]),ids=np.array(bid[proto]))
 save(out/'manifest.json',dict(windows=len(raw),uart_windows=len(global_ids),local_tokens=len(local_x),bus_windows={str(k):len(v) for k,v in bx.items()},seconds=seconds,source_inputs={str(p):sha(p) for p in source},observed_fields_only=True,compiler='Cygwin GCC -O3',fpga_modified=False))
 print('OBSERVED_C_FEATURES_EXPORTED',len(raw),'tokens',len(local_x),flush=True)
if __name__=='__main__':main()
