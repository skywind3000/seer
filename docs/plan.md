# Seer Windows 移植总体计划

本文件是 Windows 移植工作的**主跟踪文档**，面向后续接手的 coding agent
与主人。核心原则：每个 phase 有明确的目标、任务、产出物与**可执行的
验证方法（出口条件）**；出口条件不满足，不进入下一阶段。

配套约定：`AGENTS.md`（项目通用，进版本库）、`AGENTS.local.md`
（本机环境，不进版本库）。本文件只写跨机器通用的内容，本机路径、
工具位置一律引用 `AGENTS.local.md` 的对应章节，不要抄绝对路径进来。

## 使用规则（给 agent）

1. 开工前先读 `AGENTS.md`；若仓库根存在 `AGENTS.local.md`，必须再读它。
2. 所有工作在 `main` 分支（fork 主干，GitHub 默认分支，D13）进行；
   `upstream-main` 保持上游镜像不动，只在同步上游时快进。合并
   上游或改动移植层前，先读 `docs/porting/invariants.md` 的红线
   清单。
3. 文件去向按类型分，不混放：
   - 临时产物（PoC 源码、运行日志、构建目录、截图）放 `.tmp/`，不提交；
   - 各 phase 的正式**文档**放 `docs/porting/`（environment.md、
     audit.md、decisions.md、specs/、verification.md），**只放文档，
     不放代码**；
   - 生产**代码**只进 `src/`（含 `src/CMakeLists.txt`）；
   - 手工测试程序进 `tests/`，沿用上游 `hello*` 命名习惯；
   - 被 decisions.md 引用的关键佐证日志，从 `.tmp/` 摘录进
     `docs/porting/evidence/`——`.tmp/` 不进版本库，裸引用会失效。
4. phase 开始时：把该 phase 状态改为「进行中」，并在「执行记录」里
   记一行日期。全部出口条件满足后改为「已完成」。需要主人复核的，
   先置「待复核」。
5. 决策与风险的记录：决策全文追加到 `docs/porting/decisions.md`
   （ADR，只追加不删改），并在文末「决策记录」表加一行索引；风险
   登记可更新「状态」列、可追加新行，但不删历史行。
6. 从 Phase 4 起，每个编码阶段结束时必须跑一次 WSL2 的 Linux 回归
   构建，结果写进该 phase 的执行记录。
7. 涉及 gdb/mi 解析、寄存器/内存/地址格式化的改动，要在 64 位与
   32 位两份 gdb 上各验证一遍（两份 gdb 的位置见 `AGENTS.local.md`
   §4）。

状态取值：`未开始` / `进行中` / `待复核` / `已完成` / `受阻`。

## 目标与范围

- 目标：Seer 在 Windows 上原生构建并运行（Qt6 + MinGW64 为主选
  工具链，MSVC 备选），核心调试功能与 Linux 版对等。console 按
  D1 分流：Linux 保持 GUI 内虚拟终端（上游实现不动），Windows
  弹出被调试程序的独立控制台窗口，不做 GUI 内虚拟终端。
- 非目标：说服上游（issue #94 已 `wontfix`）。本 fork 长期自维护，
  定期从上游拉取合并（维护策略见 Phase 7）。
- 代码组织总原则（D2）：所有改动落在 Windows / Linux 共用的单一
  代码库上，「对上游既有文件的最小 diff」是设计约束，平台差异
  优先放新增文件。
- 贯穿全程的硬约束：**Linux 构建与行为不允许回归**，以 WSL2
  Ubuntu-24.04 的构建为准（调用方式见 `AGENTS.local.md` §5）。
- 平台差异的代码策略：优先用 Qt 跨平台抽象；确实平台特有的代码，
  经 Phase 3 定义的接口层隔离，`#ifdef` 收敛到最小范围。

## Phase 总览

| Phase | 名称 | 状态 | 核心产出 |
| --- | --- | --- | --- |
| 0 | 环境与基线 | 已完成 | 双平台工具链就绪，上游 Linux 版构建通过 |
| 1 | 平台耦合审计 | 已完成 | `docs/porting/audit.md` |
| 2 | 关键技术决策与 PoC | 已完成 | `docs/porting/decisions.md`（D4–D7）+ PoC 佐证 |
| 3 | 移植 Spec | 已完成 | `docs/porting/specs/` |
| 4 | 垂直切片：Windows 全量构建 | 已完成 | Windows 上核心调试流程可用（console 走弹窗方案） |
| 5 | 平台后端补齐 | 已完成 | inferior 控制台（弹窗）/ 进程信息 / 平台杂项可用 |
| 6 | 系统性交叉验证 | 已完成 | `docs/porting/verification.md` 验证矩阵 |
| 7 | 交付 | 已完成 | `docs/build-windows.md` + 打包 + 维护策略 |

为什么这样切，见文末「与最初八步设想的映射」。

---

## Phase 0 — 环境与基线

**目标**：Windows 与 WSL2 两套构建环境就绪；上游 Linux 版在 WSL2
完整构建通过，作为后续一切改动的回归基线。

**前置**：无。

**任务**

1. 用 pacman 给 mingw64 装齐 `cmake`、`ninja`、`qt6-base`、
   `qt6-charts`、`qt6-svg`（包名与版本见 `AGENTS.local.md` §3）。
   装包改动系统环境，动手前先征得主人同意。
2. Windows 工具链冒烟：按 `AGENTS.local.md` §6 的完整参数构建一个
   最小 Qt6 Widgets 程序（源码放 `.tmp/`），确认编译、链接、运行
   （弹窗）全链路通。
3. WSL2 Ubuntu-24.04 解决 Qt6 依赖：当前 apt 源缺
   `libqt6charts6-dev` / `libqt6svg6-dev`（大概率要开 universe
   组件；改 WSL 环境前同样先征得主人同意）。
4. 在 WSL2 完成上游 Linux 版完整构建，构建目录用 `.tmp/build-wsl`，
   不要碰上游的 `src/build`。
5. 通读 issue #94 原文（`AGENTS.md` 只是摘要，动手者要看原文）。

**产出**

- `docs/porting/environment.md`：两平台的工具链版本实测记录、
  完整构建命令、踩到的坑。

**验证（出口条件）**

- Windows：最小 Qt 程序在 `.tmp/build-win64` 构建成功且运行出窗口。
- WSL2：`seergdb` 目标构建成功，无 error；构建日志尾部存档到
  `.tmp/`。（GUI 运行需要 X server，本 phase 不作硬性要求。）
- `docs/porting/environment.md` 里的命令可以被照抄复现。

**执行记录**：2026-10-20 开工。实测：MSYS2 侧 cmake / ninja /
qt6-base / qt6-svg / qt6-charts 已全部装好（版本与 `AGENTS.local.md`
§3 一致，pacman 安装步骤免做）；WSL2 的 universe 组件已启用，
Qt6 开发包尚未安装。

2026-10-20 完成。Windows 冒烟（构建 + 运行 exit 0）；WSL2 apt 装
`qt6-base-dev` / `qt6-charts-dev` / `qt6-svg-dev`（6.4.2，注意
noble 包名）；`seergdb` 基线构建 `[100%]` 通过，日志存
`.tmp/wsl-build.log`；issue #94 通读（21 条评论）。产出
`docs/porting/environment.md`，佐证摘录进 `docs/porting/evidence/`。
全部出口条件满足，置「已完成」。

---

## Phase 1 — 平台耦合审计

