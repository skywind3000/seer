# 移植 Spec 总览（Phase 3 产出）

- 日期：2026-10-20
- 依据：`docs/porting/audit.md`（A1–A9）+ `docs/porting/decisions.md`
  （D1–D7）
- 分册：`console.md`（A1/A2/A9）、`processinfo.md`（A3）、
  `gdb-glue.md`（A4/A5/A6/A8）。本文件：设计总则、改动全景、
  CMake 规格、gdb 兼容规格、追溯表、验证总纲。

## 设计总则

1. **D2 最小 diff**：对上游既有文件只做小而集中的改动；平台新代码
   唯一的新增源文件是 `src/QProcessInfo_win.cpp`。**不引入平台抽象
   层**——D1 之后 Windows 侧没有第二个 console 实现，抽象层没有
   第二个使用者；散点式小守卫比错层更利于定期 merge 上游。
2. **平台守卫风格**：一律用 Qt 宏 `Q_OS_WIN`（或其取反）；守卫区域
   最小化；`#else` 分支的 Linux 代码**逐字节保持原文**。
3. **既有文件不引 `windows.h`**：`SeerGdbWidget` 的 Windows 分支
   全部走 MI 命令（D5）；Win32 API 只出现在
   `QProcessInfo_win.cpp` 一个文件里。
4. 依 D3/D4：不做被调试程序类型探测，console 行为交给 PE 子系统
   + gdb `new-console`。

## 改动全景（上游 6 个文件、约 60 行 + 新增 1 个文件）

| 文件 | 改动内容 | 对应 | 量级 |
| --- | --- | --- | --- |
| `src/SeerGdbWidget.cpp` | 删 4 行悬空 include；`_gdbProcess` 创建处加 `CREATE_NO_WINDOW`；`handleGdbTerminalDeviceName` Windows 分支发 `new-console`；3 个缺失信号宏的占位定义；`sendGdbInterrupt` Windows 映射；5 处 `/tmp`→`QDir::tempPath()` | A2/A4/A5/A6/A8 | ~30 行 |
| `src/SeerConsoleWidget.cpp` | 守卫 2 个缺失 include + `createTerminal()` 函数体 | A1/A9 | ~8 行 |
| `src/QProcessInfo.cpp` | 两个静态 `populate` 定义包 `#ifndef Q_OS_WIN`，守卫 `pwd.h` include | A3 | ~6 行 |
| `src/SeerCommandLogsWidget.cpp` | console 标签页在 Windows 隐藏 | D1 | ~3 行 |
| `src/SeerMainWindow.cpp` | 4 个不支持信号的菜单项在 Windows 隐藏 | A4 | ~6 行 |
| `src/CMakeLists.txt` | 警告门、`SEER_GDB_NAME` 默认值、新文件入列 | A7 | ~5 行 |
| `src/QProcessInfo_win.cpp`（**新增**） | Toolhelp32 实现两个 `populate`（D6） | A3 | ~130 行 |

关键情报：**`-gdb-set mi-async on` 无需新增**——上游已在
run/attach 启动路径发送（`SeerGdbWidget.cpp:919`、`:1069`，注释
即「Turn on async mode so the 'interrupt' can happen」），Windows
上同样生效（PoC Q2 已实证 18.1/17.1 均接受且 `-exec-interrupt`
有效）。

## CMake 规格（A7）

- `src/CMakeLists.txt:313`：`if(UNIX AND NOT APPLE)` 改为
  `if((UNIX AND NOT APPLE) OR WIN32)`——MinGW 也是 GCC，同一套
  警告选项适用。
- `SEER_GDB_NAME` 默认值（:342 附近）：加 `if(WIN32)` 分支默认
  `"gdb"`（运行时靠 PATH 解析，用户可在 Seer 设置里覆盖）；非
  Windows 保持 `/usr/bin/gdb`。
- `set(SOURCE_FILES ...)`（:177 起）追加 `QProcessInfo_win.cpp`
  一行。该文件整体 `#ifdef Q_OS_WIN` 包裹，非 Windows 平台编译为
  空翻译单元，**不需要 CMake 平台条件**。
- 其余不动：WIN32 GUI 子系统处理（:305-310）已存在且正确。

## gdb 兼容规格（D5 差异清单的应对）

- 地址宽度（16/8 位 hex）：Seer 对 `addr` 一律按 QString 处理，
  无定宽假设——无需改动，Phase 6 验证。
- 寄存器名集合（x86 `eax...` / x64 `rax...`）：寄存器视图由
  `-data-list-register-names` 返回驱动——无需改动，Phase 6 验证。
- `fullname` 分隔符（18.1 正斜杠 / 17.1 反斜杠）：Windows 的
  QFile 两者都接受——无需改动，Phase 6 验证。
- `set non-stop on` 设置项（:923/926）在 Windows 原生 gdb 上未
  验证——默认 off；Phase 6 冒烟确认，若开启异常则在设置页对
  Windows 禁用该项（届时另立小决策）。

## 实施排期映射

- **Phase 4（垂直切片）**：上表全部改动一次落地（含
  `QProcessInfo_win.cpp` 完整实现——它是链接必需项，无法先
  stub）；随后全量编译扫障 + 无 console 核心调试冒烟。
- **Phase 5（收尾）**：audit.md 逐项关账、console 弹窗行为人工
  验证（含 Ctrl-C / stdin 回显）、双 gdb 回归、细节 polish。
- **stub 策略修订**：原计划的「console / 进程信息先 stub 后实现」
  不再需要——console 隐藏即是最终形态（D1），进程信息后端体量
  小直接实现；唯一运行期降级 = 信号菜单部分项隐藏
  （gdb-glue.md §5）。

## 追溯表（audit A1–A9 → spec 章节）

| audit 条目 | spec 位置 |
| --- | --- |
| A1 SeerConsoleWidget PTY | console.md §1 |
| A2 `-inferior-tty-set` 接线 | console.md §2 |
| A3 QProcessInfo /proc | processinfo.md 全文 |
| A4 kill() 中断 | gdb-glue.md §1、§5 |
| A5 悬空 include | gdb-glue.md §2 |
| A6 /tmp 硬编码 | gdb-glue.md §3 |
| A7 CMake 平台门 | 本文件「CMake 规格」 |
| A8 CREATE_NO_WINDOW | gdb-glue.md §4 |
| A9 STDOUT_FILENO | console.md §1（MinGW 可编译，随 D1 消失） |

## 验证总纲（每项改动过两关）

- **Linux 无回归**：`.tmp/build-wsl` 重构 + 冒烟（断点/单步/变量/
  console PTY/中断菜单）。主要保障是 `#else` 分支逐字节原文；唯一
  行为差异点：A6 的 `QDir::tempPath()` 在设置了 `TMPDIR` 的环境下
  路径会变（默认环境输出与 `/tmp` 一致），回归时记录确认。
- **Windows 验证**：`.tmp/build-win64` 构建 + 按 `evidence/poc-*`
  已实证的机制做运行验证（new-console 弹窗、`-exec-interrupt`、
  双 gdb 18.1/17.1）。
