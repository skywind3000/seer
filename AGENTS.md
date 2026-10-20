# AGENTS.md

本文件面向后续迭代本项目的 coding agent。硬性约束放在前面，背景资料放在后面。

## 项目概况

Seer 是 Linux 上的 gdb GUI 前端（Qt6 / C++17）。本仓库是它的 fork，
长期目标是做 Windows 移植（见下文）。

- 上游项目：<https://github.com/epasveer/seer>
- 上游关于 Windows 移植的 feature-request 及原作者的讨论：
  <https://github.com/epasveer/seer/issues/94>

issue #94 是本项目最重要的背景资料，动手移植前请先读完它。

## 硬性约束

### 1. 分支策略

- `main`：保留上游原始代码，只作为查证与 diff 对照的基线。不要在
  `main` 上提交任何改动。
- `develop`：本项目后续所有更改都在这个分支上进行，是默认工作分支。
- 需要对照原始实现时：

      git show main:src/SeerGdbWidget.cpp
      git diff main..develop -- src/

### 2. 临时文件一律放 `.tmp/`

本项目产生的所有临时文件——构建试验、抓取的日志、一次性脚本、中间
产物——都放在仓库根目录的 `.tmp/` 内，不要散落到根目录、`src/` 或
`tests/`。`.tmp/` 不提交。

### 3. 文档一律放 `docs/`

本项目后续产生的各种文档都放在仓库根目录的 `docs/` 内。

注意区分：`notes/` 是上游作者的开发笔记（`notes/README.*`），
`README.md` / `STYLEGUIDE.md` / `CHANGELOG.md` / `CONTRIBUTING.md` 是
上游文档，都保持原样，不要往里加自己的东西。

### 4. 动手前先加载 `AGENTS.local.md`

如果仓库根目录存在 `AGENTS.local.md`，**必须先读它再开始干活**。里面
记录的是本机的构建环境说明：各种构建工具装在哪里、怎么调用、需要预先
设置哪些环境变量。

分工是这样的：

- `AGENTS.md`（本文件）：项目本身的约定，跨机器通用，进版本库。
- `AGENTS.local.md`：这台机器的环境细节，因机器而异，不进版本库
  （已在 `.gitignore` 里）。

两者冲突时以 `AGENTS.local.md` 为准。也不要把 `AGENTS.local.md` 里的
本机路径、工具位置抄进本文件。

### 5. 移植工作按 `docs/plan.md` 跟踪

- Windows 移植的主计划是 `docs/plan.md`：8 个 phase，每个 phase 有
  任务清单和「验证（出口条件）」。关键决策全文在
  `docs/porting/decisions.md`（ADR，只追加不删改）。
- 做移植相关工作前**必须先读 plan.md**，找到当前进行中的 phase，按
  该 phase 的任务执行；出口条件不满足，不进入下一 phase。
- phase 开始时把该 phase 状态改为「进行中」；出口条件全部满足后，
  **自己动手**把状态改为「已完成」，并在「执行记录」里记一行；
  需要主人复核的先置「待复核」。
- 期间做出的关键决策追加到 decisions.md，并在 plan.md 文末的
  「决策记录」表加一行索引。
- 其余细则（状态取值、文件去向、Linux 回归要求）见 plan.md 的
  「使用规则（给 agent）」一节。

## 构建与验证

具体的构建工具位置与调用方式见根目录的 `AGENTS.local.md`（若存在）。
下面是项目侧的构建约定。

原版**不支持 Windows 原生构建**。如果非要 build 原版代码，请到 WSL2
里去 build。

- WSL2 发行版：`Ubuntu-24.04`（WSL1 的 `Debian` 不用于本项目）。
- 构建入口是 `src/CMakeLists.txt`，这是仓库里唯一的 CMake 工程，根目录
  没有 `CMakeLists.txt`。

      cd src/build
      cmake -DCMAKE_BUILD_TYPE=Debug -DQTVERSION=QT6 ..
      make -j$(nproc) seergdb

- 依赖的 Qt6 组件：Core、Gui、Widgets、PrintSupport、Charts、Svg、
  OpenGL。
- `src/build_seerutl/` 是 `SeerUtl` 的独立小测试工程，它的 cmake 参数
  是单个 `.`（CMakeLists.txt 就在该目录内），不是 `..`。
- `.github/workflows/windows-latest.yml` 名字有误导性：它实际是在
  WSL Ubuntu-22.04 里构建 Linux 版，并不是 Windows 原生构建。

### WSL2 环境现状（2026-10-20 探测）

- 已有：`g++`、`cmake`、`make`、`gdb`、`pkg-config`。
- 缺少：`ninja`。
- Qt6 开发包一个都没装。`qt6-base-dev` 在 apt 里有候选版本 6.4.2，但
  `libqt6charts6-dev` 和 `libqt6svg6-dev` 显示 `Candidate: (none)`，
  说明当前启用的 apt 源里没有这两个包（大概率是 universe 组件没开）。

也就是说，首次构建前需要先解决 Qt6 依赖。安装系统包属于改动 WSL 环境，
动手前先跟主人确认。

## 长期目标：Windows 移植

目标是在 Windows 上原生构建并运行 Seer（Qt6 + MinGW 或 MSVC），而不是
长期依赖 WSL 跑 Linux 版。

总体实施计划（phase 划分、各阶段验证出口条件）见 `docs/plan.md`，
已定决策见 `docs/porting/decisions.md`；跟踪规则见上面「硬性约束」
第 5 条。本节其余内容是背景资料，与决策冲突时以 decisions.md 为准。