**目标**：摸清全部平台耦合点并逐条给出候选方案。注意：**不做**
238 个文件的全量代码分析——风险集中在耦合点与 gdb/mi 链路上，
全量分析性价比低，`AGENTS.md` 已沉淀了 grep 级清单，本 phase 把它
做实做全即可。

**前置**：Phase 0 完成（审计中的「可编译验证」需要环境）。

**任务**

1. 建立可复跑的 grep 清单，覆盖至少这些模式：
   `termios.h`、`pwd.h`、`unistd.h`、`sys/ioctl.h`、`TIOCS*`、
   `posix_openpt` / `grantpt` / `unlockpt` / `ptsname`、`signal.h` /
   `SIG*`、`kill(`、`fork(`、`exec*(`、`waitpid`、`/proc/`、`pipe(`
   、`dup2`、`dirent`、`fsync`、对裸 fd 的 `QSocketNotifier`。
   清单写成文档内的命令块，保证任何人可一键复跑。
2. 精读热点文件全文：`src/SeerConsoleWidget.cpp`、
   `src/QProcessInfo.cpp`、`src/SeerGdbWidget.cpp`、
   `src/CMakeLists.txt`。
3. 理清 gdb/mi 交互链路并写成一小节：QProcess 如何启动 gdb、
   console 的 PTY 设备名如何经 `-inferior-tty-set` 交给 inferior
   （现于 `src/SeerGdbWidget.cpp:2053`）——这是 Windows 侧最大的
   架构问题，审计必须把数据流画清楚。
4. 汇总为 `docs/porting/audit.md`，每个耦合点一条记录，字段：
   位置（文件:行）、Linux 语义、Windows 候选方案、风险等级、
   状态（open / 已有方案 / 已关闭）。

**验证（出口条件）**

- grep 清单复跑后**零未解释命中**：每条命中都能对应 audit 表中
  的某个条目（含误报也要标注为何是误报）。
- 与 `AGENTS.md` 已知清单对照，已知四项（ConsoleWidget / 
  QProcessInfo / SeerGdbWidget / CMakeLists）之外的新发现全部入表。
- audit 表每一行的「Windows 候选方案」列非空。
- 主人 review 通过（状态置「待复核」→「已完成」）。

**执行记录**：2026-10-20 开工（grep 清单与热点文件精读在 Phase 0
构建等待期并行完成）。

2026-10-20 审计完成。产出 `docs/porting/audit.md`：9 个耦合点
（A1–A9）全部有候选方案；grep 命中归零核对完毕（含误报标注与
`/tmp` 假阴性教训）；AGENTS.md 清单外新发现 5 项。出口条件中仅剩
「主人 review」，置「待复核」。佐证：`evidence/audit-grep*.txt`。

2026-10-20 复核通过（主人委托 agent 复核）。复核动作：按入库脚本
`evidence/run-audit-grep.sh` 复跑 grep 清单（74 行，零未解释命中）；
用 MinGW 实编译证实 A4 断言（SIGKILL/SIGUSR1/kill/strsignal 均
不存在，SIGINT/SIGFPE/SIGSEGV 存在）；修正 A5（`unistd.h` 亦是
悬空 include，共 4 行）。全部出口条件满足，置「已完成」。

---

## Phase 2 — 关键技术决策与 PoC

**目标**：把可行性分析与替代方案选型合并成一个以**可运行证据**
为依据的决策阶段。要回答的问题：

（原「ConPTY 数据路径」与「`-inferior-tty-set` inferior 接线」
两问已由决策 D1 化解，不再必答；ConPTY 仅为日后可选增强。）

- **Q1 inferior 控制台（D1/D3 路线确认）**：gdb 以无控制台方式
  启动（`CREATE_NO_WINDOW`）后，按 2×2 矩阵实测——发不发
  `-gdb-set new-console on` × console / GUI 两种 inferior：console
  型应自动弹独立控制台且输入 / 输出 / Ctrl-C / 退出正常；GUI 型
  不弹任何窗口；两种情况下 inferior 输出都不得混入 MI 流。依 D3
  首选「不发命令」路线，new-console 仅在默认行为不达标时启用。
- **Q2 gdb MI 兼容性**：QProcess 跑 Windows 原生 gdb（64 位 18.1
  与 32 位 17.1 都要），`-file-exec-and-symbols`、`-break-insert`、
  `-exec-run`、`-exec-next`、栈/变量打印是否与 Linux 行为一致。
- **Q3 进程信息**：`QProcessInfo` 需要的进程枚举，Windows 用
  Toolhelp32 / PSAPI 是否足够。
- **Q4 工具链定案**：MinGW64（倾向）vs MSVC。

**前置**：Phase 0 完成（PoC 需要可用的编译器与 Qt）；Phase 1 的
grep 清单先行（不必等审计文档完全定稿）。

**任务**

1. 每个问题各写一个独立小 PoC，源码与日志放 `.tmp/poc-*`。
   先写下成功标准，再动手，跑完把实测输出存档。
2. Q1 的 PoC 必须端到端：QProcess 以无控制台方式启动 gdb（MI
   模式），分别运行 console 型与 GUI 型测试程序——console 型弹
   独立控制台窗口且输入 / 输出 / Ctrl-C / 退出正常，GUI 型不弹
   任何窗口，inferior 输出不得混入 MI 日志，全程记录。
3. PoC 得出的决策追加到 `docs/porting/decisions.md`（该文件已含
   先期决策 D1/D2，同样只追加），ADR 风格：背景 / 候选项 / 结论 /
   依据（引用 `.tmp/` 里的 PoC 日志，需长期保留的摘录进
   `docs/porting/evidence/`）。
4. 同步更新文末「风险登记」的状态列。

**验证（出口条件）**

- 四个问题各有 PoC 产物与**预先声明的成功标准**，实测达标。
- Q1 有端到端演示日志。
- 每条决策在 decisions.md 中都有可追溯依据；主人 review 通过。

**执行记录**：2026-10-20 开工并完成。四个 PoC：Q1 console 矩阵
（5 组合，结论 D4：`new-console on` + `CREATE_NO_WINDOW`，
「不发命令」路线被 MI 污染证伪）；Q2 双 gdb MI 全流程（x64 18.1 /
x86 17.1 全过，中断定案 `mi-async on` + `-exec-interrupt`，D5）；
Q3 Toolhelp32 枚举达标（D6）；Q4 工具链定案 MinGW64（D7）。风险
R3、R7 已化解。成功标准与佐证存 `evidence/poc-*`。仅剩
「主人 review」，置「待复核」。

2026-10-20 复核通过（主人委托 agent 复核）。对证：D4–D7 与
evidence/ 佐证逐条核实（log1 污染 21 行 / log3 零污染且
`-gdb-set new-console` 返回 `^done`、双 gdb SUMMARY 全过
errors=0、mi-async 双版本接受、Q3 计数与自证全对）。如实说明一项：
Q4 未另立独立 PoC，以「Phase 0 冒烟 + 本阶段全部 PoC 均运行于
MinGW64」为依据定案，符合「决策有可追溯依据」口径。置「已完成」。

---

## Phase 3 — 移植 Spec

**目标**：把 Phase 1 + 2 的结论落成可执行的设计文档，编码前把
「怎么改」想清楚。

**前置**：Phase 1、Phase 2 完成。

**任务**

