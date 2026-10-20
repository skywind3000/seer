# 移植环境实测记录（Phase 0 产出）

- 日期：2026-10-20
- 性质：`docs/plan.md` Phase 0 的产出物，记录双平台构建环境的实测
  状态、可照抄的命令与本机踩过的坑。机器相关的绝对路径见
  `AGENTS.local.md`，本文只写事实与结论。

## Windows（MSYS2 mingw64）

### 工具链实测（全部就绪）

| 工具 | 版本 | pacman 包 |
| --- | --- | --- |
| gcc / g++ | 16.2.0 (Rev6) | mingw-w64-x86_64-toolchain |
| gdb（64 位） | 18.1 | mingw-w64-x86_64-gdb |
| cmake | 4.4.4 | mingw-w64-x86_64-cmake |
| ninja | 1.13.2 | mingw-w64-x86_64-ninja |
| Qt6 base / svg / charts | 6.11.2 | 对应 qt6 包 |

pacman 包当时已由主人提前装好，Phase 0 的安装步骤免做。
`/d/Linux/mingw64/lib/cmake/` 下 Qt6 各模块的 CMake 配置齐全，
`find_package(Qt6)` 可用。另有一份 32 位 gdb 17.1（见
`AGENTS.local.md` §4），后续 Phase 2 的双 gdb 验证用。

### 冒烟测试（已通过）

最小 Qt6 Widgets 程序，源码在 `.tmp/smoke-qt/`（QLabel + 2 秒
QTimer 自动退出，免去人工关窗）。构建与运行：

    export PATH="/d/Linux/mingw64/bin:$PATH"

    /d/Linux/mingw64/bin/cmake.exe \
        -S E:/Github/Tools/seer/.tmp/smoke-qt \
        -B E:/Github/Tools/seer/.tmp/build-win64/smoke \
        -G Ninja \
        -DCMAKE_MAKE_PROGRAM=D:/Linux/mingw64/bin/ninja.exe \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=D:/Linux/mingw64/bin/gcc.exe \
        -DCMAKE_CXX_COMPILER=D:/Linux/mingw64/bin/g++.exe \
        -DCMAKE_PREFIX_PATH=D:/Linux/mingw64

    /d/Linux/mingw64/bin/cmake.exe --build E:/Github/Tools/seer/.tmp/build-win64/smoke -j

    E:/Github/Tools/seer/.tmp/build-win64/smoke/smokeqt.exe    # exit 0

### 坑（重要，后续 phase 都会遇到）

1. **不 `export PATH` 就调 cmake，配置阶段必失败。** 编译器探测在
   `project()` 处报错（gcc 依赖的 DLL 找不到）。`AGENTS.local.md`
   §6 命令第一行的 `export PATH` 一步都不能省，运行产物时同样需要。
2. **本机 grep 实为 ugrep，且 Git Bash 有 MSYS 路径转换**，做审计
   类 grep 有两个坑：
   - 模式以 `/` 开头且不加 `-e`：ugrep 把它当选项，直接报错
     （`grep -rn "/proc/" ...` → `invalid option -/`）；
   - 加了 `-e "/tmp"` 也可能**静默假阴性**：Git Bash 把裸 POSIX
     路径形态的参数转换成 Windows 路径，pattern 到达 grep 时已经
     不是 `/tmp` 了。本次审计的 `/tmp` 耦合点就是先被漏报、靠
     人工读码才发现的。
   - 结论：凡模式以 `/` 开头，一律加 `MSYS_NO_PATHCONV=1` 前缀
     并用 `-e` 显式给出。
3. **msys bash 的子进程不触发 WER**：bash 给子进程的 error mode 会
   抑制 Windows 错误报告——验证崩溃捕获或复现崩溃行为时，必须用
   干净父进程启动（如 PowerShell `Start-Process`），否则崩溃无
   dump、无事件日志。另：`cmd /c "app & echo %ERRORLEVEL%"` 里的
   `%ERRORLEVEL%` 是解析期展开的旧值，验证退出码不要用它。

## WSL2（Ubuntu-24.04）

### 工具链实测

| 工具 | 版本 |
| --- | --- |
| g++ | 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) |
| cmake | 3.28.3 |
| Qt6 | 6.4.2（qt6-base-dev / qt6-charts-dev / qt6-svg-dev） |

- `nproc` = 28。
- Qt6 开发包 2026-10-20 经 apt 安装（root 经 `wsl.exe -u root` 免
  sudo）。**noble 的包名是 `qt6-charts-dev` / `qt6-svg-dev`**，不是
  老资料里的 `libqt6charts6-dev` / `libqt6svg6-dev`——后者在源里
  `Candidate: (none)`，这正是 `AGENTS.md` 早前探测记录「无候选版本」
  的真实原因（包名不对，而非缺 universe；universe 一直是启用的）。

### 基线构建（已通过）

    wsl.exe -d Ubuntu-24.04 -- bash -lc 'cd /mnt/e/Github/Tools/seer && \
        cmake -S src -B .tmp/build-wsl -DCMAKE_BUILD_TYPE=Debug -DQTVERSION=QT6 && \
        cmake --build .tmp/build-wsl -j$(nproc) --target seergdb'

- 结果：`[100%] Built target seergdb`，退出码 0，无 error。
- 产物：`.tmp/build-wsl/seergdb`，约 60 MB（Debug）。
- 完整日志（151 行）存档于 `.tmp/wsl-build.log`。
- 全程在 `/mnt/e` 挂载上构建（依主人决策：WSL2 临时文件统一进仓库
  `.tmp/`），28 并发下速度可接受，无挂载相关问题。

## issue #94 通读结论

21 条评论（2022-09-27 至 2026-03-02）全部读完，时间线与 `AGENTS.md`
的摘要一致。摘要之外记录两点：

1. **epasveer 2025-08-01 确认：** 有人用 MSYS2 的 MinGW gcc（原生，
   非 Cygwin）编译 Seer，**除 `SeerConsoleWidget.cpp` 外全部编译
   通过**。这对 Phase 4 的全量编译是积极的历史证据（注意：那是
   2022-2025 年间的代码基线，仅作参考）。
2. 最新一条相关评论（2026-03-02）附了三个 PTY 相关参考链接
   （alacritty#3707、winpty#152、microsoft/terminal#57）。依决策
   D1 已不需要 GUI 内终端，仅作背景留存。

## Phase 0 出口条件核对

- [x] Windows：冒烟程序构建 + 运行成功（exit 0）
- [x] WSL2：seergdb 构建成功、无 error，日志存档 `.tmp/wsl-build.log`
- [x] 本文档中的命令可照抄复现
- [x] issue #94 通读完毕
