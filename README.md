# pipesig

This runs another program, passing input/output through, but also catching and
handling SIGPIPE/EPIPE properly. This is for command-line programs that ignore
SIGPIPE and don't do anything else to handle a loss of standard input/output.

## running

```bash
$ make
$ ./pipesig path/to/command
```

## credits and license

Copyright (c) 2020 Rob N. MIT license. See LICENSE.
