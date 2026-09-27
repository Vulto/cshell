# cs

`cs` is a small Linux shell with C-shaped control syntax and tcsh-inspired shell behavior.

## Build

```sh
clang -std=c23 -O3 -Wall -Wextra -Werror cs.c -o cs
```

## Interactive use

```sh
./cs
```

Scripts use the `.cs` extension:

```sh
./cs build.cs
```

A script can also use:

```text
#!/usr/bin/env cs
```

## Examples

The `examples/` directory is the executable language reference. Each file demonstrates one aspect of cshell and is executed by CI after every successful build.

- `basic.cs` — commands and statements
- `variables.cs` — variables and command expansion
- `if.cs` — `if` / `else`
- `while.cs` — `while`, `break`, `continue`
- `for.cs` — `for`
- `switch.cs` — `switch`, `case`, `default`
- `function.cs` — functions and parameters
- `functions_return.cs` — function return
- `array.cs` — arrays, indexing, assignment, `.length`
- `expressions.cs` — arithmetic and boolean expressions
- `strings.cs` — quoted strings
- `system_variable.cs` — `status`, `argc`, `argN`
- `arguments.cs` — script arguments
- `environment.cs` — `$NAME` environment variables
- `command_variable.cs` — command names stored in variables
- `redirection.cs` — input/output/error redirection
- `pipe.cs` — pipelines
- `glob.cs` — pathname globbing
- `command_substitution.cs` — backquote command substitution
- `background.cs` — background execution and jobs
- `alias_history.cs` — aliases and history
- `cd_exit.cs` — builtin directory handling
- `unset.cs` — removing script and environment variables
- `exit.cs` — shell exit status

To test the language locally:

```sh
clang -std=c23 -O3 -Wall -Wextra -Werror cs.c -o cs
for example in examples/*.cs; do
    ./cs "$example" || exit 1
done
```

The CI suite also checks expected output for every example, so a feature can fail CI even when the shell process itself exits successfully.

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
echo count
++count
```

Unquoted command arguments matching script variables expand to their values. This also works in command position:

```c
CC = "clang"
FLAGS = "-std=c23 -O3 -Wall -Wextra -Werror"
SOURCE = "cs.c"
BIN = "/tmp/cs-built"

CC FLAGS SOURCE -o BIN
```

Every `$NAME` reference is an environment-variable lookup. Script variables are referenced without `$`.

Control flow uses C-like blocks:

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

Arrays:

```c
files = ["main.c", "util.c", "test.c"]

echo files[0]
files[1] = "other.c"
echo files.length
```

Functions:

```c
build(name) {
    echo "building" name
    return 0
}

build("game")
```

Semicolons are optional when a newline unambiguously terminates a statement. They can separate multiple statements on one line.

## Builtins and shell features

Implemented:

- interactive REPL
- `.cs` script execution
- variables and command expansion
- arithmetic and boolean expressions
- `if` / `else`
- `while`
- `for`
- `switch` / `case` / `default`
- `break` / `continue`
- functions and `return`
- arrays and indexed access
- `cd`
- `exit`
- `status`
- script arguments through `argc` and `argN`
- environment variables through `$NAME`
- `unset`
- redirection
- pipelines
- background execution and job control
- globbing
- command substitution
- aliases
- command history
- tab completion
- SIGINT/SIGTSTP foreground job handling
