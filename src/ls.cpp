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
    bool directories = false;
    std::vector<std::string> paths;

    void args()
    {
        flag(recursive, "recursive", 'R').help("List recursively");
        flag(all, "all", 'a').help("Include . and ..");
        flag(stat, "stat", 's').help("Don't stat files. Just include filenames");
        flag(followSymlinks, "follow-symlinks", 'L').help("Follow symlinks");
        flag(absPath, "abspath", 'p').help("Absolute paths");
        flag(directories, "directories", 'd')
            .help("List directories themeselves, not their content");
        positional(paths, "paths").optional();
    }
};

enum class FileType {
    Unknown = 0,
    Fifo = S_IFIFO,
    Char = S_IFCHR,
    Directory = S_IFDIR,
    Block = S_IFBLK,
    Regular = S_IFREG,
    Link = S_IFLNK,
    Socket = S_IFSOCK,
};

std::string toString(FileType type)
{
    switch (type) {
    case FileType::Fifo:
        return "fifo";
    case FileType::Char:
        return "char";
    case FileType::Directory:
        return "directory";
    case FileType::Block:
        return "block";
    case FileType::Regular:
        return "file";
    case FileType::Link:
        return "link";
    case FileType::Socket:
        return "socket";
    case FileType::Unknown:
    default:
        return "unknown";
    }
}

FileType direntTypeToFileType(unsigned char type)
{
    switch (type) {
    case DT_BLK:
        return FileType::Block;
    case DT_CHR:
        return FileType::Char;
    case DT_DIR:
        return FileType::Directory;
    case DT_FIFO:
        return FileType::Fifo;
    case DT_LNK:
        return FileType::Link;
    case DT_REG:
        return FileType::Regular;
    case DT_SOCK:
        return FileType::Socket;
    case DT_UNKNOWN:
    default:
        return FileType::Unknown;
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

const std::string& getCwd()
{
    static std::string cwd;
    if (cwd.empty()) {
        const auto cwdStr = ::getcwd(nullptr, 0); // glib extension
        cwd = cwdStr;
        ::free(cwdStr);
    }
    return cwd;
}

using Stat = struct stat;
Stat lstat(const std::string& path)
{
    struct stat st;
    const auto res = ::lstat(path.c_str(), &st);
    if (res < 0) {
        std::cerr << "Could not stat file: " << path << std::endl;
        std::exit(4);
    }
    return st;
}

std::string getLinkTarget(const std::string& path)
{
    char target[256];
    const auto res = ::readlink(path.c_str(), target, sizeof(target));
    if (res < 0) {
        std::cerr << "Error reading symlink target: " << path << std::endl;
        std::exit(2);
    }
    return target;
}

void entry(
    Output& output, const std::string& path, FileType type, int64_t inode, const LsArgs& args)
{
    std::vector<Value> values;
    if (args.absPath) {
        values.push_back(getCwd() + "/" + path);
    } else {
        values.push_back(path);
    }
    values.push_back(toString(type));
    values.push_back(static_cast<int64_t>(inode));

    if (type == FileType::Link) {
        values.push_back(getLinkTarget(path));
    } else {
        values.push_back(std::string(""));
    }

    if (args.stat) {
        const auto st = lstat(path);

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
            std::exit(3);
        }
        values.push_back(std::string(timebuf));
    }

    output.row(values);
}

void lsDir(Output& output, const std::string& path, int64_t inode, const LsArgs& args)
{
    if (args.directories) {
        entry(output, path, FileType::Directory, inode, args);
        return;
    }

    DIR* dir = ::opendir(path.c_str());
    if (!dir) {
        std::cerr << "Could not open directory" << std::endl;
        std::exit(2);
    }

    const auto cwd = getCwd();

    ::dirent* dirent;
    while ((dirent = ::readdir(dir))) {
        auto name = std::string(dirent->d_name);
        if (name.empty()) {
            continue;
        }
        if (!args.all && name[0] == '.') {
            continue;
        }

        if (path != ".") {
            name = path + "/" + name;
        }

        entry(output, name, direntTypeToFileType(dirent->d_type), dirent->d_ino, args);
    }
    ::closedir(dir);
}
}

int ls(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<LsArgs>(argc, argv).value();

    assert(!args.recursive);
    assert(!args.followSymlinks);

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

    if (args.paths.empty()) {
        const auto st = lstat(".");
        lsDir(output, ".", st.st_mode, args);
    } else {
        for (const auto& path : args.paths) {
            const auto st = lstat(path);
            if (S_ISDIR(st.st_mode)) {
                // remove trailing slash
                const auto npath
                    = path[path.size() - 1] == '/' ? path.substr(0, path.size() - 1) : path;
                lsDir(output, npath, st.st_ino, args);
            } else {
                // TODO: Avoid stat-ing inside this function again
                entry(output, path, static_cast<FileType>(st.st_mode & S_IFMT), st.st_ino, args);
            }
        }
    }

    return 0;
}
