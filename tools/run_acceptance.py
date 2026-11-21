"""One consolidated in-app acceptance pass using native Qt and DAZ Script APIs."""
from pathlib import Path
import json,subprocess,sys,time,uuid,traceback
ROOT=Path(__file__).resolve().parents[1]
REPORTS=ROOT/'reports'
def send(kind,body,timeout=75):
    command=dict(body);command['id']=uuid.uuid4().hex
    src='script-command.json' if kind=='script' else 'command.json'
    dest='script-response.json' if kind=='script' else 'response.json'
    temp=REPORTS/('pending-'+src);temp.write_text(json.dumps(command,ensure_ascii=False),encoding='utf-8');temp.replace(REPORTS/src)
    start=time.monotonic()
    while time.monotonic()-start<timeout:
        try:
            response=json.loads((REPORTS/dest).read_text(encoding='utf-8'))
            if response.get('id')==command['id']:
                if not response.get('ok'):raise RuntimeError(json.dumps(response,ensure_ascii=True))
                return response.get('result',response.get('data',response))
        except (FileNotFoundError,json.JSONDecodeError):pass
        time.sleep(.2)
    raise TimeoutError(kind+' '+str(body)[:130])
def script(code,timeout=75):return send('script',{'code':code},timeout)
def wait_menu_capture(filename,timeout=8):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        try:
            capture=json.loads((REPORTS/'popup-screenshot.json').read_text(encoding='utf-8'))
            if capture.get('file')==filename and capture.get('saved'):return capture
        except (FileNotFoundError,json.JSONDecodeError):pass
        time.sleep(.15)
    return {'file':filename,'saved':False}
