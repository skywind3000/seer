# Seer Windows 移植验证矩阵（Phase 6）

本文档是 Phase 6 的核心产出：`tests/` 程序筛选结论、双平台验证矩阵、
边界场景记录、已知问题分级清单。

- 执行日期：2026-10-20 / 21（矩阵与边界跨两天；2026-10-21 在
  D11 终版构建上整体复跑一次矩阵做可复现性复核）
- Windows：MinGW-w64 GCC 16.2（x64）/ Qt 6.11 / gdb 18.1（x64），
  另以 gdb 17.1（x86）+ 32 位程序做交叉抽查
- Linux 对照：WSL2 Ubuntu-24.04，g++ 13.3 / Qt 6.4.2 / gdb（apt），
  GUI 场景经 Xvfb 自动化
- 验证对象：`.tmp/build-win64/seergdb.exe` 与 `.tmp/build-wsl/seergdb`。
  矩阵首跑于 D8/D9/D10 构建之上；D11 落地（中文路径修复）后，
  边界/回归即刻重验，并于 2026-10-21 在 D11 终版构建上**完整复跑
  58 程序矩阵**：首轮 8 个非 PASS 与首跑同集合同因（可复现），
  修正流程重跑后再次 58/58。

## 1. tests/ 程序筛选

### 1.1 分类标准

- **A 类（Windows 适用）**：程序本身可在 MinGW-w64 下构建运行，
  能作为 inferior 进入验证矩阵。语言/依赖在本机双平台齐备。
- **L 类（Linux-only / 不可移植）**：程序依赖 POSIX 独有设施
  （fork/vfork/exec/dlfcn/ncurses…）或本机 Windows 侧不存在的工具链
  （gnat/cobc/rustc/zig/nim/odin/mpicc/valgrind/rr/CUDA…），
  或场景本身绑定 Linux（core 文件、gdbserver、valgrind/rr 集成）。
- **E 类（嵌入式交叉）**：目标是非本机平台（树莓派、STM32），
  与 Windows 移植无关。
- 另有少量**双侧排除**项（与平台无关的程序自身问题，如 Qt5 独有 API），
  单独说明，不算任何一侧的缺陷。

筛选依据：逐目录检查源码 include 与构建规则（grep POSIX 头/API、
各 Makefile 的编译命令）、本机工具链探测（go/rustc/zig/… 逐一
`command -v`）。工具链探测结论只反映**本机**现状；换机器装上
rustc/zig 等后对应行可升级为 A 类。

### 1.2 A 类（Windows 适用，58 个程序）

批量构建命令：`.tmp/phase6/build-programs.sh win|linux`（双平台同一
脚本）。构建结果汇总：

| 平台 | 成功 | 失败 | 跳过 |
| --- | --- | --- | --- |
| Windows (MinGW64) | 58 个二进制 | 0 | 2 |
| Linux (WSL2) | 54 个二进制 | 0 | 6 |

跳过项与理由（均非 Seer 移植缺陷）：

| 程序 | 平台 | 理由 |
| --- | --- | --- |
| hellodprintf_call_channel | Windows | 程序故意引用 POSIX `dprintf()` 符号（供 gdb dprintf 断点用），mingw-w64 libc 不声明该函数 |
| helloutl | 双侧 | 测试程序使用 Qt5 独有 API `QRegExp`（Qt6 已移除），非 Windows 问题 |
| hellothreads5 / helloparallelstacks | Linux(WSL) | WSL gcc 13 无 C++23 `<print>` 头（需 gcc≥14），工具链版本限制 |
| helloglut | Linux(WSL) | WSL 未装 freeglut（Windows 侧已装并构建成功） |
| hellogo / hellogopyramid | Linux(WSL) | WSL 无 Go 工具链（Windows 侧有） |

构建侧的 Windows 专属处理（只动构建脚本，不改上游测试源码）：

- 链接库置于源文件之后（ld 从左到右解析）；
- hellounicode 需 `-std=c++17`（u8 字面量花括号初始化在 C++20 的
  char8_t 语义下不合法）；
- hellothreads5 需 `-lstdc++exp`（MinGW 的 C++23 `std::print` 终端
  支持放在实验库）；
- helloglut 需 `-DGLUT_DISABLE_ATEXIT_HACK`（mingw freeglut 头的
  atexit 包装宏）；
