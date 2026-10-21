# 合并红线：不可回退的移植不变量

本文件是**活文档**：合并上游、或改动移植层（console 分流、gdb/mi
链路、路径处理、设置落盘、平台隔离）之前必读。每条红线给出
「红线 + 坑 + 验证 + 出处」；「为什么」的完整历史在
`docs/porting/decisions.md`（ADR，只追加），这里只保留当前有效
的事实。文中行号为 2026-10-21（`v2.8.1-win1`）时的锚点，仅作
定位提示，以内容为准。

## 使用方式

- **上游 merge**（`docs/porting/maintenance.md` §1 流程第 2 步）：
  merge 前通读，冲突解决完、跑回归前逐条核对；
- **日常改动**：碰到条目覆盖的区域，先读对应条目再动手；
- **条目维护**：上游重构使某条失效时改写或标注「作废」（编号不
  重排），新红线追加编号；结构性变化另记 ADR。

## A. 结构性红线（丢失后构建/链接立刻失败，容易被发现）

### I1 — 平台新增文件不得被当作残留删除

- 红线：这些文件上游不存在，merge 碰不到它们，但任何「清理」
  「对齐上游」的操作不得删除：`src/QProcessInfo_win.cpp`（进程
  信息 Windows 后端）、`src/SeerStackTrace.{h,cpp}`（qFatal 崩溃
  栈日志）、`tests/hellopathnorm/`（路径归一回归测试）。
- 坑：`QProcessInfo_win.cpp` 的 `Q_OS_WIN` 宏守卫必须位于任何 Qt
  include 之后——放前面会让整个文件编译成空翻译单元（Phase 4
  实踩，specs/processinfo.md 有记录）。
- 验证：Windows 全量构建 0 error（命令见 `AGENTS.local.md` §6）。
- 出处：D6、D2；specs/processinfo.md。

### I2 — CMake 的 WIN32 段与新增条目

- 红线：`src/CMakeLists.txt` 里 fork 增改的条目不得在冲突解决中
  丢失：源清单中的 `QProcessInfo_win.cpp`（:180）与
  `SeerStackTrace.cpp`（:196）；WIN32 下的
  `-fno-omit-frame-pointer`（:320，SeerStackTrace 栈回溯依赖）；
  `CONFIG_SEER_GDB_NAME` 平台分支（:348——WIN32 默认 `"gdb"` 同
  目录解析，非上游的 `/usr/bin/gdb`）。
- 验证：Windows 全量构建 0 error；包内 gdb 能被默认找到。
- 出处：D12；specs/overview.md；`docs/build-windows.md`。

### I3 — console 分流：`SeerConsoleWidget` 守卫 + console 页隐藏

- 红线：`src/SeerConsoleWidget.cpp` 的 `#ifndef Q_OS_WIN` 守卫
  （:22 / :240 / :278 / :318）把 Linux PTY 实现（`posix_openpt`
  一族、裸 fd 读写、`QSocketNotifier`）圈在非 Windows 侧，
  `#else` 侧保持上游原文；`src/SeerCommandLogsWidget.cpp:179`
  的 `Q_OS_WIN` 块把 GUI 内 console 页在 Windows 下隐藏。
- 坑：上游活跃改动的文件，冲突高发。解决姿势：保上游结构，把
  守卫块重新嵌回。
- 验证：双平台构建（Windows + WSL2）；Windows 会话 GUI 内无
  console 页。
- 出处：D1；specs/console.md。

### I4 — `QProcessInfo.cpp` 的平台分流

- 红线：:14 / :24 / :126 三处 `#ifndef Q_OS_WIN` 分流到
  `QProcessInfo_win.cpp`；Windows 侧进程名用 `fileName()`（带
  .exe，attach 回填显示形态——`completeBaseName()` 是 Phase 5
  修掉的错误）。
- 验证：Windows 构建后进程列表对话框可用、能选中进程 attach。
- 出处：D6；specs/processinfo.md。

### I5 — `SeerGdbWidget.cpp` 的平台块

