# Seer Windows fork 变更记录

本文件是 fork 自身的变更记录（上游 `CHANGELOG.md` 保持原样）。
决策详情见 `docs/porting/decisions.md`（D1-D12），验证矩阵见
`docs/porting/verification.md`，总体计划见 `docs/plan.md`。

## 2026-10-21 — Windows 移植完成（Phase 0-7）

基于上游 seer 2.x（分支起点），在 `develop` 分支完成 Windows
原生移植：

- **Windows 原生构建**（MinGW-w64 + Qt 6.11，MSYS2）：
  从零构建说明见 `docs/build-windows.md`（实测可复现）。
- **控制台分流（D1/D3/D4）**：被调试程序在 Windows 弹独立控制台
  窗口（GUI 程序不弹），stdin/stdout/ANSI 颜色全链路可用；
  Linux 保持 GUI 内嵌终端不动。
- **平台后端（Phase 3-4）**：`QProcessInfo_win.cpp`（Toolhelp32
  进程列表）、`SeerStackTrace.cpp`（qFatal 崩溃栈日志）、
  中断/信号菜单的 Windows 映射与隐藏。
- **路径处理（D8/D10/D11）**：三层策略（gdb 命令正斜杠/KEY 折叠
  大小写/显示按 Windows 习惯）+ 记录级 fullname 归一；QSettings
  IniFormat（macros/scripts 目录恢复可用）；MI 线缆编码与
  `\NNN` 八进制转义还原（中文路径双平台闭环，Linux 同步修复）。
- **attach 体验（D9）**：选进程后无条件回填 Program name。
- **验证**：58 程序 GUI 会话矩阵 ×2 轮全 PASS、12 边界场景、
  32 位 gdb 交叉、Linux Xvfb 对照（`docs/porting/verification.md`）。
- **回归测试**：`tests/hellopathnorm` 13 项自检（双平台）。
- **打包（D12）**：windeployqt + MinGW 闭包 + gdb 随包，
  干净机器视角实测通过；Release 包 107MB（含 gdb）。
- **维护**：`docs/porting/maintenance.md`（上游 merge 流程、
  冲突原则、隔离边界复查清单）。

已知遗留：R8 偶发 qFatal（捕获网就绪待复现）、若干可接受级
差异清单见 verification.md §5。

## 上游同步记录

（每次 merge 上游后在此追加一行。）
