#include <ctime>
#include <iostream>

#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../cli/clipp.hpp"

#include "io.hpp"

namespace {
struct LsArgs : clipp::ArgsBase {
    bool recursive = false;
    bool all = false; // . and ..
    bool stat = false;
    bool followSymlinks = false;
    bool absPath = false;
    std::vector<std::string> paths;

    void args()
    {
        flag(recursive, "recursive", 'R').help("List recursively");
        flag(all, "all", 'a').help("Include . and ..");
        flag(stat, "stat", 's').help("Don't stat files. Just include filenames");
        flag(followSymlinks, "follow-symlinks", 'L').help("Follow symlinks");
        flag(absPath, "abspath", 'p').help("Absolute paths");
        positional(paths, "paths").optional();
    }
};

std::string typeToString(unsigned char type)
{
    switch (type) {
    case DT_BLK:
        return "block";
    case DT_CHR:
        return "char";
    case DT_DIR:
        return "directory";
    case DT_FIFO:
        return "fifo";
    case DT_LNK:
        return "link";
    case DT_REG:
        return "file";
    case DT_SOCK:
        return "socket";
    case DT_UNKNOWN:
    default:
        return "unknown";
    }
}

std::string modeToString(int mode)
{
    std::string str(4, '0');
    int mask = 7;
    for (size_t i = 0; i < 4; ++i) {
        str[3 - i] = '0' + (mode & mask) / (mask / 7);
        mask *= 8;
    }
    return str;
}
}

int ls(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<LsArgs>(argc, argv).value();

    assert(!args.recursive);
    assert(!args.followSymlinks);
    assert(args.paths.empty());

    std::vector<Column> columns;
    if (args.absPath) {
        columns.push_back(Column { "path", Column::Type::String });
    } else {
        columns.push_back(Column { "name", Column::Type::String });
    }

    columns.push_back(Column { "type", Column::Type::String });
    columns.push_back(Column { "inode", Column::Type::I64 });
    columns.push_back(Column { "target", Column::Type::String });

    if (args.stat) {
        columns.push_back(Column { "mode", Column::Type::String });
        columns.push_back(Column { "user", Column::Type::String });
        columns.push_back(Column { "group", Column::Type::String });
        columns.push_back(Column { "size", Column::Type::I64 });
        columns.push_back(Column { "mtime", Column::Type::String });
    }

    Output output(columns);

    DIR* dir = ::opendir(".");
    if (!dir) {
        std::cerr << "Could not open directory" << std::endl;
        return 2;
    }

    const auto cwdStr = ::getcwd(nullptr, 0); // glib extension
    const std::string cwd = cwdStr;
    ::free(cwdStr);

    ::dirent* dirent;
    while ((dirent = ::readdir(dir))) {
        const auto name = std::string(dirent->d_name);
        if (name.empty()) {
            continue;
        }
        if (!args.all && name[0] == '.') {
            continue;
        }

        std::vector<Value> values;
        if (args.absPath) {
            values.push_back(cwd + "/" + name);
        } else {
            values.push_back(name);
        }
        values.push_back(typeToString(dirent->d_type));
        values.push_back(static_cast<int64_t>(dirent->d_ino));

        if (dirent->d_type == DT_LNK) {
            char target[256];
            const auto res = ::readlink(name.c_str(), target, sizeof(target));
            if (res < 0) {
                std::cerr << "Error reading target of symlink '" << name << "'" << std::endl;
                return 2;
            }
            values.push_back(std::string(target));
        } else {
            values.push_back(std::string(""));
        }

        if (args.stat) {
            struct stat st;
            const auto res = ::lstat(name.c_str(), &st);
            if (res < 0) {
                std::cerr << "Could not stat file: " << name << std::endl;
                return 4;
            }

            values.push_back(modeToString(st.st_mode));

            const auto user = ::getpwuid(st.st_uid);
            values.push_back(std::string(user->pw_name));

            const auto group = ::getgrgid(st.st_gid);
            values.push_back(std::string(group->gr_name));

            if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
                values.push_back(st.st_size);
            } else {
                values.push_back(0);
            }

            char timebuf[32];
            const auto tm = std::localtime(&st.st_mtime);
            const auto sres = std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);
            if (sres == 0) {
                std::cerr << "Could not format modification time: " << st.st_mtime << std::endl;
                return 3;
            }
            values.push_back(std::string(timebuf));
        }

        output.row(values);
    }
    ::closedir(dir);
    return 0;
}
