#!/bin/sh
set -eu

snapshot_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
case $(uname -m) in
	x86_64) ;;
	*) echo "This snapshot requires x86-64 Linux." >&2; exit 1 ;;
esac

# These requirements come from the bundled host runtime, not the simulator.
kernel_release=$(uname -r)
kernel_major=${kernel_release%%.*}
kernel_rest=${kernel_release#*.}
kernel_minor=${kernel_rest%%.*}
kernel_minor=${kernel_minor%%-*}
if [ "$kernel_major" -lt 6 ] || {
	[ "$kernel_major" -eq 6 ] && [ "$kernel_minor" -lt 1 ];
}; then
	echo "This snapshot's bundled runtime requires Linux 6.1 or newer." >&2
	exit 1
fi
cpu_flags=$(grep -m 1 '^flags[[:space:]]*:' /proc/cpuinfo || true)
for feature in avx avx2 bmi1 bmi2 f16c fma movbe xsave; do
	case " $cpu_flags " in
		*" $feature "*) ;;
		*) echo "This snapshot requires an x86-64-v3 CPU; missing $feature." >&2; exit 1 ;;
	esac
done

# Use our matching loader/libc pair. Include common distribution library paths
# so the destination's Vulkan ICD can resolve its own driver dependencies.
runtime_path="$snapshot_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}:/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:/lib64:/usr/lib64:/lib:/usr/lib"
LD_LIBRARY_PATH=$runtime_path
export LD_LIBRARY_PATH
if [ "${1-}" = --check-runtime ]; then
	exec "$snapshot_dir/lib/ld-linux-x86-64.so.2" --inhibit-cache \
		--library-path "$runtime_path" --list "$snapshot_dir/bin/robot_feature_lifecycle"
fi
exec "$snapshot_dir/lib/ld-linux-x86-64.so.2" --inhibit-cache \
	--library-path "$runtime_path" "$snapshot_dir/bin/robot_feature_lifecycle" "$@"