- `unistd.h`/`sleep()`/`pthread`/`std::thread` 均为 MinGW-w64 原生
  支持，**无需任何源码改动**。

A 类程序及各自覆盖的功能点：

| 程序 | 功能点 | Win 构建 | Linux 构建 |
| --- | --- | --- | --- |
| gdbmi_condition | MI 条件断点输出解析 | OK | OK |
| hello spaces | **含空格路径**（边界必测） | OK | OK |
| hello481 / hello482 | .seer 工程/断点文件加载 | OK | OK |
| hello489 | issue 回归程序 | OK | OK |
| helloansicodes / helloansicodes2 | ANSI 转义序列（D1 弹窗 VT 渲染） | OK | OK |
| helloarray / helloarrayofstructs | 数组/结构数组 locals 展示 | OK | OK |
| hellocirlelist | 循环链表 pretty 展示 | OK | OK |
| hellocore | **程序崩溃**（边界必测） | OK | OK |
| hellocurl | 外部 DLL 依赖（libcurl） | OK | OK |
| hellodprintf (+call_nochannel 变体) | dprintf 断点 | OK | OK |
| helloebcdic | EBCDIC 编码数据 | OK | OK |
| helloexceptions | C++ 异常/catchpoint | OK | OK |
| hellofft | 计算密集 + MIKill.py | OK | OK |
| hellofibonacci | 断点/循环（Phase 5 基准程序） | OK | OK |
| helloglut | GUI inferior + OpenGL（D3：GUI 程序不弹控制台） | OK | skip |
| hellogo / hellogopyramid | Go 语言 inferior | OK | skip |
| helloimage | Qt QImage + 数据文件路径 | OK | OK |
| hellolambda | C++ lambda 栈帧 | OK | OK |
| hellolinklist | 链表结构 | OK | OK |
| hellolocals | 局部变量面板 | OK | OK |
| hellomatrix | 大数组 | OK | OK |
| helloonefile | pthread | OK | OK |
| helloparallelstacks | 并行栈（parallel-stacks.py） | OK | skip |
| hellopathnorm | **路径归一化回归测试**（Phase 5 产出，13 项自检，Phase 6 扩入八进制转义用例） | OK | OK |
| hellopoem | 文本输出 | OK | OK |
| hellopp | pretty-printer（PersonStruct_pp.py） | OK | OK |
| helloprettyprint | pretty-printer 开关场景 | OK | OK |
| helloprimethreads | pthread 多线程 | OK | OK |
| hellopythonmi | gdb Python MI 脚本（MIEcho.py） | OK | OK |
| helloqstring | QString pretty-printer | OK | OK |
| helloqtregularexpression | QRegularExpression | OK | OK |
| hellorecursive | 递归栈 + 输入文件参数 | OK | OK |
| helloscatter | 大数据结构 | OK | OK |
| helloscons | 多文件工程 | OK | OK |
| hellosegv | **崩溃 + CrashCatch**（CrashCatch.hpp 自带 `_WIN32` 分支） | OK | OK |
| helloskip | gdb skip 文件（MISkip.py） | OK | OK |
| hellostack | C 程序 + stdin 交互 | OK | OK |
| hellostar / hellosurface / helloxmas | 图形/数据展示类 | OK | OK |
| hellostart | 启动流程 | OK | OK |
| hellostatic | 静态变量断点 | OK | OK |
| hellostruct | 结构体展示 | OK | OK |
| hellosymbolfile | **分离符号文件**（--sym 场景） | OK | OK |
| hellotabs | 源码含 tab | OK | OK |
| hellothreads 1–5 | 多线程栈（1–4）、C++23 jthread（5） | OK | 1–4 OK |
| hellounicode | 宽字符/UTF 展示 | OK | OK |
| helloworld | 基准（.seer 工程/断点/隐藏点全套文件） | OK | OK |
| recursive_test | 递归调用栈（多源文件工程） | OK | OK |
| hellodebugbreak | **DEBUGBREAK 触发**（边界必测） | OK | OK |

### 1.3 L 类（Linux-only / 不可移植，32 个）

