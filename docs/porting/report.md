# Seer Windows 移植收尾报告

Phase 0-7 全部完成后的汇总（2026-10-21）。逐 phase 的详细执行
记录在 `docs/plan.md` 各 phase 节内，本报告只做移交视角的浓缩。

## 交付物清单

| 交付物 | 位置 |
| --- | --- |
| 构建说明（从零、实测可复现） | `docs/build-windows.md` |
| 验证矩阵 + 分级已知问题 | `docs/porting/verification.md` |
| 决策记录 D1-D12（ADR） | `docs/porting/decisions.md` |
| gdb 胶合层规格（含 D8/D11 路径与编码策略） | `docs/porting/specs/gdb-glue.md` |
| 维护策略（上游 merge 流程/冲突原则/边界复查） | `docs/porting/maintenance.md` |
| fork 变更记录 | `docs/CHANGELOG.md` |
| 回归测试（13 项自检，双平台） | `tests/hellopathnorm/` |
| 环境与基线 | `docs/porting/environment.md`、`audit.md` |

## 各 phase 一句话结论

| Phase | 结论 |
| --- | --- |
| 0 环境与基线 | 双平台工具链就绪，上游 Linux 版构建通过 |
| 1 耦合审计 | 阻塞点收敛为 PTY 一处 + 少量 POSIX 头（audit.md） |
| 2 决策与 PoC | D1-D7：弹窗控制台、new-console、双位宽 gdb 实证 |
| 3 接口层 | QProcessInfo/中断/路径的跨平台接口 |
| 4 平台补齐 | Windows 后端 + 崩溃捕获网（dump/cdb/栈日志） |
| 5 功能闭环 | D8/D9/D10；Linux console 冒烟 + 人工点验 |
| 6 系统性验证 | 58×2 轮矩阵、12 边界、D11（中文路径）修复、复核复现 |
| 7 交付 | 本报告 + 构建文档 + 打包实测（107MB Release 自包含包）+ 维护策略 |

## 移交要点（新接手者从这开始）

1. 先读 `AGENTS.md`（项目约定）→ `AGENTS.local.md`（本机环境）→
   `docs/plan.md`（跟踪规则与全部执行记录）。
2. 构建：照 `docs/build-windows.md` 一页即可，无需其他隐性知识
   （出口条件实测：删除构建目录后仅按文档重建，128/128 目标）。
3. 回归：`tests/hellopathnorm`（秒级）；大回归装备在
   `.tmp/phase6/`（不进库，重建方法见 maintenance.md §5）。
4. 改动原则：D2 最小 diff；平台差异先进新增文件、后加守卫块；
   每个关键决策先查 decisions.md 有没有先例。

## 遗留风险（移交时仍然有效的）

| 风险 | 状态与处置 |
| --- | --- |
| R8：偶发 Qt qFatal（0xC0000602），同实例二次 Debug 场景 | open——三层捕获网就绪（AGENTS.local.md §7），复现即自动留 dump+栈；自动化重试机制已在回归脚本内置 |
| R4：双平台 Qt 版本差（6.11 vs 6.4） | 全程以「两侧都构建+都跑矩阵」覆盖，未见版本相关问题；merge 上游升 Qt 时重跑双平台回归 |
| 可接受级差异 6 项 | verification.md §5（Stopped 残留显示、attach 错配无提醒、VT 注册表、Go 符号、POSIX dprintf 测试程序、core 加载） |
| 自动化装备在 .tmp 不进库 | 换机需重建（maintenance.md §5 有清单）；必要时提升为 tests/ 脚本 |

## 打包快照（2026-10-21 实测）

- Debug 包：229MB（exe 131MB，未 strip）——开发用；
- Release 包（strip）：**107MB**，其中 gdb+依赖约 60MB、
  Qt+第三方 DLL 约 43MB、seergdb.exe 3.7MB；
- 自包含验证：PATH 仅 System32 的「无工具链机器」视角，
  seergdb + 包内 gdb 完整调试会话（断 main/继续/退出）通过，
  证据 `.tmp/phase7/`（deploy-session.ps1 与截图）。
