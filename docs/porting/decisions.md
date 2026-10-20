# 移植决策记录（ADR）

决策按 D1、D2、… 顺序追加，只追加不删改。任何时刻（包括 phase 开始
前）做出的关键决策都落在本文件；`docs/plan.md` 文末「决策记录」表
只放一句话索引。被决策引用的 PoC 佐证日志，需长期保留的摘录进
`docs/porting/evidence/`。

## D1 — Windows 下被调试程序的控制台走独立终端窗口，不做 GUI 内虚拟终端

- 日期：2026-10-20
- 状态：已采纳（主人决策）
- 背景：Linux 版 `SeerConsoleWidget` 用 PTY 在 GUI 内实现虚拟终端，
  并经 gdb 的 `-inferior-tty-set` 把 PTY 设备名交给被调试程序
  （`src/SeerGdbWidget.cpp:2053`）。Windows 没有 `/dev/pts/N` 这类
  设备名，GUI 内虚拟终端意味着必须解决「gdb spawn 的 inferior 如何
  接到我们创建的 ConPTY」——这是原计划里最大的不确定点（原风险
  R1/R2）。而 Windows 上调试器的普遍惯例（Visual Studio、VSCode、
  vim termdebug）是不在界面里嵌虚拟终端：调试启动时被调试程序弹出
  自己的原生控制台窗口（conhost），交互直接在该窗口完成。
- 决策：Windows 下不做 GUI 内虚拟终端。被调试程序启动时弹出自己的
  独立控制台窗口（gdb `-gdb-set new-console on` 路线），stdin /
  stdout / ANSI 彩色 / Ctrl-C 交互全部在弹出的窗口里完成，由
  Windows 的 conhost 原生处理。GUI 内的 console 页在 Windows 下
  隐藏。
- 范围说明：ConPTY GUI 内虚拟终端从移植范围中移除，**不是**移植
  完成的必要条件。日后若证明想要且成本低，作为可选增强另立新决策
  追加，不影响本决策下的交付定义。
- 影响：
  - 计划 Phase 2：原「ConPTY 数据路径」「`-inferior-tty-set`
    inferior 接线」两问不再是必答题，替换为一个小的「new-console
    路线确认」PoC；
  - Phase 3：不做 console 双实现后端，改为控制台行为分流——
    Linux 保留上游 PTY 实现不动，Windows 在 `-inferior-tty-set`
    调用点改发 new-console 设置并隐藏 console 界面；
  - Phase 5：console 任务改为 new-console 接线与界面隐藏；
  - 风险 R1、R2 化解，新增 R7（new-console 组合行为待证实）。
- 依据：主人 2026-10-20 决策；Windows 调试器普遍惯例。

## D2 — 移植产物是跨平台单一代码库，最小化对上游文件的 diff

- 日期：2026-10-20
- 状态：已采纳（主人决策）
- 背景：上游对 Windows 移植持 `wontfix`（issue #94），本 fork 需
  长期自维护并定期从上游拉取合并更改，否则会与上游渐行渐远。
- 决策：所有改动落在 Windows / Linux 共用的同一份代码库
  （`develop` 分支）上，不维护 Windows 专用分支。「对上游既有文件
  的最小 diff」是设计约束：平台差异代码优先放进**新增文件**（新的
  平台实现 .cpp/.h、独立的 CMake 片段），尽量不改上游既有文件；
  必须改的，改动区域要小而集中，便于定期 merge 上游时解决冲突。
- 影响：`docs/plan.md` 的目标与范围、Phase 3 spec 评审标准（新增
  diff 最小化检查项）、Phase 7 维护策略（定期 merge 上游的流程）
  均按此更新。
- 依据：主人 2026-10-20 决策。

## D3 — 细化 D1：Windows 下只有终端程序弹终端窗口，GUI 程序不弹

- 日期：2026-10-20
- 状态：已采纳（主人决策；机制细节留 Phase 2 Q1 PoC 实证）
- 背景：D1 只说了「弹出被调试程序的控制台窗口」。主人补充：观察
  Visual Studio 的行为——调试 GUI（非终端）程序时不弹终端窗口；
  vim 的 termdebug 也是「不捕获程序终端，终端程序自动弹、GUI
  程序不弹」。
- 决策：Windows 下**不做任何「被调试程序是不是终端程序」的探测**，
  也不捕获 inferior 的终端——不给 gdb 发 console 相关设置（不发
  `-inferior-tty-set`；`new-console` 发不发由 PoC 定，见下），
  inferior 弹不弹终端窗口完全由 Windows 按可执行文件 PE 头的子系统
  字段自动决定：终端子系统程序自动获得新控制台（父进程 gdb 无
  控制台时系统分配），GUI 子系统程序永远没有控制台。
- 机制依据：CreateProcess 语义——console 子进程在父进程无控制台时
  由系统自动分配新控制台；GUI 子系统进程不分配控制台；子系统类型
  记录在 PE 头里，Seer 无需判断。
- 待 PoC 确认的两点（并入 Phase 2 Q1，构成 2×2 矩阵）：
  1. gdb 自身无控制台（`CREATE_NO_WINDOW`）且**不发任何命令**时，
     console 型 inferior 的 stdio 是落在它自己的新控制台里，还是
     继承了 gdb 的管道——若是后者会污染 Seer 的 MI 流，就必须改发
     `-gdb-set new-console on`；
  2. `new-console on`（CREATE_NEW_CONSOLE）对 GUI 型 inferior 是否
     会误弹控制台（行为不确定）——若误弹，则只能走「不发命令」
     路线。