1. 控制台行为分流（依 D1）：不做 console 双实现后端。Linux 侧
   `SeerConsoleWidget` 上游实现原样保留；Windows 侧以最小改动
   实现「隐藏 console 界面 + `-inferior-tty-set` 调用点改发
   new-console 设置」。进程信息仍做后端接口抽象（`/proc` vs
   Toolhelp32），接口面尽量小。
2. 逐文件改动说明：每个 audit 条目对应的改动内容、涉及文件、
   验证方式。
3. CMake 方案：WIN32 分支（现有 `src/CMakeLists.txt:305-310` 的
   GUI 子系统处理之上扩展）、`SEER_GDB_NAME` 默认值与探测、
   编译警告选项、Windows 构建文档化。
4. stub 策略（Phase 4 用）：console / 进程信息在 Windows 上先降
   级——console 界面隐藏、进程信息禁用；尽量运行期降级、编译期
   少 `#ifdef`。
5. gdb 兼容规格：32/64 位地址宽度、寄存器集合差异、版本差异的
   应对。
6. 产出放 `docs/porting/specs/`（可拆多个文件）。

**验证（出口条件）**

- **追溯表**：audit.md 的每一条都能映射到某份 spec 的某个章节，
  无遗漏。
- 每份 spec 都写明对应的 Linux 无回归验证手段。
- **diff 最小化检查（D2）**：逐文件核对对上游既有文件的改动是否
  小而集中；能用新增文件承载的平台代码不改上游文件。
- 主人批准后才能进入 Phase 4（这是唯一的编码闸口）。

**执行记录**：2026-10-20 开工并完成。产出 `specs/` 四份：overview
（总则、改动全景约 60 行 / 6 个上游文件 + 1 个新文件、CMake 规格、
追溯表、验证总纲）、console（A1/A2/A9）、processinfo（A3 +
`QProcessInfo_win.cpp`）、gdb-glue（A4/A5/A6/A8 + 缺失信号宏
占位）。自查：追溯表覆盖 A1–A9 无遗漏；diff 最小化核对通过
（平台新代码只在 `QProcessInfo_win.cpp`，全部 `#else` 分支为
原文）；关键发现：`mi-async` 上游已在 run/attach 路径发送
（:919/:1069），D5 的实现成本归零。置「待复核」，等待主人批准
进入 Phase 4（编码闸口）。

2026-10-20 主人批准（对话：「继续推进」），置「已完成」。

2026-10-20 补记：Phase 4 依本 spec 实施。

---

## Phase 4 — 垂直切片：Windows 全量构建

**目标**：整个项目（约 238 个源文件）在 Windows 上编译、链接、
启动，并跑通**无 console 的核心调试流程**。尽早拿到端到端反馈，
不把所有难点堆到最后。

**前置**：Phase 3 完成，spec 获批。

**任务**

1. 按 spec 改 CMake，用 `AGENTS.local.md` §6 的命令在
   `.tmp/build-win64` 做全量构建。
2. 清扫编译错误：预期是散布在各文件的小问题（头文件、类型、
   Qt 版本差异等），按 spec 的 stub 策略处理平台特有部分——
   console 界面隐藏（inferior 弹窗接线留到 Phase 5）、进程信息
   先禁用，保证可编译可运行。
3. 运行期冒烟：启动 Seer，连接 Windows 版 gdb，载入
   `tests/` 里无 stdin 依赖的程序（如 `hellofibonacci`、
   `helloarray`），走断点、单步、变量查看、栈查看、内存查看。

**验证（出口条件）**

- `.tmp/build-win64` 全量构建 0 error（warning 记录不阻塞）。
- **WSL2 Linux 回归**：`.tmp/build-wsl` 重新构建成功（这是第一次
  编码回归点，之后每个 phase 都要重复）。
- 冒烟清单逐项完成，截图 / gdb 日志存 `.tmp/`。
- 冒烟在 64 位 gdb 与 32 位 gdb 各跑一遍核心流程（载入、断点、
  单步、变量）。

**执行记录**：2026-10-20 开工并完成主体。

- spec 全部落地：6 个上游文件改动 + 新增 `QProcessInfo_win.cpp`。
- 编译扫障两轮：① 全项目约 238 个文件里仅
  `SeerConsoleWidget.cpp` 失败（`fcntl`/`F_GETFD`/`fsync` MinGW
  缺，按 spec 预案加守卫修复）；② 链接失败——
  `QProcessInfo_win.cpp` 的 `Q_OS_WIN` 守卫位于任何 Qt include
  之前导致整个文件编译为空翻译单元，加 `<QtCore/QtGlobal>` 修复
  （宏陷阱已补记进 specs/processinfo.md）。
- Windows 全量构建 0 error，产物 `.tmp/build-win64/seergdb.exe`
  （Debug，约 130MB）；WSL2 回归构建 `[100%]`、0 error。
- 冒烟（日志存 `.tmp/smoke/`）：
  - x64（gdb 18.1 + `hellofibonacci` + `--break-function main`）：
    断点命中（`*stopped,reason="breakpoint-hit"` 于
    hellofibonacci.cpp:6）、停机后栈视图自动刷新
    （`^done,stack=` ×3）、inferior 弹**可见**控制台（窗口标题=
    exe 路径）、gdb 无黑窗、MI 流干净；
  - x86（gdb 17.1 + `helloarray` 跑到结束）：`exited-normally`、
    MI 流无程序输出污染、17.1 的 `fullname` 反斜杠差异实证。
  - 观察：启动期若干 `^error "No registers."`（寄存器视图查询撞
    上 inferior 运行态的竞态，与平台无关，Phase 6 复核）。
- **剩余人工项（GUI 交互无法自动化）**：单步执行、变量视图、内存
  视图的点验，以及界面整体观感——待主人点验后关账。置「待复核」。

2026-10-20 追记：主人复测**无法重现**崩溃（外层 gdb 抓栈会话在
跑，未捕获）；基本点验通过（开调试、设断点、观察 local 正常）。
QProcessInfo_win 直测通过（Phase 5 预备，日志
`.tmp/poc-q3/`）：枚举 504 条，name/path/user 覆盖 503/325/332，
自身进程四字段全对，System 类进程正确走方括号降级。崩溃捕获网
补强：HKCU LocalDumps 实测无效（只认 HKLM），待主人管理员会话
注册。Phase 4 维持「待复核」；单步 / 内存视图人工项并入
Phase 6 矩阵。

2026-10-20 关账：主人以管理员权限重启会话后完成崩溃捕获网部署
（WER LocalDumps + DontShowUI + 外层 gdb 包装，端到端实测 fail-fast
也能留 dump；配置与 msys 父进程坑记入 `AGENTS.local.md` §7 与
`environment.md`）。点验结论：基本流程通过，崩溃转 R8 跟踪。
全部出口条件按「可自动化项 + 基本人工点验」达成，置「已完成」。

---

## Phase 5 — 平台后端补齐

**目标**：把 Phase 4 的 stub 换成真实实现，audit 清单全部关闭。

**前置**：Phase 4 完成。

**任务**

1. inferior 控制台接线（依 D1/D3）：按 Phase 2 Q1 定案的机制，
   首选「不发任何 console 命令、由 PE 子系统自动决定弹不弹」；
   GUI 内 console 页在 Windows 下隐藏；gdb 自身黑窗保持抑制。
2. `QProcessInfo` 的 Windows 实现（Toolhelp32 或定案方案）。
3. `SeerGdbWidget.cpp` 中 termios / signal 相关逻辑的平台替换。
4. CMake 收尾：gdb 默认路径、Windows 特有编译选项、资源文件。
5. audit.md 的状态列逐项更新为「已关闭」。

