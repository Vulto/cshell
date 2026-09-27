# cs

`cs` is a small interactive Linux shell with C-shaped control syntax and tcsh-inspired shell behavior where the syntax has not been deliberately changed.

## Build

```sh
cc -std=c23 -O3 -Wall -Wextra -Werror cs.c -o cs
```

## Interactive use

```sh
./cs
```

The shell reads and executes commands immediately. A closing `}` completes a multiline construct.

## Script files

Scripts use the `.cs` extension:

```sh
./cs build.cs
```

A script can also be executable with:

```text
#!/usr/bin/env cs
```

## Syntax

Commands remain shell commands:

```c
ls -la
cd /tmp
printf "%s\\n" hello
```

Variables have no `$` prefix:

```c
count = 0
printf "%d\\n" count
++count
```

An unquoted command argument that matches a defined variable expands to its value. Quoted text is literal.

Control flow:

```c
if (count < 10) {
    echo count
} else {
    echo "done"
}

while (count < 10) {
    ++count
}

for (count = 0; count < 10; ++count) {
    echo count
}

switch (count) {
case 10: {
    echo "ten"
}
default: {
    echo "other"
}
}
```

Semicolons are optional when the newline unambiguously terminates the statement. They can still separate multiple statements on one line.

Functions use C-like syntax:

```c
build(name) {
    echo "building" name
    return 0
}

build("game")
```

Function parameters and local variables are local to the function call.

## Variables and commands

Variable names may be the same as command names, following the shell-style distinction between command position and argument expansion.

The first word of a command is always resolved as a command. Unquoted arguments matching defined variables are expanded.

For example:

```c
ls = "value"
ls
echo ls
echo "ls"
```

Here `ls` in command position still runs the executable, while the unquoted argument expands to the variable value.

The special variable `status` contains the exit status of the most recently completed command.

Script arguments are available as `argc`, `arg0`, `arg1`, and so on. `argc` counts arguments after the script filename; `arg0` is the script filename.

Environment variables are imported at startup. Assignments update both the shell variable and the process environment:

```c
CC = "clang"
PATH = "/usr/bin:/bin"
unset CC
```

`cd` also keeps `PWD` and `OLDPWD` synchronized after a successful directory change.

For example:

```sh
./build.cs game debug
```

makes `argc` equal to `2`, with `arg0` set to `build.cs`, `arg1` to `game`, and `arg2` to `debug`.

## Current shell scope

Implemented:

- interactive shell
- `.cs` script execution
- C-style blocks
- optional statement semicolons
- `if` / `else`
- `while`
- `for`
- `switch` / `case` / `default`
- `break` / `continue`
- functions and `return`
- variables and arithmetic/boolean expressions
- automatic variable expansion in command arguments
- double-quoted and single-quoted strings
- external command execution through `PATH`
- `cd` and `exit`
- command exit status exposed as the `status` variable
- script arguments exposed as `argc` and `argN` variables
- environment variables imported and synchronized with the process environment
- `unset` builtin
- SIGINT handling for foreground commands

Not implemented yet by design:

- pipelines
- redirection
- background execution/job control
- globbing
- command substitution
- aliases
- command history
- completion
