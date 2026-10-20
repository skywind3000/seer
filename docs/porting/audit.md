# 平台耦合审计（Phase 1 产出）

- 日期：2026-10-20
- 性质：`docs/plan.md` Phase 1 的产出物。范围：`src/` 的平台耦合点
  全量清单（不含 `tests/` 手工测试程序），每条含 Linux 语义、
  Windows 候选方案、风险与状态。
- 原始 grep 输出存档：`docs/porting/evidence/audit-grep.txt`、
  `evidence/audit-grep2.txt`。

## 1. 可复跑的 grep 清单

以下命令在仓库根执行；两组坑必须规避（详见
`docs/porting/environment.md`「坑」一节）：本机 grep 是 ugrep，
模式以 `/` 开头必须 `-e` 显式给出，且必须加 `MSYS_NO_PATHCONV=1`
防止 Git Bash 把模式当 POSIX 路径转换掉（曾造成 `/tmp` 静默漏报）。

    MSYS_NO_PATHCONV=1 { \
    echo "### A. PTY/termios/ioctl/fcntl"; grep -rnE "termios|posix_openpt|grantpt|unlockpt|ptsname|TIOCS|TIOCG|cfset|tcgetattr|tcsetattr|fcntl|O_NDELAY|ioctl\s*\(" src --include='*.cpp' --include='*.h'; \
    echo "### B. POSIX headers"; grep -rnE "#include\s*<(unistd|pwd|sys/ioctl|sys/wait|sys/types|dirent|signal|pthread)\.h>" src --include='*.cpp' --include='*.h'; \
    echo "### C. process/IO calls"; grep -rnE "\bkill\s*\(|\bfork\s*\(|\bexec(v|l|p)[a-z]*\s*\(|\bwaitpid\s*\(|\bfsync\s*\(|\bpipe\s*\(|\bdup2\s*\(|\bgetpwuid|\bmkstemp|\bmkdtemp" src --include='*.cpp' --include='*.h'; \
    echo "### D. /proc 与路径硬编码"; grep -rn -e "/proc/" -e "/tmp" -e "/dev/" src --include='*.cpp' --include='*.h'; \
    echo "### E. QSocketNotifier"; grep -rn "QSocketNotifier" src --include='*.cpp' --include='*.h'; \
    echo "### F. signals"; grep -rnE "\bSIG[A-Z]+\b" src --include='*.cpp' --include='*.h'; \
    echo "### G. CMake platform gates"; grep -nE "UNIX|WIN32|APPLE|LINUX" src/CMakeLists.txt; \
    echo "### I. raw fd IO"; grep -rnE "::open\s*\(|::close\s*\(|::read\s*\(|::write\s*\(|::fsync\s*\(" src --include='*.cpp' --include='*.h'; \
    } 2>&1 | tee .tmp/audit-grep.txt

## 2. gdb/mi 交互链路（移植的主要风险面）

Seer 与 gdb 的全部交互都经 `SeerGdbWidget` 持有的 `QProcess`
（`_gdbProcess`）完成，**链路本身 100% 是 Qt 可移植代码**：

1. **启动**（`SeerGdbWidget.cpp:3359-3426` `startGdb()`）：
   拼接 `rawcommand = launcher + gdbProgram + gdbArguments`，默认
   `gdbArguments = "--interpreter=mi"`（`:73`）、`gdbProgram` 取
   CMake 宏 `SEER_GDB_NAME`（`:61-64`，默认 `/usr/bin/gdb`）；经
   `Seer::expandEnv` 展开环境变量、`Seer::split` 拆词后交给
   `QProcess::setProgram/setArguments`，环境里强制 `LANG=C`（防 MI
   输出被本地化，Windows 下无害），`start()`。
   另有 `startGdbRR()`（`:3428+`，rr 录制回放）——rr 是 Linux 工具，
   Windows 上该功能自然不可用，运行期降级即可。
2. **输入**：`handleGdbCommand()` → `_gdbProcess->write()`（`:853`）。
3. **输出**：`readyReadStandardOutput/Error` → `GdbMonitor` 解析
   MI 记录 → 分发给各 `Gdb*`/`Seer*` 部件（纯 Qt 字符串处理）。
4. **inferior 的 console 接线**：`SeerCommandLogsWidget` 持有
   `SeerConsoleWidget`（PTY）；5 个启动 handler 在启动 inferior 前
   调 `handleGdbTerminalDeviceName()`（`:2049-2058`），向 gdb 发
   `-inferior-tty-set <ptsname>`。调用点：`:935`
   `handleGdbRunExecutable`、`:1079` `handleGdbAttachExecutable`、
   `:1183` `handleGdbConnectExecutable`、`:1304`
   `handleGdbRRExecutable`、`:1425` `handleGdbCoreFileExecutable`。
5. **中断**：`sendGdbInterrupt()`（`:3899-3931`）——`signal < 0`
   时发 MI `-exec-interrupt`；否则 POSIX `kill(pid, sig)` 直发。