**验证（出口条件）**

- console 专项（Windows，弹窗方案）：载入读 stdin 的程序，在弹
  出的独立控制台里验证输入、输出、ANSI 彩色（conhost 渲染）、
  Ctrl-C 中断、程序退出后的窗口行为；另载入 GUI 型测试程序确认
  不弹任何多余窗口；确认 GUI 内 console 页已隐藏、没有多余的
  gdb 黑窗。
- Linux 侧 console 功能回归：PTY 虚拟终端照常可用（冒烟）。
- 进程列表对话框在 Windows 下可用且能选中进程 attach。
- 双 gdb（x64 / x86）回归核心调试流程。
- WSL2 Linux 回归构建通过。
- audit.md 全部条目状态为「已关闭」，否则记录原因并入风险登记。

**执行记录**：2026-10-20 开工。进展：

- 任务 1（inferior 控制台接线）：Phase 4 已随 spec 落地并经冒烟实证
  （弹窗 + MI 干净）；剩 console 交互人工项（stdin 回显 / ANSI /
  Ctrl-C / 弹窗关闭行为）待主人点验。
- 任务 2（QProcessInfo）：实现 + 直测通过（枚举 504 条、自身四字段
  全对、System 类正确降级，`.tmp/poc-q3/`）；进程列表对话框人工
  点验待做。
- 任务 3（termios/signal 替换）：sendGdbInterrupt 映射已落地构建
  通过；-exec-interrupt/mi-async 机制 PoC Q2 实证，菜单目验待做。
- 任务 4（CMake 收尾）：gdb 默认值 / 警告门 / 资源均已随 Phase 4
  完成。
- 任务 5（audit 关账）：A1–A9 全部关闭（记录见 audit.md §7；
  A4 的中断运行期验证与 R8 一并归入 Phase 6 矩阵）。
- 待做：Linux 侧 console 运行期冒烟（WSL2 无显示，方案：apt 装
  xvfb 后 CLI 冒烟验证 `-inferior-tty-set` 路径，代码层已由
  「#else 原文 + 构建通过」保障）。

2026-10-20 追记（主人提的两项需求）：

- **WinDbg 分析链路打通**（主人已装 WinDbg）：经典
  `cdb.exe`（Windows Kits）对 fail-fast dump 实测出完整调用栈
  （系统 DLL 带符号、自家帧给模块+偏移）；自家帧用 MinGW 自带
  `addr2line` 解析 DWARF。路径与用法记入 `AGENTS.local.md` §7。
- **fatal 栈日志落地**（依主人要求，未引第三方库——MinGW 的
  DWARF 符号 dbghelp 系库解析不了，原生采集 + addr2line 离线
  解析更稳）：新增 `src/SeerStackTrace.{h,cpp}`（R8 调查配套，
  也是长期可观测性改进）——qFatal/Q_ASSERT/std::terminate 时写
  `%TEMP%\seer-fatal-<pid>.log`；Windows 用
  `CaptureStackBackTrace`（链接时基址从 PE 文件头读，规避加载器
  改写），Linux 用 `backtrace()`；CMake 为 WIN32 加
  `-fno-omit-frame-pointer`。端到端实测：fatal-test 的日志经
  addr2line 解析出 `capture`/`writeFatalLog`/`messageHandler`
  的函数:文件:行号。

2026-10-20 追记（换机）：主人更换开发机，环境按 `AGENTS.local.md`
重建并全部实测通过：pacman 补装 Qt6 三件套（6.11.2，与旧机同版；
moc/uic 实际在 `share/qt6/bin`，文档已更正）；WSL2 补装
`qt6-{base,charts,svg}-dev`（6.4.2）与 xvfb（Linux console 冒烟的
前置）；Windows 全量构建 0 error（`[128/128]`，offscreen 启动正常）、
WSL2 回归构建 `[100%]` 双通过；崩溃捕获网三层在新机器重新部署并
端到端实测（空指针崩溃 → WER dump → cdb `.ecxr;k` 出栈 → addr2line
解析回 `crash.c:3`，cdb 调用的三个坑与部署细节记入
`AGENTS.local.md` §7）。环境就绪，Phase 5 剩余项可继续。

2026-10-20 追记（Linux console 冒烟）：「待做」清单最后一项完成。
WSL2 无显示环境下用 Xvfb + xdotool + tesseract OCR 实现 CLI 驱动的
运行期冒烟（佐证存 `.tmp/smoke-linux/`：5 张截图 + 脚本 + console
stdout 日志），验证点全部通过：

- `-inferior-tty-set` 接线（硬证据：inferior 的 fd 0/1/2 →
  `/dev/pts/N`，master 端在 seergdb）；
- PTY 输出路径：程序提示语渲染进 console（OCR + stdout 日志双证）；
- PTY 输入路径：stdin 输入框键入 "Linwei"，inferior 应答
  "Thanks Linwei!" 并正常退出（PTY 回显 + 应答都留在日志里）；
- ANSI 16 色真实渲染（console 区域颜色统计：红/绿/黄/蓝/品红/青
  像素俱在，QAnsiTextEdit 工作正常）；
- MI 流与 console 无混流（console 只有程序输出与一行 gdb 的
  controlling-terminal 警告——WSL 下 TIOCSCTTY 受限所致，上游行为）。

排障中确认两处**环境坑**（详记 `AGENTS.local.md` §5）：WSLg 给进程
注入 `WAYLAND_DISPLAY`，Qt6 平台选择优先 Wayland，GUI 会绕过 Xvfb
弹到 Windows 桌面，自动化必须 `QT_QPA_PLATFORM=xcb`；Messages 页签
的 `raisetabmode` 默认会在程序启动时抢焦点，冒烟需预写 QSettings。
至此 Phase 5 出口条件仅剩主人人工点验项（console 交互观感 / 进程
列表对话框 / 中断菜单）。

2026-10-20 追记（人工点验第一批反馈的查证）：主人按 A–E 清单实测，
D（GUI 型不弹窗）、E（Interrupt 按钮）通过；A/B/C 的疑点经 Linux
Xvfb 对照 + 裸 gdb MI harness 查证定性，均非移植缺陷：

- **退出事件两平台逐字一致**：`*stopped,reason="exited",exit-code="01"`
  + `=thread-group-exited`（18.1 与 15.1 输出相同；mi-async 下退出后
  `-thread-info` 均返回 `^done,threads=[]`）。「程序退出后 Thread
  视图显示 Stopped」属上游代码路径上的显示细节——Windows 侧 MinGW
  程序自带 4 个运行时线程使退出级联更长，Linux 对照表现为该行直接
  消失；不影响功能（程序确实退出、记录正确），列入 Phase 6 已知
  差异观察项。
- **`--run` 不停机时源码区不自动打开 = 上游设计**（源码区跟随停机
  帧渲染）。Linux `--start` 实测：编辑器自动打开 helloname.cpp 并
  停在 main（佐证 `.tmp/smoke-linux/shotVS1.png`）。
- **弹窗随程序退出立即关闭是 D1 弹窗方案的固有行为**（B 项"一闪
  而过"非缺陷；Linux 版输出留在 GUI 终端里可回看，是两种 console
  形态的已知取舍，decisions.md D1 已涵盖）。另编了结尾 getchar 的
  `helloansicodes-pause.exe` 供复测观感。