| 程序 | 理由 |
| --- | --- |
| helloada / helloadaexceptions / helloadatasks / hellodaitem | Ada（gnat），Windows 无工具链 |
| helloasm / helloasm2 | 32 位 ELF 原始 `int 0x80` 系统调用汇编，无 Windows 对应物 |
| hellocheck | 依赖 libcheck 单元测试框架 |
| hellocobol | gnucobol 编译器 |
| hellocuda / hellocudadevices | CUDA 工具链；后者本身是 Linux ELF 二进制 |
| hellocurses / helloucurses | ncurses 终端 UI |
| hellodlopen | `<dlfcn.h>` 动态加载 |
| helloexecl | fork + execl 进程替换 |
| hellofork / helloforksegv / helloinferior / hellovfork | fork/vfork 家族（inferior 进程模型，POSIX 独有） |
| hellofortran90 | gfortran |
| hellogdbserver | gdbserver 无 Windows native 版本（远程目标场景可另做「Windows seergdb + WSL gdbserver」交叉实验，未纳入本矩阵） |
| hellompi | MPI（mpirun 启动模型） |
| helloname | 程序可移植，但场景是加载 Linux ELF core 文件（`core.helloname.*`）；Windows core 形态不同（另见 §4 边界实验） |
| hellonim / hellorust / hellozig / helloodin / helloobjectivec | 语言工具链本机 Windows 未装（非本质上不可移植） |
| hellorr / hellorrmnwe | rr（record & replay），Linux 专属 |
| hellosanitizer | ASan 运行时在 MinGW-w64 下支持不完整，未纳入 Windows 侧 |
| hellosimavr | AVR 仿真嵌入式工程 |
| hellovalgrind | valgrind 集成（Seer 的 valgrind 消息面板），Linux 专属 |

### 1.4 E 类（嵌入式交叉，2 个）

| 程序 | 理由 |
| --- | --- |
| Rpi4 | 树莓派交叉目标 |
| stm32f1_blink | STM32 交叉目标 |

### 1.5 汇总

93 个测试条目：**58 个 A 类**（Windows 验证矩阵主体）、
32 个 L 类、2 个 E 类、1 个双侧排除（helloutl）。
L/E 类不进入 Windows 矩阵；其中与 Seer 行为强相关的
（core 加载、gdbserver 远程）在 §4 边界场景中另有交代。

## 2. 验证方法

### 2.1 Windows 会话自动化

每个 A 类程序跑一次**真实 GUI 会话**，脚本
`.tmp/phase6/auto/session.ps1`：

1. `seergdb --gdb-program <gdb> --bf main -r <程序>` 启动
   （CLI 组合：自动载入程序、`-break-insert main`、运行）；
2. 轮询 `Get-Process <程序名>`——进程出现即证明已停在 main
   （断点未命中则程序会立刻跑完消失）；
3. 截图（停点态）；
4. `AppActivate` + `SendKeys {F8}`（Continue）；
5. 轮询进程退出（normal 流程）或等待再停（stop/never 流程）；
6. 截图（继续后态）；
7. `CloseMainWindow` 正常关闭 seergdb，清理残留进程。

判定：`PASS` = 停点检出 ∧（normal 流程）程序退出 ∧ seergdb 存活
∧无残留；任何一步失败给出 `FAIL_STOP` / `FAIL_EXIT` /
`FAIL_SEER_CRASH` 定位。截图经 OCR（WSL tesseract）抽读，核对
标题栏路径（反斜杠显示）、"Program reached breakpoint" 消息、
栈帧面板内容。

流程分类：`normal`（跑完退出，绝大多数）、`stop`（继续后停在
信号/断点：hellocore、hellosegv、hellodebugbreak）、`never`
（事件循环不退出：helloglut、hellorecursive）、`stdin`（控制台
阻塞等输入：hellostack，交互输入另见 §4）。

### 2.2 Linux Xvfb 对照

同一套场景在 WSL2 内以 Xvfb + xdotool 驱动（复用 Phase 5
`.tmp/smoke-linux` 的装备）：`QT_QPA_PLATFORM=xcb` 下启动
seergdb，xdotool 发 F8，`import` 截图，tesseract OCR 核验。
对照范围：A 类代表性程序 + 全部边界场景（见 §4 各小节的
「Linux 对照」行）。

### 2.3 与上游行为的对照原则

