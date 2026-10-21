# Seer Windows fork 维护策略

本 fork 长期自维护（上游对 Windows 移植持 `wontfix` 态度，见
`AGENTS.md` 引 issue #94），因此维护流程是交付物的一部分。
核心约束来自 D2：**跨平台单一代码库 + 对上游文件最小 diff**，
一切流程围绕「让定期 merge 上游保持低成本」设计。

## 1. 上游同步

- 节奏：跟随上游 release（上游大约每季度一个版本）。每个上游
  release 发布后，做一次 merge；小改动攒着随 release 一起合，
  不逐 commit 追。
- 分支（D13/D15）：`main` 是 fork 主干（GitHub 默认分支）；
  `upstream-main` 是上游 `main` 的只读镜像，只在本流程快进；
  `develop`（中长期集成分支）不参与本流程——上游 merge 只落
  `main`，`develop` 下次使用前先 `git merge main` 同步（分支
  策略见 `AGENTS.md` 硬性约束 1）。上游 remote 名为 `upstream`
  （指向 `https://github.com/epasveer/seer`，推送已禁用）。
  **不要用 GitHub 的 "Sync fork" 按钮**——它绕过冲突处理与回归
  验证，直接把上游代码怼进 `main`。
- 流程：
  1. `git fetch upstream`；
     `git log --oneline upstream-main..upstream/main` 预览上游
     新增；
  2. 通读 `docs/porting/invariants.md` 红线清单，明确这次
     merge 哪些不能丢（B 类「行为性红线」最危险：丢失后构建
     绿灯、功能静默坏）；
  3. `git tag pre-sync-<yyyymmdd> main` 留回滚锚点，
     `git switch -c sync/<yyyymmdd> main`，在其上
     `git merge upstream/main`，冲突处理原则见 §2；
  4. 冲突解决完逐条核对 invariants.md，再跑双平台回归：WSL2
     全量构建 + `tests/hellopathnorm` 13 项 + Windows 矩阵抽样
     （`.tmp/phase6/` 装备，建议每类抽 2-3 个：normal/stop/
     stdin/never 各一 + 一个中文路径会话）+ §3 隔离边界 grep
     复查；
  5. 通过后 `git switch main && git merge sync/<yyyymmdd>`（快进）
     并 `git push origin main`（触发 CI）；随后
     `git fetch upstream main:upstream-main`、
     `git push origin upstream-main` 让镜像跟进，删除集成分支；
  6. 更新本 fork 的 CHANGELOG，并在「上游同步记录」追加一行，
     注明「invariants 红线逐条核对无回退」。
- 上游无 release 的窗口期，只在明确需要某个上游修复时按需
  cherry-pick。

## 2. 冲突处理原则（依 D2）

- **平台特有代码放新增文件**（如 `QProcessInfo_win.cpp`、
  `SeerStackTrace.cpp`）——这些文件上游不存在，永不冲突；
- 上游文件内的改动保持「最小、集中、带 `#ifdef Q_OS_WIN` 守卫
  和注释」——冲突时优先保上游结构，把我们的平台块重新嵌入；
- 合并前后的红线核对以 `docs/porting/invariants.md` 为准（它是
  decisions.md 各条「影响面」的提炼）；decisions.md D1-D13 的
  「影响面」一节是细则出处，需要时再深入；
- 冲突解决后必须跑双平台回归（§1 流程第 3 步），不看 diff 自信。

## 3. 平台隔离边界复查

每次上游 merge 后，检查隔离边界是否被新代码穿透：

- grep 清单（新代码里出现即需处理）：
  `posix_openpt|fork\(|vfork|execl|dlfcn|termios|QSocketNotifier.*fd`
  ——Windows 下这些 API 不可用，新代码若引入需走 D1-D3 的
  分流路线；
- `QSettings` 相关：新功能若从 `settings.fileName()` 派生目录，
  检查 D10 的 IniFormat 前提仍成立（Windows 上 fileName() 是
  ini 路径）；
- 路径进 gdb 命令的新入口：按 D8 三层策略补
  `normalizeSourcePath` 包裹（参考 gdb-glue.md §6 的命令点清单，
  把新命令点补进该表）。

## 4. CHANGELOG

本 fork 自己的变更记录在 `docs/CHANGELOG.md`；上游的
`CHANGELOG.md` 保持原样不动。每个 phase 收口/上游 merge 时各记
一条。

## 5. 已知长期观察项

- **R8（plan.md 风险登记）**：偶发的 Qt qFatal（0xC0000602），
  三层捕获网已就绪（AGENTS.local.md §7）；下次复现时 dump +
  fatal 栈日志自动落地，届时解析归因；
- 自动化会话脚本存在约 10-15% 的 F8 早发时序丢弃（早于按钮
  使能），表现为会话不继续——重发即恢复，非 Seer 缺陷，回归
  脚本里已内置重试；
- Linux 侧 `uname` 装备（`.tmp/smoke-linux`、`.tmp/phase6/
  linux-verify.sh`）与 Windows 侧（`.tmp/phase6/`）都在 `.tmp`
  （不进库），换机后按 `AGENTS.local.md` 重建环境时可一并重
  建——必要的话未来把它们提升为 `tests/` 下的脚本。