- **Ctrl-C 打断阻塞 stdin 是平台共有语义**：Linux gdb+PTY 实测
  （`cctrlc.sh`）——程序停于 `__GI___libc_read`，continue 后 cin
  语义性失败（Windows 为 ReadConsole abort → 空名 "Thanks !"；
  Linux 为 EINTR → cin 可继续读），"Thanks!" + continue 后退出
  两平台同构。
佐证：`.tmp/smoke-linux/{v-verify.sh,cctrlc.sh,shotVR1/2,shotVS1}`。

2026-10-20 追记（第一批点验发现两个真缺口，已修复）：主人在 x86
（gdb 17.1）会话与 B 复测中复现两个移植缺口——**非换机回归**（两机
同一 commit；第一台机器的 x86 冒烟只到 CLI 级，未做过 GUI 断点交互，
Phase 4 记录的「17.1 fullname 反斜杠差异实证」正是未拆的地雷）：

- **G：同一源文件双 tab + 按源码行建断点失败**。根因（裸 MI
  harness 实证）：gdb 17.1 `fullname` 字段用反斜杠、符号表 `file`
  字段用正斜杠（18.1 一致为正斜杠）——Seer 以 fullname 做编辑器
  键控与断点命令拼接，双拼写→双 tab；`--source "E:\..."` 引号
  转义被 gdb 吃掉反斜杠→"No source file named E:Github..."。
  修复：新增 `Seer::normalizeSourcePath()`（SeerUtl，
  `Q_OS_WIN` 守卫），应用于编辑器 entries 键控与 sourceArea 的
  `open()`/`handleText`（实施细节见 specs/gdb-glue.md §6）。
  Windows `[58/58]`、WSL2 `[100%]` 双构建 0 error。待主人复测。
- **B：弹窗 console 不渲染 ANSI**。根因：conhost 默认不开 VT
  处理，转义序列按字面打印（Linux 版是 Seer 自绘终端，不受影响；
  属 D1 弹窗方案此前未暴露的缺口）。本机已设
  `HKCU\Console\VirtualTerminalLevel=1`（AGENTS.local.md §8）供
  复测；程序化启用列为 Phase 5/6 候选任务。
- A-1 补充确认：主人复验退出 5 秒后红色 "Stopped" 行**持久残留**
  （Messages 正确记录 "Program exited normally" / "Program
  terminated"）——纯显示残留，上游代码路径，列入 Phase 6 已知
  差异清单，不阻塞。

2026-10-20 追记（F 首测失败的查证）：主人首测 F（attach 后栈只有
一条垃圾帧 `level 0 / addr 0x1 / arch i386`、无 helloloop 源码）。
系统性排除后定位：**Debug 对话框会用当前会话的可执行名预填
Program name（SeerMainWindow.cpp:648），attach 选进程后 pid 回填
但非空的 exe 名不会被覆盖**——主人在 G 测试（hellofib32，32 位）
之后的同一窗口里做 F，实际加载的是 32 位 hellofib32 的符号去
attach 64 位 helloloop，gdb 以 i386 架构状态解 64 位栈即产出垃圾
帧（harness 1:1 复现：`*stopped,frame={addr="0x0",func="??",
arch="i386"}`）。**定性为 gdb 语义下的用法陷阱，非移植缺陷**（
Linux 上符号错配 attach 同样垃圾栈）；「exe 与 pid 不匹配无提示」
列为 Phase 6 UX 候选。排除过程中实测干净的项（attach 相关）：
反斜杠引号路径、`selectedFullname` 的无扩展名混合分隔符形态
（`path/name`，gdb 自动补 .exe）、`-gdb-set new-console on`、
无头全流程复刻（MI 流里 `^done,stack` 三帧完整含
`main at helloloop.c:8`）。另证：32 位 gdb attach 64 位进程直接
失败（exec 格式不识别 + error 50），不会出现「猎物被挂起」——
与主人观察到的挂起行为互斥，反证 F 用的是 64 位 gdb。
佐证：`.tmp/mancheck/{attach-harness*.sh,mismatch-test.sh,
noext-test.sh,repro-f2.sh,mi.log}`。

2026-10-20 追记（F 复测中的两处观察定性 + 一处修正）：主人复测
发现新窗口 Program name 仍预填 hellofib32——确认为上游「默认项目」
特性（`loadDefaultProjectSettings`，新窗口恢复上次调试目标，Linux
同样行为），非缺陷；主人清空后再选进程的做法即正确姿势。「attach
时 exe 与 pid 不匹配无提示」维持 Phase 6 UX 候选。另一处
`selectedFullname` 回填 `E:/.../mancheck/helloloop`（无 .exe）是
移植粗糙点：`QProcessInfo_win` 的 name 用了 `completeBaseName()`
（过度模仿 Linux 无扩展名语义）——功能上 gdb 会自动补 .exe（实测），
但显示形态不对。已修正为 `fileName()`（Windows 进程名天然带 .exe，
与 Linux「/proc 名即 basename」语义对齐，回填
`E:/.../mancheck/helloloop.exe`）。双平台重建 0 error（Windows
重编 QProcessInfo_win.cpp；WSL 回归通过）。

2026-10-20 关账：主人完成全部人工点验（A–G）。F 修正版通过——
清空预填的 Program name 后选中 helloloop（回填完整
`.../helloloop.exe`），attach 后 Frames 三帧干净
（ZwDelayExecution / SleepEx / main@helloloop.c:8）、Source 面板
有 helloloop.c、Continue / Interrupt / 断点全部正常。收尾项在
attach 模式下显示为 **"Detach"**（`SeerMainWindow.cpp:1148`——
同一动作按模式改名，上游设计），点击后会话结束、helloloop 进程
随之结束（上游 `-exec-kill` 路径对 attach 场景即此行为，Linux
一致）。

Phase 5 出口条件逐项达成：

- console 专项（弹窗方案）：stdin / 输出 / ANSI 彩色（conhost VT
  注册表启用后渲染正常）/ Ctrl-C 中断 / 退出关窗 / GUI 型不弹窗 /
  GUI 内 console 页隐藏 / 无 gdb 黑窗 ✓
- Linux 侧 console 回归：PTY 双向 + ANSI 16 色（Xvfb 冒烟三重取
  证）✓
- 进程列表对话框 + attach ✓（QProcessInfo Windows 后端，含 name
  带 .exe 修正）
- 双 gdb（x64 18.1 / x86 17.1）核心调试流程 ✓（含 §6 路径归一
  化修复后的断点交互）
- WSL2 Linux 回归构建 ✓（每次编码改动后均复跑）
- audit.md A1–A9 全部「已关闭」✓

本 phase 累计修复：路径分隔符归一化（`Seer::normalizeSourcePath`
+ 编辑器键控/open 接入，specs/gdb-glue.md §6）、QProcessInfo 的
name 带扩展名、conhost VT 启用。遗留观察项归档：退出后 Thread
视图 "Stopped" 残留（Phase 6 已知差异清单）、attach 时 exe/pid
不匹配无提示（Phase 6 UX 候选）、Seer 程序化启用 VT（Phase 5/6
候选，现为注册表方案）。置「已完成」。

2026-10-20 追记（关账后加固，依 D8）：主人点验后追加路径处理
系统性审计与三层归一化实施（决策 D8，细则 specs/gdb-glue.md §6）：
- **命令层**：`-file-exec-and-symbols` / `-file-exec-file` /
  `-file-symbol-file` / `-environment-cd` / `source -v`（--bl）/
  `-target-select core` / `-skip-create-file` 七类命令点全部
  `normalizeSourcePath` 包裹（此前仅编辑器侧 `--source` 已归一）；
