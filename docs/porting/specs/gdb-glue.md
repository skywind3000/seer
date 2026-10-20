# Spec：SeerGdbWidget 平台粘合（A4 / A5 / A6 / A8）

## §0 缺失信号宏的占位定义（A4 的编译前提）

MinGW CRT 没有 `SIGKILL` / `SIGUSR1` / `SIGUSR2` 宏（A4 实编译
实证），而 `SeerGdbWidget.cpp:1749-1777` 的中断 handler 与
`sendGdbInterrupt` 的比较逻辑直接使用它们（`SIGINT`/`SIGFPE`/
`SIGSEGV` MinGW 有）。在 `SeerGdbWidget.cpp` 头部 include 区之后
加：

    #ifdef Q_OS_WIN
    // MinGW CRT 缺 POSIX 专属信号宏（audit A4 实证）。
    // 占位值仅用于菜单分发与 sendGdbInterrupt 的映射比较。
    #define SIGKILL  9
    #define SIGUSR1 10
    #define SIGUSR2 12
    #endif

这样 :1749-1777 的 handler 函数与所有 `QObject::connect` 关系
**零改动**。不支持的信号在运行期被 §1 的映射挡下 + §5 的菜单
隐藏。

## §1 sendGdbInterrupt 的 Windows 映射（A4，依 D5）

文件：`src/SeerGdbWidget.cpp:3899-3931`。`signal < 0` 分支
（`-exec-interrupt`）两平台共用不动；`signal >= 0` 分支改为：

    #ifdef Q_OS_WIN
        // D5：MinGW 无 kill()。Windows 映射到 MI 命令。
        // （mi-async 已由上游 :919/:1069 开启，-exec-interrupt
        // 对运行中的 inferior 有效——PoC Q2 实证。）
        if (signal == SIGINT) {
            handleGdbCommand("-exec-interrupt");
            return;
        }
        if (signal == SIGKILL) {
            handleGdbCommand("kill");   // gdb CLI：终止 inferior
            return;
        }
        qWarning() << "Signal" << signal << "is not supported on Windows.";
        return;
    #else
        int stat = kill(executablePid(), signal);
        if (stat < 0) {
            QMessageBox::warning(...);   // 原文不动
        }
    #endif

- 全 MI 命令、无 Win32 调用（总则 3）。
- 「SIGKILL → gdb kill」与 POSIX `kill(pid, SIGKILL)` 的语义差异
  （gdb 会结束并清理 target 状态）记入 Phase 6 验证项。

## §2 悬空 include 清理（A5）

`src/SeerGdbWidget.cpp:34/36/37/38` 删除 `unistd.h` / `fcntl.h` /
`termios.h` / `sys/ioctl.h` 四行——全文件无任何调用（A5 复核实
测）。`signal.h`（:39）保留（kill/SIG 宏在用）。纯删除，WSL2
重构即验证。

## §3 /tmp 硬编码替换（A6）

`src/SeerGdbWidget.cpp:1490 / 1517 / 2034 / 2046 / 3733` 五处的
`"/tmp/..."` 字面量改为 `QDir::tempPath()` 拼接（`<QtCore/QDir>`
已包含）。要点：

- Linux 默认环境 `tempPath() == "/tmp"`，gdb 命令输出不变；设置
  了 `TMPDIR` 的环境路径会变（行为更合理，记录在案）。
- Windows 上 `tempPath()` 返回形如 `C:/Users/<user>/AppData/Local/
  Temp` 的正斜杠路径，Windows gdb 接受（PoC Q2 已在 MI 命令里
  用过正斜杠路径的 inferior 路径）。
- `:3733` 的 MI 脚本暂存目录注意保留文件名拼接逻辑。

## §4 gdb 进程的无窗启动（A8，依 D4）

`src/SeerGdbWidget.cpp:91` `_gdbProcess = new QProcess(this);`
之后加：

    #ifdef Q_OS_WIN
        // A8：防止 gdb.exe（console 程序）从 GUI 进程启动时弹出
        // 自己的控制台黑窗（PoC Q1 用例 5 实证）。
        _gdbProcess->setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments* args) {
                args->flags |= 0x08000000u;   // CREATE_NO_WINDOW
            });
    #endif

（0x08000000 写常量，避免为它引 windows.h——总则 3。）

## §5 不支持信号菜单项隐藏（A4 配套）

`src/SeerMainWindow.cpp:118-121`：六个 `addAction` 保留（连接
关系不动），其后加：

    #ifdef Q_OS_WIN
        // D5：FPE/SEGV/USR1/USR2 在 Windows 无 kill() 语义。
        interruptActionSIGFPE->setVisible(false);
        interruptActionSIGSEGV->setVisible(false);
        interruptActionSIGUSR1->setVisible(false);
        interruptActionSIGUSR2->setVisible(false);
    #endif

SIGINT / SIGKILL 在 Windows 有 §1 的映射，菜单保留。

## §6 源文件路径分隔符归一化（2026-10-20 实施补记）

实测（裸 MI harness，hellofibonacci 双位宽）：gdb 17.1 的
`fullname` 字段用反斜杠（`E:\Github\...`）而符号表 `file` 字段用
正斜杠（`E:/Github/...`）；gdb 18.1 两者一致用正斜杠。Seer 以
fullname 字符串做编辑器 tab 键控与断点命令拼接，17.1 下同一文件
出现两种拼写——双 tab + `-break-insert -f --source "E:\..."` 的
引号转义又被 gdb 吃掉反斜杠导致断点失败。

方案：`Seer::normalizeSourcePath()`（`SeerUtl`，`Q_OS_WIN` 守卫，
反斜杠→正斜杠），应用点：

- `SeerEditorManagerWidget`：`hasEntry` / `addEntry` /
  `findEntry`（键与 `entry.fullname` 均归一）；
- `SeerEditorWidgetSourceArea`：`open()` 的 `_fullname` 与
  `handleText()` 解析出的 `fullname_text`（两处拼写归一后，
  `fullname_text != fullname()` 的重开判断保持稳定）。

归一后断点命令用正斜杠，与符号表一致，17.1/18.1 均接受；Linux
为 no-op。

## 验证

- Linux 无回归：§1 `#else` 原文、§2 纯删除、§3 默认路径不变——
  WSL2 重构 + 冒烟（中断菜单的 kill 路径、断点保存/恢复走
  /tmp 的行为）；§6 为 no-op，双平台重构通过。
- Windows：§1 用 t-flow 场景复测（`-exec-interrupt` 打断忙循环、
  gdb `kill` 终止）；§4 调试全程无 gdb 黑窗；§5 菜单目视确认
  （Phase 5）；§6 用 17.1 会话复测单 tab + 按源码行建断点。
