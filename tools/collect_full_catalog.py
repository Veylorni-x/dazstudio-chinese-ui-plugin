"""Read-only inventory of installed UI, local help, shader annotations and content labels."""
from pathlib import Path
import os,re,json,gzip,html,collections,xml.etree.ElementTree as ET,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/pylib'))
import pefile
APP=Path(r'C:\Program Files\DAZ 3D\DAZStudio6')
LIB=Path(r'C:\Users\Public\Documents\My DAZ 3D Library')
catalog={};counts=collections.Counter();files=collections.Counter();errors=[]

def add(s,kind,origin):
    if not isinstance(s,str):return
    s=s.strip()
    if not re.search('[A-Za-z]',s) or not 1<len(s)<16000:return
    if re.match(r'^(?:https?://|file://|urn:|[A-Za-z]:[\\/]|#[A-Za-z0-9])',s):return
    if re.fullmatch(r'[a-fA-F0-9\-]{16,}',s):return
    entry=catalog.setdefault(s,{'kinds':[],'origins':[]})
    if kind not in entry['kinds']:entry['kinds'].append(kind);counts[kind]+=1
    if origin not in entry['origins'] and len(entry['origins'])<4:entry['origins'].append(origin)

def xml(f,kind,base):
    try:t=ET.parse(f)
    except (ET.ParseError,UnicodeError) as e:errors.append({'file':str(f),'error':str(e)});return
    for n in t.iter():
        if n.tag.lower() in ['string','label','caption','description','title','tooltip','whatsthis'] and n.text:add(n.text,kind,str(f.relative_to(base)))
        for k,v in n.attrib.items():
            if k.lower() in ['label','text','caption','tooltip','description','title']:add(v,kind,str(f.relative_to(base)))
            if k.upper()=='VALUE' and n.tag.lower() in ['name','product','productname','userfacingname','category','categorypath','contenttype','artist','tag','description','title','label']:
                add(v,kind,str(f.relative_to(base)))
                if '/' in v:
                    for part in v.split('/'):add(part,kind,str(f.relative_to(base)))

def json_labels(f):
    origin=str(f.relative_to(LIB))
    try:
        with f.open('rb') as b:magic=b.read(2)
        opener=gzip.open if magic==b'\x1f\x8b' else open
        # Streaming regex reads label fields without loading geometry arrays.
        carry=''
        with opener(f,'rt',encoding='utf-8-sig',errors='replace') as stream:
            while True:
                part=stream.read(512*1024)
                if not part:break
                data=carry+part
                for match in re.finditer(r'"(label|group|presentation_label|description)"\s*:\s*("(?:[^"\\]|\\.)*")',data):
                    try:label=json.loads(match.group(2))
                    except ValueError:continue
                    add(label,'asset_'+match.group(1),origin)
                    if match.group(1)=='group':
                        for bit in label.split('/'):add(bit,'asset_group',origin)
                carry=data[-32768:]
    except (OSError,ValueError) as e:errors.append({'file':str(f),'error':str(e)})

static=json.loads((ROOT/'reports/static-candidates.json').read_text(encoding='utf-8'))
for s,origins in static.items():
    for origin in origins[:1]:add(s,'compiled_ui',origin)

for f in [APP/'DzCore.dll',*sorted((APP/'plugins').glob('dsp_*.dll'))]:
    pe=pefile.PE(str(f),fast_load=True)
    for section in pe.sections:
        if not section.Name.startswith((b'.rdata',b'.data')):continue
        data=section.get_data()
        for m in re.finditer(rb'(?:[\x09\x0a\x0d\x20-\x7e]\x00){3,6000}',data):
            s=m.group().decode('utf-16-le').strip()
            if ' ' in s or re.fullmatch('[A-Z][a-z]{2,30}',s):add(s,'compiled_ui_unicode',f.name)

for dp,ds,ns in os.walk(APP):
    for name in ns:
        f=Path(dp)/name;suffix=f.suffix.lower();origin=str(f.relative_to(APP))
        if suffix in ['.ui','.dsx']:
            files['ui_xml']+=1;xml(f,'ui_xml',APP)
        elif suffix in ['.htm','.html']:
            files['local_help']+=1
            raw=f.read_text(encoding='utf-8-sig',errors='replace')
            raw=re.sub(r'<(?:script|style)\b.*?</(?:script|style)>','',raw,flags=re.S|re.I)
            for m in re.finditer(r'>([^<>]+)<',raw):add(html.unescape(m.group(1)),'local_help',origin)
        elif suffix=='.dsa':
            files['script_ui']+=1;raw=f.read_text(encoding='utf-8-sig',errors='replace')
            for m in re.finditer(r'(?:(?:qsTr|tr|translate)\s*\(|\.(?:text|label|title|toolTip|whatsThis|caption)\s*=)\s*("(?:[^"\\]|\\.)*")',raw):
                try:s=json.loads(m.group(1))
                except ValueError:continue
                add(s,'script_ui',origin)
        elif suffix=='.mdl':
            files['shader_annotations']+=1;raw=f.read_text(encoding='utf-8-sig',errors='replace')
            for m in re.finditer(r'anno::(?:display_name|description|in_group)\s*\(\s*("(?:[^"\\]|\\.)*")',raw):
                try:s=json.loads(m.group(1))
                except ValueError:continue
                add(s,'shader_annotation',origin)
        elif suffix=='.txt' and any(part in f.parts for part in ['tip pages','guide pages','Lesson Strips','wizard pages']):
            files['local_help_and_tips']+=1
            raw=f.read_text(encoding='utf-8-sig',errors='replace').strip()
            add(raw,'local_help_and_tips',origin)
            for line in raw.splitlines():add(line,'local_help_and_tips',origin)

for dp,ds,ns in os.walk(LIB):
    for name in ns:
        f=Path(dp)/name;suffix=f.suffix.lower()
        if suffix in ['.duf','.dsa','.dse','.dsj'] and not f.is_relative_to(LIB/'data'):
            add(f.stem,'asset_display_name',str(f.relative_to(LIB)))
            for parent in f.relative_to(LIB).parts[:-1]:add(parent,'asset_folder',str(f.relative_to(LIB)))
        if suffix in ['.duf','.dsf']:
            files['asset_data']+=1;json_labels(f)
            if files['asset_data']%500==0:print('READ_CONTENT',files['asset_data'],'catalog',len(catalog),flush=True)
        elif suffix=='.dsx':files['asset_metadata']+=1;xml(f,'asset_metadata',LIB)

runtime=ROOT/'reports/coverage-runtime.json'
if runtime.exists():
    for entry in json.loads(runtime.read_text(encoding='utf-8')).get('entries',[]):add(entry['source'],'runtime_ui',entry['context'])
out={'target':'6.26.2026.30901','scanned_files':dict(files),'source_counts':dict(counts),'strings':catalog,'read_errors':errors}
(ROOT/'reports/full-source-catalog.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print('FULL_CATALOG_READY',len(catalog),json.dumps(dict(counts)), 'errors',len(errors),flush=True)