- **KEY 层**：新增 `Seer::normalizeFileKey()`（正斜杠 + 小写
  折叠，Windows 文件系统大小写不敏感），编辑器 entries 键控
  改用之；存储 fullname 保留正斜杠原大小写；
- **记录级**：`GdbMonitor` stdout 读行后对 `fullname="..."` 值
  做 `\\`→`/` 归一（单点覆盖全部消费方；单测五项全过：17.1
  转换 / 18.1 原样 / 多字段 / 无字段不伤 / `~` 转义文本不误伤）；
- **显示层**：栈帧与断点表格 fullname 列、编辑器 tab tooltip、
  主窗口标题、进程列表 path 列、编辑器文档标题改为
  `QDir::toNativeSeparators()`（反斜杠 + 真实大小写，Linux no-op）。
审计边界（有意不覆盖）：`-exec-arguments` 用户参数、CLI `--bs`
与断点对话框手工输入路径（Phase 6 矩阵覆盖）。Windows `[60/60]`、
WSL2 `[100%]` 双构建 0 error；无头冒烟无崩溃。
- 同批落地 **D9**（主人拍板）：Windows 下 attach 选进程后无条件
  回填 Program name（空路径/`[name]` 伪路径不覆盖，字段选中后仍
  可手改；Linux 保持上游「非空不覆盖」原文）——根治默认项目预填
  与 attach 的符号错配陷阱。Windows `[3/3]`、WSL2 `[100%]` 双
  构建 0 error。
- 主人复测发现 attach 回填出现混合分隔符路径（`E:\...\mancheck/
  helloloop.exe`）——根因是显示层改造把**同时作为数据源**的表格
  列（进程列表 path 列被 `selectedFullname()` 读回拼接）转成了反
  斜杠。修复：**数据/显示分离**——既是显示又是数据的列用
  `Qt::UserRole` 存内部正斜杠形态，列文本仅做显示转换，读取方一
  律读 UserRole（进程列表 / 栈帧 fullname 列 / 断点 fullname 列
  三处）；Debug 对话框回填后字段显示全反斜杠原生形态。同时把
  「进 gdb 路径归一」的剩余入口全部关闭（主人要求无例外）：
  断点创建对话框 `--source`、CLI `--bs` 的 file:line 规格、
  printpoint `dprintf-channel`。至此除 `-exec-arguments`（用户
  参数，非 gdb 路径）与 pre/post 原始 gdb 脚本外，所有路径入口
  均归一（细则 specs/gdb-glue.md §6）。双平台构建 0 error。
- 主人要求**路径四维全量扫描**（传递/保存/显示/拼接），发现并修
  复五处 Windows 问题：①宏文件与用户 MI 脚本目录派生自
  `QSettings::fileName()`，Windows 注册表形态使其静默失效——
  `main()` 强制 IniFormat（D10，实测
  `%APPDATA%\seergdb\seergdb.ini`）；②源码浏览器列表的去重
  `QMap<fullname>` 未折叠大小写 + fullname 列被双击回读——改
  `normalizeFileKey` 键 + UserRole 数据/显示分离；③外部编辑器
  `%{file}` 替换值改为原生分隔符（OS 进程启动应给原生形态）；
  ④源码 alternate 目录列表 setter 归一（QFileDialog 反斜杠来
  源）；⑤"Locate File" 的 `_alternateDirectory` 在 open() 归一。
  扫描确认无恙的项：temp 路径、MI 脚本拷贝、CLI attach 拼接、
  项目 .seer 读写（editor fullname 已归一）。遗留 Phase 6 项：
  `source -v` / `source <脚本>` / `-target-select core` 等未加引
  号的路径命令在路径含空格时断裂（上游行为，跨平台；`tests/
  hello spaces` 场景覆盖）。双平台构建 0 error。
- 补充正式回归测试 `tests/hellopathnorm/`（fork 新增，自检式、
  exit code 语义）：链接**真实** SeerUtl/GdbMonitor 源码（含
  moc），覆盖 normalizeSourcePath 4 例、normalizeFileKey 3 例
  （含「同一文件不同拼写命中同一键」）、GdbMonitor 记录级归一
  集成 4 例（真实 MI 线格式经假 gdb 进程喂入、按 SeerGdbWidget
  同款接线收信号断言：17.1 反斜杠转换 / 18.1 原样 / `~` 转义
  文本不误伤 / 无 fullname 记录不伤）；期望值按平台条件化
  （Windows 变换、其余恒等）。回归入口 `.tmp/regression/run.sh`
  （临时脚本：双平台构建+运行+无头冒烟，输出 REGRESSION_PASS/
  FAIL）。实测：Windows 12/12、Linux 12/12、冒烟无 fatal——
  首轮全绿。构建注意：测试链接需 Qt6Core/Gui/Widgets/Charts
  （SeerUtl 的 chartTheme 辅助用了 QChart）+ GdbMonitor 的 moc
  产物。

---

## Phase 6 — 系统性交叉验证

**目标**：把「测试与验证」做成系统性的矩阵，而不是零散体验。

**前置**：Phase 5 完成。

**任务**

1. 从 `tests/` 的 93 个程序中筛选 Windows 适用子集；明确不可移植
   的（fork / exec / dlopen / curses / cobol / cuda / fortran 类）
   标记 Linux-only 并说明理由。
2. 建立验证矩阵：每行一个场景，字段：测试程序、验证功能点、
   Windows 结果、（可行的话）WSL2 Linux 对照结果、结论。console
   相关功能点在 Windows 下按 D1 验证弹窗行为，对照行标注为
   「行为分流」而非缺陷。
3. 边界场景必测：程序崩溃（`hellocore` 类）、含空格路径
   （`tests/hello spaces`）、非 ASCII 路径（中文目录）、长输出
   滚动、attach / detach、多会话、`hellodebugbreak`。
4. 产出 `docs/porting/verification.md`：矩阵 + 已知问题清单
   （分级：阻塞 / 应修 / 可接受）。

**验证（出口条件）**

- 矩阵行数不少于筛选后适用子集数，每行有结论。
- 无「阻塞」级未解决问题；「应修」级问题全部有 issue 式记录。
- 主人抽查若干场景确认验收。

**执行记录**：

- 2026-10-20：Phase 6 开工。先做 tests/ 程序筛选分类，再批量构建
  Windows 适用子集，复用 Phase 5 的自动化装备（Xvfb/MI harness/
  截图验证）跑矩阵，边界场景逐项验证。
