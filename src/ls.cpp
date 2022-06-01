#include <iostream>

#include <dirent.h>
#include <sys/stat.h>

#include "../../cli/clipp.hpp"

#include "io.hpp"

namespace {
struct LsArgs : clipp::ArgsBase {
    bool recursive = false;
    bool all = false; // . and ..
    bool noStat = false;
    bool followSymlinks = false;
    bool absPath = false;
    std::vector<std::string> paths;

    void args()
    {
        flag(recursive, "recursive", 'R').help("List recursively");
        flag(all, "all", 'a').help("Include . and ..");
        flag(noStat, "no-stat", 's').help("Don't stat files. Just include filenames");
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
}

int ls(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<LsArgs>(argc, argv).value();

    assert(!args.recursive);
    assert(!args.noStat);
    assert(!args.followSymlinks);
    assert(!args.absPath);
    assert(args.paths.empty());

    Output output({
        Column { "name", Column::Type::String },
        // Column { "mtime", Column::Type::String },
        // Column { "user", Column::Type::String },
        // Column { "group", Column::Type::String },
        // Column { "mode", Column::Type::I64 },
        Column { "type", Column::Type::String },
        // Column { "size", Column::Type::I64 },
        // Column { "target", Column::Type::String },
        Column { "inode", Column::Type::I64 },
    });

    DIR* dir = ::opendir(".");
    if (!dir) {
        std::cerr << "Could not open directory" << std::endl;
        return 2;
    }

    ::dirent* dirent;
    while ((dirent = ::readdir(dir))) {
        const auto name = std::string(dirent->d_name);
        if (name.empty()) {
            continue;
        }
        if (!args.all && name[0] == '.') {
            continue;
        }
        output.row({ name, typeToString(dirent->d_type), static_cast<int64_t>(dirent->d_ino) });
    }
    ::closedir(dir);
    return 0;
}
