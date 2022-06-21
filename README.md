# jutils
Inspired by PowerShell's structured IO jutils is a collection of some core utils that output structured data instead of plain text. This makes restructuring (sorting, reordering, filtering) much easier and avoids the fiddly and error-prone `awk/tr/cut/grep`-ing around.

They output/expect a custom binary data format that can only represent tabular data with either string or integer columns. If stdout is a TTY, they output the tabular data in a human readable (plain text) format instead.

It's all just an experiment and I think it's kind of neat, but it's probably not a great idea to actually use these. It was also an excuse to implement `ps` and `netstat` myself and can (imho) serve as a compact example of how to do something like that.

## Examples
### ls
Usage: `jls [--help] [--recursive] [--all] [--stat] [--follow-symlinks] [--abspath] [--directories] [paths...]`

```
$ jls
name         type       inode     target
------------------------------------------
deps         directory  9706585
untracked    directory  9705529
test         directory  9706571
builddir     directory  9968664
README.md    file       9708018
build        directory  10368461
meson.build  file       9707902
src          directory  9974134

$ jls --stat
name         type       inode     target  mode  user  group  size  mtime
----------------------------------------------------------------------------------------
deps         directory  9706585           0775  joel  joel   0     2022-06-04 23:47:48
untracked    directory  9705529           0775  joel  joel   0     2022-06-05 15:34:07
test         directory  9706571           0775  joel  joel   0     2022-06-04 23:18:16
builddir     directory  9968664           0775  joel  joel   0     2022-06-21 21:30:49
README.md    file       9708018           0664  joel  joel   873   2022-06-21 21:41:39
build        directory  10368461          0775  joel  joel   0     2022-06-05 23:00:37
meson.build  file       9707902           0664  joel  joel   353   2022-06-05 19:01:13
src          directory  9974134           0775  joel  joel   0     2022-06-05 23:07:10
```

### sort
Usage: `jsort [--help] [--reverse] column`

```
$ jls -s | jsort inode
name         type       inode     target  mode  user  group  size  mtime
----------------------------------------------------------------------------------------
untracked    directory  9705529           0775  joel  joel   0     2022-06-05 15:34:07
test         directory  9706571           0775  joel  joel   0     2022-06-04 23:18:16
deps         directory  9706585           0775  joel  joel   0     2022-06-04 23:47:48
meson.build  file       9707902           0664  joel  joel   353   2022-06-05 19:01:13
README.md    file       9708018           0664  joel  joel   3472  2022-06-21 21:45:43
builddir     directory  9968664           0775  joel  joel   0     2022-06-21 21:30:49
src          directory  9974134           0775  joel  joel   0     2022-06-05 23:07:10
build        directory  10368461          0775  joel  joel   0     2022-06-05 23:00:37
```

### select
Usage: `jselect [--help] columns [columns...]`

```
$ jls -s | jselect name type mode
name         type       mode
------------------------------
deps         directory  0775
untracked    directory  0775
test         directory  0775
builddir     directory  0775
README.md    file       0664
build        directory  0775
meson.build  file       0664
src          directory  0775
```

### filter
Usage: `jfilter [--help] [--invert-match] [--unique UNIQUE]`

```
$ jls -s | jselect name type mode | jfilter type == directory
name       type       mode
----------------------------
deps       directory  0775
untracked  directory  0775
test       directory  0775
builddir   directory  0775
build      directory  0775
src        directory  0775

$ jls -s | jselect name type mode | jfilter name =~ b
name         type       mode
------------------------------
builddir     directory  0775
build        directory  0775
meson.build  file       0664

$ jls -s | jselect name type mode | jfilter name =~ ^b
name      type       mode
---------------------------
builddir  directory  0775
build     directory  0775

$ jls -s | jselect name type mode | jfilter --unique type
name       type       mode
----------------------------
deps       directory  0775
README.md  file       0664
```

### slice
Usage:  `jslice [--help] [--offset OFFSET] [--num NUM] [--step STEP]`

