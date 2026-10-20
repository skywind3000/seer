#!/bin/sh
# 平台耦合审计 grep 清单（docs/porting/audit.md 第 1 节的可执行形态）。
# 用法：bash docs/porting/evidence/run-audit-grep.sh [输出文件]
# 输出默认写到仓库 .tmp/audit-grep.txt。
# 注意：本机 grep 为 ugrep 且 Git Bash 有 MSYS 路径转换，模式以 / 开头
# 的必须 -e 显式给出，并前置 MSYS_NO_PATHCONV=1（见
# docs/porting/environment.md 的「坑」一节）。

OUT="${1:-.tmp/audit-grep.txt}"
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$REPO" || exit 1

export MSYS_NO_PATHCONV=1

{
echo "### A. PTY/termios/ioctl/fcntl"
grep -rnE "termios|posix_openpt|grantpt|unlockpt|ptsname|TIOCS|TIOCG|cfset|tcgetattr|tcsetattr|fcntl|O_NDELAY|ioctl\s*\(" src --include='*.cpp' --include='*.h'
echo "### B. POSIX headers"
grep -rnE "#include\s*<(unistd|pwd|sys/ioctl|sys/wait|sys/types|dirent|signal|pthread)\.h>" src --include='*.cpp' --include='*.h'
echo "### C. process/IO calls"
grep -rnE "\bkill\s*\(|\bfork\s*\(|\bexec(v|l|p)[a-z]*\s*\(|\bwaitpid\s*\(|\bfsync\s*\(|\bpipe\s*\(|\bdup2\s*\(|\bgetpwuid|\bmkstemp|\bmkdtemp" src --include='*.cpp' --include='*.h'
echo "### D. /proc 与路径硬编码"
grep -rn -e "/proc/" -e "/tmp" -e "/dev/" src --include='*.cpp' --include='*.h'
echo "### E. QSocketNotifier"
grep -rn "QSocketNotifier" src --include='*.cpp' --include='*.h'
echo "### F. signals"
grep -rnE "\bSIG[A-Z]+\b" src --include='*.cpp' --include='*.h'
echo "### G. CMake platform gates"
grep -nE "UNIX|WIN32|APPLE|LINUX" src/CMakeLists.txt
echo "### I. raw fd IO"
grep -rnE "::open\s*\(|::close\s*\(|::read\s*\(|::write\s*\(|::fsync\s*\(" src --include='*.cpp' --include='*.h'
} 2>&1 | tee "$OUT" | wc -l
