"""Build with portable official MSVC/Windows SDK/Qt development components."""
from pathlib import Path
import os,sys,subprocess,json,hashlib,shutil,argparse
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--qt-root',type=Path,default=ROOT/'tools/qt')
parser.add_argument('--vc-root',type=Path)
parser.add_argument('--sdk-root',type=Path,default=ROOT/'tools/msvc-build/Windows Kits/10')
parser.add_argument('--application',type=Path,default=Path(r'C:\Program Files\DAZ 3D\DAZStudio6'))
parser.add_argument('--target-daz',default='6.26.2026.30901')
args=parser.parse_args()
qt=args.qt_root.resolve()
msvc=ROOT/'tools/msvc-build/Contents/VC/Tools/MSVC'
vc=args.vc_root.resolve() if args.vc_root else sorted(p for p in msvc.iterdir() if p.is_dir())[-1]
sdk=args.sdk_root.resolve()
sdkver=sorted(p.name for p in (sdk/'Include').iterdir() if p.is_dir())[-1]
env=os.environ.copy()
env['PATH']=os.pathsep.join([str(vc/'bin/Hostx64/x64'),str(sdk/'bin'/sdkver/'x64'),str(qt/'bin'),env['PATH']])
env['INCLUDE']=os.pathsep.join([str(vc/'include'),*[str(sdk/'Include'/sdkver/n) for n in ['ucrt','shared','um','winrt','cppwinrt']]])
env['LIB']=os.pathsep.join([str(vc/'lib/x64'),*[str(sdk/'Lib'/sdkver/n/'x64') for n in ['ucrt','um']]])
env['VSCMD_ARG_HOST_ARCH']='x64';env['VSCMD_ARG_TGT_ARCH']='x64'
env['VCToolsInstallDir']=str(vc)+os.sep
env['WindowsSdkDir']=str(sdk)+os.sep
env['WindowsSDKVersion']=sdkver+os.sep
out=ROOT/'build';out.mkdir(exist_ok=True)
includes=[qt/'include',*[qt/'include'/module for module in ['QtCore','QtGui','QtWidgets','QtQuick','QtQuickWidgets','QtQml','QtQmlModels','QtQmlMeta','QtQmlIntegration']]]
defines=['WIN32','_WINDOWS','QT_NO_DEBUG','QT_WIDGETS_LIB','QT_GUI_LIB','QT_CORE_LIB']
subprocess.run([str(qt/'bin/moc.exe'),*[f'-I{p}' for p in includes],*[f'-D{x}' for x in defines],str(ROOT/'source/plugin.h'),'-o',str(out/'moc_plugin.cpp')],cwd=out,env=env,check=True)
mh=ROOT/'source/third_party/minhook/src'
subprocess.run([str(vc/'bin/Hostx64/x64/cl.exe'),'/nologo','/c','/O2','/MD','/DWIN32','/D_WINDOWS',*[str(mh/name) for name in ['buffer.c','hook.c','trampoline.c','hde/hde64.c']]],cwd=out,env=env,check=True)
subprocess.run([str(vc/'bin/Hostx64/x64/cl.exe'),'/nologo','/LD','/O2','/MD','/std:c++17','/Zc:__cplusplus','/permissive-','/EHsc','/utf-8',
                *[f'/I{p}' for p in includes],*[f'/D{x}' for x in defines],str(ROOT/'source/dazzhcn.cpp'),
                str(out/'moc_plugin.cpp'),
                *[str(out/name) for name in ['buffer.obj','hook.obj','trampoline.obj','hde64.obj']],
                '/Fe:'+str(ROOT/'runtime/generic/dazzhcn.dll'),'/link','/LIBPATH:'+str(qt/'lib'),
                'Qt6QuickWidgets.lib','Qt6Quick.lib','Qt6Qml.lib','Qt6Widgets.lib','Qt6Gui.lib','Qt6Core.lib'],cwd=out,env=env,check=True)
runtime=ROOT/'runtime'
for module in ['qtbase','qtdeclarative','qtwebengine','qt_help','qtmultimedia','qtconnectivity','qtlocation','qtserialport','qtwebsockets']:
    name=module+'_zh_CN.qm';shutil.copyfile(qt/'translations'/name,runtime/name)
app=args.application.resolve()
qt_version=subprocess.check_output([str(qt/'bin/qmake.exe'),'-query','QT_VERSION'],env=env,text=True).strip()
data={'target_daz':args.target_daz,'daz_sdk':'6.26.2026.30200','qt':qt_version,'architecture':'x64','msvc':vc.name,'windows_sdk':sdkver,'loading':'Dual-Mode: Daz Studio 6.26 Native SDK Plugin (DzPlugin) + Qt QGenericPlugin',
      'application_core_sha256':hashlib.sha256((app/'DzCore.dll').read_bytes()).hexdigest(),
      'qt_gui_sha256':hashlib.sha256((app/'Qt6Gui.dll').read_bytes()).hexdigest(),
      'qt_widgets_sha256':hashlib.sha256((app/'Qt6Widgets.dll').read_bytes()).hexdigest(),
      'plugin_sha256':hashlib.sha256((runtime/'generic/dazzhcn.dll').read_bytes()).hexdigest()}
(ROOT/'build-info.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
print('PLUGIN_READY',json.dumps(data,ensure_ascii=True),flush=True)
