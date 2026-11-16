"""Exercise the Bash uploader with fake port enumeration and a stubbed CLI."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'upload.sh'


class UploadScriptTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'serial/tools').mkdir(parents=True)
        (self.root / 'serial/__init__.py').write_text('')
        (self.root / 'serial/tools/__init__.py').write_text('')
        (self.root / 'serial/tools/list_ports.py').write_text(
            'import json, os\nfrom types import SimpleNamespace\n'
            'def comports():\n'
            '    return [SimpleNamespace(**p) for p in json.loads(os.environ["ADR_TEST_PORTS"])]\n')
        self.command = self.root / 'command.txt'
        wrapper = self.root / 'python3'
        wrapper.write_text('''#!/usr/bin/env bash
if [[ "$1" == "-B" ]]; then
    printf '%s\\n' "$@" > "$ADR_TEST_COMMAND"
    exit "${ADR_TEST_UPLOAD_EXIT:-0}"
fi
exec "$ADR_TEST_PYTHON" "$@"
''')
        wrapper.chmod(0o755)
        self.env = dict(os.environ, PATH=str(self.root) + os.pathsep + os.environ['PATH'],
                        PYTHONPATH=str(self.root), ADR_TEST_PYTHON=sys.executable,
                        ADR_TEST_COMMAND=str(self.command))

    def run_script(self, ports, *args, upload_exit=0):
        env = dict(self.env, ADR_TEST_PORTS=json.dumps(ports), ADR_TEST_UPLOAD_EXIT=str(upload_exit))
        return subprocess.run([str(SCRIPT), *args], cwd=self.root, env=env,
                              capture_output=True, text=True, timeout=10)

    def test_one_matching_device_uploads_current_source_from_another_directory(self):
        result = self.run_script([{'device': '/dev/ttyACM9', 'vid': 1234, 'pid': 5678},
                                  {'device': '/dev/ttyUSB7', 'vid': 0x1a86, 'pid': 0x7523}])
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual(['-B', str(SCRIPT.parent / 'tools/cyberpi_cli.py'), '-p', '/dev/ttyUSB7',
                          'upload', str(SCRIPT.parent / 'main.py'), '--project-name', 'ADR', '--monitor'],
                         self.command.read_text().splitlines())

    def test_no_matching_device_does_not_upload(self):
        result = self.run_script([])
        self.assertEqual(1, result.returncode)
        self.assertIn('found 0: none', result.stderr)
        self.assertFalse(self.command.exists())

    def test_multiple_matching_devices_do_not_upload(self):
        result = self.run_script([{'device': name, 'vid': 0x1a86, 'pid': 0x7523}
                                  for name in ('/dev/ttyUSB0', '/dev/ttyUSB1')])
        self.assertEqual(1, result.returncode)
        self.assertIn('found 2:', result.stderr)
        self.assertFalse(self.command.exists())

    def test_arguments_are_rejected(self):
        result = self.run_script([], 'unexpected')
        self.assertEqual(2, result.returncode)
        self.assertIn('no arguments', result.stderr)
        self.assertFalse(self.command.exists())

    def test_upload_failure_is_propagated(self):
        result = self.run_script([{'device': '/dev/ttyUSB0', 'vid': 0x1a86, 'pid': 0x7523}], upload_exit=7)
        self.assertEqual(7, result.returncode)


if __name__ == '__main__':
    unittest.main()