- 红线：六处 fork 改动不得丢：
  1. :39-44——MinGW CRT 缺失的 POSIX 信号宏占位（`SIGKILL` 等）；
  2. :99-103——gdb 以 `CREATE_NO_WINDOW`（0x08000000u）启动；
  3. :862-867——MI 写侧 Windows `toLocal8Bit`；
  4. :2074-2080——`handleGdbTerminalDeviceName()`：Windows 发
     `-gdb-set new-console on`（不发 `-inferior-tty-set`）；
  5. :3956 起——中断映射：SIGINT→`-exec-interrupt`、SIGKILL→
     gdb CLI `kill`，其余信号走 `qWarning`（Linux 走 `kill()`）；
  6. `src/SeerMainWindow.cpp:126`——中断菜单里 Windows 不可映射
     的信号（SIGFPE/SEGV/USR1/USR2）隐藏。
- 坑：不发 new-console 时 inferior 输出会污染 MI 流（D4 PoC 证
  伪过「不发命令」路线）；`mi-async` 无需守——上游本就在
  run/attach 路径发送。
- 验证：双平台构建 + Windows 会话：console 程序弹独立控制台、
  MI 流干净、GUI 程序不弹窗、gdb 无黑窗、Interrupt 按钮可用。
- 出处：D4、D5、D11。

### I6 — `SeerDebugDialog.cpp` 的 attach 回填（D9）

- 红线：:513 起的 `Q_OS_WIN` 块——从进程列表选中并确认后**无
  条件**回填 Program name（守卫空路径与 `[name]` 伪路径；字段
  选中后仍可手改；Linux 保持上游「非空不覆盖」原文）。
- 坑：丢了它，attach 会踩「默认项目预填 + 符号错配 = 垃圾栈」
  的上游陷阱（D9 背景里有 1:1 复现记录）。
- 验证：attach 会话：清空预填 → 选进程 → 回填完整路径 → 栈帧
  干净。
- 出处：D9。

## B. 行为性红线（丢失后构建绿灯、功能静默坏——merge 时最危险）

### I7 — 路径处理三层策略（D8）

- 红线（三层各司其职，`Q_OS_WIN` 守卫、Linux no-op；实现在
  `SeerUtl.{h,cpp}` 的 `normalizeSourcePath` / `normalizeFileKey`）：
  1. **命令层**：嵌路径的 gdb 命令一律 `normalizeSourcePath()`
     包裹。已包裹命令点：`-file-exec-and-symbols` /
     `-file-exec-file` / `-file-symbol-file` / `-environment-cd` /
     `source -v`（--bl）/ `-target-select core` /
     `-skip-create-file` / 编辑器与断点对话框的 `--source` /
     CLI `--bs` 的 file:line / printpoint `dprintf-channel`。
     **merge 后上游新增的路径入口同样要补包裹**（命令点表见
     specs/gdb-glue.md §6）；
  2. **KEY 层**：文件名做 map 键处用 `normalizeFileKey()`（正斜
     杠 + 小写折叠）：编辑器 entries、源码浏览器去重键；
  3. **显示层**：用户可见路径 `QDir::toNativeSeparators()`；既是
     显示又是数据的表格列（进程列表 / 栈帧 / 断点的 fullname
     列）用 `Qt::UserRole` 存内部正斜杠形态，读取方一律读
     UserRole。
- 坑：gdb 17.1 `fullname` 反斜杠 + 引号转义吃反斜杠 → 断点失败、
  双 tab；大小写不敏感 → 键控漂移；把数据源列直接转反斜杠 →
  回填出混合分隔符路径（Phase 5 实踩）。有意不覆盖：
  `-exec-arguments`（用户参数）与用户手输路径。消费方
  （grep `normalize` 可复核）：`SeerEditorManagerWidget` /
  `SeerBreakpointCreateDialog` / `SeerGdbWidget` /
  `SeerEditorWidgetSourceAreas` / `SeerSourceBrowserWidget`。
- 验证：`tests/hellopathnorm` 全过（含「同一文件不同拼写命中同
  一键」用例）+ 一个中文路径会话。
- 出处：D8；specs/gdb-glue.md §6。

### I8 — MI 线缆编码与 `\NNN` 转义还原（D11）

- 红线：`src/GdbMonitor.cpp` 的配套——:64-65 / :102-103 读侧
  （Windows `fromLocal8Bit`、其余 `fromUtf8`）；:144-175 记录级
  `fullname="..."` 重写块（Windows 先 `\\`→`/`，再 `\NNN` 八进制
  还原成字节，按 host-charset 解码：Windows=ANSI(GBK)、其余=
  UTF-8；ASCII 快路径直通）；写侧 `SeerGdbWidget.cpp:862-867`
  的 `toLocal8Bit`。
