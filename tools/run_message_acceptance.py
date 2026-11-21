"""Verify a translated modal warning with prompts enabled."""
from pathlib import Path
import subprocess,sys,time,json,uuid
ROOT=Path(__file__).resolve().parents[1];REPORTS=ROOT/'reports'
def send(body,timeout=40):
    command=dict(body);command['id']=uuid.uuid4().hex
    temp=REPORTS/'pending-command.json';temp.write_text(json.dumps(command,ensure_ascii=False),encoding='utf8');temp.replace(REPORTS/'command.json')
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        try:
            response=json.loads((REPORTS/'response.json').read_text(encoding='utf8'))
            if response.get('id')==command['id']:
                if not response.get('ok'):raise RuntimeError(json.dumps(response,ensure_ascii=True))
                return response
        except (FileNotFoundError,json.JSONDecodeError):pass
        time.sleep(.2)
    raise TimeoutError(str(body))

subprocess.run([sys.executable,str(ROOT/'tools/launch_test.py'),'--interactive'],check=True)
send({'op':'dump'},45)
dump=json.loads((REPORTS/'ui-dump.json').read_text(encoding='utf-8'))
dialogs=[w for w in dump['widgets'] if w['visible'] and any(k in w['class'] for k in ['Dialog','Wizard','MessageBox'])]
radios=[w for w in dump['widgets'] if w['visible'] and ('Radio' in w['class'] or 'Offline' in w['name'])]
print('MESSAGE_TEST_STARTUP',json.dumps({'dialogs':dialogs[:8],'offline_controls':radios[:12]},ensure_ascii=True),flush=True)
if any(w['visible'] and w['name']=='StartupWizardDlg' for w in dump['widgets']):send({'op':'startupOffline'},45)
send({'op':'message','text':'Could not open the file.'})
dialog=send({'op':'grab','class':'QMessageBox','file':'08-error-message-cn.png'},25)
image=ROOT/'reports/screenshots/08-error-message-cn.png'
if not image.exists():raise RuntimeError('Warning-dialog screenshot missing')
send({'op':'closeDialog'})
send({'op':'quit'})
record={'source':'Could not open the file.','capture':dialog,'screenshot':str(image),'closed':True}
(REPORTS/'message-acceptance.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf8')
print('MESSAGE_DIALOG_VERIFIED',json.dumps(record,ensure_ascii=True),flush=True)