6. **gdb 程序发现**：`SEER_GDB_NAME` 宏（`src/CMakeLists.txt:342`）
   → QSettings 覆盖（`SeerMainWindow.cpp:1854`、
   `SeerGdbConfigPage.cpp:132`）。

**Windows 侧的分歧点只有四处**：tty 设备名语义（A2）、`kill()`
（A4）、gdb 路径默认值（A7）、以及一个注意事项——`QProcess` 启动
console 程序（gdb.exe）时 Windows 默认会给它弹一个控制台黑窗，需要
`setCreateProcessArgumentsModifier` 加 `CREATE_NO_WINDOW`（A8，
即风险 R7 的一半）。

## 3. 耦合点清单

| 编号 | 位置 | 内容 | Linux 语义 | Windows 候选方案 | 风险 | 状态 |
| --- | --- | --- | --- | --- | --- | --- |
| A1 | `SeerConsoleWidget.cpp:19-24`（头文件）、`:266-272`（stdin：write+fsync）、`:286-305`（stdout：read+EIO 断开）、`:308-363`（createTerminal：posix_openpt/grantpt/unlockpt/fcntl O_NDELAY/TIOCSWINSZ 80x20/TIOCSCTTY/ptsname）、`:375`（close）、`:390-392`（QSocketNotifier 监听裸 fd） | PTY 全生命周期 | master fd + `/dev/pts/N` 设备名，交给 gdb 作为 inferior 的 stdio | 依 D1 不再需要：该控件在 Windows 隐藏；编译隔离（悬空 POSIX 调用移出 Windows 编译路径——#ifdef 最小化或平台后端文件，Phase 3 spec 定）。控件内的 ANSI 渲染（QAnsiTextEdit）本身是纯 Qt，无耦合 | 低 | 有方案（D1） |
| A2 | `SeerGdbWidget.cpp:2049-2058`，调用点 `:935/1079/1183/1304/1425` | `-inferior-tty-set <设备名>` | 把 PTY slave 设备名交给 gdb，inferior 的 stdio 指向它 | 依 D1/D3：Windows 不发该命令，也不探测程序类型——终端程序由 Windows 自动弹控制台、GUI 程序不弹（PE 子系统决定）；是否补发 `-gdb-set new-console on` 由 Phase 2 Q1 的 2×2 PoC 实测；同时保证 gdb 自身无黑窗（A8）。R7 | 中 | 有方案（D1/D3，PoC 待做） |
| A3 | `QProcessInfo.cpp:14-17`（头）、`:26-32`（exe 符号链接）、`:35-57`（status 文件）、`:60-81`（Uid + `getpwuid`）、`:84-114`（cmdline）、`:119-143`（遍历 /proc） | 进程枚举（attach 对话框用） | 读 `/proc` 文件系统拼出 pid/名字/路径/用户/命令行 | Toolhelp32 快照（`CreateToolhelp32Snapshot`）给 pid/进程名/路径；username 可用进程 owner SID→名字（可选）；cmdline 代价高（读 PEB 或 WMI），可先留空。入口只有 `populate()` 两个静态函数，接口抽象面小 | 低 | 有候选 |
| A4 | `SeerGdbWidget.cpp:3899-3931`（`kill()` + `strsignal`）、`:1749-1777`（SIG 宏）、`SeerMainWindow.cpp:118-121`（中断菜单） | POSIX `kill(pid, sig)` 直发 inferior，信号集 SIGINT/SIGKILL/SIGFPE/SIGSEGV/SIGUSR1/SIGUSR2 | 绕过 gdb 直接发信号——注释说明原因：`-exec-until` 落到别处时 `-exec-interrupt` 无效 | 候选 a：全部改走 gdb（`-exec-interrupt` 或 CLI `signal N`）；候选 b：Win32 API（GenerateConsoleCtrlEvent 发 Ctrl-C / DebugBreakProcess / TerminateProcess）。注意 MinGW CRT **没有** SIGKILL/SIGUSR1/SIGUSR2 宏，`kill()` 也没有——这部分代码原样必编译失败。行为等价性需 Phase 2 PoC 验证（R3/R7 相关） | 中 | 有候选（PoC 待做） |
| A5 | `SeerGdbWidget.cpp:36-38` | 悬空 include：`fcntl.h` / `termios.h` / `sys/ioctl.h`（全文件无任何调用，残留物；`unistd.h`/`signal.h` 有实际使用） | 无 | 直接删除这三行（对 Linux 构建无影响——回归验证即可）；MinGW 有 `unistd.h`/`fcntl.h`，无 `termios.h`/`sys/ioctl.h` | 低 | 有方案（删除） |
| A6 | `SeerGdbWidget.cpp:1490`（save breakpoints）、`:1517`（remove）、`:2034`（构造文件名）、`:2046`（source 恢复）、`:3733`（MI 脚本从资源拷出后让 gdb source） | 硬编码 `/tmp`，共 5 处 | gdb 的 `save/source` 命令需要真实文件路径 | `QDir::tempPath()`（Qt 跨平台）一次替换 5 处；注意 `/tmp/breakpoints.seer.%1` 的语义是「本进程独占临时文件」 | 低 | 有候选 |
| A7 | `src/CMakeLists.txt:313`（`-Wall` 等仅 `UNIX AND NOT APPLE`）、`:342`（`SEER_GDB_NAME` 默认 `/usr/bin/gdb`）、`SEER_GDB_LAUNCHER`（flatpak-spawn，默认空） | CMake 平台门与默认值 | Linux 默认路径与警告选项 | 警告选项加 `MINGW` 分支；`SEER_GDB_NAME` 在 WIN32 下默认改 `gdb`（靠 PATH 解析）或 `gdb.exe`；launcher 默认空串不阻塞。CMake 工程本身可移植（Phase 0 冒烟已证 Qt6 检测可用） | 低 | 有候选 |
| A8 | `SeerGdbWidget.cpp:3359-3426`（QProcess 启动 gdb） | 无 POSIX 调用（纯 Qt） | — | 无需改动逻辑，但 Windows 上启动 console 程序（gdb.exe）默认弹黑窗：`QProcess::setCreateProcessArgumentsModifier` 加 `CREATE_NO_WINDOW` | 低 | 有方案 |
| A9 | `SeerConsoleWidget.cpp:85`（`::write(STDOUT_FILENO)`）、`:238`（`fcntl(STDOUT_FILENO, F_GETFD)`） | console 内容回显到启动 Seer 的终端 | GUI 程序通常无有效 stdout，`:238` 的检查失败即自动禁用 | MinGW 提供 `unistd.h`/`STDOUT_FILENO`/`fcntl`，可编译；且依 D1 该控件 Windows 隐藏，此项随之消失 | 低 | 有方案（随 A1） |

