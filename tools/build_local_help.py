from pathlib import Path
import json,re,html
ROOT=Path(__file__).resolve().parents[1]
APP=Path(r'C:\Program Files\DAZ 3D\DAZStudio6')
d=json.loads((ROOT/'dictionary/zh_CN.json').read_text(encoding='utf-8-sig'))['strings']
overrides=json.loads((ROOT/'dictionary/overrides.json').read_text(encoding='utf-8-sig'))['strings'];d.update(overrides)
out=ROOT/'help';out.mkdir(exist_ok=True)
mapping={};translated=0;missing=[]
for source in (APP/'docs').rglob('*'):
 if source.suffix.lower() not in ['.html','.htm']:continue
 raw=source.read_text(encoding='utf-8-sig',errors='replace')
 def replace(m):
  global translated
  text=m.group(1);clean=html.unescape(text).strip()
  if not clean or not re.search('[A-Za-z]',clean):return m.group()
  target=d.get(clean)
  if target is None:missing.append({'file':source.name,'source':clean});return m.group()
  translated+=1
  return '>'+html.escape(target,quote=False)+'<'
 raw=re.sub(r'>([^<>]+)<',replace,raw)
 raw=re.sub(r'charset\s*=\s*[\w-]+','charset=utf-8',raw,flags=re.I)
 # Existing images/styles remain shared with the real installation.
 raw=re.sub(r'<head([^>]*)>',lambda m:m.group()+'<base href="'+source.parent.as_uri()+'/">',raw,count=1,flags=re.I)
 name=source.name
 (out/name).write_text(raw,encoding='utf-8')
 mapping[source.as_posix().lower()]=name
(out/'local-help-map.json').write_text(json.dumps(mapping,ensure_ascii=False,indent=2),encoding='utf-8')
(ROOT/'reports/local-help-build.json').write_text(json.dumps({'files':len(mapping),'translated_text_nodes':translated,'missing':missing},ensure_ascii=False,indent=2),encoding='utf-8')
print('LOCAL_HELP_BUILT',len(mapping),translated,'missing',len(missing))
