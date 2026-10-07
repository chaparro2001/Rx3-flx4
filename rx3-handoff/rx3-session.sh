#!/bin/bash
# Swap the desktop for the player, and back when the player stops. Runs as root, as unit rx3-desktop (rx3-desktop.service),
# which the "XDJ-RX3" desktop icon starts. The player needs the framebuffer and the sound card to itself, so:
#   1. drop to multi-user.target, which stops the display manager / greeter;
#   2. end the user's graphical sessions, and stop their PipeWire, which would otherwise keep the controller's card;
#   3. start rx3 and wait until the player is gone (ESC held 1 s on a keyboard stops it, see rx3-hotkeys.py);
#   4. back to graphical.target: the desktop comes up again.
. "$(dirname "$(readlink -f "$0")")/rx3-env.sh"
PW="pipewire.socket pipewire-pulse.socket pipewire pipewire-pulse wireplumber"

restore(){
  trap - EXIT TERM INT
  systemctl stop rx3
  logger -t rx3 "session: player stopped, back to the desktop"
  systemctl isolate graphical.target
  systemctl --user -M "$RX3_USER@" start $PW 2>/dev/null
  exit 0
}
trap restore EXIT TERM INT

logger -t rx3 "session: closing the desktop for the player"
systemctl isolate multi-user.target
for s in $(loginctl list-sessions --no-legend 2>/dev/null | awk '{print $1}'); do
  case "$(loginctl show-session -p Type --value "$s" 2>/dev/null)" in
    wayland|x11|mir) loginctl terminate-session "$s";;
  esac
done
systemctl --user -M "$RX3_USER@" stop $PW 2>/dev/null
sleep 1

systemctl start rx3 || exit 1
# rx3 is Type=forking with RemainAfterExit, so it stays "active" even if the player dies: watch the process too.
for i in $(seq 1 30); do pgrep -x rbp-pi >/dev/null && break; sleep 1; done
while systemctl is-active -q rx3 && pgrep -x rbp-pi >/dev/null; do sleep 2; done