console 相关功能点在 Windows 下按 D1 验证**弹窗行为**
（被调试程序弹独立控制台窗口），与 Linux GUI 内嵌终端的
差异标注为「行为分流」而非缺陷。显示层路径（反斜杠、真实
大小写）按 D8 验证。

## 3. 核心矩阵（Windows 会话 × 58 程序）

每行一次真实 GUI 会话（§2.1 流程）。判定列：全部 PASS——停点检出
（断在 main）∧（normal 流程）inferior 正常退出 ∧ seergdb 存活 ∧
无残留进程。OCR 抽查 58 张停点截图全部有 GUI 调试态证据（标题栏
原生路径 / "Program reached breakpoint" / 栈帧面板 / 源码浏览
器）。矩阵跑了**两轮**（D11 前首跑 + D11 终版复跑）：两轮的
非 PASS 集合完全一致（8 个，全部溯源为程序自身行为：长运行、
stdin 阻塞、事件循环、Go 无 `main` 符号——见各行 flow 列），按
正确流程分类重跑后两轮均为 58/58。截图与逐行 TSV 在
`.tmp/phase6/matrix-win.tsv` 与 `.tmp/phase6/shots/`。

| 程序 | 流程 | Windows | Linux 对照 |
| --- | --- | --- | --- |
| gdbmi_condition | normal | PASS | 构建 OK |
| hello spaces | normal | PASS | PASS（会话） |
| hello481 | normal | PASS | 构建 OK |
| hello482 | normal | PASS | 构建 OK |
| hello489 | normal | PASS | 构建 OK |
| helloansicodes | normal | PASS | 构建 OK |
| helloansicodes2 | normal | PASS | 构建 OK |
| helloarray | normal | PASS | 构建 OK |
| helloarrayofstructs | normal | PASS | 构建 OK |
| hellocirclelist | normal | PASS | 构建 OK |
| hellocore | stop | PASS | PASS（会话） |
| hellocurl | normal | PASS | 构建 OK |
| hellodebugbreak | stop | PASS | PASS（会话） |
| hellodprintf | normal | PASS | 构建 OK |
| hellodprintf_call_nochannel | normal | PASS | 构建 OK |
| helloebcdic | normal | PASS | 构建 OK |
| helloexceptions | normal | PASS | 构建 OK |
| hellofft | never | PASS | 构建 OK |
| hellofibonacci | normal | PASS | PASS（会话） |
| helloglut | never | PASS | skip（WSL 无 freeglut） |
| hellogo | normal | PASS | skip（WSL 无 Go） |
| hellogopyramid | stdin | PASS | skip（WSL 无 Go） |
| helloimage | normal | PASS | 构建 OK |
| hellolambda | normal | PASS | 构建 OK |
| hellolinklist | normal | PASS | 构建 OK |
| hellolocals | normal | PASS | 构建 OK |
| hellomatrix | normal | PASS | 构建 OK |
| helloonefile | normal | PASS | 构建 OK |
| helloparallelstacks | normal | PASS | skip（WSL gcc13 无 \<print\>） |
| hellopoem | normal | PASS | 构建 OK |
| hellopp | normal | PASS | 构建 OK |
| helloprettyprint | normal | PASS | 构建 OK |
| helloprimethreads | normal | PASS | 构建 OK |
| hellopythonmi | normal | PASS | 构建 OK |
| helloqstring | normal | PASS | 构建 OK |
| helloqtregularexpression | normal | PASS | 构建 OK |
| hellorecursive | never | PASS | 构建 OK |
| helloscatter | normal | PASS | 构建 OK |
| helloscons | normal | PASS | 构建 OK |
| hellosegv | stop | PASS | 构建 OK |
| helloskip | normal | PASS | 构建 OK |
| hellostack | stdin | PASS | 构建 OK |
| hellostar | normal | PASS | 构建 OK |
| hellostart | normal | PASS | 构建 OK |
| hellostatic | normal | PASS | 构建 OK |
| hellostruct | normal | PASS | 构建 OK |
| hellosurface | normal | PASS | 构建 OK |
| hellosymbolfile | normal | PASS | 构建 OK |
| hellotabs | normal | PASS | 构建 OK |
| hellothreads | normal | PASS | PASS（会话） |
| hellothreads2 | normal | PASS | 构建 OK |
| hellothreads3 | normal | PASS | 构建 OK |
| hellothreads4 | normal | PASS | 构建 OK |
| hellothreads5 | stdin | PASS | skip（WSL gcc13 无 \<print\>） |
| hellounicode | normal | PASS | 构建 OK |
| helloworld | normal | PASS | PASS（会话） |
| helloxmas | normal | PASS | 构建 OK |
| recursive_test | never | PASS | 构建 OK |

