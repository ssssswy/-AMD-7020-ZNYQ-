"""Adapt a freshly extracted work copy after validating its signed software inputs."""
from pathlib import Path
import argparse,hashlib,json

HERE=Path(__file__).resolve().parent
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--work',required=True);a=p.parse_args()
    root=Path(a.work).resolve(); evidence=root/'evidence/v49_final_port_manifest.json'
    signed=json.loads((HERE/'report/software/source_inputs.json').read_text(encoding='utf-8'))
    expected=(HERE/'report/preflight_missing_allowlist.txt').read_text(encoding='utf-8').splitlines()
    baseline=json.loads((root/'evidence/v49_baseline_restore.json').read_text(encoding='utf-8'))
    data=json.loads(evidence.read_text(encoding='utf-8'));allowed=set(data['changed_baseline_files'])
    actual=[]
    for n,h in baseline['source_files'].items():
        if n in allowed or n=='dfx_runtime.txt': continue
        f=root/n
        if not f.is_file() or digest(f)!=h:actual.append(n)
    change_record=json.loads((HERE.parent/'changes.json').read_text(encoding='utf-8'))
    archive_entry=next(c for c in change_record['changes'] if c['path']=='fpga/src/source_snapshot.zip')
    changed_docs={m['member']:m['current_sha256'] for m in archive_entry['members'] if m['member'].endswith('.md')}
    extra=set(actual)-set(expected)
    if set(actual)&set(expected)!=set(expected) or not extra.issubset(changed_docs):
        raise RuntimeError('Unexpected baseline differences; inspect manually before proceeding')
    for n in sorted(extra):
        if digest(root/n)!=changed_docs[n]:raise RuntimeError('Changed document does not match recorded derivative '+n)
    for n in expected:
        if not n.startswith('software_v4_0/'):raise RuntimeError('Unexpected non-software input '+n)
        key=n[len('software_v4_0/'):]
        if signed.get(key)!=digest(root/n):raise RuntimeError('Source does not match selected software build '+n)
    receipt=root/'evidence/staging_allowlist_adaptation.json'
    if receipt.exists():raise RuntimeError('Preserve existing receipt')
    data['changed_baseline_files']+=sorted(actual)
    evidence.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    receipt.write_text(json.dumps(dict(scope='Adapted work-copy baseline allowlist after checking paired software source hashes and documented derivative text',
       added=sorted(actual),software_checked=len(expected),derivative_documents_checked=len(extra),original_receipt_unchanged=True,programmed=False,board_accepted=False),indent=2),encoding='utf-8')
    print('Validated and added',len(expected),'software files and',len(extra),'document files to work-copy allowlist')
if __name__=='__main__':main()