- 2026-10-21：Phase 6 主体完成，产出
  `docs/porting/verification.md`。摘要：
  - **筛选**：93 条目 → 58 适用 + 32 Linux-only + 2 嵌入式 +
    1 双侧排除（helloutl，Qt5 QRegExp）。构建 Windows 58/0/2、
    Linux 54/0/6 全绿（跳过均有理由记录）。
  - **核心矩阵**：58 程序 × 真实 GUI 会话（`seergdb --bf main -r`
    启动 + 进程级停点检测 + F8 继续 + 退出判定 + 截图 OCR 抽查）
    **58/58 PASS**。首轮 8 个非 PASS 全部溯源为程序自身行为
    （长运行/stdin/事件循环/Go 符号语义），按正确流程重跑通过。
  - **边界场景 12 项**（§4）：崩溃（SIGSEGV）、空格路径、中文
    exe 路径、中文源码路径、长输出、Ctrl+I 打断、attach/detach
    （detach 后 inferior 存活）、hellodebugbreak 8×SIGTRAP、
    弹窗控制台 stdin 注入（原生 WriteConsoleInput）、32 位
    gdb 17.1 交叉 3/3、同实例 Restart 多会话——除 B11 对话框
    二次会话留人工抽查外全部 PASS。core 加载确认为平台已知差异。
  - **D11（应修→已修）**：中文路径三重缺陷（写侧 UTF-8 vs gdb
    ANSI、MI `\NNN` 八进制转义从不还原、转义字节 host-charset
    误解码——Linux 同中招转义层）。三处配套修复 + hellopathnorm
    扩至 13 项双平台全绿。ADR 见 decisions.md D11，规格
    gdb-glue.md §6b。
  - **Linux 对照**：Xvfb 会话 9 行（含中文路径两行、D11 后
    PASS），其余程序构建 OK；hellostack 交互引用 Phase 5
    v-verify 证据。
  - 装备沉淀（`.tmp/phase6/`）：build-programs.sh 双平台批量构建、
    session-functions.ps1 会话自动化、coninj.c 控制台注入器、
    linux-verify.sh 对照 runner、矩阵 TSV 与截图证据。
  - 出口条件：行数/结论 ✔、阻塞级无 ✔、应修级有记录 ✔
    （K1 已修、K2=R8 捕获网就绪）、主人抽查 → **待复核**
    （建议抽 B3/B4 中文路径、B7 attach/detach、B11 二次会话）。
- 2026-10-21：复核完成（代主人抽查）。全矩阵在 D11 终版构建上
  完整重跑：非 PASS 集合与首轮一致（可复现）、修正流程后再次
  58/58；边界 stdin/attach/gdb32 复跑通过。复核修正文档三处
  （验证对象双跑事实/日期/项数）、清掉两处证据污染、记录一个
  八进制还原理论边角（gdb-glue.md §6b）与一次 gdb32 偶发
  FAIL_STOP（重试 0.7s 干净通过，观察项）。B11 二次会话对话框
  留主人手工点验。Phase 6 置「已完成」（主人令推进 Phase 7
  即验收确认）。

---

## Phase 7 — 交付

**目标**：可移交的构建文档、打包方案与长期维护策略。

**前置**：Phase 6 完成。

**任务**

1. `docs/build-windows.md`：从零开始的 Windows 构建说明，通用
   写法；本机特有路径引用 `AGENTS.local.md`。
2. 打包：windeployqt 收集 DLL；决策 gdb 分发策略（随包分发 vs
   要求用户自备），写入决策记录。
3. 维护策略：定期 merge 上游的流程与节奏、冲突处理原则（依 D2
   的最小 diff 约束）、平台隔离边界复查、CHANGELOG 记在本 fork
   自己的文档里（上游 `CHANGELOG.md` 不动）。
4. 收尾报告：各 phase 执行记录汇总、遗留风险。

**验证（出口条件）**

- 删除 `.tmp/build-*` 后，**仅**按 `docs/build-windows.md` 从零
  构建成功（模拟新机器，全程不查 `AGENTS.local.md` 之外的隐性
  知识）。
- 主人验收通过。

**执行记录**：

- 2026-10-21：Phase 7 开工。写 docs/build-windows.md → 从零构建
  实测（删 .tmp/build-win64 重建）→ windeployqt 打包与独立运行
  实测 → D12（gdb 分发策略）→ 维护策略与 fork CHANGELOG →
  收尾报告。
- 2026-10-21：Phase 7 完成。产出与实测：
  - **build-windows.md**：从零构建出口条件实测——删除
    .tmp/build-win64 后仅按文档 configure+build，128/128 目标
    通过、新二进制会话冒烟通过（含一次已知 F8 时序偶发，重试
    即过）。文档参数取自原构建的 CMakeCache（Ninja/Debug/
    CMAKE_PREFIX_PATH/QT6），非拍脑袋。
  - **打包实测**：windeployqt 之外发现 MSYS2 工具链必须补两块
    （MinGW 运行时 + Qt 第三方依赖 icu/freetype/harfbuzz/
    glib 等，导入表闭包两轮迭代收敛）——缺任何一块
    0xC0000135 静默失败。配方已写入 build-windows.md §4。
  - **独立运行验证**：PATH 仅 System32 的「无工具链机器」视角，
    Debug 包与 Release 包（strip，107MB，含 gdb）完整调试会话
    均通过（断 main/继续/退出，截图留证 .tmp/phase7/）。
  - **D12**：gdb 随包分发（同目录并置，--gdb-program 可覆盖）。
  - **维护策略**：maintenance.md（上游 merge 流程/冲突原则/
    隔离边界 grep 复查清单/CHANGELOG 约定/长期观察项）。
  - **fork CHANGELOG**：docs/CHANGELOG.md 首条（上游 CHANGELOG
    不动）。
  - **收尾报告**：docs/porting/report.md（交付物清单/phase 汇总/
    移交路径/遗留风险/打包快照）。
  - 出口条件：从零构建 ✔（仅按文档，未查隐性知识）；
    主人验收 → **待复核**。
- 2026-10-21 追记（换回第一台机器复验）：仓库同步 `b293bc5` 后对
  phase 4–7 成果做整机复验。通过项：仅按 build-windows.md 从零全量
  构建 `[128/128]` 0 error；hellopathnorm 双平台 13/13；WSL2 回归
  `[100%]`；x64 gdb 18.1 与 x86 gdb 17.1 真实会话冒烟（断 main →
  F8 继续 → 退出 → Alt+F4 优雅关窗 code 0，无孤儿 gdb；x86 首跑
  复现已知的 gdb32 偶发卡停，重试即过——两机各一次，维持观察项）；
  干净 PATH（仅 System32）打包会话实证：seergdb 启动、spawn gdb、
  inferior 停在 main（截图留证）。**发现并修复 build-windows.md §4
  缺口**：初版手工 DLL 清单漏 9 个（zstd/zlib1/bz2/jpeg/graphite2/
  gobject/gio/intl/iconv），仅按文档从零复现即加载失败——已改为
  「导入表闭包迭代到不动点」权威流程并附收敛全集（含「新拷 DLL
  必须重扫」教训）。本机环境补齐：HKCU Console VT 注册表
  （AGENTS.local.md §8）、moc/uic 位置更正（在 `share/qt6/bin`）、
  ConstrainedLanguage PowerShell 说明（§9）。佐证：`.tmp/verify-m1/`。
  Phase 7 维持「待复核」。
- 2026-10-21 追记（维护期启动）：主人确认 phase 0-7 交付完成，
  项目转入长期维护期。起始动作：分支重组（D13——`main` 转为
  fork 主干与 GitHub 默认分支、`upstream-main` 镜像上游、
  `develop` 删除）与合并红线清单
  （`docs/porting/invariants.md`）。置「已完成」。

---

## 决策记录

