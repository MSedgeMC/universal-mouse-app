#!/usr/bin/env python3
"""Run a GUI command on a temporary headless Wayland screen.

Example: python3 tools/virtual-screen.py -- python3 clock/verification/glass-material.py
No nested compositor window is shown on the working desktop. The private
compositor, session bus and runtime files are removed when the command exits.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from xml.sax.saxutils import escape


def private_processes(runtime):
    """Find only children carrying this runner's exact private runtime."""
    marker = ('KUBUNTU_VIRTUAL_SCREEN_RUNTIME='+str(runtime)).encode()
    pids = []
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            if marker in (process/'environ').read_bytes().split(b'\0'):
                pids.append(int(process.name))
        except OSError:
            pass
    return pids


def stop_private_processes(runtime):
    # Some Qt helpers create their own process group. Match the exact private
    # environment instead of touching any process in the working session.
    for sig in (signal.SIGTERM, signal.SIGKILL):
        for pid in private_processes(runtime):
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                pass
        time.sleep(0.15)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--width', type=int, default=1366)
    parser.add_argument('--height', type=int, default=768)
    parser.add_argument('--timeout', type=float, default=120,
                        help='Maximum command runtime in seconds (default: 120).')
    parser.add_argument('--log', type=Path, help='Retain the compositor log at this path.')
    parser.add_argument('--hardware-rendering', action='store_true',
                        help='Use the render node within the private virtual backend for measured GPU checks.')
    parser.add_argument('--kwin-effect', type=Path, action='append', default=[],
                        help='Stage a native effect in the private plugin path; never installs it.')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('Provide a command after --.')
    if args.width <= 0 or args.height <= 0 or args.timeout <= 0:
        parser.error('Width, height and timeout must be positive.')
    for executable in ('kwin_wayland', 'dbus-run-session'):
        if not shutil.which(executable):
            parser.error(f'{executable} is required.')

    original_data = Path(os.environ.get('XDG_DATA_HOME', str(Path.home()/'.local/share')))
    original_config = Path(os.environ.get('XDG_CONFIG_HOME', str(Path.home()/'.config')))
    with tempfile.TemporaryDirectory(prefix='codex-virtual-screen-') as directory:
        base = Path(directory)
        for name in ('runtime', 'config', 'data', 'cache', 'state'):
            (base/name).mkdir(mode=0o700)
        # Only fonts are shared, for accurate rendering; caches and writes
        # stay private. Widget sources are supplied by the test command.
        if (original_data/'fonts').is_dir():
            (base/'data/fonts').symlink_to(original_data/'fonts', target_is_directory=True)

        env = os.environ.copy()
        for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
                     'DBUS_SESSION_BUS_ADDRESS', 'DBUS_SESSION_BUS_PID',
                     'DBUS_STARTER_ADDRESS', 'DBUS_STARTER_BUS_TYPE',
                     'SESSION_MANAGER', 'XDG_SESSION_ID', 'XDG_SESSION_PATH',
                     'XDG_SEAT', 'XDG_SEAT_PATH', 'XDG_VTNR',
                     'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORMTHEME',
                     'QT_QUICK_BACKEND', 'QT_WAYLAND_RECONNECT', 'QT_PLUGIN_PATH'):
            env.pop(name, None)
        env.update({
            'XDG_RUNTIME_DIR': str(base/'runtime'),
            'XDG_CONFIG_HOME': str(base/'config'),
            'XDG_CONFIG_DIRS': '/etc/xdg',
            'XDG_DATA_HOME': str(base/'data'),
            'XDG_DATA_DIRS': '/usr/local/share:/usr/share',
            'XDG_CACHE_HOME': str(base/'cache'),
            'XDG_STATE_HOME': str(base/'state'),
            'XDG_CURRENT_DESKTOP': 'KDE',
            'XDG_SESSION_TYPE': 'wayland',
            'QT_QPA_PLATFORM': 'offscreen',
            'QSG_RHI_BACKEND': 'opengl',
            'LIBGL_ALWAYS_SOFTWARE': '0' if args.hardware_rendering else '1',
            'KWIN_COMPOSE': 'O2',
            'KWIN_WAYLAND_SOCKET': 'codex-test-screen',
            'KUBUNTU_VIRTUAL_SCREEN': '1',
            'KUBUNTU_VIRTUAL_SCREEN_RUNTIME': str(base/'runtime'),
            'KUBUNTU_WORKING_CONFIG': str(original_config),
            'KUBUNTU_WORKING_DATA': str(original_data),
        })
        if args.kwin_effect:
            plugin_root = base/'plugins'
            target = plugin_root/'kwin/effects/plugins'
            target.mkdir(parents=True)
            for plugin in args.kwin_effect:
                if not plugin.is_file() or plugin.suffix != '.so':
                    parser.error('Expected a built native effect .so: '+str(plugin))
                shutil.copy2(plugin, target/plugin.name)
            env['QT_PLUGIN_PATH'] = str(plugin_root)
        (base/'command.json').write_text(json.dumps(command))
        # No service activation: desktop portals can daemonize, mount a FUSE
        # document filesystem and outlive a test. This bus serves only the
        # compositor and explicitly launched clients.
        bus_config = base/'session-bus.conf'
        bus_config.write_text(f'''<busconfig>
  <type>session</type>
  <listen>unix:tmpdir={escape(str(base/'runtime'))}</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
''')
        (base/'config/kwinrc').write_text('[Plugins]\nkwin4_effect_shapecornersEnabled=false\n')
        # KWin starts this only after its virtual output and socket are ready.
        # Its Wayland client env is inherited by the command; no real display
        # or session bus address is available to the test.
        session = base/'session.py'
        session.write_text(f'''#!{sys.executable}
import json, os, subprocess, sys
from pathlib import Path
base = Path(__file__).parent
env = os.environ.copy()
env['QT_QPA_PLATFORM'] = 'wayland'
env.pop('DISPLAY', None)
env.pop('WAYLAND_SOCKET', None)
env['WAYLAND_DISPLAY'] = 'codex-test-screen'
runtime = Path(env['XDG_RUNTIME_DIR'])
assert runtime == base/'runtime'
assert (runtime/env['WAYLAND_DISPLAY']).is_socket()
assert str(runtime) in env['DBUS_SESSION_BUS_ADDRESS']
print(json.dumps({{'virtual_screen': True, 'size': [{args.width}, {args.height}],
    'runtime': str(runtime), 'wayland_display': env['WAYLAND_DISPLAY'],
    'private_session_bus': env['DBUS_SESSION_BUS_ADDRESS']}}), flush=True)
try:
    result = subprocess.run(json.loads((base/'command.json').read_text()), env=env)
    code = result.returncode if result.returncode >= 0 else 128-result.returncode
except OSError as error:
    print(str(error), file=sys.stderr)
    code = 127
(base/'exit-code').write_text(str(code))
sys.exit(code)
''')
        session.chmod(0o700)
        log = base/'compositor.log'
        process = None
        result = 1
        try:
            with log.open('w') as output:
                process = subprocess.Popen([
                    'dbus-run-session', '--config-file', str(bus_config), '--', 'kwin_wayland', '--virtual',
                    '--width', str(args.width), '--height', str(args.height),
                    '--output-count', '1', '--socket', 'codex-test-screen',
                    '--no-lockscreen', '--no-global-shortcuts', '--no-kactivities',
                    '--exit-with-session', str(session),
                ], env=env, stdout=output, stderr=subprocess.STDOUT,
                    start_new_session=True)
                deadline = time.monotonic()+args.timeout
                while process.poll() is None:
                    # A session may finish while KWin is still shutting down.
                    if (base/'exit-code').exists():
                        result = int((base/'exit-code').read_text())
                        break
                    if time.monotonic() >= deadline:
                        print('Virtual-screen command timed out.', file=sys.stderr)
                        result = 124
                        break
                    time.sleep(0.1)
                else:
                    result = int((base/'exit-code').read_text()) if (base/'exit-code').exists() else process.returncode or 1
        except KeyboardInterrupt:
            result = 130
        finally:
            if process is not None:
                if (base/'exit-code').exists():
                    # Let KWin close its rendering/worker threads before
                    # interrupting the group, avoiding late cache writes.
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        pass
                # Stop only the process group created for this virtual session.
                # Also catch background children after the compositor exits.
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)
            stop_private_processes(base/'runtime')
            if log.exists():
                print(log.read_text(), end='')
                if args.log:
                    args.log.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(log, args.log)
            # All writers are stopped; retry a rare final directory-entry
            # race before TemporaryDirectory performs its last cleanup.
            for attempt in range(3):
                try:
                    shutil.rmtree(base)
                    break
                except FileNotFoundError:
                    break
                except OSError:
                    if attempt == 2:
                        raise
                    time.sleep(0.15)
        return result


if __name__ == '__main__':
    sys.exit(main())
