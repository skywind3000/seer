# Seer Windows 移植总体计划

本文件是 Windows 移植工作的**主跟踪文档**，面向后续接手的 coding agent
与主人。核心原则：每个 phase 有明确的目标、任务、产出物与**可执行的
验证方法（出口条件）**；出口条件不满足，不进入下一阶段。

配套约定：`AGENTS.md`（项目通用，进版本库）、`AGENTS.local.md`
（本机环境，不进版本库）。本文件只写跨机器通用的内容，本机路径、
工具位置一律引用 `AGENTS.local.md` 的对应章节，不要抄绝对路径进来。

## 使用规则（给 agent）

1. 开工前先读 `AGENTS.md`；若仓库根存在 `AGENTS.local.md`，必须再读它。
2. 所有工作在 `develop` 分支进行；`main` 保持上游基线不动。
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
| 0 | 环境与基线 | 未开始 | 双平台工具链就绪，上游 Linux 版构建通过 |
| 1 | 平台耦合审计 | 未开始 | `docs/porting/audit.md` |
| 2 | 关键技术决策与 PoC | 未开始 | `docs/porting/decisions.md` + PoC 佐证 |
| 3 | 移植 Spec | 未开始 | `docs/porting/specs/` |
| 4 | 垂直切片：Windows 全量构建 | 未开始 | Windows 上核心调试流程可用（console 暂缺） |
| 5 | 平台后端补齐 | 未开始 | inferior 控制台（弹窗）/ 进程信息 / 平台杂项可用 |
| 6 | 系统性交叉验证 | 未开始 | `docs/porting/verification.md` 验证矩阵 |
| 7 | 交付 | 未开始 | `docs/build-windows.md` + 打包 + 维护策略 |

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

**执行记录**：（进行中时在此记日期与备注）

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

**执行记录**：

---

## Phase 2 — 关键技术决策与 PoC

**目标**：把可行性分析与替代方案选型合并成一个以**可运行证据**
为依据的决策阶段。要回答的问题：

（原「ConPTY 数据路径」与「`-inferior-tty-set` inferior 接线」
两问已由决策 D1 化解，不再必答；ConPTY 仅为日后可选增强。）

- **Q1 inferior 控制台（D1 路线确认）**：MI 模式下用
  `-gdb-set new-console on` 让被调试程序弹出独立控制台窗口，确认
  弹窗内输入 / 输出 / Ctrl-C / 程序退出全部正常；同时确认
  QProcess 启动 gdb 时能抑制 gdb 自己的黑窗（如
  `CREATE_NO_WINDOW`），不出现多余窗口。
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
   模式），实际运行一个 console 测试程序，被调试程序弹出独立
   控制台窗口；窗口内输入能到达程序、程序输出可见、Ctrl-C 能
   中断、程序退出后窗口行为符合预期，全程记录 MI 日志。
3. PoC 得出的决策追加到 `docs/porting/decisions.md`（该文件已含
   先期决策 D1/D2，同样只追加），ADR 风格：背景 / 候选项 / 结论 /
   依据（引用 `.tmp/` 里的 PoC 日志，需长期保留的摘录进
   `docs/porting/evidence/`）。
4. 同步更新文末「风险登记」的状态列。

**验证（出口条件）**

- 四个问题各有 PoC 产物与**预先声明的成功标准**，实测达标。
- Q1 有端到端演示日志。
- 每条决策在 decisions.md 中都有可追溯依据；主人 review 通过。

**执行记录**：

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

**执行记录**：

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

**执行记录**：

---

## Phase 5 — 平台后端补齐

**目标**：把 Phase 4 的 stub 换成真实实现，audit 清单全部关闭。

**前置**：Phase 4 完成。

**任务**

1. inferior 控制台接线（依 D1）：Windows 启动调试时改发
   new-console 设置，被调试程序弹出独立控制台窗口；GUI 内
   console 页在 Windows 下隐藏；gdb 自身黑窗保持抑制。
2. `QProcessInfo` 的 Windows 实现（Toolhelp32 或定案方案）。
3. `SeerGdbWidget.cpp` 中 termios / signal 相关逻辑的平台替换。
4. CMake 收尾：gdb 默认路径、Windows 特有编译选项、资源文件。
5. audit.md 的状态列逐项更新为「已关闭」。

**验证（出口条件）**

- console 专项（Windows，弹窗方案）：载入读 stdin 的程序，在弹
  出的独立控制台里验证输入、输出、ANSI 彩色（conhost 渲染）、
  Ctrl-C 中断、程序退出后的窗口行为；确认 GUI 内 console 页已
  隐藏、没有多余的 gdb 黑窗。
- Linux 侧 console 功能回归：PTY 虚拟终端照常可用（冒烟）。
- 进程列表对话框在 Windows 下可用且能选中进程 attach。
- 双 gdb（x64 / x86）回归核心调试流程。
- WSL2 Linux 回归构建通过。
- audit.md 全部条目状态为「已关闭」，否则记录原因并入风险登记。

**执行记录**：

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

---

## 决策记录

| 日期 | 决策 | 依据 | phase |
| --- | --- | --- | --- |
| 2026-10-20 | 总体路线：8 个 phase，先审计/PoC 后编码，垂直切片先行 | 本计划 | 规划 |
| 2026-10-20 | 主选工具链 MinGW64（MSVC 备选），待 Phase 2 PoC 定案 | `AGENTS.local.md` §1 | 规划 |
| 2026-10-20 | D1：Windows 被调试程序控制台走独立终端窗口，不做 GUI 内虚拟终端 | 主人决策，全文见 `docs/porting/decisions.md` | 规划 |
| 2026-10-20 | D2：跨平台单一代码库，最小化对上游文件的 diff，便于定期 merge 上游 | 主人决策，全文见 `docs/porting/decisions.md` | 规划 |

## 风险登记

| 编号 | 风险 | 等级 | 状态 | 对应 |
| --- | --- | --- | --- | --- |
| R1 | inferior 与 console 的接线：`-inferior-tty-set` 是设备名语义，Windows 无直接对应物 | 高 | 已化解（D1） | — |
| R2 | `QSocketNotifier` 在 Windows 只支持 socket，ConPTY 管道需另行接入 Qt 事件循环（overlapped IO / 读线程） | 高 | 已化解（D1，不再做 GUI 内终端） | — |
| R3 | 双 gdb（x64 18.1 / x86 17.1）的 mi 输出细节、地址宽度、寄存器集合差异 | 中 | open | Phase 2 Q2 |
| R4 | WSL2 的 Qt（apt 6.4.x）与 Windows 的 Qt（MSYS2 6.11.x）版本差，代码要同时兼容两个版本 | 中 | open | 全程 |
| R5 | WSL2 apt 源缺 `libqt6charts6-dev` / `libqt6svg6-dev`，Phase 0 被阻 | 中 | open | Phase 0 |
| R6 | `tests/` 相当一部分程序本身不可移植，验证矩阵必须先行筛选 | 低 | open | Phase 6 |
| R7 | `new-console` 在「MI 模式 + QProcess 无控制台启动 gdb」组合下的实际行为未经证实（弹窗时机、多余窗口、Ctrl-C 传递） | 中 | open | Phase 2 Q1 |

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
