#include <iostream>

int ls(int argc, char** argv);
int print(int argc, char** argv);
int sort(int argc, char** argv);

int main(int argc, char** argv)
{
    std::string prog = argv[0];
    const auto lastSlash = prog.rfind('/');
    if (lastSlash != std::string::npos) {
        prog = prog.substr(lastSlash + 1);
    }

    if (prog == "jls") {
        return ls(argc, argv);
    } else if (prog == "jprint") {
        return print(argc, argv);
    } else if (prog == "jsort") {
        return sort(argc, argv);
    } else {
        std::cerr << "Please run this executable through a symlink. argv[0] = " << prog
                  << std::endl;
        return 1;
    }
}