```
$ jls -s | jslice -n 3 # essentially `head`
name       type       inode    target  mode  user  group  size  mtime
-------------------------------------------------------------------------------------
deps       directory  9706585          0775  joel  joel   0     2022-06-04 23:47:48
untracked  directory  9705529          0775  joel  joel   0     2022-06-05 15:34:07
test       directory  9706571          0775  joel  joel   0     2022-06-04 23:18:16

$ jls -s | jslice -o -3 # essentially `tail`
name         type       inode     target  mode  user  group  size  mtime
----------------------------------------------------------------------------------------
build        directory  10368461          0775  joel  joel   0     2022-06-21 21:48:33
meson.build  file       9707902           0664  joel  joel   353   2022-06-05 19:01:13
src          directory  9974134           0775  joel  joel   0     2022-06-05 23:07:10

$ jls -s | jslice -s -2 # every other row, reversed
name       type       inode     target  mode  user  group  size  mtime
--------------------------------------------------------------------------------------
src        directory  9974134           0775  joel  joel   0     2022-06-05 23:07:10
build      directory  10368461          0775  joel  joel   0     2022-06-21 21:48:33
builddir   directory  9968664           0775  joel  joel   0     2022-06-21 21:30:49
untracked  directory  9705529           0775  joel  joel   0     2022-06-05 15:34:07
```

### parse
Usage: `jparse [--help] [--rowdelim ROWDELIM] [--regex REGEX] [--csv CSV] [--trim] columns [columns...]`

```
$ cat test/test.csv
foo1;bar1;baz1;bat1;bla1
foo2;bar2;baz2;bat2;bla2
foo3;bar3;baz3;bat3;bla3
foo4;bar4;baz4;bat4;bla4
foo5;bar5;baz5;bat5;bla5
foo6;bar6;baz6;bat6;bla6
foo7;bar7;baz7;bat7;bla7

$ cat test/test.csv | jparse -c ";" col1 col2 col3 col4 col5
col1  col2  col3  col4  col5
------------------------------
foo1  bar1  baz1  bat1  bla1
foo2  bar2  baz2  bat2  bla2
foo3  bar3  baz3  bat3  bla3
foo4  bar4  baz4  bat4  bla4
foo5  bar5  baz5  bat5  bla5
foo6  bar6  baz6  bat6  bla6
foo7  bar7  baz7  bat7  bla7

$ cat test/test.csv | jparse -c ";" col1 col2 col3
col1  col2  col3
----------------------------
foo1  bar1  baz1;bat1;bla1
foo2  bar2  baz2;bat2;bla2
foo3  bar3  baz3;bat3;bla3
foo4  bar4  baz4;bat4;bla4
foo5  bar5  baz5;bat5;bla5
foo6  bar6  baz6;bat6;bla6
foo7  bar7  baz7;bat7;bla7

$ cat test/test2.csv
06-01 11:55:03|DEBUG|0xffdfdd|    main.cpp:30 | main | Log Message with pipes || #1
06-01 11:55:04|DEBUG|0xffdfdd|    main.cpp:30 | main | Log Message with pipes || #2
06-01 11:55:06|DEBUG|0xffdfdd|    main.cpp:30 | main | Log Message with pipes || #3
06-01 11:55:08|DEBUG|0xffdfdd|    main.cpp:30 | main | Log Message with pipes || #4
06-01 11:55:10|DEBUG|0xffdfdd|    main.cpp:30 | main | Log Message with pipes || #5
06-01 11:55:12|DEBUG|0xffdfdd|    main.cpp:30 | main | Log Message with pipes || #6

$ cat test/test2.csv | jparse -tc "|" time severity this_ptr sourceloc func message
time            severity  this_ptr  sourceloc    func  message
-------------------------------------------------------------------------------------
06-01 11:55:03  DEBUG     0xffdfdd  main.cpp:30  main  Log Message with pipes || #1
06-01 11:55:04  DEBUG     0xffdfdd  main.cpp:30  main  Log Message with pipes || #2
06-01 11:55:06  DEBUG     0xffdfdd  main.cpp:30  main  Log Message with pipes || #3
06-01 11:55:08  DEBUG     0xffdfdd  main.cpp:30  main  Log Message with pipes || #4
06-01 11:55:10  DEBUG     0xffdfdd  main.cpp:30  main  Log Message with pipes || #5
06-01 11:55:12  DEBUG     0xffdfdd  main.cpp:30  main  Log Message with pipes || #6

$ cat test/test.regex
foo1: bar1 baz1 (bat1)
foo2: bar2 baz2 (bat2)
foo3: bar3 baz3 (bat3)
foo4: bar4 baz4 (bat4)
foo5: bar5 baz5 (bat5)⏎

$ cat test/test.regex | jparse -r "(\w+): (\w+) (\w+) \\((\w+)\\)" col1 col2 col3 col4
col1  col2  col3  col4
------------------------
foo1  bar1  baz1  bat1
foo2  bar2  baz2  bat2
foo3  bar3  baz3  bat3
foo4  bar4  baz4  bat4
```