流程分类说明：`normal`=跑完退出；`stop`=继续后停在信号
（hellocore/hellosegv=SIGSEGV，hellodebugbreak=SIGTRAP）；`never`=
事件循环/海量输出不退出（helloglut、hellorecursive、hellofft）；
`stdin`=等控制台输入（hellostack、hellothreads5、hellogopyramid）。
`hellofft` 为海量输出程序（10000×3×N 次 printf），并入 §4 长输出
边界。`hellogo` 首轮 FAIL_STOP 溯源为 Go 二进制无 `main` 符号
（raw gdb 复现 "Function main not defined"），以 `--bf main.main`
重跑通过——Seer 行为正确，属 gdb×Go 符号语义。
`hellopathnorm`（第 59 个 A 类条目）为自检式回归测试，不作为
inferior 会话，其结论＝13/13 双平台（§5 已知问题 K1 的回归覆盖）。

Linux 侧会话对照（Xvfb 自动化）覆盖 9 行：helloworld /
hellofibonacci / hellothreads / hello spaces / hellocore /
hellodebugbreak / 你好世界（中文 exe 路径）/ cnsrc-src（中文源
码路径，D11 后 PASS）/ hellostack（stdin 交互引用 Phase 5
v-verify 证据，`.tmp/smoke-linux/v-verify.sh`：向 GUI 内嵌终端
键入文本→inferior 消费→退出，Phase 5 出口条件之一）。其余程序
Linux 侧为「构建 OK」（机制同构，未逐一跑 GUI 会话）。

## 4. 边界场景

每个场景给出：结论、判定依据、证据位置。截图在
`.tmp/phase6/shots/`（子目录 b-*）。

| # | 场景 | 结论 | 判定依据与备注 |
| --- | --- | --- | --- |
| B1 | 程序崩溃（hellocore） | PASS | F8 后停在 SIGSEGV（OCR 消息面板 "Program encountered a 'SIGSEGV' signal"），seergdb 存活；矩阵 stop 行。hellosegv 同（CrashCatch.hpp 自带 `_WIN32` 分支，可直接构建） |
| B2 | 含空格路径（hello spaces） | PASS | 目录名带空格：构建、CLI 传参、gdb 加载、断点、退出全链路；矩阵 PASS 行 + Linux 会话对照 PASS |
| B3 | 非 ASCII 路径——中文 exe 目录 | PASS（D11 后） | `bin/中文目录/你好世界.exe`：停 main + 退出；标题栏显示正确中文（Win32 API 读回真值验证） |
| B4 | 非 ASCII 路径——中文源码目录 | PASS（D11 后） | `cnsrc/中文源码/`（DWARF comp_dir 中文）：停 main、编辑器打开中文路径源码（findFile 命中，对话框消失）、退出。首轮失败的 "Can't find source file" 模态框即 D11 第 2/3 层缺陷的症状。Linux 对照行同场景修复后 PASS |
| B5 | 长输出滚动（hellofft） | PASS | F8 后持续 30s+ 海量输出，inferior 一直存活（console 滚动正常），截图 longout-running |
| B6 | Interrupt（Ctrl+I）打断运行 | PASS | B5 运行中发 Ctrl+I：inferior 转挂起（进程存活），消息面板/帧面板回到停止态，seergdb 存活；对应 Phase 5 A-4 争议项的运行期实证 |
| B7 | attach / detach | PASS | `--attach <pid>` CLI 附加 helloloop：自动停止、帧面板出栈；Control 菜单 Detach 后 **inferior 仍存活**（detach 语义正确）；关闭 seergdb 清理。标题显示被调试程序路径（QProcessInfo + D9 链路） |
| B8 | hellodebugbreak（DEBUGBREAK） | PASS | fib(15) 触发 8 次 debug_break：逐次 SIGTRAP 停止（8 张截图），最终程序正常退出 |
| B9 | stdin 交互（弹窗控制台） | PASS | hellostack（两个 gets）：向弹窗控制台输入缓冲注入两行（原生 WriteConsoleInput，等价用户键入）→ 程序消费退出；hellothreads5：注入 "exit" 命令 → 10 线程收尾退出。证明 D1 弹窗控制台 stdin 接线完好。Linux 对照引用 Phase 5 内嵌终端证据 |
| B10 | 32 位 gdb（17.1 x86）交叉 | PASS | bin32 三程序（helloworld32/hellocore32/hellothreads32）× x86 gdb：停点、崩溃停、线程、退出 3/3（gdb 17.1 的 fullname 反斜杠由 D8 记录级归一覆盖） |
| B11 | 多会话（同实例） | PASS（部分） | Alt+R 同实例重启会话：会话1 退出→Restart→0.4s 重停 main→F8→退出，杀旧 gdb+重起 inferior 链路完好。**同实例换程序的二次 Debug 对话框**：对话框可开（截图验证）、OK 链路代码核对无误，但自动化无法向 Qt 模态对话框字段注入文本（前台锁/焦点限制，非 Seer 缺陷——主人 Phase 5 已人工操作过该对话框且 R8 场景人工复现）。列为人工抽查项 |
| B12 | core 文件加载（helloname 类） | 不适用（已知差异） | gdb (MinGW) 不读 Linux ELF core；Windows core 形态为 WER dump，gdb 无对应 target。L 类记录在 §1.3，不构成缺陷 |

