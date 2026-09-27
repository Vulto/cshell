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

## Variable/command conflicts

A variable name cannot be the name of an executable found through `PATH`. The shell rejects the assignment and requires a different variable name.

`PATH` changes are checked against existing variables before the new value is accepted.

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
- command/variable name collision checking
- command exit status
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
