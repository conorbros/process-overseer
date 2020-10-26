# Process Overseer

## Usage

Run `make` which will create the `overseer` and `controller` in the build directory.

## Description

This software is a distributed process overseer and controller networked over BSD sockets. You can send commands to the overseer using the controller to run processes and then control and monitor these processes while they are running.

### Usage of the Overseer

Specify the port for the overseer to run on. Mandatory arguments are marked with `<>` and optional with `[]`.

```Bash
overseer <port>
```


### Usage of the Controller

There are three usages of the controller.

```Bash
controller <address> <port> {[-o out_file] [-log log_file] [-t seconds] <file> [arg...] | mem [pid] | memkill <percent>}
```

This command will send a command to execute the file specified by `<file>` with the arguments in `[args...]`. There are options for the file to redirect output of the process to, the file that the overseer will log its running of the process and the time in seconds to wait before killing the process. The default timeout is 10 seconds. If a process does not terminate 5 seconds after sending a `SIGTERM`, the process will be killed with `SIGKILL`.

```Bash
controller <address> <port> -o [out_file] -log [log_file] -t [seconds] <file> [args...]
```

This command will retrieve the memory usage in bytes of all currently running processes on the overseer. If the `[pid]` argument is supplied it will retrieve memory usage records for each second that the process has been running.

```Bash
controller <address> <port> mem [pid]
```

This command will kill all processes currently using more than the specified percent of total system memory. They will be immediately killed with `SIGKILL`.

```Bash
controller <address> <port> memkill <percent>
```