### netstat
Usage: `jnetstat [--help] [--tcp] [--udp] [--ipv4] [--ipv6] [--process]`

```
$ sudo jnetstat -p | jfilter state == LISTEN | jselect srcaddr srcport user inode pid comm
srcaddr        srcport  user             inode     pid      comm
-----------------------------------------------------------------------------
0.0.0.0        33727    root             34311     1897     rpc.mountd
0.0.0.0        2049     root             35298     -1
0.0.0.0        46023    root             34275     1897     rpc.mountd
0.0.0.0        45129    root             34290     1897     rpc.mountd
0.0.0.0        38283    root             35312     -1
0.0.0.0        111      root             18721     1        systemd
127.0.0.1      43921    root             30804     1044     containerd
0.0.0.0        34899    joel             41008084  11270    spotify
0.0.0.0        57621    joel             69933     11270    spotify
0.0.0.0        52981    statd            32280     1895     rpc.statd
192.168.122.1  53       root             31406     1247     dnsmasq
127.0.0.53     53       systemd-resolve  27728     725      systemd-resolve
0.0.0.0        22       root             24536     1057     sshd
127.0.0.1      631      root             39951739  2735612  cupsd
::             33695    root             35271     1897     rpc.mountd
::             42783    root             34301     1897     rpc.mountd
::             2049     root             35308     -1
::             59399    root             35241     1897     rpc.mountd
::             111      root             18038     1        systemd
::             1716     joel             67166     10578    kdeconnectd
::             50165    statd            32285     1895     rpc.statd
::             22       root             24538     1057     sshd
::1            631      root             39951738  2735612  cupsd
::             34973    root             35313     -1
```

### ps
Usage: `jps [--help] [--verbose] [--all]`

Note: `--all` will show processes that don't belong to the executing user and `--verbose` will include pretty much everything that is in `/proc/[fd]/stat` (an additional 47 columns).

```
$ jps | jfilter cmdline =~ "jps"
user  pid     ppid   state  cpuusage  memusage  vsize    rss      starttime            cputime  cmdline
----------------------------------------------------------------------------------------------------------------------
joel  150559  43923  R      0         0         7077888  2174976  2022-06-21 21:59:56  0        jps
joel  150560  43923  R      0         0         6942720  2170880  2022-06-21 21:59:56  0        jfilter cmdline =~ jps
```

## Ideas / To Do
* `jutils install` subcommand that creates symlinks to the jutils binary in the current working directory.
* Transform expressions for `jselect`, e.g.: `jselect pid cmdline cputime="utime + stime"`.
  Integers: `+-*/`, `humanizebytes`, `humanizesibytes`, `humanizetimestamp`.
  Strings: `abspath`, `basename`, `dir`, `substr`.
  Conversions: `int(s)`, `str(i)`.
  Special variables: `__rowindex__`.
* Complex expressions for `jfilter` (logical operators, full set of integer comparison operators, maybe startswith/endswith). Also allow all expressions from jselect.
* `jsqlite` that reads from an SQLite database and emits jutils compatible structured data, e.g. `jsqlite data.db 'select * from table;'` and also reads structured data from stdio into an SQLite table and executes queries on them.
* `jjson` that either parses JSON from stdin and emits structured data or reads structured data and outputs JSON to stdout.
* `jls --recursive` which essentially acts like the `find` command.
* `jforeach "rm {name}"` which can execute commands for each row (a bit like `xargs`).
* `jsplice` to combine data row-wise (if columns are the same) or column-wise (if the number of rows are the same). Not sure how to take multiple inputs right now.
* Add a command that outputs the data as text (formatted as a table, like in the examples above) to a non-tty stdout. Maybe a `jtee` that takes additional positional arguments as output files?
* `jsort --shuffle`