| 日期 | 决策 | 依据 | phase |
| --- | --- | --- | --- |
| 2026-10-20 | 总体路线：8 个 phase，先审计/PoC 后编码，垂直切片先行 | 本计划 | 规划 |
| 2026-10-20 | 主选工具链 MinGW64（MSVC 备选），待 Phase 2 PoC 定案 | `AGENTS.local.md` §1 | 规划 |
| 2026-10-20 | D1：Windows 被调试程序控制台走独立终端窗口，不做 GUI 内虚拟终端 | 主人决策，全文见 `docs/porting/decisions.md` | 规划 |
| 2026-10-20 | D2：跨平台单一代码库，最小化对上游文件的 diff，便于定期 merge 上游 | 主人决策，全文见 `docs/porting/decisions.md` | 规划 |
| 2026-10-20 | D3：细化 D1——Windows 只有终端程序弹终端窗口，GUI 程序不弹；不探测程序类型，由 PE 子系统自动决定 | 主人决策，全文见 `docs/porting/decisions.md` | 规划 |
| 2026-10-20 | D8：路径处理三层策略——gdb 命令正斜杠、KEY 折叠大小写、显示按 Windows 习惯；GdbMonitor 记录级 fullname 归一 | 主人拍板，全文见 `docs/porting/decisions.md` | Phase 5 |
| 2026-10-20 | D9：Windows 下 attach 选进程后无条件回填 Program name（守卫伪路径；Linux 保持上游原文） | 主人拍板，全文见 `docs/porting/decisions.md` | Phase 5 |
| 2026-10-20 | D10：Windows 下 QSettings 强制 IniFormat（macros/scripts 目录派生自 fileName()，注册表形态失效） | 路径四维扫描产出，全文见 `docs/porting/decisions.md` | Phase 5 |
| 2026-10-21 | D11：MI 线缆编码与转义还原——写 toLocal8Bit（Win）/读 fromLocal8Bit、`fullname` 的 `\NNN` 八进制按 host-charset 还原（全平台） | Phase 6 中文路径边界产出，全文见 `docs/porting/decisions.md` | Phase 6 |
| 2026-10-21 | D12：gdb 随包分发（同目录并置 gdb.exe + MinGW 依赖，--gdb-program 可覆盖） | Phase 7 打包实测产出，全文见 `docs/porting/decisions.md` | Phase 7 |
| 2026-10-21 | D13：维护期分支重组——`main` 转 fork 主干（GitHub 默认分支），`upstream-main` 镜像上游，`develop` 删除；新增合并红线清单 `docs/porting/invariants.md` | 主人决策，全文见 `docs/porting/decisions.md` | 维护期 |
| 2026-10-21 | D14：Windows 默认等宽字体按偏好列表解析（Cascadia Code → Mono → Consolas → Courier New），用户已设字体不受影响，Linux 零变化 | 主人决策，全文见 `docs/porting/decisions.md` | 维护期 |
| 2026-10-21 | D15：恢复长期 develop——中长期功能集成分支（动手前先 merge main，完成后归并回 main；上游同步仍只落 main） | 主人决策，全文见 `docs/porting/decisions.md` | 维护期 |
| 2026-10-21 | D16：仓库更名 seer-windows（发布准备）——仓库层改名、产品/二进制层保持 seergdb；origin remote 更新 | 主人决策，全文见 `docs/porting/decisions.md` | 维护期 |
| 2026-10-21 | D17：发布 CI——release 触发自动构建并上传 `seer-<版本>-win64.zip`（闭包打包 + gdb 随包）；win32 经核实不可行（MSYS2 无 i686 Qt6，且实测 64 位 gdb 可调 32 位程序） | 主人决策，全文见 `docs/porting/decisions.md` | 维护期 |

## 风险登记

| 编号 | 风险 | 等级 | 状态 | 对应 |
| --- | --- | --- | --- | --- |
| R1 | inferior 与 console 的接线：`-inferior-tty-set` 是设备名语义，Windows 无直接对应物 | 高 | 已化解（D1） | — |
| R2 | `QSocketNotifier` 在 Windows 只支持 socket，ConPTY 管道需另行接入 Qt 事件循环（overlapped IO / 读线程） | 高 | 已化解（D1，不再做 GUI 内终端） | — |
| R3 | 双 gdb（x64 18.1 / x86 17.1）的 mi 输出细节、地址宽度、寄存器集合差异 | 中 | 已化解（Q2 实证全流程通过，差异清单见 D5） | Phase 2 Q2 |
| R4 | WSL2 的 Qt（apt 6.4.x）与 Windows 的 Qt（MSYS2 6.11.x）版本差，代码要同时兼容两个版本 | 中 | open | 全程 |
| R5 | WSL2 apt 源缺 `libqt6charts6-dev` / `libqt6svg6-dev`，Phase 0 被阻 | 中 | 已化解（包名问题：noble 用 `qt6-charts-dev` / `qt6-svg-dev`） | Phase 0 |
| R6 | `tests/` 相当一部分程序本身不可移植，验证矩阵必须先行筛选 | 低 | 已化解（Phase 6：58 适用 / 32 Linux-only / 2 嵌入式 / 1 双侧排除，理由与跳过全记录于 verification.md §1） | Phase 6 |
| R8 | 主人人工点验发现：第二次 File→Debug 会话中设置断点时 seergdb 偶发崩溃（事件日志：Qt6Core.dll +0x8045，异常 0xC0000602 fail-fast = Qt qFatal/Q_ASSERT 路径，非内存踩踏；复现一次后重试不崩） | 高 | open（三层捕获网已上线并全部实测：①WER LocalDumps 对正常启动 100% 留 dump（含 fail-fast）；②cdb 可从 dump 出完整调用栈（系统 DLL 帧带符号）；③**fatal 栈日志**——新增 `src/SeerStackTrace.cpp`，qFatal/Q_ASSERT/std::terminate 时自动写 `%TEMP%\seer-fatal-<pid>.log`（消息+位置+调用栈+addr2line 地址），addr2line 离线解析已实测精确到函数:文件:行号。配置见 `AGENTS.local.md` §7） | Phase 4/6 |
| R7 | `new-console` 在「MI 模式 + QProcess 无控制台启动 gdb」组合下的实际行为未经证实（弹窗时机、多余窗口、Ctrl-C 传递） | 中 | 已化解（Q1 实证见 D4；Ctrl-C 人工交互项留 Phase 5） | Phase 2 Q1 |

## 与最初八步设想的映射

| 最初设想 | 本计划 | 理由 |
| --- | --- | --- |
| 1. 全量代码分析写到 docs/legacy | Phase 1（聚焦审计） | 238 个文件全量分析性价比低；风险集中在耦合点与 gdb/mi 链路，`AGENTS.md` 已有 grep 级底子 |
| 2. 可行性分析、找卡点 | 并入 Phase 2 | issue #94 的卡点已知（PTY），可行性重点变成 PoC 实证 |
| 3. 找替代方案 | 并入 Phase 2 | 方案选型与可行性天然一体，用 PoC 定案 |
| 4. 生成 specs | Phase 3 | 不变 |
| 5. 决定测试与验证、交叉测试 | 拆两半：各 phase 的「验证」小节 + Phase 6 | 验证前移：每阶段都有出口条件，综合交叉验证集中在 Phase 6 |
| 6. 编码 | Phase 4 + Phase 5 | 拆成「先全量构建跑通骨架（stub）」与「补平台后端」，尽早拿到端到端反馈 |
| 7. 模块验证 + 综合验证 | Phase 5 单项验证 + Phase 6 | 对应拆分 |
| 8. 交付 | Phase 7 | 基本不变 |
| （新增） | Phase 0 | 没有就绪的双平台构建环境，后面每一步的「验证」都无从谈起 |
