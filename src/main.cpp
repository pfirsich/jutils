#include <iostream>

int ls(int argc, char** argv);
int sort(int argc, char** argv);
int select(int argc, char** argv);
int filter(int argc, char** argv);
int slice(int argc, char** argv);
int parse(int argc, char** argv);
int netstat(int argc, char** argv);
int ps(int argc, char** argv);

int main(int argc, char** argv)
{
    std::string prog = argv[0];
    const auto lastSlash = prog.rfind('/');
    if (lastSlash != std::string::npos) {
        prog = prog.substr(lastSlash + 1);
    }

    if (prog == "jls") {
        return ls(argc, argv);
    } else if (prog == "jsort") {
        return sort(argc, argv);
    } else if (prog == "jselect") {
        return select(argc, argv);
    } else if (prog == "jfilter") {
        return filter(argc, argv);
    } else if (prog == "jslice") {
        return slice(argc, argv);
    } else if (prog == "jparse") {
        return parse(argc, argv);
    } else if (prog == "jnetstat") {
        return netstat(argc, argv);
    } else if (prog == "jps") {
        return ps(argc, argv);
    } else {
        std::cerr << "Please run this executable through a symlink. argv[0] = " << prog
                  << std::endl;
        return 1;
    }
}
