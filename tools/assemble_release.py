"""Assemble verified native CI artifacts into a macOS universal release on macOS."""
import argparse, hashlib, json, os, pathlib, shutil, stat, subprocess, sys, zipfile

root=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--artifacts',type=pathlib.Path,required=True)
args=parser.parse_args()
if sys.platform!='darwin': raise SystemExit('Run universal assembly on macOS')
version=(root/'VERSION').read_text().strip()
stage=root/'build/release-native'
stage.mkdir(parents=True,exist_ok=False)
reports={}
packages={}
for platform in ('windows-x64','macos-arm64','macos-x86_64'):
    matches=list((args.artifacts/f'AtracDEnc-AAL-{platform}').glob(f'AtracDEnc-AAL-v{version}-{platform}.zip'))
    if len(matches)!=1: raise SystemExit('Expected one native package: '+platform)
    packages[platform]=matches[0]
    destination=stage/platform;destination.mkdir()
    with zipfile.ZipFile(matches[0]) as archive:
        if archive.testzip(): raise SystemExit('Damaged native artifact')
        for item in archive.infolist():
            path=pathlib.PurePosixPath(item.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in item.filename:
                raise SystemExit('Unsafe artifact entry')
            if stat.S_ISLNK(item.external_attr>>16): raise SystemExit('Unexpected artifact symlink')
            output=pathlib.Path(archive.extract(item,destination))
            if output.is_file(): output.chmod((item.external_attr>>16)&0o777)
    folder=destination/f'AtracDEnc-AAL-v{version}-{platform}'
    info=json.loads((folder/'BUILD-INFO.json').read_text())
    checks=info['offline_checks']
    if checks['version']!=version or not checks['passed'] or len(checks['checks'])!=8:
        raise SystemExit('Missing successful native checks: '+platform)
    for name,digest in info['binary_sha256'].items():
        if hashlib.sha256((folder/name).read_bytes()).hexdigest()!=digest:
            raise SystemExit('Native artifact binary hash mismatch: '+name)
    reports[platform]={'checks':checks,'native_package_sha256':hashlib.sha256(matches[0].read_bytes()).hexdigest()}

bin_dir=stage/'universal';bin_dir.mkdir()
arm=stage/'macos-arm64'/f'AtracDEnc-AAL-v{version}-macos-arm64'
intel=stage/'macos-x86_64'/f'AtracDEnc-AAL-v{version}-macos-x86_64'
shutil.copytree(arm/'AtracDEnc-AAL-GUI.app',bin_dir/'AtracDEnc-AAL-GUI.app')
gui=pathlib.Path('AtracDEnc-AAL-GUI.app/Contents/MacOS/AtracDEnc-AAL-GUI')
def run(*command):
    result=subprocess.run([str(x) for x in command],capture_output=True,text=True,timeout=300)
    if result.returncode:
        raise SystemExit(f"Command failed ({result.returncode}): {' '.join(str(x) for x in command)}\n{result.stdout}{result.stderr}")
    return {'command':[str(command[0]),*[pathlib.Path(str(x)).name if '/' in str(x) else str(x) for x in command[1:]]],
            'passed':True,'stdout':result.stdout,'stderr':result.stderr}
run('/usr/bin/lipo','-create',arm/'atracdenc-aal',intel/'atracdenc-aal','-output',bin_dir/'atracdenc-aal-macos-universal')
run('/usr/bin/lipo','-create',arm/gui,intel/gui,'-output',bin_dir/gui)
for name in ('atracdenc-aal-macos-universal',str(gui)):
    run('/usr/bin/lipo',bin_dir/name,'-verify_arch','arm64','x86_64')
run('/usr/bin/codesign','--force','--sign','-',bin_dir/'atracdenc-aal-macos-universal')
run('/usr/bin/codesign','--force','--sign','-',bin_dir/'AtracDEnc-AAL-GUI.app')
run('/usr/bin/codesign','--verify','--strict',bin_dir/'AtracDEnc-AAL-GUI.app')
universal_checks=[run(bin_dir/'atracdenc-aal-macos-universal','--self-test-all'),run(bin_dir/gui,'--self-test','--lang','en')]
for platform,name in ((arm,'aarch64'),(intel,'x86_64')):
    shutil.copy2(platform/'lib/libatracdenc_aal.a',bin_dir/f'libatracdenc_aal-{name}-macos.a')
dist=root/'dist';dist.mkdir(exist_ok=True)
shutil.copy2(packages['windows-x64'],dist/packages['windows-x64'].name)
subprocess.run([sys.executable,str(root/'tools/package_release.py'),'--platform','macos-universal','--bin-dir',str(bin_dir)],check=True)
subprocess.run([sys.executable,str(root/'tools/package_release.py'),'--platform','source'],check=True)
record={'project':'AtracDEnc-AAL','version':version,'source_commit':os.environ.get('GITHUB_SHA'),
        'ci_url':f"https://github.com/{os.environ.get('GITHUB_REPOSITORY')}/actions/runs/{os.environ.get('GITHUB_RUN_ID')}",
        'native_checks':reports,'universal_arm64_execution':universal_checks,
        'gui_windows_opened':False,'gui_visual_interaction_tested':False,'walkman_hardware_tested':False,
        'macos_signing':'ad-hoc; no Developer ID or notarization',
        'supported_mode':'High-Speed Transfer only; Standard unsupported'}
(dist/'RELEASE-VALIDATION.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Assembled native Windows and macOS universal converters, source, checksums and execution record')
