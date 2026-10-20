# Spec：console 行为分流（A1 / A2 / A9）

依 D1/D3/D4：Linux 保持上游 PTY 实现原样；Windows 不做 GUI 内
终端，inferior 经 gdb `new-console` 弹独立窗口；console 界面在
Windows 隐藏。

## §1 SeerConsoleWidget 的平台守卫（A1 / A9）

文件：`src/SeerConsoleWidget.cpp`。目标：Windows 可编译、终端永不
创建（`_ttyFD` 保持 -1，`terminalDeviceName()` 返回空串——这同时
让 §2 的接线点自然短路）。

1. 头文件（:22-23）：`#include <termios.h>` 与
   `#include <sys/ioctl.h>` 两行包 `#ifndef Q_OS_WIN`（MinGW 没有
   这两个头；`unistd.h`/`fcntl.h`/`stdlib.h` MinGW 都有，保留——
   A9 的 `STDOUT_FILENO` 相关代码在 Windows 也能编译，且控件已
   隐藏，行为无关紧要）。
2. `createTerminal()`（:308-363）：函数体整体包
   `#ifndef Q_OS_WIN`。Windows 侧为空函数体（`_ttyFD`、
   `_terminalDeviceName` 保持构造函数初值）。
3. 其余成员**不加守卫**：`deleteTerminal` / `connectTerminal` /
   `handleTerminalOutput` / `handleStdinLineEdit` 都先检查
   `isTerminalCreated()`，终端未创建时自然短路。唯一待编译确认项：
   `fsync`（:272）在 MinGW 的可用性——若链接报缺符号，单独给该行
   加 `#ifndef Q_OS_WIN`（Phase 4 编译时定，预计不需要，因为该行
   在短路 return 之后才会执行到，且 MinGW-w64 通常提供 fsync）。
4. `SeerConsoleWidget.h` 不改（`QSocketNotifier* _ttyListener`
   声明本身可编译）。

Linux 无回归验证：`#ifndef` 内为原文；WSL2 重构 + console 冒烟
（PTY 创建、ANSI 显示、stdin 回显）。

## §2 inferior 接线点（A2，依 D4）

文件：`src/SeerGdbWidget.cpp:2049-2058`
`handleGdbTerminalDeviceName()`，改为：

    void SeerGdbWidget::handleGdbTerminalDeviceName () {
    #ifdef Q_OS_WIN
        // D4：Windows 不设 inferior 的 tty，改让 gdb 给 inferior
        // 独立控制台（console 型弹窗、GUI 型不弹，由 PE 子系统
        // 决定）。MI 流不被 inferior 输出污染（PoC Q1 实证）。
        handleGdbCommand("-gdb-set new-console on");
    #else
        if (commandLogsWidget->console()->terminalDeviceName() != "") {
            handleGdbCommand(QString("-inferior-tty-set ") + commandLogsWidget->console()->terminalDeviceName());
        }else{
            qWarning() << "Can't set TTY name because the name is blank.";
        }
    #endif
    }

- 5 个调用点（:935/1079/1183/1304/1425）**零改动**。
- `mi-async` 无需新增：上游已在 run/attach 路径发送
  （:919/:1069）。

Linux 无回归验证：`#else` 分支原文。Windows 验证（Phase 5）：按
`evidence/poc-q1-matrix.txt` 场景复测——console 型 inferior 弹可见
独立控制台（stdin 回显、ANSI、Ctrl-C 人工项）、GUI 型不弹、MI 流
干净、无多余 gdb 黑窗。

## §3 console 界面在 Windows 隐藏（依 D1）

文件：`src/SeerCommandLogsWidget.cpp:167-180` `createConsole()`，
`addTab`（:177）之后加：

    #ifdef Q_OS_WIN
        // D1：Windows 无 GUI 内终端，隐藏 Console 标签页。
        logsTabWidget->setTabVisible(_consoleIndex, false);
    #endif

- `setTabVisible` 是 QTabWidget 自 Qt 5.15 的 API；实施时确认
  `QDetachTabWidget` 继承自 QTabWidget（预计是）。
- 实施时 grep `src/` 中其他引用 console 入口的 UI（菜单 / 工具栏
  / 快捷键），一并隐藏，逐个记入 Phase 4 执行记录（预计很少）。
