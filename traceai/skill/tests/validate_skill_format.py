"""Run the installed skill format validator using an isolated dependency wheel."""
from pathlib import Path
import argparse,contextlib,hashlib,io,json,runpy,sys
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--validator',type=Path,required=True);a=p.parse_args()
 root=Path(__file__).resolve().parents[1];wheel=next((root/'tests/validation_dependencies').glob('pyyaml-6.0.2-*.whl'))
 skill=root/'fpga-build-evidence';old_argv=sys.argv[:];old_path=sys.path[:];output=io.StringIO();code=0
 try:
  sys.path.insert(0,str(wheel));sys.argv=[str(a.validator),str(skill)]
  with contextlib.redirect_stdout(output),contextlib.redirect_stderr(output):
   try:runpy.run_path(str(a.validator),run_name='__main__')
   except SystemExit as e:code=e.code or 0
 finally:sys.argv=old_argv;sys.path[:]=old_path
 result={'passed':code==0,'scope':'Official skill-creator quick_validate format checks only; not FPGA or board validation',
  'validator':str(a.validator),'validator_sha256':hashlib.sha256(a.validator.read_bytes()).hexdigest(),
  'skill_sha256':hashlib.sha256((skill/'SKILL.md').read_bytes()).hexdigest(),
  'isolated_dependency':wheel.name,'dependency_sha256':hashlib.sha256(wheel.read_bytes()).hexdigest(),
  'global_package_installed':False,'output':output.getvalue(),'programmed':False,'board_accepted':False}
 (root/'tests/format_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(output.getvalue().strip());assert result['passed'],result
if __name__=='__main__':main()