## 4. 与 AGENTS.md 已知清单的对照

AGENTS.md 列的四个区域全部覆盖并细化：

| AGENTS.md 条目 | 对应 |
| --- | --- |
| SeerConsoleWidget（PTY） | A1、A9、A2 |
| QProcessInfo（/proc） | A3 |
| SeerGdbWidget（三个头文件） | A4、A5、A6（只有 `unistd.h`+`signal.h` 真有使用） |
| CMakeLists.txt（警告、gdb 名） | A7、A8 |

**清单之外的新发现**（AGENTS.md 未记载）：

1. A4——`sendGdbInterrupt` 实际调用 POSIX `kill()`（AGENTS.md 只
   提了 include `signal.h`，没提真调用），且 MinGW 缺 SIGKILL/
   SIGUSR1/USR2 宏；
2. A5——`termios.h` / `sys/ioctl.h` / `fcntl.h` 在 SeerGdbWidget
   是悬空 include；
3. A6——5 处硬编码 `/tmp`；
4. A8——`CREATE_NO_WINDOW` 注意点；
5. `QProcessInfo.cpp` 的耦合面是整个文件（不止 `:26` 一处）。

## 5. grep 命中归零核对

全部命中均已归类（原始输出见 evidence/）：

- **入表**：A-I 各组的有效命中全部映射到第 3 节的条目。
- **标注为误报/非耦合的**：
  - C 组 `help->raise()` / `_consoleWidget->raise()` /
    `_miniMap->raise()` 等约 30 条——`\braise\s*\(` 匹配到了 Qt 的
    窗口抬升方法，与 POSIX `raise()` 无关；
  - `_gdbProcess->kill()`（`:390/3512`）——`QProcess::kill()`，
    Qt 可移植 API，非 POSIX kill；
  - F 组 `SeerMainWindow.cpp:1439/1511`、
    `SeerSignalValuesBrowserWidget.cpp` 全部——mi 输出示例字符串
    或注释，非代码；
  - I 组 `SeerEditorWidgetSourceAreas.cpp:611/764`——Qt 自家
    open/close 成员函数，非裸 fd IO；
  - E 组 `SeerConsoleWidget.h:12/76`——声明，已并入 A1。
- **假阴性教训（已修复）**：`/tmp` 初查因 MSYS 路径转换静默漏报
  （0 命中），人工读码发现后用 `MSYS_NO_PATHCONV=1` 重跑补齐 5 处。
  复跑清单（第 1 节）已内置规避。

## 6. 结论

平台耦合的总量与 issue #94 的历史经验吻合（MSYS2 MinGW 下除
`SeerConsoleWidget` 外全部可编译）：**硬耦合集中在 2 个文件**
（`SeerConsoleWidget`——依 D1 整体绕开；`QProcessInfo`——做
Toolhelp32 后端），**`SeerGdbWidget` 有 4 类点**（tty 接线、kill
中断、悬空 include、/tmp），**CMake 一处**（默认值/警告门），其余
全部是可移植的 Qt 代码。依 D1 化解 A1/A2 后，剩余的技术不确定性
只有两处，都留给 Phase 2 的 PoC：A2/A8 的 new-console 组合行为
（R7）、A4 的中断方式等价性（R3 相关）。全部条目「Windows 候选
方案」列非空，满足 Phase 1 出口条件，待主人 review。
