"""Package the editable offline language extension, sources and build utilities."""
from pathlib import Path
import json,zipfile,hashlib,argparse
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--candidate',action='store_true');args=parser.parse_args()
if (ROOT/'SAFETY-DISABLED.json').exists() and not args.candidate:raise SystemExit('尚未完成稳定性验收。可用 --candidate 生成候选包。')
DIST=ROOT/'dist';DIST.mkdir(exist_ok=True)
info=json.loads((ROOT/'build-info.json').read_text(encoding='utf-8-sig'))
version=info['target_daz']
files=[]
for folder in ['runtime','dictionary','source','help']:
    for f in (ROOT/folder).rglob('*'):
        if f.is_file() and f.suffix.lower() not in ['.moc','.obj','.exp','.lib','.pdb','.ilk']:
            files.append(f)
for f in ROOT.iterdir():
    if f.is_file() and (f.suffix in ['.cmd','.ps1'] or f.name in ['README.md','README-简体中文.md','.gitignore','LICENSE.txt','THIRD-PARTY-NOTICES.md','build-info.json']):files.append(f)
if args.candidate and (ROOT/'SAFETY-DISABLED.json').exists():files.append(ROOT/'SAFETY-DISABLED.json')
for name in ['build_plugin.py','collect_full_catalog.py','collect_renderer_messages.py','build_dictionary.py','build_local_help.py','package_release.py','launch_test.py','test_bridge.dsa','control.py','download_translation_model.py','run_acceptance.py','run_message_acceptance.py','baseline_original.py','exit_monitor.py']:
    files.append(ROOT/'tools'/name)
for name in ['官方接口来源.md','验收结果.md','acceptance-workflow.json','message-acceptance.json','crash-analysis.json','具体漏翻位置.csv','未核对候选文字.csv','dictionary-build.json','local-help-build.json','translation-model-source.json','original-files-verification.json','paint-adapter.json']:
    f=ROOT/'reports'/name
    if f.exists():files.append(f)
reviewed_screenshots=[
    '02-main-menu-chinese.png','03-character-parameters-zh.png','03-viewport-character-zh.png',
    '04-parameters-options-zh.png','06-render-options-zh.png','07-english.png',
    '08-error-message-cn.png','10-surfaces-zh.png','11-shaping-zh.png',
    '12-posing-zh.png','13-render-settings-zh.png','14-simulation-zh.png'
]
for name in reviewed_screenshots:
    f=ROOT/'reports/screenshots'/name
    if f.exists():files.append(f)
manifest={str(f.relative_to(ROOT)).replace('\\','/'):{'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'bytes':f.stat().st_size} for f in files}
(DIST/'package-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
suffix='-候选待验收' if args.candidate else ''
archive=DIST/f'DazStudio-简体中文-{version}-Windows64-含源码词库{suffix}.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for f in sorted(files):z.write(f,'DazStudio简体中文/'+str(f.relative_to(ROOT)).replace('\\','/'))
    z.write(DIST/'package-manifest.json','DazStudio简体中文/package-manifest.json')
print('PACKAGED',str(archive),archive.stat().st_size,'bytes','files',len(files))
