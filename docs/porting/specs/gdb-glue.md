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

## §6 路径处理三层策略（依 D8，2026-10-20 实施补记）

实测（裸 MI harness，双位宽 gdb）：gdb 17.1 的 `fullname` 字段用
反斜杠（MI 转义 `\\`）而符号表 `file` 字段用正斜杠；18.1 一致为
正斜杠。Seer 以字符串做编辑器键控与断点命令拼接，双拼写造成双
tab + `-break-insert -f --source "E:\..."` 的引号转义被 gdb 吃掉
反斜杠而失败。Windows 文件系统又大小写不敏感，引入第三维度。
决策 D8 定为三层（全部 `Q_OS_WIN` 守卫，Linux no-op）：

**1. gdb 命令层——正斜杠、原大小写**（`Seer::normalizeSourcePath`）：

| 命令点（SeerGdbWidget.cpp） | 变量 |
| --- | --- |
| `-file-exec-and-symbols "<p>"` / `-file-exec-file` | executableName |
| `-file-symbol-file "<p>"` | executableSymbolName |
| `-environment-cd "<p>"` | executableWorkingDirectory |
| `source -v <p>`（--bl 断点文件） | executableBreakpointsFilename |
| `-target-select core <p>` | executableCoreFilename |
| `-skip-create-file <p>` | skip 对话框 |

编辑器侧断点命令（`--source`）与 `runToLine` 用的 fullname 经
第 3 条记录级归一后天然为正斜杠。`-exec-arguments` 为用户传给被
调试程序的参数，非 gdb 路径，有意不归一。

**2. KEY 层——正斜杠 + 小写折叠**（`Seer::normalizeFileKey()`）：
编辑器 entries（`hasEntry` / `addEntry` / `findEntry`）用折叠键；
`entry.fullname` 存 `normalizeSourcePath` 形态（正斜杠、原大小写），
供显示转换与命令构造复用。

**3. 记录级——GdbMonitor 单点归一**：stdout 读行后对每条 MI 记录
的 `fullname="..."` 值做 `\\`→`/`（按段重建整行；转义对与 `/` 等
长不等，勿原地替换）。覆盖全部消费方：编辑器键控、断点创建对话
框预填、栈帧/断点表格。`~` 记录里的 `fullname=\"...\"` 转义形态
不匹配模式、天然不受伤（单测验证）。

**4. 显示层——Windows 习惯**（`QDir::toNativeSeparators()`，反斜
杠 + 真实大小写，Linux no-op）：栈帧 fullname 列
（SeerStackFramesBrowserWidget）、断点 fullname 列
（SeerBreakpointsBrowserWidget）、编辑器 tab tooltip 与文档标题、
主窗口标题（SeerMainWindow）、进程列表 path 列
（QProcessInfoWidget）、Debug 对话框 attach 回填后的 Program name
栏（SeerDebugDialog，与 QFileDialog 填充的原生形态一致）。

**数据/显示分离（重要教训）**：上述表格列会被代码**读回当数据**
（`selectedFullname()` 拼 `text(2)+"/"+text(3)`；栈帧/断点行双击
回读 fullname 列）——直接把列文本转反斜杠会把数据弄脏（混合
分隔符路径）。规则：**既是显示又是数据的列，用 `Qt::UserRole`
存内部形态（正斜杠原大小写），列文本只做显示转换，读取方一律
读 UserRole**（QProcessInfoWidget / SeerStackFramesBrowserWidget /
SeerBreakpointsBrowserWidget 三处已按此实施）。

**断点/打印点规格的路径入口（补齐后无例外）**：断点创建对话框
的 `--source`（`SeerBreakpointCreateDialog::breakpointText()`）、
CLI `--bs` 的 file:line 规格（`setExecutableBreakpointSourceName`
setter）、printpoint 的 `dprintf-channel` 输出文件
（`handleGdbPrintpointInsert`）均已 `normalizeSourcePath` 包裹。
断点/条件字符串本体不做整体归一——条件里合法出现反斜杠。

**有意不覆盖**（唯一剩余）：`-exec-arguments`（用户传给被调试
程序的参数，非 gdb 路径）与 pre/post gdb 命令（用户原始 gdb 脚
本，按原样执行）。

## 验证

- Linux 无回归：§1 `#else` 原文、§2 纯删除、§3 默认路径不变——
  WSL2 重构 + 冒烟（中断菜单的 kill 路径、断点保存/恢复走
  /tmp 的行为）；§6 全部为 no-op，双平台重构通过。
- Windows：§1 用 t-flow 场景复测（`-exec-interrupt` 打断忙循环、
  gdb `kill` 终止）；§4 调试全程无 gdb 黑窗；§5 菜单目视确认
  （Phase 5）；§6 用 17.1 会话复测单 tab + 按源码行建断点
  （已过），Phase 6 矩阵覆盖 `--bs` / 手工输入路径边界。
