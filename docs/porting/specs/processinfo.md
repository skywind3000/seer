# Spec：QProcessInfo 的 Windows 后端（A3，依 D6）

## 结构

- `src/QProcessInfo.cpp`：两个静态 `populate` 函数定义
  （:20-117 `populate(uint32_t)`、:119-143 `populate()`）整体包
  `#ifndef Q_OS_WIN`；`#include <pwd.h>`（:14）加同样的守卫
  （`unistd.h`/`sys/types.h` MinGW 有，保留）。其余成员函数
  （getter/setter）平台无关，不动。
- **新增** `src/QProcessInfo_win.cpp`：文件整体 `#ifdef Q_OS_WIN`
  包裹，实现同名的两个静态成员函数（与 Linux 版在链接期二选一）。
  该文件内可用 `windows.h` / `tlhelp32.h`，是全移植**唯一**的
  Win32 新代码文件。

## Windows 实现要点（按 D6，机制已在 PoC Q3 验证）

- `QProcessInfo::populate()`（枚举全部进程）：
  `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)` +
  `Process32FirstW`/`Process32NextW` 遍历，对每个 pid 调
  `populate(pid)`，返回 `QProcessList`。
- `QProcessInfo::populate(uint32_t pid)`（单个进程）：
  - name：`pe.szExeFile`（进程名）→ `completeBaseName` 语义对齐
    Linux 版；
  - path：`OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, ...)` +
    `QueryFullProcessImageNameW`（失败/拒绝则留空）；
  - username：`OpenProcessToken` + `GetTokenInformation
    (TokenUser)` + `LookupAccountSidW`（失败则留空）；
  - cmdline：**留空**（D6 决策；日后需要再走 PEB/WMI，另立决策）；
  - 参考实现即 PoC 代码（`.tmp/poc-q3/procinfo.cpp`，实测覆盖率：
    path 63%、user 65%，受保护进程字段为空——与 Linux 版 /proc
    不可读时的降级一致）。
- 字符串转换用 `QString::fromWCharArray`；命名与排版遵循
  STYLEGUIDE（4 空格、camelCase、`*` 贴类型）。

## 验证

- Linux 无回归：`#ifndef Q_OS_WIN` 内原文；WSL2 重构 + attach
  对话框进程列表冒烟（条目数、字段、选中 attach）。
- Windows（Phase 5）：进程列表对话框可用、字段 populated、能
  选中进程 attach；用双 gdb（x64/x86）各过一遍 attach 流程。
