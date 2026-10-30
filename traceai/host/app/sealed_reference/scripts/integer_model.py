"""Batched CPU float64 execution with integer arithmetic contract checks.
All products/sums are exact integers <2**53; every layer enforces INT32 bounds.
Independent NumPy integer ROM interpreter remains the comparison oracle.
"""
import sys
from common import *
import torch
import torch.nn.functional as F
sys.path.insert(0,str(PROJECT/'scripts'))
from v44_rom_reference import RomReference,mem
torch.set_num_threads(4);torch.backends.mkldnn.enabled=False
class BatchRom:
 def __init__(self):self.ref=RomReference(PROJECT);assert self.ref.digest==CFG['model_hash']
 def infer(self,model,x,batch=96):
  r=self.ref;x=np.asarray(x,np.int8).reshape(-1,*r.models[model]['input_shape']);outputs=[]
  for at in range(0,len(x),batch):
   a=x[at:at+batch].astype(np.int64)
   for index in range(8):
    d=r.layers[8*model+index];op,no,ni,length,k,pad,dil,wb,pb=[d[n] for n in ('op','n_out','n_in','ln','kernel','padding','dilation','wb','pb')]
    if op==2:a=np.concatenate((a.max(2),(a.sum(2)+length//2)//length),1);continue
    w=r.weights[wb:wb+no*ni*k].reshape(no,ni,k) if op==0 or model>=4 else r.weights[wb:wb+no*ni].reshape(no,ni)
    bound=int(np.max(np.sum(np.abs(w),axis=tuple(range(1,w.ndim)))))*max(128,int(np.max(np.abs(a))))+int(np.max(np.abs(r.bias[pb:pb+no])))
    assert bound<2**53,'float64 exact-integer contract'
    ta=torch.from_numpy(a.astype(np.float64));tw=torch.from_numpy(w.astype(np.float64));bias=torch.from_numpy(r.bias[pb:pb+no].astype(np.float64))
    z=(F.conv1d(ta,tw,bias,padding=pad,dilation=dil) if op==0 or model>=4 else F.linear(ta.reshape(len(a),-1),tw,bias)).numpy()
    assert np.all(z==np.rint(z)) and np.all((z>=-2**31)&(z<2**31))
    z=z.astype(np.int64)
    if op==3:
     if model<4:out=z
     else:
      residual=z[:,1:]-z[:,:1];out=np.concatenate((np.zeros((len(z),1),np.int64),residual.max(2),residual.max(1)),1)
     assert np.all((out>=-2**31)&(out<2**31));outputs.append(out.astype(np.int32));break
    z=np.maximum(z,0)
    for ch,sh in enumerate(r.shift[pb:pb+no]):
     if sh:z[:,ch]=(z[:,ch]+(1<<(int(sh)-1)))>>int(sh)
    a=np.clip(z,0,127)
    if op==0 and model<4:a=np.maximum(a[:,:,::2],a[:,:,1::2])
  return np.concatenate(outputs) if outputs else np.empty((0,r.models[model]['outputs']),np.int32)
def selftest():
 freeze();m=BatchRom();rng=np.random.default_rng(20261027461);checked=0
 for mid in range(6):
  shape=m.ref.models[mid]['input_shape'];x=rng.integers(-128,128,(7,*shape),dtype=np.int8);want=np.stack([m.ref.infer(mid,v) for v in x]);np.testing.assert_array_equal(m.infer(mid,x),want);checked+=len(x)
 cases=json.loads((PROJECT/'model/r11/manifest.json').read_text())['testcases']
 for c in cases:
  folder=PROJECT/'model/r11';x=mem(folder/(c['stem']+'_input.mem'),8).astype(np.int8);want=mem(folder/(c['stem']+'_logits.mem'),32)
  np.testing.assert_array_equal(m.infer(c['id'],x)[0],want[:m.ref.models[c['id']]['outputs']]);checked+=1
 for mid,name in ((4,'spi'),(5,'i2c')):
  x=np.stack([mem(PROJECT/'model/v43_vectors'/f'{name}_{i:03d}_input.mem',8).astype(np.int8) for i in range(256)])
  want=np.stack([mem(PROJECT/'model/v43_vectors'/f'{name}_{i:03d}_logits.mem',32) for i in range(256)]);np.testing.assert_array_equal(m.infer(mid,x),want);checked+=256
 save(ROOT/'evidence/integer_batch_selftest.json',dict(passed=True,cases=checked,random_full_int8=42,canonical=524,model_hash=m.ref.digest,method='float64 exact integer bound <2**53; INT32 checked every layer; NumPy integer and existing RTL gold comparison',fpga_modified=False));print('EXACT_BATCH_REFERENCE_PASS',checked,flush=True)
if __name__=='__main__':selftest()
