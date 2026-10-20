# Seer 的 Windows 构建

从零开始的 Windows 原生构建说明。适用于任何装有 MSYS2 MinGW64
的机器；本文只写**通用**内容，具体某台机器的工具路径见仓库根的
`AGENTS.local.md`（不进版本库，因机而异）。

Linux 构建见 `docs/porting/environment.md`（WSL2）；整体移植
背景见 `AGENTS.md` 与 `docs/plan.md`。

## 1. 前置条件

全部来自 MSYS2 的 `mingw-w64-x86_64-*` 仓库，用 pacman 安装：

    pacman -S --needed \
        mingw-w64-x86_64-toolchain \
        mingw-w64-x86_64-cmake \
        mingw-w64-x86_64-ninja \
        mingw-w64-x86_64-qt6-base \
        mingw-w64-x86_64-qt6-charts \
        mingw-w64-x86_64-qt6-svg \
        mingw-w64-x86_64-gdb

版本参考（2026-10 实测，见 `AGENTS.local.md` §1/§2）：GCC 16.x、
CMake ≥ 3.28、Qt 6.11、gdb 18.1。Qt 的工具（moc/windeployqt）
和 CMake 配置都在标准布局（`bin/`、`lib/cmake/`）下，
`find_package(Qt6)` 自动可用，无需手工指路。

依赖的 Qt 模块：Core、Gui、Widgets、PrintSupport、Charts、Svg、
OpenGL（qt6-base + charts + svg 覆盖）。

## 2. 构建

设 `<MINGW64>` 为 MinGW64 根目录（MSYS2 默认
`C:\msys64\mingw64`）。以下在 **MSYS2 MinGW64 shell** 里执行
（该 shell 的 PATH 已含工具链）。构建目录按仓库约定放
`.tmp/`（不进版本库、不污染源码树）：

    cd <seer仓库>/src
    cmake -G Ninja \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_PREFIX_PATH=<MINGW64> \
          -DQTVERSION=QT6 \
          -B ../.tmp/build-win64
    cmake --build ../.tmp/build-win64 -j 8

产物：`.tmp/build-win64/seergdb.exe`。

要点：

- `CMAKE_PREFIX_PATH` 指向 MinGW64 根，Qt 的 CMake 配置从
  `<MINGW64>/lib/cmake/` 解析。
- `QTVERSION=QT6`（上游为兼容 Qt5 留的开关，本分支只用 QT6；
  不传时 CMakeLists 默认也是 QT6）。
- gdb 的默认程序名在 Windows 分支默认为 `gdb`（按 PATH 查找）；
  运行时可用 `--gdb-program <路径>` 覆盖。构建期无需指定。
- **若不在 MSYS2 shell 而用其他 shell（PowerShell/cmd）直接调
  cmake**：必须先把 `<MINGW64>\bin` 放进 PATH——编译器驱动能
  启动，但 cc1plus 的运行时 DLL 找不到时会**静默退出**（无任何
  错误输出，ninja 只报 exit code 1），这是最常见的坑。
- 想要 gdb 通信日志（调试解析问题）：configure 加
  `-DCONFIG_SEER_GDB_LOGOUT=1`。

## 3. 运行

seergdb.exe 依赖 MinGW64 的 Qt/运行时 DLL，两种方式任选：

1. **开发机直跑**：PATH 里带 `<MINGW64>\bin` 后直接运行
   （MSYS2 shell 天然满足）。
2. **打包分发**：见 §4（windeployqt 收集 DLL，无需 PATH）。

快速冒烟（断在 main、继续、退出）：

    seergdb.exe --gdb-program <gdb路径> --bf main -r <测试程序>

控制台弹窗（被调试程序的独立终端，见 decisions.md D1）要正确
渲染 ANSI 颜色，需要启用 Windows 的 VT（一次性系统设置）：

    reg add HKCU\Console /v VirtualTerminalLevel /t REG_DWORD /d 1 /f

## 4. 打包

windeployqt 只收 Qt 自己的 DLL；**MSYS2 工具链下还有两块要手工补**
（2026-10-21 实测，缺任何一块都会以 0xC0000135 静默失败）：

1. MinGW 运行时（`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、
   `libwinpthread-1.dll`）；
2. Qt DLL 的第三方依赖（icu78、freetype、harfbuzz、pcre2-16、
   png16、brotli、b2、double-conversion、md4c、glib 家族等——
   从 `<MINGW64>\bin` 拷贝）。

完整流程（PowerShell/MSYS2 shell，`<D>` 为输出目录）：

    mkdir <D> && cp ..\.tmp\build-win64\seergdb.exe <D>\
    <MINGW64>\bin\windeployqt.exe <D>\seergdb.exe
    # MinGW 运行时 + Qt 第三方依赖（可用导入表闭包迭代补齐，
    # 参考脚本 .tmp/phase7/；两轮迭代收敛）：
    cp <MINGW64>\bin\{libgcc_s_seh-1,libstdc++-6,libwinpthread-1}.dll <D>\
    cp <MINGW64>\bin\{libicuin78,libicuuc78,libicudt78,libfreetype-6,
        libharfbuzz-0,libpcre2-16-0,libpng16-16,libbrotlidec,
        libbrotlicommon,libbrotlienc,libb2-1,libdouble-conversion,
        libmd4c,libglib-2.0-0,libgmodule-2.0-0,libpcre2-8-0,libffi-8}.dll <D>\

分发体积参考：Debug 构建 ≈ 230MB（exe 131MB，未裁剪）；
Release 构建 + gdb 随包 ≈ 数十 MB 量级（见
`docs/porting/report.md`）。

gdb 分发：**随包分发**（决策 D12）——把 gdb.exe 及其依赖
（expat/gmp/iconv/intl/lzma/mpfr/ncursesw/python/readline/xxhash/
zstd/zlib 一族）一并拷入 `<D>`，与 seergdb 同目录。用户仍可用
`--gdb-program` 覆盖指向自己的 gdb。实测（干净 PATH、无任何
工具链的机器视角）：seergdb + 包内 gdb 完整调试会话通过。

验证打包完整性（可选）：

    # 在 PATH 只含 System32 的环境里跑一次完整会话：
    <D>\seergdb.exe --gdb-program <D>\gdb.exe --bf main -r <测试程序>

## 5. 常见问题

| 症状 | 原因与处置 |
| --- | --- |
| 编译「失败」但没有任何错误文本，ninja 报 exit 1 | cc1plus 的 DLL 找不到——PATH 缺 `<MINGW64>\bin`（§2 要点） |
| 运行报缺 Qt6*.dll | PATH 缺工具链 bin，或未 windeployqt（§3） |
| 被调试程序控制台不显示颜色 | HKCU\Console\VirtualTerminalLevel 未设（§3） |
| 中文路径加载被调试程序失败 | 已由 D11 修复；若复现，检查系统「非 Unicode 程序语言」设置是否被改动 |
| gdb 不启动 | `--gdb-program` 指定完整路径；gdb 同样依赖 MinGW 运行时 DLL |

## 6. 本机差异

各机器的 MinGW64 位置、是否用 scoop/独立安装的 cmake、Qt 版本
细节，一律见根目录 `AGENTS.local.md`。本文档不写死任何机器的
绝对路径。
