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

## D8 — 路径处理三层策略：gdb 命令正斜杠、KEY 折叠大小写、显示按 Windows 习惯

- 日期：2026-10-20
- 状态：已采纳（主人拍板）
- 背景：gdb 17.1 的 `fullname` 字段用反斜杠、符号表 `file` 字段用
  正斜杠（18.1 一致为正斜杠）——同一文件多种拼写导致编辑器按键
  控开双 tab、`--source "E:\..."` 的断点命令又被 gdb 的引号转义
  吃掉反斜杠而失败；Windows 文件系统大小写不敏感又引入第三种
  维度（`E:/Github/x` vs `e:/github/X` 同一文件）；而 Windows 用户
  界面习惯看到反斜杠与真实大小写。三个层面需求互相冲突。
- 决策（三层各司其职，全部 `Q_OS_WIN` 守卫、Linux 为 no-op）：
  1. **gdb 命令层**：所有嵌路径的 MI 命令一律正斜杠、保留原大
     小写（`Seer::normalizeSourcePath()`，应用于 `-file-exec-
     and-symbols` / `-file-exec-file` / `-file-symbol-file` /
     `-environment-cd` / `source -v` / `-target-select core` /
     `-skip-create-file` 及编辑器侧 `--source` 断点命令）。
  2. **KEY 层**：以文件名做 map 键处用正斜杠 + 小写折叠
     （`Seer::normalizeFileKey()`，现用于编辑器 entries；存储的
     fullname 保留正斜杠原大小写）。
  3. **显示层**：面向用户的路径按 Windows 习惯渲染——
     `QDir::toNativeSeparators()`（反斜杠、真实大小写；Linux 上
     天然 no-op），应用于栈帧/断点表格的 fullname 列、编辑器
     tab tooltip、主窗口标题、进程列表 path 列、编辑器文档标题。
- 配套机制：`GdbMonitor` 在 stdout 读行后对每条 MI 记录做
  `fullname="..."` 值内 `\`→`/` 的**记录级归一**（单点覆盖全部
  消费方：编辑器键控、断点对话框预填、各表格；单测验证 17.1
  反斜杠转换、18.1 原样、多字段、无字段不伤、`~` 转义文本不
  误伤五项全过）。
- 有意不覆盖：`-exec-arguments`（用户参数原样）、CLI `--bs` /
  断点对话框里用户手工输入的路径（边界场景，Phase 6 矩阵覆盖）。
- 影响：Phase 5 的 G 双 tab / 断点失败问题的根治方案升级为
  本决策；specs/gdb-glue.md §6 为实施细则。
- 依据：裸 MI harness 实测（17.1/18.1 路径形态）、gdb 引号转义
  吃反斜杠的失败实证、主人对三层职责的拍板。

## D9 — Windows 下 attach 选进程后无条件回填 Program name

- 日期：2026-10-20
- 状态：已采纳（主人拍板，选「Q_OS_WIN 守卫」方案）
- 背景：Debug 对话框会以「默认项目」预填 Program name（上游特性：
  记住上次调试目标），attach 流程选进程后 pid 回填但 exe 名**非空
  不覆盖**（上游规则）——残留的旧目标与被 attach 进程错配时，gdb
  以错误符号解栈，产出垃圾帧（Phase 5 F 首测实测；Linux 上同样
  存在此陷阱）。
- 决策：仅 Windows（`#ifdef Q_OS_WIN`）下，从进程列表选中并确认
  后**无条件**把 Program name 回填为选中进程的可执行路径；守卫：
  空路径与 `[name]` 伪路径（System/不可达进程）不覆盖。字段在选中
  后仍可手改，刻意错配（如配独立符号文件走 Symbol file 栏）不受
  影响。Linux 分支保持上游「非空不覆盖」原文。
- 影响：Windows 侧 attach 流程不再踩预填陷阱；有意偏离上游行为
  仅限 Windows 分支，rebase 时冲突面为单个函数。
- 依据：`.tmp/mancheck/mismatch-test.sh` 的符号错配 1:1 复现；
  主人拍板。

## D10 — Windows 下 QSettings 强制 IniFormat

- 日期：2026-10-20
- 状态：已采纳（路径四维扫描的产出）
- 背景：上游两处功能从 `QSettings::fileName()` 派生真实目录——宏
  文件（`SeerMacroToolButton`：`<configdir>/macros/<ctx>/<M?>.macro`）
  与用户 MI 脚本（`SeerGdbWidget::handleGdbSourceScripts`：
  `<configdir>/scripts/*.gdb`）。Linux 的 NativeFormat 就是 INI 文
  件，成立；Windows 的 NativeFormat 是**注册表**，`fileName()` 返
  回 `\HKEY_CURRENT_USER\...`——两个功能在 Windows 上静默失效
  （目录不存在 / QFile 无法写注册表路径）。
