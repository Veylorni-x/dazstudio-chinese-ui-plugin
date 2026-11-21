"""Collect user-facing diagnostic templates from installed import/render components."""
from pathlib import Path
import re,json,sys,collections
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/pylib'))
import pefile
APP=Path(r'C:\Program Files\DAZ 3D\DAZStudio6')
path=ROOT/'reports/full-source-catalog.json';document=json.loads(path.read_text(encoding='utf-8'));catalog=document['strings']
keywords=re.compile(r'\b(cannot|could not|unable|failed|failure|error|warning|invalid|unsupported|not found|not supported|out of memory|missing|required|incompatible|exception|timeout|cancelled)\b',re.I)
files=set()
for folder in ['libs','renderers','renderplugins','sceneparsers','geometryloaders']:
 files.update((APP/folder).glob('*.dll'))
files.update((APP/'plugins').rglob('dsp_*.dll'))
before=len(catalog);scanned=0
for f in sorted(files):
 try:p=pefile.PE(str(f),fast_load=True)
 except pefile.PEFormatError:continue
 added=0
 for section in p.sections:
  if not section.Name.startswith(b'.rdata'):continue
  for m in re.finditer(rb'(?<=\x00)[\x09\x0a\x0d\x20-\x7e]{10,4096}\x00',section.get_data()):
   s=m.group()[:-1].decode('ascii').strip()
   if not keywords.search(s) or len(s.split())<3:continue
   if re.search(r'::|^\?|^\.\?AV|\\\\|^https?://|[{}]|[A-Za-z]:[\\/]',s):continue
   if not re.search('[a-z]{2}',s):continue
   item=catalog.setdefault(s,{'kinds':[],'origins':[]})
   if 'renderer_import_error' not in item['kinds']:item['kinds'].append('renderer_import_error');added+=1
   origin=str(f.relative_to(APP))
   if origin not in item['origins'] and len(item['origins'])<4:item['origins'].append(origin)
 scanned+=1
 if added:print('DIAGNOSTICS',f.name,added,flush=True)
counts=collections.Counter()
for item in catalog.values():counts.update(item['kinds'])
document['source_counts']=dict(counts);document['scanned_diagnostic_binaries']=scanned
path.write_text(json.dumps(document,ensure_ascii=False,indent=2),encoding='utf-8')
print('DIAGNOSTICS_READY','added',len(catalog)-before,'total',len(catalog),'binaries',scanned,flush=True)
