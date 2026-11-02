"""Publish a checked, initially draft GitHub release from an authorized CI run.

Uses the job-scoped GITHUB_TOKEN only in GitHub Actions. Never prints credentials.
Existing published releases are never changed. A failed draft upload can be retried.
"""
import hashlib,json,os,pathlib,re,urllib.error,urllib.parse,urllib.request

root=pathlib.Path(__file__).resolve().parents[1]
if os.environ.get('GITHUB_ACTIONS')!='true' or os.environ.get('GITHUB_REF')!='refs/heads/main':
    raise SystemExit('Release publication requires the manually authorized main-branch CI job')
version=(root/'VERSION').read_text().strip()
if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise SystemExit('Invalid version')
repository=os.environ['GITHUB_REPOSITORY'];commit=os.environ['GITHUB_SHA'];token=os.environ['GH_TOKEN']
tag='v'+version;base='https://api.github.com/repos/'+repository
def request(method,url,value=None,content_type='application/json'):
    data=value if isinstance(value,bytes) else json.dumps(value).encode() if value is not None else None
    req=urllib.request.Request(url,data=data,method=method,headers={
        'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json',
        'X-GitHub-Api-Version':'2022-11-28','Content-Type':content_type,'User-Agent':'AtracDEnc-AAL-release'})
    with urllib.request.urlopen(req,timeout=180) as response:
        raw=response.read();return json.loads(raw) if raw else None
dist=root/'dist'
names=[f'AtracDEnc-AAL-v{version}-{p}.zip' for p in ('windows-x64','macos-universal','source')]+['SHA256SUMS.txt','RELEASE-VALIDATION.json']
assets={name:(dist/name).read_bytes() for name in names}
record=json.loads(assets['RELEASE-VALIDATION.json'])
if record['source_commit']!=commit or not all(x['checks']['passed'] for x in record['native_checks'].values()):
    raise SystemExit('Release validation does not match the checked source')
notes=(root/f'docs/releases/{tag}.md').read_text(encoding='utf-8')
notes+=f"\n\nSource commit: `{commit}`\n\nNative build and headless checks: {record['ci_url']}\n"
try:ref=request('GET',base+'/git/ref/tags/'+tag)
except urllib.error.HTTPError as e:
    if e.code!=404:raise
    ref=request('POST',base+'/git/refs',{'ref':'refs/tags/'+tag,'sha':commit})
if ref['object']['sha']!=commit:raise SystemExit('Release tag does not match the checked commit')
try:release=request('GET',base+'/releases/tags/'+tag)
except urllib.error.HTTPError as e:
    if e.code!=404:raise
    release=request('POST',base+'/releases',{'tag_name':tag,'target_commitish':commit,
        'name':'AtracDEnc-AAL '+version+' — Windows / macOS converters','body':notes,'draft':True,'prerelease':True})
if not release['draft']:raise SystemExit('Refusing to change an already published release')
ref=request('GET',base+'/git/ref/tags/'+tag)
if ref['object']['sha']!=commit:raise SystemExit('Release tag does not match the checked commit')
existing={a['name']:a for a in request('GET',base+f"/releases/{release['id']}/assets")}
for name,data in assets.items():
    digest='sha256:'+hashlib.sha256(data).hexdigest()
    old=existing.get(name)
    if old and old.get('digest')==digest:continue
    if old:request('DELETE',base+f"/releases/assets/{old['id']}")
    url=release['upload_url'].split('{')[0]+'?'+urllib.parse.urlencode({'name':name})
    uploaded=request('POST',url,data,'application/zip' if name.endswith('.zip') else 'application/octet-stream')
    if uploaded.get('state')!='uploaded' or uploaded.get('digest')!=digest:
        raise SystemExit('Asset verification failed; release remains draft: '+name)
verified=request('GET',base+f"/releases/{release['id']}/assets")
for name,data in assets.items():
    found=next((x for x in verified if x['name']==name),None)
    if not found or found.get('digest')!='sha256:'+hashlib.sha256(data).hexdigest():
        raise SystemExit('Asset verification failed; release remains draft: '+name)
published=request('PATCH',base+f"/releases/{release['id']}",{'draft':False,'prerelease':True,'body':notes})
print('Published '+published['html_url'])
