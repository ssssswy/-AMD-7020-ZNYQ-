"""Behavioral self-test using generated local evidence fixtures, without FPGA tools."""
from pathlib import Path
import hashlib,importlib.util,json,subprocess,sys,tempfile,zipfile
ROOT=Path(__file__).resolve().parents[1];SCRIPT=ROOT/'fpga-build-evidence/scripts/verify_bundle.py'
s=importlib.util.spec_from_file_location('bundle',SCRIPT);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def main():
 checks=[]
 def record(name,fn):
  try:fn()
  except Exception as e:checks.append({'name':name,'passed':False,'error':repr(e)})
  else:checks.append({'name':name,'passed':True})
 def eq(a,b):assert a==b,(a,b)
 with tempfile.TemporaryDirectory(prefix='fpga_evidence_test_') as td:
  r=Path(td);bit=r/'test.bit';bit.write_bytes(b'LOCAL_SYNTHETIC_BIT_NOT_VALID_FPGA_PROGRAM\x00\x01')
  xsa=r/'test.xsa'
  with zipfile.ZipFile(xsa,'w') as z:z.writestr('design.bit',bit.read_bytes())
  manifest=r/'manifest.json';manifest.write_text(json.dumps({'artifacts':{'test.bit':m.digest(bit)}}))
  status=r/'status.json';status.write_text(json.dumps({'wns_ns':0.105,'whs_ns':0.035,'drc_errors':0,'drc_critical':0}))
  record('matching_bundle',lambda:eq(m.verify(r,manifest,bit,xsa,status)['passed'],True))
  manifest.write_text(json.dumps({'artifacts':{'test.bit':'0'*64}}))
  record('changed_bytes_fail',lambda:eq(m.verify(r,manifest)['passed'],False))
  manifest.write_text(json.dumps({'artifacts':{'../outside':'0'*64}}))
  record('relative_escape_rejected',lambda:eq(m.verify(r,manifest)['passed'],False))
  manifest.write_text(json.dumps({'artifacts':{str(bit.resolve()):m.digest(bit)}}))
  record('absolute_manifest_path_rejected',lambda:eq(m.verify(r,manifest)['passed'],False))
  manifest.write_text(json.dumps({'artifacts':{'test.bit':'not-a-digest'}}))
  record('malformed_digest_rejected',lambda:eq(m.verify(r,manifest)['passed'],False))
  with zipfile.ZipFile(xsa,'w') as z:z.writestr('design.bit',b'different')
  record('wrong_xsa_pair_fails',lambda:eq(m.verify(r,bit=bit,xsa=xsa)['passed'],False))
  with zipfile.ZipFile(xsa,'w') as z:z.writestr('a.bit',b'a');z.writestr('b.bit',b'b')
  record('multiple_bit_members_fail',lambda:eq(m.verify(r,bit=bit,xsa=xsa)['passed'],False))
  for label,data in [('negative_setup',{'wns_ns':-.001,'whs_ns':.01,'drc_errors':0,'drc_critical':0}),('negative_hold',{'wns_ns':.1,'whs_ns':-.01,'drc_errors':0,'drc_critical':0}),('missing_metric',{'wns_ns':.1}),('nan_metric',{'wns_ns':float('nan'),'whs_ns':.01,'drc_errors':0,'drc_critical':0}),('drc_error',{'wns_ns':.1,'whs_ns':.01,'drc_errors':1,'drc_critical':0}),('drc_critical',{'wns_ns':.1,'whs_ns':.01,'drc_errors':0,'drc_critical':1})]:
   status.write_text(json.dumps(data));record(label,lambda:eq(m.verify(r,status=status)['passed'],False))
  def reject_one_bit():
   try:m.verify(r,bit=bit)
   except ValueError:return
   raise AssertionError('unpaired BIT accepted')
  record('one_bit_without_xsa_rejected',reject_one_bit)
  def no_inputs():
   try:m.verify(r)
   except ValueError:return
   raise AssertionError('empty evidence accepted')
  record('empty_evidence_rejected',no_inputs)
  p=subprocess.run([sys.executable,str(SCRIPT),'--root',str(r),'--bit',str(bit),'--xsa',str(xsa),'--output',str(r/'cli_result.json')],capture_output=True,text=True)
  record('cli_propagates_failure',lambda:eq(p.returncode,1))
 result={'passed':all(c['passed'] for c in checks),'scope':'Generated local fixtures only, not FPGA build or board test','programmed':False,'board_accepted':False,'checks':checks,'source_inputs':{str(SCRIPT.relative_to(ROOT)):m.digest(SCRIPT),str(Path(__file__).relative_to(ROOT)):m.digest(Path(__file__))}}
 (ROOT/'tests/result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps({'passed':result['passed'],'checks':len(checks),'failures':[x for x in checks if not x['passed']]},ensure_ascii=False));assert result['passed']
if __name__=='__main__':main()
