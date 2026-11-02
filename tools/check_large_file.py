"""Complete large-file/RSS acceptance. Python is a test tool, not a runtime dependency.

Default: deterministic noise PCM >1 GiB, AALP352 encode, complete decode and
exact sample/hash comparison. Fixed-size generation/hashing; no GUI is opened.
Only files created in a new output directory can be removed by this script.
"""
import argparse, ctypes, datetime, hashlib, json, os, pathlib, platform, random, re, struct, subprocess, sys, time

parser=argparse.ArgumentParser()
parser.add_argument('--cli',type=pathlib.Path,required=True)
parser.add_argument('--output-dir',type=pathlib.Path,required=True)
parser.add_argument('--report',type=pathlib.Path,default=pathlib.Path('build/large-file-validation.json'))
parser.add_argument('--pcm-bytes',type=int,default=(1<<30)+(4<<20))
parser.add_argument('--memory-limit-mib',type=int,default=64)
parser.add_argument('--keep-files',action='store_true')
args=parser.parse_args()
if args.pcm_bytes<4 or args.pcm_bytes%4 or args.pcm_bytes>0xffffffff-36:
    raise SystemExit('This RIFF acceptance generator needs 4-byte-aligned PCM up to UINT32_MAX-36.')
cli=args.cli.resolve()
if not cli.is_file():raise SystemExit('CLI not found')
out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=False)
wav=out/'input-noise.wav';oma=out/'encoded.oma';decoded=out/'decoded.wav'
report={'project':'AtracDEnc-AAL','version':(pathlib.Path(__file__).resolve().parents[1]/'VERSION').read_text().strip(),'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'platform':platform.system()+'/'+platform.machine(),'profile':'aalp-352','cli_sha256':hashlib.sha256(cli.read_bytes()).hexdigest(),
        'pcm_bytes':args.pcm_bytes,'sample_frames':args.pcm_bytes//4,'gui_windows_opened':False,
        'memory_limit_mib':args.memory_limit_mib,'runs':[],'passed':False,
        'scope':'CLI processes; no child-process aggregation, GUI, device or total system file-cache measurement.'}
