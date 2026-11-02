"""Run offline executable checks; no installed audio components are used."""
import argparse, json, pathlib, subprocess, sys

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--bin-dir', type=pathlib.Path, default=root/'bin')
parser.add_argument('--report', type=pathlib.Path, default=root/'build/offline-checks.json')
args = parser.parse_args()
suffix = '.exe' if sys.platform == 'win32' else ''
commands = [
    ['atracdenc-aal'+suffix, '--self-test-all'],
    ['aal-conformance'+suffix, str(root/'tests/enhancement-vectors.bin')],
    ['aal-conformance'+suffix, str(root/'tests/plus-enhancement-vectors.bin')],
    ['aal-c-api-test'+suffix],
    ['aal-gui-i18n-test'+suffix],
    ['aal-decoder-test'+suffix, str(root/'tests/decoder-vectors.bin')]]
checks = []
for command in commands:
    result = subprocess.run([str(args.bin_dir.resolve()/command[0]), *command[1:]], cwd=root,
                            capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
    checks.append({'program': command[0], 'arguments': [pathlib.Path(x).name if x.endswith('.bin') else x for x in command[1:]],
                   'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
                   'passed': result.returncode == 0})
    print(result.stdout, end='')
    if result.returncode: print(result.stderr, file=sys.stderr)
report = {'project': 'AtracDEnc-AAL', 'version': (root/'VERSION').read_text().strip(),
          'passed': all(x['passed'] for x in checks), 'checks': checks,
          'sony_runtime_used': False, 'hardware_tested': False}
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
if not report['passed']: raise SystemExit(1)
