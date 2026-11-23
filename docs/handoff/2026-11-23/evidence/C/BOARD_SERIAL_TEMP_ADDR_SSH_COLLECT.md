# C-PF-SSH-002 serial read-only collect

Archived_at: 2026-11-17T11:34:37+08:00 (Asia/Shanghai UTC+8)

AT=2026-11-17T11:33:24+08:00 Asia/Shanghai
PHASE=SERIAL_COLLECT_READONLY
PREOPEN DtrEnable=False RtsEnable=False Handshake=None Baud=115200
OPENED=True AT=2026-11-17T11:33:24+08:00 POSTOPEN_DTR=False RTS=False
OBSERVE_NO_WRITE: empty
SENT=one_CR AT=2026-11-17T11:33:26+08:00
AFTER_ONE_CR prompt: /opt #
STATE=HASH_PROMPT (prompt is /opt #, already authenticated; no login attempted)
Board date: Tue Nov 17 11:33:47 CST 2026
uptime: 11:33:47 up 3 days, 18:13, 1 users, load average: 23.59, 23.32, 23.20
uname: Linux Axera 4.19.125 #1 SMP PREEMPT Sat Aug 2 14:38:28 CST 2025 aarch64 GNU/Linux
hostname: Axera
eth0: UP,LOWER_UP mac=00:18:f5:a0:a9:6e inet 10.100.100.101/24 brd 10.100.100.255
lo: 127.0.0.1/8
no 192.168.151.x on board at collect time
default via 10.100.100.1 dev eth0
10.100.100.0/24 dev eth0 scope link src 10.100.100.101
mounts: /dev/root / squashfs ro; /dev/mmcblk0p14 /opt ext4 rw,noatime,nodiratime,discard;
  /dev/mmcblk0p14 /var/mnt/emmc/freq_rw ext4 rw,nosuid,nodev,relatime,discard;
  /dev/mmcblk0p13 /var/mnt/emmc/scare_rw ext4 rw,nosuid,nodev,relatime;
  plus proc/sys/tmpfs/squashfs as recorded in session
ext4 errors_count MEASURED_NOW: mmcblk0p13=0; mmcblk0p14=2168
ssh listen: netstat servers show 554,9006,9007,9008,80,6000,6001,8082,21,10080,127.0.0.1:9000,:::23; NO tcp/22
false_positive_stop: script matched marker __EXT4_ERRORS__ against ext4_error; no actual I/O error text; watchdog printk after prompt is not a boot sequence (uptime 3d18h)
COM3_CLOSED AT=2026-11-17T11:33:40+08:00
persistent_board_writes: none
