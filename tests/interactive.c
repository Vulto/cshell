#define main cs_shell_main
#include "../cs.c"
#undef main

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

static void Fail(const char *message)
{
    fprintf(stderr, "completion-test: %s\n", message);
    exit(1);
}

static void ExpectCompletion(const char *input, const char *expected)
{
    char buffer[PATH_MAX];
    snprintf(buffer, sizeof(buffer), "%s", input);

    size_t length = strlen(buffer);
    if (!CompleteLine(buffer, &length, sizeof(buffer)))
        Fail("completion was not produced");

    if (strcmp(buffer, expected))
        Fail("unexpected completion result");
}

int main(void)
{
    char temp[] = "/tmp/cshell-completion-XXXXXX";
    if (!mkdtemp(temp))
        Fail("mkdtemp failed");

    char bin[PATH_MAX];
    char file[PATH_MAX];
    char command[PATH_MAX];

    snprintf(bin, sizeof(bin), "%s/bin", temp);
    snprintf(file, sizeof(file), "%s/completion file.txt", temp);
    snprintf(command, sizeof(command), "%s/cstest-command", bin);

    if (mkdir(bin, 0700) < 0)
        Fail("mkdir failed");

    FILE *f = fopen(file, "w");
    if (!f)
        Fail("cannot create completion file");
    fputs("path-ok\n", f);
    fclose(f);

    const char *true_path = access("/usr/bin/true", X_OK) == 0
        ? "/usr/bin/true"
        : "/bin/true";
    if (symlink(true_path, command) < 0)
        Fail("cannot create command target");

    const char *old_path = getenv("PATH");
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "%s:%s", bin, old_path ? old_path : "");
    setenv("PATH", path, 1);

    if (chdir(temp) < 0)
        Fail("chdir failed");

    int saved_stdout = dup(STDOUT_FILENO);
    int null = open("/dev/null", O_WRONLY);
    if (saved_stdout < 0 || null < 0)
        Fail("cannot redirect stdout");
    dup2(null, STDOUT_FILENO);
    close(null);

    ExpectCompletion("cstest", "cstest-command");
    ExpectCompletion("printf x | cstest", "printf x | cstest-command");
    ExpectCompletion("cat 'completion", "cat 'completion file.txt");

    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    unlink(command);
    unlink(file);
    rmdir(bin);
    rmdir(temp);
    return 0;
}
