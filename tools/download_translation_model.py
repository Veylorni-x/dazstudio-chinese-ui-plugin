from pathlib import Path
import requests,zipfile,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
meta=json.loads((ROOT/'reports/translation-model-source.json').read_text(encoding='utf-8'))
url=meta['links'][0]
dest=ROOT/'tools/downloads/en_zh.argosmodel'
print('Downloading offline English-Chinese build-time model',flush=True)
with requests.get(url,stream=True,timeout=(20,60)) as r:
 r.raise_for_status()
 with dest.open('wb') as f:
  for data in r.iter_content(1024*1024):f.write(data)
print('Model downloaded',dest.stat().st_size,flush=True)
out=ROOT/'tools/translation-model'
with zipfile.ZipFile(dest) as z:
 for info in z.infolist():
  p=(out/info.filename).resolve()
  if not p.is_relative_to(out.resolve()):raise RuntimeError('Invalid archive path')
 z.extractall(out)
meta['sha256']=hashlib.sha256(dest.read_bytes()).hexdigest()
(ROOT/'reports/translation-model-source.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
print('MODEL_READY',out,flush=True)
