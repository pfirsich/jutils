#include <cstring>
#include <ctime>
#include <iostream>

#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <clipp/clipp.hpp>

#include "io.hpp"
#include "util.hpp"

namespace {
struct PsArgs : clipp::ArgsBase {
    bool verbose = false;
    bool all = false;

    void args()
    {
        flag(verbose, "verbose", 'v').help("Display all columns.");
        flag(all, "all", 'a').help("Display processes of all users.");
    }
};

// [PT] fields require ptrace access modes PTRACE_MODE_READ_FSCREDS | PTRACE_MODE_NOAUDIT
// If access is denied, the fields are 0
struct ProcStat {
    int pid; // %d
    std::string comm; // %s - filename of the executable (truncated to TASK_COMM_LEN=16)
    char state; // %c - see man page, one of: RSDZTtWXxKWP
    int ppid; // %d - Parent PID
    int pgrp; // %d - Process Group ID

    int session; // %d - Session ID
    int tty_nr; // %d - Minor device no in bits 31 to 20 and 7 to 0, major in bits 15 to 8
    int tpgid; // %d - ID of foreground process group
    unsigned int flags; // %u - kernel flags of the process
    unsigned long minflt; // %lu - number of minor faults

    unsigned long cminflt; // %lu - number of minor faults of process's waited-for children
    unsigned long majflt; // %lu - number of major faults
    unsigned long cmajflt; // %lu - number of major faults of process's waited-for children
    unsigned long utime; // %lu - Amount of time scheduled in user mode
    unsigned long stime; // %lu - Amount of time scheduled in kernel mode

    long cutime; // %ld - Amount of time waited-for children have spent in user mode
    long cstime; // %ld - Amount of time waited-for chidlren have spent in kernel mode
    long priority; // %ld
    long nice; // %ld
    long num_threads; // %ld

    long itrealvalue; // %ld - Time in jiffies before the next SIGALRM is sent due to interval timer
    unsigned long long starttime; // %llu - Time in clock ticks the process started after boot
    unsigned long vsize; // %lu - Virtual memory size in bytes
    long rss; // %ld - Resident set size (number of pages in real memory)
    unsigned long rsslim; // %lu - Current soft limit on the rss of the process in bytes

    unsigned long startcode; // %lu [PT] - Address above which program text can run
    unsigned long endcode; // %lu [PT] - Address below which program text can run
    unsigned long startstack; // %lu [PT] - Address of the start (bottom) of the stack
    unsigned long kstkesp; // %lu [PT] - Current value of the ESP (stack pointer)
    unsigned long kstkeip; // %lu [PT] - Current EIP (instruction pointer)

    unsigned long signal; // %lu - Bitmap of pending signals. Obsolete, use /proc/[pid]/status
    unsigned long blocked; // %lu - Bitmap of blocked signals. Obsolete, use /proc/[pid]/status
    unsigned long sigignore; // %lu - Bitmap of ignored signals. Obsolete, use /proc/[pid]/status
    unsigned long sigcatch; // %lu - Bitmap of caught signals. Obsolete, use /proc/[pid]/status
    unsigned long wchan; // %lu [PT] - Channelin which the process is waiting.

    unsigned long nswap; // %lu - Number of pages swapped (not maintained)
    unsigned long cnswap; // %lu - Cumulative nswap for child processes (not maintained)
    int exit_signal; // %d - Signal to be sent to parent when process dies
    int processor; // %d - CPU number last executed on
    unsigned int rt_priority; // %u - Real-time scheduling priority

    unsigned int policy; // %u - Scheduling policy
    unsigned long long delayacct_blkio_ticks; // %llu - Aggregated block IO delays in clock ticks
    unsigned long guest_time; // %lu - Guest time (time spent running in virtual CPU) in ticks
    long cguest_time; // %ld - Guest time for children
    unsigned long start_data; // %lu [PT] - Address above which BSS is placed

    unsigned long end_data; // %lu [PT] - Address below which BSS is placed
    unsigned long start_brk; // %lu [PT] - Address above which program heap can be extended (brk(2))
    unsigned long arg_start; // %lu [PT] - Address above which argv is placed
    unsigned long arg_end; // %lu [PT] - Address below which argv is placed
    unsigned long env_start; // %lu [PT] - Address above which environment is placed

