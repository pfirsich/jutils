#include <iostream>

/* TODO:
    * Rename: jtransform -> jselect
    * Rename to jutils
    * Default jprint --sponge output
    * Add install command (creates symlinks)
    * Transform Expressions: jtransform pid cmdline cputime="(utime + stime) / 2"
      integers: /+-*(), humanizebytes, humanizesibytes, humanizetimestamp
      strings: abspath, basename, dir, substr, shell(command)
      conversions: int(s), str(i)
      add magic variable __rowindex__
    * jslice 15 # head -n15
      jslice -15 # tail -n15
      jslice 5 18 # element 6 to 19
      jslice 5 16 2 # skip
    * jfilter thread_count > 1000 and cmdline =~ REGEX
      all expressions from jtransform (for string and ints)
      add "and", "or", "not", "xor"
      add comparisons for ints (==, !=, <, >, <=, >=)
      for strings add ==, != and contains(str, needle), startswith, endswith
    * jfilter --unique <column>
    * jsqlite data.db 'select * from table;'
    * jcsv # will transform binary records to csv or csv to binary records (stdin -> stdout)
        06-01 11:55:03|DEBUG|0xffdfdd|    main.cpp:30 | main | Log Message with pipes ||
        jcsv --delim "|" --trim --columns time,severity,!DISCARD,sourceloc,function,message
    * jjson # same (stdin -> stdout)
    * jls --recursive # like find!
    * jforeach "cmd {column} {othercolumn}"
    * jss would be very cool
    * jproc exposes ps-like information
    * jsplice to cat row (if columns are equal) and column-wise (if #rows is equal)! What as input?
    * jprint -> jdump with optional positional arguments as output files?
    * jsort --shuffle
 */

int ls(int argc, char** argv);
int print(int argc, char** argv);
int sort(int argc, char** argv);
int transform(int argc, char** argv);

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
    } else if (prog == "jtransform") {
        return transform(argc, argv);
    } else {
        std::cerr << "Please run this executable through a symlink. argv[0] = " << prog
                  << std::endl;
        return 1;
    }
}