- 影响：Phase 2 Q1 扩为 2×2 矩阵；`docs/plan.md`（Q1、任务 2、
  Phase 5 任务与验证）与 `docs/porting/audit.md`（A2 候选方案）
  已同步更新。
- 依据：主人 2026-10-20 决策；Visual Studio 与 vim termdebug 的
  行为参照。

## D4 — Windows inferior 控制台机制定案：`new-console on` + gdb 无窗启动

- 日期：2026-10-20
- 状态：已采纳（Phase 2 Q1 PoC 实证）
- 背景：D3 的首选「不发任何命令」路线需要实证。
- PoC 结论（`.tmp/poc-q1`，5 组合矩阵，成功标准与佐证见
  `evidence/poc-q1-*`）：
  1. **默认（不发 new-console）路线被证伪**：gdb 在 Windows 上默认
     把 inferior 的 stdio 接到自己继承的管道（即 Seer 的 MI 流），
     inferior 输出直接污染 MI 流（evidence/poc-q1-log1 第 49 行起
     出现裸 `INFERIOR-OUTPUT`）；
  2. `-gdb-set new-console on`（gdb 18.1 返回 `^done`）：MI 流干净；
     console 型 inferior 自动获得**可见**独立控制台窗口
     （`tasklist /V` 实证：t-console.exe 的窗口标题为完整路径）；
     GUI 型 inferior 不弹任何窗口（conhost 增量与 gdb 隐形控制台
     基线一致）——D3 的「终端程序弹、GUI 程序不弹、无需探测」
     预期在 new-console 下成立；
  3. gdb 不加 `CREATE_NO_WINDOW` 会自带黑窗且同样污染 MI 流
     （evidence/poc-q1-log5）。
- 决策：Windows 上 Seer 启动调试时向 gdb 发 `-gdb-set new-console
  on`，gdb 本身经 `QProcess` 以 `CREATE_NO_WINDOW` 启动；程序类型
  不做任何探测。D3 的「首选不发命令」由本决策替代定案。
- 影响：audit A2 候选落定；Phase 5 接线方案明确；风险 R7 化解。
- 依据：PoC 实测（矩阵结果见 `evidence/poc-q1-matrix.txt`）。

## D5 — gdb MI 兼容性与中断机制：`mi-async on` + `-exec-interrupt`

- 日期：2026-10-20
- 状态：已采纳（Phase 2 Q2 PoC 实证）
- PoC 结论（`.tmp/poc-q2`，佐证 `evidence/poc-q2-*`）：
  - MI 全流程（断点 / run / next / 栈 / 变量求值 / continue /
    退出）在 gdb 18.1 x64 与 gdb 17.1 x86 上**全部通过**，
    `^error` 计数为 0；
  - 中断机制三级实测：① MI 同步模式（默认）下 `-exec-interrupt`
    无法打断运行中的 inferior（与 Seer 在 Linux 的既有行为一致，
    见 `SeerGdbWidget.cpp:3914` 注释）；② Win32 `DebugBreakProcess`
    在 17.1 上被静默吞掉（版本依赖）；③ **`-gdb-set mi-async on`
    + `-exec-interrupt` 两个版本均有效**（`*stopped` SIGTRAP）；
  - 版本差异清单（实现时处理）：地址宽度 16 vs 8 位十六进制；
    `fullname` 路径分隔符正斜杠（18.1）vs 反斜杠（17.1）；
    `arch` 字符串 `i386:x86-64` vs `i386`。
- 决策：Windows 上 Seer 启动 gdb 时发 `-gdb-set mi-async on`；
  「中断」走 `-exec-interrupt`。`sendGdbInterrupt` 的信号菜单在
  Windows 的映射：SIGINT → `-exec-interrupt`；SIGKILL →
  `TerminateProcess`（或 gdb 的 kill）；其余信号 → 运行期禁用或
  经 gdb 的 `signal` 命令（Phase 3 spec 细化）。
- 影响：audit A4 候选细化为「mi-async + -exec-interrupt 为主」；
  风险 R3 化解。
- 依据：PoC 实测。

## D6 — 进程信息后端：Toolhelp32 + QueryFullProcessImageName + token SID

- 日期：2026-10-20
- 状态：已采纳（Phase 2 Q3 PoC 实证）
- PoC 结论（`evidence/poc-q3-output.txt`）：枚举 total=501（tasklist
  对照 503，差 0.4%）；withPath=318（63%）；withUser=325；自身进程
  的 pid / 名字 / 完整路径 / 用户名全部正确。成功标准 4 项全过。
- 决策：`QProcessInfo` 的 Windows 实现采用 Toolhelp32 快照枚举 +
  `QueryFullProcessImageNameW` 取路径 + 进程 token 的 SID 经
  `LookupAccountSidW` 取用户名；cmdline 留空（日后需要再走 PEB /
  WMI，另立决策）。
- 影响：audit A3 候选落定。
- 依据：PoC 实测。

## D7 — 工具链定案：MinGW64，MSVC 出局

- 日期：2026-10-20
- 状态：已采纳
- 决策：Windows 原生构建只用 MSYS2 MinGW64。MSVC 不作为 Seer 的
  构建路径（如个别行为需要 VC 对照验证，仅编译测试片段，不进
  构建体系）。
- 依据：Phase 0 冒烟与 Phase 2 全部 PoC 均在 MinGW64 上通过；
  MSVC 路线需另装 Qt-MSVC（本机没有）且无对应收益——被调试目标
  以 MinGW 构建为主，gdb 与 MSYS2 工具链生态一致，
  `AGENTS.local.md` 的整套环境也按 MinGW64 搭建。