- 决策：`main()` 里 `QSettings::setDefaultFormat(QSettings::IniFormat)`
  （`Q_OS_WIN` 守卫）。实测 settings 落在
  `%APPDATA%\seergdb\seergdb.ini`（真实文件、可检视、可携带），
  macros/scripts 目录随之恢复可用。Linux 的 NativeFormat 本就是
  INI，零变化。
- 影响：Windows 上此前写入注册表的测试期配置不再被读取（一次
  性回到默认值）；与 D8/D9 一起构成路径处理决策族。
- 依据：`--xxx-debug` 无头实测 SETTINGS 路径。

## D11 — MI 线缆编码与转义还原（中文路径三层修复）

- 日期：2026-10-21
- 状态：已采纳（Phase 6 边界场景「非 ASCII 路径」的产出）
- 背景：中文路径（`中文目录/你好世界.exe`、DWARF comp_dir 为
  中文）在 Windows 上三级失败，且第三级连 Linux 也中招：
  1. **写侧**：`SeerGdbWidget::handleGdbCommand` 把 MI 命令按
     UTF-8 写入 gdb stdin，而 Windows gdb 在本机 ANSI 代码页
     （zh-CN=GBK）下解释 stdin——UTF-8 字节的中文路径使
     `-file-exec-and-symbols` 报 `No such file or directory`
     （raw gdb + python 喂 GBK 字节可正常加载，UTF-8 不行，
     双向实证）。
  2. **读侧**：gdb 的 MI 输出（如 `fullname`）里的非 ASCII 以
     `\NNN` 八进制转义出现——线上是纯 ASCII，但 Seer 的解析器
     从不还原 MI 转义，拼出的「文件名」是带 `\344\270\255` 字面
     文本的字符串，编辑器打不开源码并弹
     「Can't find source file」模态框。此为**跨平台缺陷**：
     Linux 上中文源码路径同样失败（Linux 对照行复现）。
  3. **转义字节编码**：gdb 按**host charset** 决定转义字节的编
     码。Seer 给 gdb 的环境是 `LANG=C`（上游为了 MI 消息不被翻
     译）——此时 Windows host charset=ANSI(GBK)，Linux
     host charset=UTF-8。用 python 抓 raw gdb 字节双向实证：
     Windows 发 `\326\320\316\304...`（GBK 八进制）、Linux 发
     `\344\270\255\346\226\207...`（UTF-8 八进制）。
