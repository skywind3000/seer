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
