# AGENTS.local.md

本机（Windows）特有的构建环境说明。**不进版本库**（已在 `.gitignore`
里）。项目侧的通用约定见 `AGENTS.md`；两者冲突时以本文件为准。

以下所有路径与版本号为 2026-10-20 实测。

## 1. MinGW64 工具链——主构建环境

Windows 原生构建使用 MSYS2 的 mingw64 环境：

    D:\Linux\mingw64\bin

**关键：这个目录必须先加进 `$PATH` 才能用。** 两种场景都要加：

1. 调用工具链本身（`gcc` / `g++` / `gdb` / `mingw32-make` / `windres`）
   之前；
2. 运行构建产物之前——只要不是 `-static` 静态链接，产物依赖的
   `libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll` 以及
   Qt6 的一堆 DLL 都在这个目录里，不加就会报找不到 DLL。

Git Bash / MSYS 里用 POSIX 路径：

    export PATH="/d/Linux/mingw64/bin:$PATH"

cmd / PowerShell 里用 Windows 路径：

    set PATH=D:\Linux\mingw64\bin;%PATH%

想彻底免掉 DLL 依赖，可以在链接时加 `-static`（但 Qt6 是动态库，Seer
这种 Qt 程序静态链接不现实，老老实实加 `$PATH`）。

### 已装的工具

| 工具 | 版本 | 备注 |
| --- | --- | --- |
| `gcc` / `g++` | 16.2.0 (Rev6, MSYS2) | host/target 均为 `x86_64-w64-mingw32` |
| `gdb` | 18.1 | target `x86_64-w64-mingw32`，见第 4 节 |
| `mingw32-make` | — | 注意名字不是 `make` |
| `windres` | — | 资源编译器 |

### 还没装的（构建 Seer 前必须先装）

`D:\Linux\mingw64\bin` 下目前**没有** `cmake`、`ninja`、`pkg-config`，
也**没有** Qt6（`qmake` / `moc` / `uic` / `lib/cmake/Qt6` 全都不存在）。
这些统一用 pacman 装 mingw64 版本，见第 2、3 节。

MSYS 层 `D:\Linux\usr\bin` 里有 `make` 和 `pkg-config`，但那是 MSYS
运行时的，不能拿来构建 mingw64 目标，别混用。

## 2. CMake 与 Ninja——统一用 mingw64 版本

本项目的构建工具**全部集中在 mingw64 里**，cmake 和 ninja 都用 pacman
装的 `mingw-w64-x86_64-*` 版本，装完后位于：

    D:\Linux\mingw64\bin\cmake.exe
    D:\Linux\mingw64\bin\ninja.exe

这样工具链、构建系统、Qt6 全在同一棵目录树下，`$PATH` 只需要加
`D:\Linux\mingw64\bin` 一处（见第 1 节），版本也不会互相错配。

### 关键：系统 `$PATH` 里已经有别的 cmake，调用时必须精确引用

本机 `C:\Users\Linwei\scoop\shims` 已经在**系统** `$PATH` 里，那下面有
一份 scoop 装的 cmake：

    C:\Users\Linwei\scoop\shims\cmake.exe       # cmake 4.3.4

所以装完 mingw64 版之后，机器上会同时存在两份 cmake，光敲 `cmake` 抓到
哪一份取决于 `$PATH` 顺序，不可靠。**一律用绝对路径调用 mingw64 的那
份**，不要依赖 `$PATH`：

    # Git Bash / MSYS
    /d/Linux/mingw64/bin/cmake.exe --version

    # cmd / PowerShell
    D:\Linux\mingw64\bin\cmake.exe --version

`ninja` 同理，虽然目前系统 `$PATH` 里没有第二份，但为了和 cmake 保持一
致，也建议写全路径 `/d/Linux/mingw64/bin/ninja.exe`。

注意 `$PATH` 仍然需要加 `D:\Linux\mingw64\bin`（第 1 节）——那是为了让
编译器、Qt6 的 DLL 和 `moc`/`uic` 等能被找到，和 cmake 用绝对路径调用
这件事不冲突，两者都要做。

如果哪次不确定实际生效的是哪一份，用这个查：

    which -a cmake

### 构建时的两个坑

1. **必须显式指定 `-G` 生成器。** 本机装有 Visual Studio，cmake 在
   Windows 上的默认生成器会选到 VS（scoop 那份 cmake 4.3.4 实测默认是
   `Visual Studio 18 2026`，即 `cmake --help` 里带 `*` 号那个），不指定
   就会去生成 VS 工程、用 MSVC 编译。构建 Seer 一律用 `-G Ninja`。
2. **必须让它找到 mingw64 的编译器**，而且要用绝对路径指定，不要只靠
   `$PATH`：

       -DCMAKE_C_COMPILER=D:/Linux/mingw64/bin/gcc.exe
       -DCMAKE_CXX_COMPILER=D:/Linux/mingw64/bin/g++.exe

   否则它可能抓到 MSVC，或者 `D:\Dev\mingw32\bin` 里的 32 位 gcc。
   ninja 也用 `-DCMAKE_MAKE_PROGRAM` 写死绝对路径。

第 6 节有一份写全了这些参数的完整命令，直接照抄。

## 3. 用 MSYS2 的 pacman 装包

