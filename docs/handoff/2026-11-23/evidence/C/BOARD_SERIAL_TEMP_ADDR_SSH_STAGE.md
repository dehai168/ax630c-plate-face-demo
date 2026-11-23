# C-PF-SSH-002 串口临时 151 地址与 SSH 只读

生成时间：2026-11-17T11:38:26+08:00（Asia/Shanghai, UTC+8）

合同：`C/BOARD_SERIAL_TEMP_ADDR_SSH_CONTRACT.json` SHA256 `7612b6311b5848575dc04062e3f551909a5e1da50e2650394e060786327dcf97`（冻结于 2026-11-17T11:30:19+08:00）。结果：`C/BOARD_SERIAL_TEMP_ADDR_SSH_RESULT.json` SHA256 `430d393a3ebb3cd603084bbfbc192438bf9a7bf19d76ee6144034bfc115c0664`。采集摘要：`C/BOARD_SERIAL_TEMP_ADDR_SSH_COLLECT.md`。主机公钥指纹：`C/BOARD_SERIAL_TEMP_ADDR_SSH_HOSTKEY_FPS.json`。这不是发布通过。

## 结论

已在已认证 COM3 shell 上，对 eth0 执行内核 RAM 最小添加 `ip addr add 192.168.151.200/24 dev eth0`（ADD_EXIT=0）。原地址 `10.100.100.101/24`、MAC `00:18:f5:a0:a9:6e`、默认路由 `default via 10.100.100.1 dev eth0` 均保留。未写持久配置，未 flush/replace/restart，未重启。level 在 2026-11-17T11:36:45+08:00 对 `192.168.151.200` ICMP 成功，邻居 MAC 与板卡一致且 Reachable；**TCP/22 失败**。串口 netstat 亦无 22 监听。未启用或安装 sshd，未尝试 SSH 登录。回退命令已记录、未自动执行：`ip addr del 192.168.151.200/24 dev eth0`。

## 串口与身份

- 2026-11-17T11:31:36+08:00：COM3 仍在，PnP 与 R34 同一 Prolific InstanceId；PC 以太网仍为 `192.168.151.152/24`（未改 PC 网卡）。
- 2026-11-17T11:33:24+08:00 打开前已设 DtrEnable=false、RtsEnable=false、Handshake=None。未故意脉冲/break/reset。uptime 仍为 3 days, 18:13，不是本次打开导致的重启。
- 有限观察后一次回车，得到已认证提示符 `/opt #`，未登录、未使用密码。
- 主机名 `Axera`，`Linux Axera 4.19.125 aarch64`，eth0 MAC `00:18:f5:a0:a9:6e`，原 IP `10.100.100.101/24`。确认为该板。

## 地址冲突与添加

- 添加前板卡没有 192.168.151.x。候选仅 `192.168.151.200`（有历史证据），未碰网关 `.1` 或 PC `.152`，未扫子网。
- 2026-11-17T11:34:50+08:00：Get-NetNeighbor 无条目；单目标 SendARP 返回 code=67、无 MAC。仅 ping 无回包不会被当作无冲突证据。
- 2026-11-17T11:35:51–11:36:14+08:00 执行 add。之后 `ip addr` 同时有 10.100.100.101/24 与 192.168.151.200/24。内核自动增加 connected 路由 `192.168.151.0/24 dev eth0 scope link src 192.168.151.200`，默认路由未改。SendARP MAC=`00:18:f5:a0:a9:6e`，无外来冲突，不回退。

## 只读健康（经串口，非 SSH）

板时钟 2026-11-17T11:33:47 CST：

- uptime：3 days, 18:13；load average 23.59, 23.32, 23.20
- `/proc/mounts` 要点：`/dev/mmcblk0p14` 挂于 `/opt`（ext4 rw,discard），并挂于 `/var/mnt/emmc/freq_rw`；`/dev/mmcblk0p13` 挂于 `/var/mnt/emmc/scare_rw`
- **当前实测** ext4 `errors_count`：mmcblk0p13=0，**mmcblk0p14=2168**。历史 R48 也是 2168，这是新测量，不是把旧数拿来顶替。
- 无新的 I/O error 文本。第一次脚本因标记 `__EXT4_ERRORS__` 误匹配已纠正，未因此停止后续授权操作。
- 监听：21/23/80/554/6000/6001/8082/9000(localhost)/9006-9008/10080；无 TCP/22。

## SSH

- `/etc/ssh` 有主机密钥与 `sshd_config`，已只读采集公钥指纹（见指纹文件）。未读私钥。无 dropbear 公钥输出。
- 因无 22 监听，未做 SSH 登录，未改全局 known_hosts，未关闭主机校验，未 AutoAdd。
- 不得把 ICMP 可达写成 SSH 已认证，也不得写成发布通过。

## 持久写入与未测项

- 板端持久文件写入：无。PC 网卡：未改。本轮临时脚本均已从 TEMP 删除，未留秘密文件。
- 未测：SSH 认证与经 SSH 的健康检查；重启后临时地址是否仍在（有意不持久、未重启）；把其他监听端口当作 SSH。

## 下一真正依赖

板上已有 ssh 主机密钥文件，但当前没有 SSH 监听。本执行器不得启动或启用 sshd。临时地址 `192.168.151.200/24` 仍在 RAM，level 可 ICMP 到达。若随后已有 SSH 监听，可用本次串口读取的指纹做内存主机校验后再登录。
