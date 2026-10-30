"""Reassemble checked project archives downloaded as GitHub-sized parts."""
from pathlib import Path
import hashlib,json,os

ROOT=Path(__file__).resolve().parent
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def main():
 manifest=json.loads((ROOT/'large_files.json').read_text(encoding='utf-8'))
 for item in manifest['files']:
  target=ROOT/item['path']
  if target.exists():
   if sha(target)!=item['sha256']:raise RuntimeError('Existing file differs: '+str(target))
   print('Already present:',target.relative_to(ROOT));continue
  tmp=target.with_suffix(target.suffix+'.partial')
  if tmp.exists():raise RuntimeError('Preserve existing temporary file: '+str(tmp))
  h=hashlib.sha256();written=0
  try:
   with tmp.open('xb') as out:
    for part in item['parts']:
     p=ROOT/part['path']
     if not p.is_file() or sha(p)!=part['sha256']:raise RuntimeError('Missing or changed part: '+str(p))
     with p.open('rb') as stream:
      for block in iter(lambda:stream.read(1024*1024),b''):
       out.write(block);h.update(block);written+=len(block)
   if written!=item['bytes'] or h.hexdigest()!=item['sha256']:raise RuntimeError('Assembled file mismatch: '+str(target))
   os.replace(tmp,target)
  except BaseException:
   if tmp.exists():tmp.unlink()
   raise
  print('Restored:',target.relative_to(ROOT))
if __name__=='__main__':main()
