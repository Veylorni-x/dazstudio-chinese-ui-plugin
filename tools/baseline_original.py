"""Run an original-Daz GUI control pass without the language extension."""
from pathlib import Path
import os,json,subprocess,time,datetime,argparse
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--with-translation',action='store_true');args_cli=parser.parse_args()
APP=Path(r'C:\Program Files\DAZ 3D\DAZStudio6')
reports=ROOT/'reports';folder='candidate-control' if args_cli.with_translation else 'original-baseline';out=reports/folder;out.mkdir(exist_ok=True)
script=Path(os.environ['PUBLIC'])/'Documents/DazZhCNAcceptance/baseline.dsa'
script.parent.mkdir(parents=True,exist_ok=True)
code=r'''
var baselineRoot=__ROOT__;
function bWrite(data){var f=new DzFile(baselineRoot+"/original-baseline/result.json");if(f.open(DzFile.WriteOnly)){var s=JSON.stringify(data,null,2).replace(/[^\x00-\x7f]/g,function(c){return "\\u"+("0000"+c.charCodeAt(0).toString(16)).slice(-4);});f.write(s);f.close();}}
try{var r={version:App.longVersionString,phase:"loading",extension:false};bWrite(r);
 r.loaded=App.getContentMgr().openFile("C:/Users/Public/Documents/My DAZ 3D Library/People/Genesis 9/Genesis 9.duf",true);
 r.nodes=Scene.getNumNodes();var n=Scene.getNode(0);while(n.getNodeParent())n=n.getNodeParent();
 r.name=n.getName();r.label=n.getLabel();n.getXPosControl().setValue(3.25);r.property=n.getXPosControl().getName();r.value=n.getXPosControl().getValue();
 var m=App.getAssetIOMgr();var f=m.findFilter("DzSceneAssetFilter");if(typeof f==="number")f=m.getFilter(f);
 var options=new DzFileIOSettings();options.setBoolValue("compressOutput",false);options.setBoolValue("RunSilent",true);
 r.saveError=Number(m.doSaveWithOptions(f,options,false,baselineRoot+"/original-baseline/scene.duf"));r.phase="saved";bWrite(r);
 var exporter=App.getExportMgr().findExporterByClassName("DzObjExporter");var eo=new DzFileIOSettings();exporter.getDefaultOptions(eo);
 eo.setIntValue("RunSilent",1);eo.setBoolValue("CollectMaps",false);eo.setBoolValue("ConvertMaps",false);eo.setBoolValue("WriteMtllib",false);
 r.exportError=Number(exporter.writeFile(baselineRoot+"/original-baseline/scene.obj",eo));r.phase="completed";bWrite(r);
 App.delayedExit(0);
}catch(e){bWrite({phase:"error",error:String(e),line:e.lineNumber});App.delayedExit(1);}
'''
code=code.replace('__ROOT__',json.dumps(reports.as_posix(),ensure_ascii=True))
if args_cli.with_translation:code=code.replace('/original-baseline/','/candidate-control/').replace('extension:false','extension:true')
script.write_text(code,encoding='ascii')
env=os.environ.copy()
for key in ['DAZ_ZHCN_ROOT','DAZ_ZHCN_TEST','DAZ_ZHCN_TRANSLATOR_ONLY']:
    env.pop(key,None)
plugins=[s for s in env.get('QT_QPA_GENERIC_PLUGINS','').split(',') if s and s!='dazzhcn']
if plugins:env['QT_QPA_GENERIC_PLUGINS']=','.join(plugins)
else:env.pop('QT_QPA_GENERIC_PLUGINS',None)
if args_cli.with_translation:
 env['DAZ_ZHCN_ROOT']=str(ROOT);env['QT_PLUGIN_PATH']=str(ROOT/'runtime');env['QT_QPA_GENERIC_PLUGINS']='dazzhcn'
 (ROOT/'settings.json').write_text('{"chinese":true,"paint_adapter":true}',encoding='utf-8')
instance=('DazCandidateControl' if args_cli.with_translation else 'DazOriginalBaseline')+datetime.datetime.now().strftime('%H%M%S')
args=[str(APP/'DAZStudio.exe'),'-instanceName',instance,'-copyAppSettings','~','-copySessionUI','~','-noPrompt',str(script)]
process=subprocess.Popen(args,cwd=APP,env=env,stdout=(out/'stdout.log').open('wb'),stderr=(out/'stderr.log').open('wb'),stdin=subprocess.DEVNULL)
record={'pid':process.pid,'instance':instance,'args':args,'extension_loaded':args_cli.with_translation,'started':datetime.datetime.now().isoformat()}
(out/'process.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print('ORIGINAL_BASELINE_STARTED',process.pid,flush=True)
try:
    process.wait(timeout=100)
    record['exit_code']=process.returncode
except subprocess.TimeoutExpired:
    record['still_running']=True
log=Path(os.environ['APPDATA'])/f'DAZ 3D/Studio6 [{instance}]/log.txt'
if log.exists():
    text=log.read_text(encoding='utf-8',errors='replace');(out/'daz.log').write_text(text,encoding='utf-8')
    record['clean_exit_log']='exited ------------------' in text
result=out/'result.json'
if result.exists():record['workflow']=json.loads(result.read_text(encoding='utf-8'))
(out/'summary.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print('ORIGINAL_BASELINE_RESULT',json.dumps(record,ensure_ascii=True)[:2000],flush=True)
