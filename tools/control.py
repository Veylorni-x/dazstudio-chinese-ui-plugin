"""Send a bounded test command through the plugin or Daz Script file interface."""
from pathlib import Path
import sys,json,uuid,time
ROOT=Path(__file__).resolve().parents[1]
kind=sys.argv[1]
if kind=='script':command={'code':sys.argv[2]};src='script-command.json';dest='script-response.json'
else:command=json.loads(sys.argv[2]);src='command.json';dest='response.json'
command['id']=uuid.uuid4().hex
temp=ROOT/'reports'/('pending-'+src)
temp.write_text(json.dumps(command,ensure_ascii=False),encoding='utf-8');temp.replace(ROOT/'reports'/src)
end=time.time()+30
while time.time()<end:
 try:
  response=json.loads((ROOT/'reports'/dest).read_text(encoding='utf-8'))
  if response.get('id')==command['id']:
   if 'data' in response:response['data']='Saved reports/ui-dump.json'
   print(json.dumps(response,ensure_ascii=True));sys.exit(0 if response.get('ok') else 1)
 except (FileNotFoundError,json.JSONDecodeError):pass
 time.sleep(0.2)
print('COMMAND_TIMEOUT',command['id']);sys.exit(2)