- 坑：`LANG=C` 下 Windows 的 host charset 是 ANSI(GBK) 而非
  UTF-8；还原层是**全平台**的——Linux 的中文路径同样依赖它；
  八进制还原的理论边角见 specs/gdb-glue.md §6b。
- 验证：`tests/hellopathnorm` 13 项（含八进制转义用例，期望值
  平台条件化）。
- 出处：D11；specs/gdb-glue.md §6b。

### I9 — QSettings 强制 IniFormat（D10）

- 红线：`src/seergdb.cpp:72` 的
  `QSettings::setDefaultFormat(QSettings::IniFormat)`（`Q_OS_WIN`
  守卫）不得丢。
- 坑：上游的宏文件与用户 MI 脚本目录从 `QSettings::fileName()`
  派生——Windows NativeFormat 是注册表，`fileName()` 返回
  `HKEY_...` 形态，两个功能**静默失效**（无报错）。上游若新增
  从 `fileName()` 派生路径的功能，受 IniFormat 保护、无需另改，
  但 merge 后要过一遍验证。
- 验证：启动后 `%APPDATA%\seergdb\seergdb.ini` 存在，宏/脚本
  目录可用。
- 出处：D10。

### I10 — 32 位 gdb 兼容（D5）

- 红线：代码不得假设 64 位：地址/指针宽度（8 个十六进制位）、
  寄存器集合（`eax/...` vs `rax/...`）、`arch` 字符串（`i386`
  vs `i386:x86-64`）、gdb 版本差异（17.1/18.1）。涉及 gdb/mi
  解析、寄存器/内存视图、地址格式化的改动，双 gdb 各验一遍
  （两份 gdb 的位置见 `AGENTS.local.md` §4）。
- 坑：17.1 的 `fullname` 反斜杠由 I8 的记录级归一覆盖——归一块
  丢失时 17.1 会话率先露馅。
- 验证：32 位 gdb 会话冒烟（已知偶发 FAIL_STOP 重试即过，观察
  项）。
- 出处：D5。

### I11 — gdb 随包分发前提（D12）

- 红线：`CONFIG_SEER_GDB_NAME` 的 WIN32 默认 `"gdb"`（同目录
  解析，见 I2）不得被上游的 `/usr/bin/gdb` 覆盖回来；
  `--gdb-program` 运行时覆盖必须继续有效；打包的 DLL 闭包迭代
  流程（`docs/build-windows.md` §4，「新拷 DLL 必须重扫」）不得
  简化。
- 验证：PATH 仅 System32 的会话能完整调试（断 main/继续/退出）。
- 出处：D12；`docs/build-windows.md`。

## C. 环境前提（代码之外，验证时依赖）

- conhost VT 渲染：`HKCU\Console\VirtualTerminalLevel=1`
  （`AGENTS.local.md` §8）——不设则弹窗 console 的 ANSI 转义按
  字面打印，易误判为回退；
- 崩溃捕获网（`AGENTS.local.md` §7）：R8 观察项配套，调查崩溃
  前先确认它在位；
- WSL2 自动化冒烟需 `QT_QPA_PLATFORM=xcb`（WSLg 的 Wayland 会让
  GUI 绕过 Xvfb，`AGENTS.local.md` §5）。

## D. 验证装备与最低回归配

| 装备 | 位置 | 状态 |
| --- | --- | --- |
| 路径归一回归 13 项 | `tests/hellopathnorm/` | 在库 |
| Windows / WSL2 构建命令 | `AGENTS.local.md` §5/§6、`docs/build-windows.md` | 在库 |
| 58 程序会话矩阵装备 | `.tmp/phase6/` | 不在库，换机重建（maintenance.md §5） |
| 双平台回归入口 | `.tmp/regression/run.sh` | 不在库，同上 |

每次上游 merge 的最低回归配（不可裁剪）：①Windows 全量构建；
②WSL2 Linux 构建；③`hellopathnorm` 双平台；④至少一个真实会话
冒烟（断 main → 继续 → 退出，x64/x86 gdb 各一）。

验证结果判读：已知非缺陷差异（退出后 Thread 视图 "Stopped" 残
留、弹窗随程序退出关闭、Ctrl-C 后 cin 语义等）见
`verification.md` §5——不要当作 merge 回退去「修」。