    unsigned long env_end; // %lu [PT] - Address below which environment is placed
    int exit_code; // %d - The thread's exit status as reported by waitpid(2)
};

std::optional<ProcStat> readProcStat(const std::string& statFilePath)
{
    auto f = std::fopen(statFilePath.c_str(), "r");
    if (!f) {
        std::cerr << "Could not open file" << std::endl;
        return std::nullopt;
    }
    ProcStat procStat;
    char commBuffer[32];
    std::memset(commBuffer, 0, sizeof(commBuffer));
    const auto r = std::fscanf(f,
        "%d (%[^)]) %c %d %d %d %d %d %u %lu "
        "%lu %lu %lu %lu %lu %ld %ld %ld %ld %ld "
        "%ld %llu %lu %ld %lu %lu %lu %lu %lu %lu "
        "%lu %lu %lu %lu %lu %lu %lu %d %d %u "
        "%u %llu %lu %ld %lu %lu %lu %lu %lu %lu "
        "%lu %d",
        &procStat.pid, commBuffer, &procStat.state, &procStat.ppid, &procStat.pgrp,
        &procStat.session, &procStat.tty_nr, &procStat.tpgid, &procStat.flags, &procStat.minflt,
        &procStat.cminflt, &procStat.majflt, &procStat.cmajflt, &procStat.utime, &procStat.stime,
        &procStat.cutime, &procStat.cstime, &procStat.priority, &procStat.nice,
        &procStat.num_threads, &procStat.itrealvalue, &procStat.starttime, &procStat.vsize,
        &procStat.rss, &procStat.rsslim, &procStat.startcode, &procStat.endcode,
        &procStat.startstack, &procStat.kstkesp, &procStat.kstkeip, &procStat.signal,
        &procStat.blocked, &procStat.sigignore, &procStat.sigcatch, &procStat.wchan,
        &procStat.nswap, &procStat.cnswap, &procStat.exit_signal, &procStat.processor,
        &procStat.rt_priority, &procStat.policy, &procStat.delayacct_blkio_ticks,
        &procStat.guest_time, &procStat.cguest_time, &procStat.start_data, &procStat.end_data,
        &procStat.start_brk, &procStat.arg_start, &procStat.arg_end, &procStat.env_start,
        &procStat.env_end, &procStat.exit_code);
    std::fclose(f);
    if (r != 52) {
        std::cerr << "Could not parse " << statFilePath << std::endl;
        return std::nullopt;
    }
    procStat.comm = std::string(commBuffer);
    return procStat;
}

// Boot time in seconds since the epoch
std::optional<int64_t> getBootTime()
{
    const auto file = readFile("/proc/stat");
    if (!file) {
        return std::nullopt;
    }
    const auto id = "btime ";
    const auto start = file->find(id);
    if (start == std::string::npos) {
        std::cerr << "Could not fine btime in /proc/stat" << std::endl;
        return std::nullopt;
    }

    int64_t btime;
    if (std::sscanf(file->data() + start, "btime %ld\n", &btime) != 1) {
        std::cerr << "Could not parse btime from /proc/stat" << std::endl;
        return std::nullopt;
    }
    return btime;
}

std::optional<std::string> getCmdLine(const std::string& cmdLinePath)
{
    const auto cmdLineFileData = readFile(cmdLinePath);
    if (!cmdLineFileData) {
        std::cerr << "Could not read " << cmdLinePath << std::endl;
        return std::nullopt;
    }
    // TODO: Add quotes/escaping
    // TODO: Handle empty arguments. Mailspring appends 598 NULLs at the end of it's cmdline.
    // Why? I do not know, but it turns into a bunch of empty space.
    std::string cmdLine = *cmdLineFileData;
    for (auto& ch : cmdLine) {
        if (ch == 0) {
            ch = ' ';
        }
    }
    return cmdLine;
}

std::optional<uint64_t> getMemTotal()
{
    const auto fileData = readFile("/proc/meminfo");
    if (!fileData) {
        std::cerr << "Could not read /proc/meminfo" << std::endl;
        return std::nullopt;
    }
    uint64_t memTotal;
    if (std::sscanf(fileData->data(), "MemTotal: %lu kB", &memTotal) != 1) {
        std::cerr << "Could not parse /proc/meminfo" << std::endl;
        return std::nullopt;
    }
    return memTotal * 1024;
}
}

