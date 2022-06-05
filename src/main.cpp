#include <iostream>

/* TODO:
    * Add install command (creates symlinks)
    * Transform Expressions: jtransform pid cmdline cputime="(utime + stime) / 2"
      integers: /+-*(), humanizebytes, humanizesibytes, humanizetimestamp
      strings: abspath, basename, dir, substr, shell(command)
      conversions: int(s), str(i)
      add magic variable __rowindex__
    * jfilter thread_count > 1000 and cmdline =~ REGEX
      all expressions from jtransform (for string and ints)
      add "and", "or", "not", "xor"
      add comparisons for ints (==, !=, <, >, <=, >=)
      for strings add ==, != and contains(str, needle), startswith, endswith
    * jsqlite data.db 'select * from table;'
    * jjson # parse JSON from stdin or dump JSON to stdout
    * jls --recursive # like find!
    * jforeach "cmd {column} {othercolumn}"
    * jproc exposes ps-like information
    * jsplice to cat row (if columns are equal) and column-wise (if #rows is equal)! What as input?
    * Now that all output to tty is done as text, I need a command that outpus text to a non-tty.
      like tee it could take additional positional arguments as output files. "jtee"?
    * jsort --shuffle
 */

int ls(int argc, char** argv);
int sort(int argc, char** argv);
int select(int argc, char** argv);
int filter(int argc, char** argv);
int slice(int argc, char** argv);
int parse(int argc, char** argv);
int netstat(int argc, char** argv);

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
    } else {
        std::cerr << "Please run this executable through a symlink. argv[0] = " << prog
                  << std::endl;
        return 1;
    }
}
