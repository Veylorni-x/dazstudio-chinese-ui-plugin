"""Build offline draft dictionary, with hand-maintained terminology overriding it."""
from pathlib import Path
import sys,json,re,xml.etree.ElementTree as ET,time
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/pylib'))
import ctranslate2,sentencepiece
APP=Path(r'C:\Program Files\DAZ 3D\DAZStudio6')

def glossary():
    result={}
    for line in (ROOT/'dictionary/glossary.tsv').read_text(encoding='utf-8').splitlines():
        if '\t' in line:
            en,zh=line.split('\t',1); result[en]=zh
    return result

sources=set(json.loads((ROOT/'reports/static-candidates.json').read_text(encoding='utf-8')))
full=ROOT/'reports/full-source-catalog.json'
if full.exists():sources.update(json.loads(full.read_text(encoding='utf-8'))['strings'])
for f in (APP/'resources').rglob('*'):
    if f.suffix.lower() not in ['.ui','.dsx']:continue
    try:tree=ET.parse(f)
    except ET.ParseError:continue
    for node in tree.iter():
        if node.tag=='string' and node.text:sources.add(node.text)
        for key in ['label','text','caption','tooltip','description']:
            if node.get(key):sources.add(node.get(key))
runtime=ROOT/'reports/coverage-runtime.json'
if runtime.exists():
    for row in json.loads(runtime.read_text(encoding='utf-8')).get('entries',[]):sources.add(row['source'])
manual=glossary(); sources.update(manual)
derived={}
for side,zh_side in [('Left','左'),('Right','右')]:
    for part,zh_part in [('Thumb','拇指'),('Index','食指'),('Middle','中指'),('Mid','中指'),('Ring','无名指'),('Pinky','小指')]:
        derived[side+' '+part]=zh_side+zh_part
        for number in range(1,4):derived[f'{side} {part} {number}']=f'{zh_side}{zh_part}第{number}节'
for number in range(1,13):
    derived[f'G9 Eyebrows Card Style {number:02}']=f'G9 发片眉毛款式 {number:02}'
    derived[f'G9 Eyebrows Fiber Style {number:02}']=f'G9 纤维眉毛款式 {number:02}'
for source,meaning in [('Black','黑色'),('Blond','金色'),('Brown','棕色'),('Dark Blue','深蓝色'),('Fuchsia','品红色'),('Grey','灰色'),('Neon Blue','霓虹蓝'),('Red','红色'),('White','白色')]:
    derived['G9 Eyebrows Color '+source]='G9 眉毛颜色：'+meaning
for character,chinese in [('Amala','阿玛拉'),('Fabrice','法布里斯'),('Kat','凯特'),('Laura','劳拉'),('Matt','马特'),('Ty','泰伊')]:
    for part,zh in [('All MAT','完整材质'),('Eyelashes','睫毛'),('Eyes','眼睛'),('Skin MAT','皮肤材质')]:derived[f'{character} G9 {part}']=f'{chinese} G9 {zh}'
manual.update(derived);sources.update(derived)
cache_file=ROOT/'reports/machine-draft-cache.json'
cache=json.loads(cache_file.read_text(encoding='utf-8')) if cache_file.exists() else {}
model=ROOT/'tools/translation-model/translate-en_zh-1_9'
sp=sentencepiece.SentencePieceProcessor(model_proto=(model/'sentencepiece.model').read_bytes())
translator=ctranslate2.Translator('model',files={f.name:f.read_bytes() for f in (model/'model').iterdir() if f.is_file()},device='cpu',compute_type='int8',inter_threads=2,intra_threads=4)
protected=re.compile(r'(<(?:code|pre|script|style)\b[^>]*>[\s\S]*?</(?:code|pre|script|style)>|<[^>]+>|&(?:amp|lt|gt|quot|apos|nbsp);|%L?\d+|%n|%[sdfux]|https?://[^\s<>]+|\n|\t)',re.I)
tasks={}
rejected=[]
for source in sorted(sources):
    if source in cache or source in manual:continue
    if (not re.search('[A-Za-z]',source) or len(source)>6000 or
        re.search(r'^[A-Za-z]:[\\/]|^https?://|^::|^const |^Dz[A-Z]|^Qt[A-Z]|^Q[A-Z][A-Za-z]+[ *&]|^/|::|\b[A-Za-z]+\*',source)):
        rejected.append(source);continue
    parts=protected.split(source)
    for part in parts:
        if not part or protected.fullmatch(part):continue
        clean=re.sub(r'&(?![A-Za-z]+;)','',part).strip()
        if re.search('[A-Za-z]',clean) and clean not in cache and clean not in manual:tasks[clean]=None
