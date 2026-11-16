#!/usr/bin/env bash
set -euo pipefail

if (( $# )); then
	echo "Usage: $0 (no arguments)" >&2
	exit 2
fi

adr_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
adr_python=""
for candidate in python3 /tmp/cprobot-adr-usb/bin/python /tmp/adr-usb-venv/bin/python; do
	if "$candidate" -c 'from serial.tools import list_ports' >/dev/null 2>&1; then
		adr_python="$candidate"
		break
	fi
done
if [[ -z "$adr_python" ]]; then
	python3 -m venv /tmp/cprobot-adr-usb
	adr_python=/tmp/cprobot-adr-usb/bin/python
	"$adr_python" -m pip install -r "$adr_dir/tools/requirements.txt"
fi

adr_port="$("$adr_python" - <<'PY'
import sys
from serial.tools import list_ports

ports = [p.device for p in list_ports.comports() if (p.vid, p.pid) == (0x1A86, 0x7523)]
if len(ports) != 1:
    print("Expected one CyberPi CH340 USB port (1A86:7523); found %d: %s" %
          (len(ports), ", ".join(ports) or "none"), file=sys.stderr)
    print("Connect CyberPi and disconnect any other matching USB adapters.", file=sys.stderr)
    sys.exit(1)
print(ports[0])
PY
)"

echo "Uploading ADR to $adr_port; Ctrl-C ends serial monitoring after upload." >&2
exec "$adr_python" -B "$adr_dir/tools/cyberpi_cli.py" -p "$adr_port" \
	upload "$adr_dir/main.py" --project-name ADR --monitor