### 上游的态度（issue #94）

原作者 epasveer 的立场几经反复，截至 2026-03-02 issue 仍是 open，但带
`wontfix` 标签：

- 2022-09-27 首次回复：认为「代码里没什么真正 OS 相关的东西」，只需要
  Windows 上的 Qt、cmake 和 gdb。
- 2022-09-28 社区反馈 `termios.h` 在原生 Windows 编译器（含 MSYS2 的
  gcc/clang）上根本不存在，Cygwin 可以但「会失去意义」。作者认同。
- 2025-07-31 作者以「不借助 Cygwin 或 WSL 在 Windows 上构建 Seer，我
  认为不可能，尤其是这个 console 问题」为由关闭 issue。
- 2025-08-03 又重开：「Reviving task ... again. There's some hope.」
- 作者反复强调的现实障碍是**他没有 Windows 开发机**，不打算为这一个
  特性买一台。
- 2026-03-02 最新总结：Seer 依赖两样东西——(1) 带 MI 接口的 gdb，
  (2) 通过 Linux PTY 接口实现的 console 窗口，「两者都是必须的」。

### 核心阻塞点：PTY

真正的硬骨头只有一个，就是 `SeerConsoleWidget` 用 POSIX 伪终端给被调试
程序提供 stdin/stdout：

- `src/SeerConsoleWidget.cpp:317-358` `createTerminal()`：
  `posix_openpt` / `grantpt` / `unlockpt` / `fcntl(F_SETFL, O_NDELAY)` /
  `ioctl(TIOCSWINSZ)` / `ioctl(TIOCSCTTY)` / `ptsname`
- `src/SeerConsoleWidget.cpp:266,272,286`：直接 `write` / `fsync` /
  `read` 裸 fd
- `src/SeerConsoleWidget.cpp:390`：用 `QSocketNotifier` 监听该 fd

Windows 没有这套 API。上游讨论中提出过的替代方案（均未落地）：

| 方案 | 出处 | 备注 |
| --- | --- | --- |
| ConPTY | 作者 2024-01-08 提出 | Windows 10+ 原生，作者自己也不确定是否等价于 Unix PTY |
| termiWin | 作者 2025-07-31 提出 | 已 deprecated |
| libuv 的 tty API | 社区 2025-10-12 提出 | 带 ANSI 转义码控制的 TTY |
| sff / termqt | 作者 2025-11-02 提出 | 来自 Qt 社区讨论的终端候选 |

（上面表格是上游讨论时的候选，均未落地。实际路线已按决策 D1 定案：
Windows 不做 GUI 内虚拟终端，被调试程序弹出独立控制台窗口，见
`docs/porting/decisions.md`。平台特有的代码仍要经接口层或集中、
最小化的改动来隔离，而不是在 `SeerConsoleWidget` 里到处撒
`#ifdef`。）

### 其余平台耦合点

除 PTY 外范围很小（2026-10-20 grep 结果）：

| 位置 | 耦合内容 |
| --- | --- |
| `src/QProcessInfo.cpp:14,16,26` | `<pwd.h>`、`<unistd.h>`，直接读 `/proc/<pid>` |
| `src/SeerGdbWidget.cpp:34,37,39` | `<unistd.h>`、`<termios.h>`、`<signal.h>` |
| `src/CMakeLists.txt:313` | 警告选项只在 `UNIX AND NOT APPLE` 下添加 |
| `src/CMakeLists.txt:342` | `SEER_GDB_NAME` 默认硬编码 `/usr/bin/gdb` |

移植原则：

- 优先用 Qt 自带的跨平台抽象（`QProcess`、`QStandardPaths` 等）替换
  POSIX 调用，而不是自己写 Win32 分支。
- 确实无法抽象的平台特有代码，用 `#ifdef` 隔离到最小范围。
- `src/CMakeLists.txt:305-310` 已经有 WIN32 GUI 子系统的处理，可以
  在此基础上扩展。
- 每一步改动都要保证 Linux 构建不回归——在 WSL2 里验证通过再往下走。
- 上游的 `wontfix` 标签意味着这些改动大概率不会被上游接受，本 fork
  需要长期自己维护，因此平台隔离要做干净，方便日后 rebase 上游。

## 代码风格

遵循 `STYLEGUIDE.md`，关键几条：

- 4 空格缩进，源码里不允许出现真实的 tab 字符。
- 变量名 camelCase、首字母小写；类的私有成员额外加 `_` 前缀；静态变量
  camelCase、首字母大写。
- getter 不加 `get` 前缀，setter 以 `set` / `make` 之类动词开头。
- `{` 放在行尾，`}` 单独一行。
- `*` 和 `&` 贴类型不贴变量名：`int* values`，不是 `int *values`。
- 头文件只放声明，函数实现和 doxygen 注释都写在 `.cpp` 里。
- 头文件里的函数声明、以及 `QObject::connect` 调用，按列对齐排版。
- 不要整文件重新格式化，只格式化自己改动的区域，否则 diff 无法 review。

## 代码组织背景

- `src/` 是扁平目录，约 200 个文件，命名前缀有约定：`Seer*` 是业务
  窗口与对话框，`Q*` 是通用 Qt 控件封装，`Gdb*` 是 gdb/mi 交互层。
- `tests/` 下是大量 `hello*` 手工测试程序，用于人工验证 Seer 的各项
  功能，**不是**自动化测试套件；项目没有单元测试框架。