## 5. 已知问题清单（分级）

**阻塞级：无。**

**应修级（均有 issue 式记录）：**

| 编号 | 问题 | 记录位置 | 状态 |
| --- | --- | --- | --- |
| K1 | 非 ASCII 路径三重失败（MI 写编码 / MI 八进制转义不还原 / 转义字节 host-charset 解码），Linux 同中招转义层 | decisions.md D11、gdb-glue.md §6b | **已修复**（本 phase），hellopathnorm 13/13 双平台回归 |
| K2 | R8：同实例二次 Debug 会话设断点时偶发 Qt qFatal 崩溃（0xC0000602） | plan.md 风险登记 R8 | open——三层捕获网（WER dump + cdb + fatal 栈日志）已就绪待复现；人工抽查项 |

**可接受级（不修或观察）：**

| 编号 | 问题 | 备注 |
| --- | --- | --- |
| A-1 | inferior 退出后「Stopped」栈帧残留显示 | 上游代码路径，纯显示；多会话截图可见 |
| A-2 | attach 时 exe 与 pid 不匹配无提醒 | 上游行为；D9 已消除主要错配来源 |
| A-3 | Seer 程序化启用 VT（conhost ANSI） | 依赖注册表 HKCU\Console\VirtualTerminalLevel（本机已设）；分发给新用户时需文档说明 |
| A-4 | Go 程序 `--bf main` 无停点 | gdb×Go 符号语义（无 `main` 函数符号），用 `main.main`；文档化即可 |
| A-5 | `hellodprintf_call_channel` 无法在 Windows 构建 | 程序自身引用 POSIX `dprintf` 符号，mingw-w64 libc 无声明；上游测试程序限制 |
| A-6 | gdb（MinGW）不支持 Windows core dump 加载 | 平台能力差异；崩溃调试走 attach/WER 路径 |

## 6. 结论与出口条件对照

| 出口条件 | 结果 |
| --- | --- |
| 矩阵行数 ≥ 筛选后适用子集数，每行有结论 | 满足：适用子集 58+1 条目；矩阵 58 程序行 + 12 边界行（§3/§4），每行有结论 |
| 无「阻塞」级未解决；「应修」级全部有 issue 式记录 | 满足：阻塞级无；应修级 K1 已修复闭环、K2 有 R8 记录与捕获网 |
| 主人抽查若干场景确认验收 | 待复核（B11 对话框二次会话为建议抽查项；另建议抽 B3/B4 中文路径与 B7 attach/detach） |

副产出：D11 修复 + hellopathnorm 扩至 13 项（含平台条件化
GBK/UTF-8 八进制用例）；`.tmp/phase6/` 沉淀可复用的会话自动化
装备（session-functions.ps1 / coninj 注入器 / build-programs.sh /
linux-verify.sh），后续回归可一键重跑。

