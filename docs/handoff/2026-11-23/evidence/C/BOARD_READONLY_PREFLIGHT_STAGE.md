# C 只读连接预检与 IF007 归档（2026-11-17T10:26:31+08:00，Asia/Shanghai (UTC+8)）

合同先于任何 machine 命令冻结：[`BOARD_READONLY_PREFLIGHT_CONTRACT.json`](BOARD_READONLY_PREFLIGHT_CONTRACT.json)（2026-11-17T10:25:53+08:00）。完整结果：[`BOARD_READONLY_PREFLIGHT_RESULT.json`](BOARD_READONLY_PREFLIGHT_RESULT.json)。

## C-IF-007

`DONE_OFFLINE_ACTUAL_FUNCTION_MOCK_ONLY`。归档 [`IF007_EVIDENCE/RESULT.md`](IF007_EVIDENCE/RESULT.md) SHA256 `a98a671fa927fe8d6164954d3ce0843e694c7dc39d2762011fd7af6c54a9e5d9`，合同 `a36361a061122263abd929609b08e28b8d272b7b8ee242c7e042a7c289bf0bbc`。52 断言 PASS。不是 NPU / runtime / video。2241e28 六源未改。父已向用户交付。

## 预检 C-PF-001（本轮完成）

- ListMachines：`level` `697781b2-8ff0-4a80-a5ef-b65276461177` connected=true；其余 PC 不可用。
- Machine Shell 原生审批后成功（exit 0，4377 ms），未拒卡、未绕卡。
- 2026-11-17T10:26:04+08:00 当前：仅有 `COM3`，PnP `OK`，FriendlyName `Prolific USB-to-Serial Comm Port (COM3)`，InstanceId 与 R34 相同 `USB\VID_067B&PID_2303\8&34BE1EAC&1&3`。
- 已知工具进程名（putty/plink/python/ssh/socat 等，无 CommandLine）为空。白名单目录 `D:\CodeProjectHub\ai_plate_face` 存在，无 serial/board/uart/com3/com6/bridge 文件名匹配。
- **未打开串口、未发送字节、未改 DTR/RTS、未读凭据、未在板或用户 PC 写文件。**
- 历史（R34）曾用 COM3/115200 打开并发送 newline；该方法本轮禁止。历史能连 ≠ 当前能连。未发现无 DTR/RTS 切换且已 authenticated 的持续桥接。
- 因此 **不读取** mount / findmnt / 内存 / uptime / p14 `errors_count`。历史 2168 **不得**当作本次重测。旧 8 项备份不是系统备份。

## 当前安全可执行范围与下一项

可做：已完成的 PC 侧端口元数据；box 档案。在证明“不切换 DTR/RTS、不破坏生产、且已 authenticated”的只读通道之前，不可做板端 fs/健康读取或任何板上写/重启。

下一项依赖：由父准备或确认该安全通道。若需要登录，父按 vault 规程处理。不向用户重复索取已知 COM3 或密码。