todo=sorted(tasks,key=lambda s:(len(s),s))
print('TRANSLATION_BEGIN',len(todo),'segments; sources',len(sources),flush=True)
start=time.time()
for i in range(0,len(todo),64):
    batch=todo[i:i+64]
    encoded=[sp.encode(s,out_type=str)[:500] for s in batch]
    results=translator.translate_batch(encoded,beam_size=3,max_batch_size=2048,batch_type='tokens',max_decoding_length=640,replace_unknowns=True,length_penalty=0.2)
    for source,result in zip(batch,results):
        text=sp.decode(result.hypotheses[0]).replace('\u2581',' ').strip()
        text=re.sub(r'(?<=[\u3400-\u9fff]) +(?=[\u3400-\u9fff，。；：、！？])','',text)
        text=text.replace('渲染器设置','渲染设置').replace('变形','形变').replace('元资料','元数据')
        cache[source]=text
    if i%512==0 or i+64>=len(todo):
        cache_file.write_text(json.dumps(cache,ensure_ascii=False,indent=2),encoding='utf-8')
        print('TRANSLATED',min(i+64,len(todo)),'/',len(todo),'elapsed',round(time.time()-start),flush=True)

out=dict(manual)
for source in sorted(sources):
    if source in out or source in rejected:continue
    parts=protected.split(source);target=[]
    for part in parts:
        if not part:continue
        if protected.fullmatch(part):target.append(part);continue
        clean=re.sub(r'&(?![A-Za-z]+;)','',part).strip()
        translated=manual.get(clean,cache.get(clean,part))
        if part.startswith(' '):translated=' '+translated
        if part.endswith(' '):translated+=' '
        target.append(translated)
    zh=''.join(target)
    hotkey=re.search(r'(?<!&)&([A-Za-z])(?![A-Za-z]*;)',source)
    if hotkey and '&' not in zh:zh+='(&'+hotkey.group(1).upper()+')'
    if sorted(re.findall(r'%L?\d+|%n|%[sdfux]',source))!=sorted(re.findall(r'%L?\d+|%n|%[sdfux]',zh)):
        rejected.append(source);continue
    if zh and zh!=source:out[source]=zh
for source,target in manual.items():out[source]=target
for source,target in cache.items():
    if len(source)>2 and re.search('[\u3400-\u9fff]',target) and not re.match(r'^[A-Za-z]:|^https?://|^/',source):
        out.setdefault(source,target)
folded={key.casefold():value for key,value in manual.items()}
for source in list(out):
    normal=re.sub(r'&(?![A-Za-z]+;)','',source).strip()
    trailing_key=re.search(r'\s*\(([A-Za-z])\)\s*$',normal)
    if trailing_key:normal=normal[:trailing_key.start()].strip()
    normal=re.sub(r'(?:\.\.\.|…|:)$','',normal).strip()
    if normal.casefold() in folded and source not in manual:
        out[source]=folded[normal.casefold()]
        base_source=source[:source.rfind('(')].strip() if trailing_key else source
        if base_source.rstrip().endswith(('...','…')):out[source]+='…'
        if source.rstrip().endswith(':'):out[source]+='：'
        key=re.search(r'(?<!&)&([A-Za-z])(?![A-Za-z]*;)',source)
        if key:out[source]+='(&'+key.group(1).upper()+')'
        elif trailing_key:out[source]+='('+trailing_key.group(1).upper()+')'
    out[source]=out[source].replace('\u2581',' ').strip()