`D:\Linux` 是一个完整的 MSYS2 安装。要给 mingw64 环境装任何包，用它
自带的 pacman：

    D:\Linux\usr\bin\pacman.exe

也可以在 MSYS2 shell 里直接敲 `pacman`（入口脚本
`D:\Linux\msys2_shell.cmd`）。

构建 Seer 需要的包（版本号为 2026-10-20 仓库实测，确认存在）：

    D:\Linux\usr\bin\pacman.exe -S --needed \
        mingw-w64-x86_64-toolchain \
        mingw-w64-x86_64-cmake \
        mingw-w64-x86_64-ninja \
        mingw-w64-x86_64-qt6-base \
        mingw-w64-x86_64-qt6-charts \
        mingw-w64-x86_64-qt6-svg

| 包 | 版本 |
| --- | --- |
| `mingw-w64-x86_64-cmake` | 4.4.4-3 |
| `mingw-w64-x86_64-ninja` | 1.13.2-1 |
| `mingw-w64-x86_64-qt6-base` | 6.11.2-2 |
| `mingw-w64-x86_64-qt6-charts` | 6.11.2-1 |
| `mingw-w64-x86_64-qt6-svg` | 6.11.2-1 |

装完后 cmake / ninja 都在 `D:\Linux\mingw64\bin` 下。因为系统 `$PATH` 里
还有 scoop 那份 cmake，调用时务必写绝对路径，见第 2 节。

`src/CMakeLists.txt` 要求的 Qt6 组件是 Core、Gui、Widgets、
PrintSupport、Charts、Svg、OpenGL。其中 Charts 和 Svg 是独立包，其余
应随 `qt6-base` 一起装上；如果 cmake 报某个 Qt6 组件找不到，就补装对应
的 `mingw-w64-x86_64-qt6-*` 包。

装完包记得重新开一个 shell，或重新 `export PATH`，否则新装的可执行文件
不生效。

**装包会改动本机环境，动手前先跟主人确认。**

## 4. GDB

测试用的 gdb 是 mingw64 那个：

    D:\Linux\mingw64\bin\gdb.exe        # GNU gdb 18.1, x86_64-w64-mingw32

**但移植后的 Seer 必须同时兼容 32 位的 gdb。** 本机可用的 32 位 gdb：

    D:\Dev\mingw32\bin\gdb.exe          # GNU gdb 17.1, i686-w64-mingw32

这意味着写代码时不能假设：

- 地址/指针宽度是 64 位——32 位 gdb 下是 8 个十六进制位，不是 16 个；
- 寄存器名字与寄存器集合固定——x86 是 `eax/ebx/...`，x86_64 是
  `rax/rbx/...` 且多出一批；
- gdb 版本号固定——18.1 与 17.1 的 gdb/mi 输出细节可能有差异。

改动涉及 gdb/mi 解析、寄存器视图、内存视图、地址格式化时，两个 gdb 都
要过一遍。

## 5. WSL2

本机的 WSL2 发行版叫：

    Ubuntu-24.04

需要验证 Linux 侧构建有没有回归时用它（原版 Seer 只能在 Linux 下构建，
详见 `AGENTS.md` 的「构建与验证」）。从 Windows 侧调用：

    wsl.exe -d Ubuntu-24.04 -- bash -lc "<命令>"

注意该 WSL2 环境目前也没装 Qt6 开发包，`libqt6charts6-dev` 和
`libqt6svg6-dev` 在已启用的 apt 源里还没有候选版本，首次构建前需要先
解决。

（另有一个 WSL1 的 `Debian`，本项目不用。）

## 6. 构建目录

Windows 原生构建**不要**用 `src/build`——那是上游的 Linux 构建目录，
混用会把 cmake cache 搞坏。Windows 侧的构建产物放到 `.tmp/` 下。

完整命令（cmake 用绝对路径，原因见第 2 节）：

    export PATH="/d/Linux/mingw64/bin:$PATH"

    /d/Linux/mingw64/bin/cmake.exe \
          -S src -B .tmp/build-win64 \
          -G Ninja \
          -DCMAKE_MAKE_PROGRAM=D:/Linux/mingw64/bin/ninja.exe \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_C_COMPILER=D:/Linux/mingw64/bin/gcc.exe \
          -DCMAKE_CXX_COMPILER=D:/Linux/mingw64/bin/g++.exe \
          -DQTVERSION=QT6 \
          -DCMAKE_PREFIX_PATH=D:/Linux/mingw64

    /d/Linux/mingw64/bin/cmake.exe --build .tmp/build-win64 -j

`-G` 、`CMAKE_MAKE_PROGRAM` 和两个 `COMPILER` 参数都不要省——本机同时
存在 Visual Studio、scoop 的 cmake 和 `D:\Dev\mingw32` 的 32 位工具链，
全部写死绝对路径才不会抓错。

运行产物前同样要保证 `D:\Linux\mingw64\bin` 在 `$PATH` 里（第 1 节）。

WSL2 侧的构建同理，另开一个目录，例如 `.tmp/build-wsl`。WSL2 里
产生的临时文件也统一放仓库 `.tmp/`（从 WSL2 内部经
`/mnt/e/Github/Tools/seer/.tmp/` 访问），不要放 WSL2 自己的文件
系统里，方便统一清理。
