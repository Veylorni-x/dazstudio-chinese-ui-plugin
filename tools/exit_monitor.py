"""Record the final exit status of an owned Daz acceptance process."""
import ctypes,ctypes.wintypes as w,sys,json,time
from pathlib import Path
pid=int(sys.argv[1]);output=Path(sys.argv[2])
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
k.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD];k.WaitForSingleObject.restype=w.DWORD
k.GetExitCodeProcess.argtypes=[w.HANDLE,ctypes.POINTER(w.DWORD)]
k.CloseHandle.argtypes=[w.HANDLE]
handle=k.OpenProcess(0x100000|0x1000,False,pid)
if not handle:raise SystemExit('Acceptance process already exited')
try:
    output.write_text(json.dumps({'pid':pid,'observing':True}),encoding='utf-8')
    while k.WaitForSingleObject(handle,1000)==258:pass
    code=w.DWORD();k.GetExitCodeProcess(handle,ctypes.byref(code))
    output.write_text(json.dumps({'pid':pid,'exit_code':code.value,'exit_hex':hex(code.value),'observing':False},indent=2),encoding='utf-8')
finally:k.CloseHandle(handle)
