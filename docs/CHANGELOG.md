# Seer Windows fork 变更记录

本文件是 fork 自身的变更记录（上游 `CHANGELOG.md` 保持原样）。
决策详情见 `docs/porting/decisions.md`（D1-D20），验证矩阵见
`docs/porting/verification.md`，总体计划见 `docs/plan.md`。

## 2026-10-21 — Windows exe 内嵌应用图标（D21）

- 新增 `src/resources/icons/seergdb.ico`（上游 512px 美术生成，
  16-256 七档）、`src/seergdb.rc`（`1 ICON` + `1 24`
  RT_MANIFEST）、`src/seergdb.exe.manifest`（asInvoker +
  supportedOS——显式替代 ld 对自带 .rsrc 的 exe 不再自动补的
  默认 manifest）；`src/CMakeLists.txt` WIN32 块接线
  `enable_language(RC)`；`.gitignore` 对 `seergdb.ico` 加定向例外
  （上游 `icons/` 规则挡掉了它）。零 C++ 改动，Linux 构建不受影响。
- 验证：PE 资源目录逐层解析（1 组图标 + 7 档 + 1 manifest）、
  提取图标与 ico 条目逐像素比对、Windows/WSL2 双平台构建、exe
  冒烟。随 v2.8.1-win3 发布。

## 2026-10-21 — README 头部加 fork 说明块（D20）

- `README.md` 头部新增双语（英文 + 中文）fork 说明块：fork 定位
  与上游关系（issue #94）、维护状态、releases 下载（自包含 zip、
  版本号规则）、Windows 下 console 差异、双平台构建入口、fork
  文档入口；块尾标明其后为上游原文。上游文档「保持原样」约束对
  README 头部开此唯一例外（AGENTS.md §3 已同步）。

## 2026-10-21 — 废除 push 级 CI（D19）

- 删除 `build-latest.yml`：push / PR / develop 归并不再触发任何
  CI；自动构建只在发布 release 时发生（`windows-release.yml`）。
  「Linux 不回归」守卫回归本地 WSL2 构建（maintenance.md §1 的
  双平台回归仍为 merge 强制步骤）。

## 2026-10-21 — 发布版本号规则成文（D18）

- tag 一律 `v{上游版本}-win{N}`：fork 发布计数 +1、上游基线
  升级时重置；tag 只在真发布时打、打在要发布的 `main` 提交
  上。细则见 `docs/porting/maintenance.md` §6。

## 2026-10-21 — 发布 CI：release 自动构建并上传 Windows 包（D17）

- 新增 `windows-release.yml`：发布 release 时自动用 MSYS2
  MINGW64 构建（Release + strip），按 `docs/build-windows.md`
  §4 配方打包（windeployqt + DLL 导入表闭包 + gdb 随包），产
  出 `seer-<版本>-win64.zip` 挂回该 release；支持
  workflow_dispatch 指定 tag 演练（无 release 时自动建 draft）。
- win32 包经核实不可行（MSYS2 无 i686 Qt6）且非必要（实测
  64 位 gdb 可调 32 位程序，见 D17）；不采用 Qt 静态链接
  （D17 分析）。
- 废弃删除上游遗留的手动打包 workflow（snap/flatpak/debian/
  macos 等 11 个），保留 `build-latest.yml` 作 Linux CI 守卫。

## 2026-10-21 — 仓库更名 seer-windows（发布准备，D16）

- GitHub 仓库更名为 `skywind3000/seer-windows`（fork 关系、tag
  与 CI 历史保留，旧 URL 自动重定向）；开发机 origin remote
  更新为 <https://github.com/skywind3000/seer-windows.git>。产品
  与二进制名保持 `seergdb` 不变——设置路径（D10）、WER 注册键、
  打包（D12）均不受影响。

## 2026-10-21 — 恢复 develop 集成分支（D15）

- 建立长期 `develop`（自 `main` 顶端分离并推送 origin）：中长期
  功能先在其上开发调试（动手前 `git merge main` 同步，完成后
  归并回 `main`）；短期修改仍直接改 `main`；上游同步只落
  `main`，`develop` 不从 upstream 取更新。

## 2026-10-21 — Windows 默认等宽字体按偏好列表解析（D14）

- 未设置字体的默认分支在 Windows 下按 **Cascadia Code →
  Cascadia Mono → Consolas → Courier New** 取首个已安装项
  （此前 Qt 对泛型 `monospace` 的替换是机器相关的任意值，实测
  落 BIZ UDGothic / Courier New）；Linux 行为零变化，用户已设
  字体不受影响。12 处默认字体调用点经
  `Seer::resolveMonospaceFamily` 包裹，双平台构建 + fonttest
  实测通过（`evidence/font-default-test.txt`）。

## 2026-10-21 — 维护期启动：分支重组（D13）+ 合并红线清单

- **分支重组（D13）**：`main` 转为 fork 主干（GitHub 默认分支，
  原 `develop` 内容纯快进合入），上游镜像另存为 `upstream-main`
  并保留在 origin；`develop` 删除。配置 `upstream` remote（指向
  epasveer/seer，推送禁用）。同步上游走
  `docs/porting/maintenance.md` §1 的本地 merge 流程（pre-sync
  锚点 + 红线核对 + 双平台回归），不用 GitHub 的 "Sync fork"
  按钮。
- **新增 `docs/porting/invariants.md`**：合并红线清单——结构
  性/行为性不变量逐条「红线 + 坑 + 验证 + 出处」，merge 冲突
  解决后逐条核对，防止移植成果被无声回退。
- 交付点打 tag `v2.8.1-win1`；phase 0-7 经主人确认完结
  （plan.md Phase 7 置「已完成」）。

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