int ps(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<PsArgs>(argc, argv).value();

    assert(!args.verbose);

    std::vector<Column> columns {
        { "user", Column::Type::String },
        { "pid", Column::Type::I64 },
        { "ppid", Column::Type::I64 },
        { "state", Column::Type::String },
        { "cpuusage", Column::Type::I64 }, // TODO: double, cputime / time process is running
        { "memusage", Column::Type::I64 }, // TODO: double, rss / memtotal
        { "vsize", Column::Type::I64 },
        { "rss", Column::Type::I64 },
        { "starttime", Column::Type::String },
        { "cputime", Column::Type::I64 }, // user + system
        { "cmdline", Column::Type::String },
    };
    if (args.verbose) {
        std::vector<Column> moreColumns {
            { "comm", Column::Type::String },

            { "pgrp", Column::Type::I64 },
            { "session", Column::Type::I64 },
            { "tty_nr", Column::Type::I64 },
            { "tpgid", Column::Type::I64 },
            { "flags", Column::Type::I64 },
            { "minflt", Column::Type::I64 },
            { "cminflt", Column::Type::I64 },
            { "majflt", Column::Type::I64 },
            { "cmajflt", Column::Type::I64 },
            { "utime", Column::Type::I64 },
            { "stime", Column::Type::I64 },
            { "cutime", Column::Type::I64 },
            { "cstime", Column::Type::I64 },
            { "priority", Column::Type::I64 },
            { "nice", Column::Type::I64 },
            { "num_threads", Column::Type::I64 },
            { "itrealvalue", Column::Type::I64 },
            { "starttime", Column::Type::I64 },

            { "rsslim", Column::Type::I64 },
            { "startcode", Column::Type::I64 },
            { "endcode", Column::Type::I64 },
            { "startstack", Column::Type::I64 },
            { "kstkesp", Column::Type::I64 },
            { "kstkeip", Column::Type::I64 },
            { "signal", Column::Type::I64 },
            { "blocked", Column::Type::I64 },
            { "sigignore", Column::Type::I64 },
            { "sigcatch", Column::Type::I64 },
            { "wchan", Column::Type::I64 },
            { "nswap", Column::Type::I64 },
            { "cnswap", Column::Type::I64 },
            { "exit_signal", Column::Type::I64 },
            { "processor", Column::Type::I64 },
            { "rt_priority", Column::Type::I64 },
            { "policy", Column::Type::I64 },
            { "delayacct_blkio_ticks", Column::Type::I64 },
            { "guest_time", Column::Type::I64 },
            { "cguest_time", Column::Type::I64 },
            { "start_data", Column::Type::I64 },
            { "end_data", Column::Type::I64 },
            { "start_brk", Column::Type::I64 },
            { "arg_start", Column::Type::I64 },
            { "arg_end", Column::Type::I64 },
            { "env_start", Column::Type::I64 },
            { "env_end", Column::Type::I64 },
            { "exit_code", Column::Type::I64 },
        };
        columns.insert(columns.end(), moreColumns.begin(), moreColumns.end());
    }

    Output output(columns);

    std::unordered_map<uid_t, std::string> usernames;
    const auto uid = ::getuid();

    auto procDir = ::opendir("/proc/");
    if (!procDir) {
        std::cerr << "Could not open /proc/" << std::endl;
        return 2;
    }

    const auto pageSize = ::getpagesize();
    const auto clockTicksHz = ::sysconf(_SC_CLK_TCK);
    assert(clockTicksHz > 0);

    const auto bootTime = getBootTime();
    if (!bootTime) {
        return 3;
    }

    const auto memTotal = getMemTotal();
    if (!memTotal) {
        return 3;
    }

    ::dirent* procDirent;
    while ((procDirent = ::readdir(procDir))) {
        if (procDirent->d_type != DT_DIR) {
            continue;
        }

        int pid;
        if (::sscanf(procDirent->d_name, "%d", &pid) != 1) {
            continue;
        }

        const auto procPath = std::string("/proc/") + procDirent->d_name;

        const auto procStatPath = procPath + "/stat";

        struct stat st;
        if (::stat(procStatPath.c_str(), &st) == -1) {
            std::cerr << "Could not stat " << procStatPath << std::endl;
            continue;
        }

        if (!args.all && st.st_uid != uid) {
            continue;
        }

        auto it = usernames.find(st.st_uid);
        if (it == usernames.end()) {
            const auto user = ::getpwuid(st.st_uid);
            assert(user);
            it = usernames.emplace(st.st_uid, user->pw_name).first;
        }
        const auto& username = it->second;

        const auto procStat = readProcStat(procStatPath);
        if (!procStat) {
            continue;
        }

        const int64_t startTimestamp = *bootTime + procStat->starttime / clockTicksHz;
        const auto tm = std::localtime(&startTimestamp);
        char timebuf[32];
        if (std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm) == 0) {
            std::cerr << "Cannot format timestamp " << startTimestamp << std::endl;
            return 4;
        }
        const std::string startTime(timebuf);

        const auto cpuTime = (procStat->utime + procStat->stime) / clockTicksHz;

        const auto now = static_cast<int64_t>(std::time(nullptr));
        const auto age = now - startTimestamp;
        // This is what ps does, but I never really found it particularly useful.
        const auto cpuUsage = age > 0 ? cpuTime * 100 / age : 0;
        const auto memUsage = procStat->rss * pageSize * 100 / *memTotal;

        const auto cmdLinePath = procPath + "/cmdline";
        const auto cmdLineFileData = readFile(cmdLinePath);
        if (!cmdLineFileData) {
            std::cerr << "Could not read " << cmdLinePath << std::endl;
            continue;
        }
        const auto cmdLine = getCmdLine(cmdLinePath);
        if (!cmdLine) {
            continue;
        }

        std::vector<Value> values {
            username,
            static_cast<int64_t>(procStat->pid),
            static_cast<int64_t>(procStat->ppid),
            std::string(1, procStat->state),
            static_cast<int64_t>(cpuUsage),
            static_cast<int64_t>(memUsage),
            static_cast<int64_t>(procStat->vsize),
            static_cast<int64_t>(procStat->rss * pageSize),
            startTime,
            static_cast<int64_t>(cpuTime),
            *cmdLine,
        };
        output.row(values);
    }
    ::closedir(procDir);

    return 0;
}
