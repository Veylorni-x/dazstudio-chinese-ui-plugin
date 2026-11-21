from pathlib import Path
import subprocess,os,json,shutil,datetime,sys
ROOT=Path(__file__).resolve().parents[1]
APP=Path(r'C:\Program Files\DAZ 3D\DAZStudio6')
data=Path(os.environ['APPDATA'])/'DAZ 3D/Studio6'
backup=ROOT/'reports/original-profile-backup'
backup.mkdir(exist_ok=True)
for name in ['actions.dsx','customactions.dsx','layout.dse','layout.dsx','menus.dsx','toolbars.dsx','StartupScript.dsa']:
 p=data/name
 if p.exists() and not (backup/name).exists():shutil.copy2(p,backup/name)
env=os.environ.copy()
env['DAZ_ZHCN_ROOT']=str(ROOT);env['DAZ_ZHCN_TEST']='1'
(ROOT/'settings.json').write_text('{"chinese":true}',encoding='utf-8')
env['QT_PLUGIN_PATH']=str(ROOT/'runtime')
env['QT_QPA_GENERIC_PLUGINS']='dazzhcn'
env['QT_FORCE_STDERR_LOGGING']='1'
if '--baseline' in sys.argv:
 env.pop('QT_QPA_GENERIC_PLUGINS',None)
if '--translator-only' in sys.argv:env['DAZ_ZHCN_TRANSLATOR_ONLY']='1'
script=Path(os.environ['PUBLIC'])/'Documents/DazZhCNAcceptance/bridge.dsa'
script.parent.mkdir(parents=True,exist_ok=True)
source=(ROOT/'tools/test_bridge.dsa').read_text(encoding='utf-8')
source=source.replace('__DAZ_ZHCN_ROOT__',ROOT.as_posix())
source=''.join(('\\u%04x'%ord(c)) if ord(c)>127 else c for c in source)
script.write_text(source,encoding='ascii')
for name in ['command.json','response.json','script-command.json','script-response.json','script-probe.json','script-probe-error.json','plugin-status.json','ui-dump.json','coverage-runtime.json','popup-screenshot.json']:
 (ROOT/'reports'/name).unlink(missing_ok=True)
instance='DazZhCNTest'+datetime.datetime.now().strftime('%H%M%S')
args=[str(APP/'DAZStudio.exe'),'-instanceName',instance,'-copyAppSettings','~','-copySessionUI','~']
if '--interactive' not in sys.argv:args.append('-noPrompt')
args.append(str(script))
process=subprocess.Popen(args,env=env,cwd=APP,stdout=(ROOT/'reports/daz-test-stdout.log').open('wb'),stderr=(ROOT/'reports/daz-test-stderr.log').open('wb'))
(ROOT/'reports/test-process.json').write_text(json.dumps({'pid':process.pid,'instance':instance,'args':args,'start':datetime.datetime.now().isoformat()},ensure_ascii=False,indent=2),encoding='utf-8')
subprocess.Popen([sys.executable,str(ROOT/'tools/exit_monitor.py'),str(process.pid),str(ROOT/'reports/interactive-exit.json')],cwd=ROOT,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
print('DAZ_STARTED',process.pid)