- 决策（三处配套，全部最小 diff）：
  1. 写侧 `Q_OS_WIN` 下 `str.toLocal8Bit()`，其余平台维持上游
     `toUtf8()`（Linux UTF-8 locale 下两者字节一致，上游行为零
     变化）。
  2. 读侧 `GdbMonitor` 两处 `QString(buf)` 改为 Windows
     `fromLocal8Bit` / 其余 `fromUtf8`——与写侧同一规则。
  3. `GdbMonitor` 的记录级 `fullname="..."` 重写块（D8 引入）扩
     展为**全平台**：先（仅 Windows）把 `\` 对折成 `/`（D8 原
     逻辑），再把 `\NNN` 八进制还原成字节，最后按与读侧相同的
     规则解码（Windows=GBK/ANSI，其余=UTF-8）成真实字符。
     ASCII 快路径（字段不含 `\`）零开销直通。
- 影响：中文（及一切非 ASCII）路径下的程序加载、断点、源码显示
  在双平台闭环；ASCII 路径字节完全不变（58 程序矩阵重跑验证）。
  hellopathnorm 回归测试从 12 项扩到 13 项（新增八进制转义用例，
  平台条件化：Windows 用 GBK 八进制、Linux 用 UTF-8 八进制，
  期望两侧同为真实中文）。
- 依据：`.tmp/phase6/` 边界场景 A/B 双行 + raw gdb python 复现
  脚本 + hex 级诊断日志（open-debug/monitor-debug，临时探针，
  已从代码移除）；`tests/hellopathnorm` 13/13 双平台。

## D12 — gdb 随包分发

- 日期：2026-10-21
- 状态：已采纳（Phase 7 打包实测的产出）
- 背景：Seer 的两个硬依赖（见 AGENTS.md 引 issue #94）之一就是
  「带 MI 接口的 gdb」。分发选择：随包携带 gdb，或要求用户自备。
- 决策：**随包分发**。打包目录内并置 `gdb.exe` 及其 MinGW 依赖
  （expat/gmp/iconv/intl/lzma/mpfr/ncursesw/python/readline/
  xxhash/zstd/zlib 一族，与 seergdb 同源同工具链），默认
  `SEER_GDB_NAME="gdb"` 在同目录解析；用户可随时用
  `--gdb-program` 覆盖指向自备版本。
- 依据：实测（`.tmp/phase7/deploy-session.ps1`）——PATH 仅含
  System32 的「无工具链机器」视角下，seergdb + 包内 gdb 的完整
  调试会话（断 main/继续/退出）通过；gdb 与 Qt 运行时同属
  MinGW-w64 生态，DLL 互相兼容无额外成本；Seer (GPL-3) 与
  gdb (GPL) 许可兼容，随包分发无障碍。自备路径仍开放（运行时
  参数覆盖），不排除未来用户偏好。
- 影响：打包体积增加（gdb + 依赖约 60MB）；版本演进时 gdb 与
  Seer 同包升级，避免「用户 gdb 版本差异」这一整类支持负担。

## D13 — 维护期分支重组：main 转 fork 主干，upstream-main 镜像上游

- 日期：2026-10-21
- 状态：已采纳（主人决策）
- 背景：移植 phase 0-7 交付完成，项目转入长期维护（自行修复 +
  定期合并上游）。原布局（`main`=上游基线镜像、`develop`=工作
  分支）有三个问题：GitHub 默认分支是纯上游代码，访客打开 fork
  看不到任何移植成果；长期拿 `develop` 当默认分支语义不达意；
  CI（`build-latest.yml`）触发条件是 push `main`，原布局下永不
  触发。主人提出两个候选：①`main` 继续跟踪上游、`develop` 改名
  `master` 设为默认；②现 `main` 另存为镜像分支、`develop` 合入
  `main` 作为日后开发分支。
- 决策：采方案②之修正版：
  1. `main`：fork 主干与 GitHub 默认分支。原 `develop` 内容纯
     快进合入（原 `main` 是其祖先，无需 force push；GitHub 默认
     分支名不变，零设置改动）。日常开发落点——小修直接提交，
     非平凡改动走短命 `fix/*` 分支合入后删除；`develop` 删除，
     不再维护长期开发分支。
  2. `upstream-main`：上游 `main` 的只读镜像（建自原 `main` 的
     内容——恰为上游 HEAD `8d19e2d`，本地与 origin 同名），
     只在同步流程中快进（`git fetch upstream main:upstream-main`）。
     镜像不叫裸名 `upstream`——与 remote 名撞车，人眼与脚本都
     易混。
  3. `upstream` remote 指向 `https://github.com/epasveer/seer`，
     push URL 设为 DISABLE 防误推。同步一律走 maintenance.md §1
     的本地 merge 流程（pre-sync 锚点 tag + invariants 红线
     核对 + 双平台回归），不用 GitHub 的 "Sync fork" 按钮。同步
     用 merge 不用 rebase：永久 fork 下 rebase 每次重放全部移植
     史、重解一遍冲突且改写已发布 hash。
  4. 弃方案①（main=镜像、develop 改名 master 设默认）的理由：
     「main=上游镜像」违反 main 为主干的通行惯例，本项目由
     agent 长期维护，反直觉命名是持续的出错面；且上游默认分支
     恰也叫 `main`，fork 内 `master`=自己、`main`=镜像上游
     `main` 的名字错位最易在 diff/show/checkout 时拿错边；CI
     触发条件也仍需改造。
  5. 同步建立合并红线清单 `docs/porting/invariants.md`（活文档，
     随上游演化更新；decisions.md 保持「为什么」的历史出处），
     作为每次 merge 前后逐条核对的依据。交付点打 tag
     `v2.8.1-win1`（基线为上游 v2.8.1 之后的 main 快照 `8d19e2d`
     + phase 0-7 移植成果；后续同步随上游 release 递增命名）。
- 影响：`AGENTS.md` 硬性约束 1（分支策略）与构建/背景章节、
  `docs/plan.md` 使用规则与 Phase 7 收尾、maintenance.md §1/§2
  同步改写；`docs/CHANGELOG.md` 记一条。上游同步流程从「从
  upstream/main 切集成分支 merge develop」简化为「在 sync/*
  分支上 merge upstream/main」——冲突标记里 ours=fork 侧，更贴
  合 D2 的冲突处理姿势。
- 依据：主人 2026-10-21 决策（两个候选方案讨论后的拍板）。