contexts={'ModelDisplay\u0004Index':'食指','Paint:DzPropertyGroupTreeView\u0004Index':'食指','ModelDisplay\u0004Back':'背部','Paint:DzPropertyGroupTreeView\u0004Back':'背部','ComboDisplay\u0004Back':'背面','Paint:QComboBox\u0004Back':'背面','ComboDisplay\u0004Front':'正面','Paint:QComboBox\u0004Front':'正面','ComboDisplay\u0004Top':'顶面','Paint:QComboBox\u0004Top':'顶面','ComboDisplay\u0004Bottom':'底面','Paint:QComboBox\u0004Bottom':'底面'}
ui_aliases={'Smart Content':'智能素材','Filter By Context':'按当前场景筛选','Filter by Context':'按当前场景筛选','Content Library':'素材库','Render Settings':'渲染设置','Draw Settings':'显示设置','Face Transfer':'面部迁移','ActivePose':'交互姿势','Surface Selection':'表面选择','Translate Tool':'平移工具','Node Weight Map Brush':'节点权重贴图画笔','Strand-Based Hair':'发丝毛发','Render Library':'渲染库','Viewport':'视口'}
for source,target in list(out.items()):
    if re.search('[\u3400-\u9fff]',source) and source not in manual:
        del out[source];continue
    pieces=protected.split(target)
    for i,piece in enumerate(pieces):
        if not piece or protected.fullmatch(piece):continue
        for en,zh in ui_aliases.items():piece=re.sub(r'(?<![A-Za-z])'+re.escape(en)+r'(?![A-Za-z])',zh,piece,flags=re.I)
        if re.search(r'\bactor',source,re.I):piece=piece.replace('演员','角色')
        if re.search(r'\bfigure',source,re.I):
            piece=piece.replace('图形','角色模型').replace('该图','该角色').replace('此图','此角色').replace('新图','新角色')
        if re.search('genesis',source,re.I):piece=piece.replace('创世纪','Genesis ').replace('创世记','Genesis ')
        if re.search('bump',source,re.I):piece=piece.replace('弹跳','凹凸').replace('跳跃','凹凸')
        if re.search('material',source,re.I):piece=piece.replace('材料','材质')
        if re.search('headlamp',source,re.I):piece=piece.replace('头印','相机照明').replace('前照灯','相机照明')
        if re.search('temple',source,re.I):piece=piece.replace('神庙','太阳穴')
        if re.search('primitive',source,re.I):piece=piece.replace('初级','基本体')
        if re.search('uncheck',source,re.I):piece=piece.replace('未检查','取消勾选').replace('不检查','取消勾选')
        if re.search('canvas',source,re.I):piece=piece.replace('帆布','画布')
        if re.search('translate tool',source,re.I):piece=piece.replace('翻译工具','平移工具')
        pieces[i]=piece
    out[source]=''.join(pieces)
data={'_meta':{'language':'zh_CN','target':'6.26.2026.30901','glossary_and_derived_entries':len(manual),'derived_entries':len(derived),'draft_entries':len(out)-len(manual),'review':'Offline draft with a curated terminology glossary; refer to the separate acceptance report for checked UI coverage.'},'strings':dict(sorted(out.items())),'contexts':contexts}
(ROOT/'dictionary/zh_CN.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
override=ROOT/'dictionary/overrides.json'
if not override.exists():override.write_text(json.dumps({'_note':'此文件优先级最高。将英文原文与中文译文填写在 strings 内；支持 context + U+0004 + 原文作为 contexts 键。','strings':{},'contexts':{}},ensure_ascii=False,indent=2),encoding='utf-8')
(ROOT/'reports/dictionary-build.json').write_text(json.dumps({'sources':len(sources),'entries':len(out),'manual':len(manual),'excluded':rejected,'elapsed':round(time.time()-start)},ensure_ascii=False,indent=2),encoding='utf-8')
print('DICTIONARY_READY',len(out),'manual',len(manual),flush=True)