result={'target':'6.26.2026.30901','phase':'consolidated final acceptance','steps':[]}
def record(name,value):
    result['steps'].append({'name':name,'result':value})
    (REPORTS/'acceptance-workflow.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('ACCEPTANCE',name,json.dumps(value,ensure_ascii=True)[:900],flush=True)
try:
    subprocess.run([sys.executable,str(ROOT/'tools/launch_test.py')],check=True)
    send('ui',{'op':'dump'},timeout=55)
    initial=json.loads((REPORTS/'ui-dump.json').read_text(encoding='utf-8'))
    (REPORTS/'ui-chinese-startup.json').write_text(json.dumps(initial,ensure_ascii=False,indent=2),encoding='utf-8')
    send('ui',{'op':'grab','file':'02-main-menu-chinese.png'})
    record('version',script('({version:App.versionString,language:App.uiLanguage,nodes:Scene.getNumNodes()})'))
    record('native_plugin',script('''(function(){var m=App.getPluginMgr();var p=m.findPlugin("Daz Studio 简体中文 (Simplified Chinese)");
        return {found:!!p,name:p?p.getName():"",author:p?p.getAuthor():"",version:p?p.getVersionString():"",status:p?Number(p.getStatus()):-1,filename:p?p.getFilename():""};})()'''))
    code='''(function(){var path="C:/Users/Public/Documents/My DAZ 3D Library/People/Genesis 9/Genesis 9.duf";
    var ok=App.getContentMgr().openFile(path,true);var roots=[];
    for(var i=0;i<Scene.getNumNodes();i++){var n=Scene.getNode(i);if(!n.getNodeParent())roots.push({name:n.getName(),label:n.getLabel(),type:n.className()});}
    return {loaded:ok,count:Scene.getNumNodes(),roots:roots};})()'''
    loaded=script(code,timeout=120);record('load_genesis9',loaded)
    if not loaded.get('loaded') or loaded.get('count',0)<1:raise RuntimeError('Character not loaded')
    record('adjust_parameter',script('''(function(){var n=Scene.getNode(0);while(n.getNodeParent())n=n.getNodeParent();Scene.setPrimarySelection(n);
        var p=n.getXPosControl();var before=p.getValue();p.setValue(3.25);
        return {name:n.getName(),label:n.getLabel(),propertyName:p.getName(),propertyLabel:p.getLabel(),before:before,after:p.getValue()};})()'''))
    record('parameter_panel',script('(function(){var p=MainWindow.getPaneMgr().findPane("DzParametersPane");p.showPane();return {label:p.getLabel(),visible:p.visible};})()'))
    send('ui',{'op':'grab','file':'03-character-parameters-zh.png'})
    record('viewport_with_character',script('''(function(){var g=MainWindow.getViewportMgr().getActiveViewport().get3DViewport();g.frameCameraOnSelectedNodes();var image=g.captureImage();
        return {width:image.width,height:image.height,saved:image.save(zhRoot+"/reports/screenshots/03-viewport-character-zh.png","PNG")};})()'''))
    for className,panePath,filename in [
        ('DzSurfacesPane','Surfaces','10-surfaces-zh.png'),
        ('DzShapingPane','Shaping','11-shaping-zh.png'),
        ('DzPosingPane','Posing','12-posing-zh.png'),
        ('DzRenderSettingsPane','Render Settings','13-render-settings-zh.png'),
        ('DzSimulationSettingsPane','Simulation Settings','14-simulation-zh.png')]:
        state=script('(function(){var p=MainWindow.getPaneMgr().findPane('+json.dumps(className)+');if(p)p.showPane();return {class:'+json.dumps(className)+',label:p?p.getLabel():"",visible:!!(p&&p.visible)};})()')
        record('panel_'+panePath.replace(' ','_').lower(),state)
        if state.get('visible'):send('ui',{'op':'grab','class':className,'file':filename})
    menu=script('''(function(){function read(m,d){if(d>5)return [];var a=[];for(var i=0;i<m.getNumItems();i++){var x=m.getItem(i);var o={label:x.label,type:x.type};var sub=x.getSubMenu();if(sub)o.children=read(sub,d+1);a.push(o);}return a;}return read(MainWindow.getPaneMgr().findPane("DzParametersPane").getOptionsMenu(),0);})()''')
    record('parameter_options_menu_entries',menu)
    record('parameter_options_menu',send('ui',{'op':'popupPaneMenu','class':'DzParametersPane','file':'04-parameters-options-zh.png','items':menu}))
    record('parameter_options_menu_screenshot',wait_menu_capture('04-parameters-options-zh.png'))
    renderMenu=script('''(function(){function read(m,d){if(d>5)return [];var a=[];for(var i=0;i<m.getNumItems();i++){var x=m.getItem(i);var o={label:x.label,type:x.type};var sub=x.getSubMenu();if(sub)o.children=read(sub,d+1);a.push(o);}return a;}return read(MainWindow.getPaneMgr().findPane("DzRenderSettingsPane").getOptionsMenu(),0);})()''')
    record('render_options_menu_entries',renderMenu)
    record('render_options_menu',send('ui',{'op':'popupPaneMenu','class':'DzRenderSettingsPane','file':'06-render-options-zh.png','items':renderMenu}))
    record('render_options_menu_screenshot',wait_menu_capture('06-render-options-zh.png'))
    send('ui',{'op':'language','chinese':False});send('ui',{'op':'dump'})
    english=json.loads((REPORTS/'ui-dump.json').read_text(encoding='utf-8'));(REPORTS/'ui-english.json').write_text(json.dumps(english,ensure_ascii=False,indent=2),encoding='utf-8')
    record('english_switch',{'language':english['language'],'fileMenu':[a['text'] for a in english['actions'] if a['name']=='File']})
    send('ui',{'op':'grab','file':'07-english.png'})
    send('ui',{'op':'language','chinese':True})
    send('ui',{'op':'dump'})
    chinese=json.loads((REPORTS/'ui-dump.json').read_text(encoding='utf-8'))
    (REPORTS/'ui-chinese-final.json').write_text(json.dumps(chinese,ensure_ascii=False,indent=2),encoding='utf-8')
    record('chinese_visible_interface',{'widgets':len(chinese.get('widgets',[])),'actions':len(chinese.get('actions',[])),'models':len(chinese.get('models',[]))})
    record('state_after_switch',script('''(function(){var n=Scene.getNode(0);while(n.getNodeParent())n=n.getNodeParent();var p=n.getXPosControl();return {name:n.getName(),label:n.getLabel(),propertyName:p.getName(),propertyLabel:p.getLabel(),value:p.getValue()};})()'''))
    path=(REPORTS/'验收场景.duf').as_posix()
    savecode='''(function(){var mgr=App.getAssetIOMgr();var filter=mgr.findFilter("DzSceneAssetFilter");if(typeof filter==="number")filter=mgr.getFilter(filter);
        var settings=new DzFileIOSettings();settings.setBoolValue("compressOutput",false);settings.setBoolValue("RunSilent",true);
        var error=mgr.doSaveWithOptions(filter,settings,false,%s);return {error:Number(error),className:filter.className(),filename:Scene.getFilename()};})()'''%json.dumps(path,ensure_ascii=True)
    record('save_scene',script(savecode))
    if not Path(path).exists():raise RuntimeError('Scene output missing')
    record('reopen_scene',script('''(function(){var ok=App.getContentMgr().openFile(%s,false);var n=Scene.getNode(0);while(n.getNodeParent())n=n.getNodeParent();return {opened:ok,nodes:Scene.getNumNodes(),name:n.getName(),label:n.getLabel(),value:n.getXPosControl().getValue()};})()'''%json.dumps(path,ensure_ascii=True),timeout=120))
    obj=(REPORTS/'验收导出.obj').as_posix()
    record('export_obj',script('''(function(){var e=App.getExportMgr().findExporterByClassName("DzObjExporter");var o=new DzFileIOSettings();e.getDefaultOptions(o);
        o.setIntValue("RunSilent",1);o.setBoolValue("CollectMaps",false);o.setBoolValue("ConvertMaps",false);o.setBoolValue("WriteMtllib",false);o.setBoolValue("WriteVT",true);o.setBoolValue("WriteVN",true);
        var error=e.writeFile(%s,o);return {error:Number(error),exporter:e.className()};})()'''%json.dumps(obj,ensure_ascii=True),timeout=120))
    output=Path(obj)
    if not output.exists():raise RuntimeError('OBJ output missing')
    vertices=sum(line.startswith(b'v ') for line in output.open('rb'))
    record('export_geometry',{'bytes':output.stat().st_size,'vertices':vertices})
    record('english_asset_lookup',script('App.getContentMgr().findFile("People/Genesis 9/Genesis 9.duf")'))
    record('normal_exit_requested',script('App.delayedExit(0); "exit requested"'))
    end=time.monotonic()+25;exit_state={}
    while time.monotonic()<end:
        try:
            exit_state=json.loads((REPORTS/'interactive-exit.json').read_text(encoding='utf-8'))
            if 'exit_code' in exit_state:break
        except (FileNotFoundError,json.JSONDecodeError):pass
        time.sleep(.2)
    record('normal_exit',exit_state)
    if exit_state.get('exit_code')!=0:raise RuntimeError('The translation-enabled acceptance instance did not exit cleanly')
    result['workflow_completed']=True
except Exception as error:
    result['workflow_completed']=False;result['error']=str(error)
    print('ACCEPTANCE_STOP',type(error).__name__,str(error)[:900],flush=True)
finally:
    (REPORTS/'acceptance-workflow.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