def save():
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
def measure(name,command):
    start=time.monotonic();row={'operation':name,'exit_code':None,'peak_rss_bytes':0,'peak_private_commit_bytes':None}
    stdout=out/(name+'.stdout.log');stderr=out/(name+'.stderr.log')
    if sys.platform=='win32':
        from ctypes import wintypes
        class Counters(ctypes.Structure):
            _fields_=[('cb',wintypes.DWORD),('faults',wintypes.DWORD)]+[(n,ctypes.c_size_t) for n in
                ('peak_ws','ws','peak_paged','paged','peak_nonpaged','nonpaged','commit','peak_commit','private')]
        kernel=ctypes.WinDLL('kernel32',use_last_error=True);psapi=ctypes.WinDLL('psapi',use_last_error=True)
        kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
        kernel.CloseHandle.argtypes=[wintypes.HANDLE];kernel.CloseHandle.restype=wintypes.BOOL
        psapi.GetProcessMemoryInfo.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD]
        psapi.GetProcessMemoryInfo.restype=wintypes.BOOL
        with stdout.open('wb') as log,stderr.open('wb') as errors:
            process=subprocess.Popen(list(map(str,command)),stdin=subprocess.DEVNULL,stdout=log,stderr=errors,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            handle=kernel.OpenProcess(0x400|0x10,False,process.pid)
            if not handle:
                process.kill();process.wait();raise ctypes.WinError(ctypes.get_last_error())
            samples=0;row['peak_private_commit_bytes']=0
            try:
                while True:
                    counters=Counters();counters.cb=ctypes.sizeof(counters)
                    if psapi.GetProcessMemoryInfo(handle,ctypes.byref(counters),counters.cb):
                        samples+=1;row['peak_rss_bytes']=max(row['peak_rss_bytes'],counters.peak_ws)
                        row['peak_private_commit_bytes']=max(row['peak_private_commit_bytes'],counters.peak_commit)
                    if process.poll() is not None:break
                    if time.monotonic()-start>1800:raise TimeoutError(name)
                    time.sleep(.01)
                row['exit_code']=process.wait();row['memory_samples']=samples
            finally:
                kernel.CloseHandle(handle)
                if process.poll() is None:process.kill();process.wait()
    elif sys.platform=='darwin':
        with stdout.open('wb') as log,stderr.open('wb') as errors:
            result=subprocess.run(['/usr/bin/time','-l',*map(str,command)],stdin=subprocess.DEVNULL,
                stdout=log,stderr=errors,env={**os.environ,'LC_ALL':'C'},timeout=1800)
        row['exit_code']=result.returncode
        match=re.search(r'(\d+)\s+maximum resident set size',stderr.read_text(errors='replace'))
        if not match:raise RuntimeError('macOS time did not report maximum resident set size')
        row['peak_rss_bytes']=int(match[1])
    else:raise RuntimeError('Memory acceptance supports Windows and macOS.')
    row['elapsed_seconds']=round(time.monotonic()-start,3)
    row['passed']=row['exit_code']==0 and 0<row['peak_rss_bytes']<=args.memory_limit_mib*(1<<20)
    report['runs'].append(row);save();print(json.dumps(row),flush=True)
    if not row['passed']:raise RuntimeError(name+' failed or exceeded the RSS budget; inspect logs')
def file_sha(path):
    value=hashlib.sha256()
    with path.open('rb') as file:
        while block:=file.read(1<<20):value.update(block)
    return value.hexdigest()
def pcm_sha(path):
    with path.open('rb') as file:
        if file.read(4)!=b'RIFF':raise RuntimeError('Expected ordinary RIFF output in this test')
        file.seek(12)
        while True:
            chunk=file.read(8)
            if len(chunk)!=8:raise RuntimeError('Missing PCM chunk')
            name,size=struct.unpack('<4sI',chunk)
            if name==b'data':
                value=hashlib.sha256();remaining=size
                while remaining:
                    block=file.read(min(1<<20,remaining))
                    if not block:raise RuntimeError('Truncated decoded PCM')
                    value.update(block);remaining-=len(block)
                return size,value.hexdigest()
            file.seek(size+(size&1),1)
save();print('Generating bounded synthetic PCM: '+str(args.pcm_bytes)+' bytes',flush=True)
block=random.Random(0x5a11963).randbytes(65536);value=hashlib.sha256()
with wav.open('xb') as file:
    file.write(struct.pack('<4sI4s4sIHHIIHH4sI',b'RIFF',36+args.pcm_bytes,b'WAVE',b'fmt ',16,1,2,44100,176400,4,16,b'data',args.pcm_bytes))
    remaining=args.pcm_bytes
    while remaining:
        part=block[:min(remaining,len(block))];file.write(part);value.update(part);remaining-=len(part)
expected=value.hexdigest();report['pcm_sha256']=expected;save()
measure('encode',[cli,'--profile','aalp-352',wav,oma])
encoded=json.loads(oma.with_suffix('.oma.json').read_text(encoding='utf-8'))
if encoded['pcm_sha256']!=expected or not encoded['internal_pcm_round_trip']:raise RuntimeError('Encode verification failed')
measure('decode',[cli,'--decode',oma,decoded])
size,digest=pcm_sha(decoded)
if size!=args.pcm_bytes or digest!=expected:raise RuntimeError('Decoded PCM does not exactly match the generated source')
report.update({'input_file_bytes':wav.stat().st_size,'oma_bytes':oma.stat().st_size,'oma_sha256':file_sha(oma),
    'decoded_pcm_bytes':size,'pcm_exact':True,'input_above_1gib':wav.stat().st_size>(1<<30),
    'oma_above_1gib':oma.stat().st_size>(1<<30),'decoded_pcm_above_1gib':size>(1<<30),'passed':True})
save()
if not args.keep_files:
    for file in (wav,oma,oma.with_suffix('.oma.json'),decoded):
        if file.resolve().parent!=out:raise RuntimeError('Refusing to remove a file outside the owned test directory')
        file.unlink()
print('PASS: complete encode/decode, exact PCM and bounded RSS; report '+str(args.report),flush=True)
