"""After moving/unpacking maintained sources, bind the two fonts locally.
Run before software build; never runs an FPGA or changes bitstreams/models.
"""
from pathlib import Path
import re,hashlib,json
R=Path(__file__).resolve().parents[1];sw=R/'software_v5_1';changes=[]
for p in (sw/'src').glob('embed_*.S'):
 text=p.read_text();m=re.search(r'\.incbin "([^"]+)"',text);assert m,p
 target=sw/'assets/fonts'/Path(m[1]).name;assert target.is_file() and target.read_bytes()[:4]==b'TGF1'
 revised=text[:m.start(1)]+target.as_posix()+text[m.end(1):]
 if revised!=text:p.write_text(revised);changes.append(str(p.relative_to(R)))
print(json.dumps(dict(rebased=changes,root=str(R),programmed=False),ensure_ascii=False))
