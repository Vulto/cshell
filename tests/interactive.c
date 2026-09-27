#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void Fail(const char *message)
{
    fprintf(stderr, "interactive-test: %s\n", message);
    exit(1);
}

static bool Contains(const char *text, const char *needle)
{
    return strstr(text, needle) != NULL;
}

static void Require(const char *output, const char *needle)
{
    if (!Contains(output, needle)) {
        fprintf(stderr, "interactive-test: missing: %s\n", needle);
        fprintf(stderr, "interactive-test: output:\n%s\n", output);
        exit(1);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
        Fail("usage: interactive-test ./cs");

    char cs_path[PATH_MAX];
    if (!realpath(argv[1], cs_path))
        Fail("cannot resolve cs path");

    char temp[] = "/tmp/cshell-interactive-XXXXXX";
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
        Fail("cannot create command completion target");

    const char *old_path = getenv("PATH");
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "%s/bin:%s", temp, old_path ? old_path : "");
    setenv("PATH", path, 1);

    const char *input =
        "cstest\t\n"
        "printf \"x\" | cstest\t\n"
        "if (1) {\n"
        "ech\t \"multiline-ok\"\n"
        "}\n"
        "cat 'completion\t'\n"
        "sleep 1 &\n"
        "jobs\n"
        "fg 1\n"
        "alias ll = echo\n"
        "ll \"alias-ok\"\n"
        "exit\n";

    int master = -1;
    int slave = -1;

    if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
        Fail("openpty failed");

    pid_t pid = fork();
    if (pid < 0)
        Fail("fork failed");

    if (pid == 0) {
        close(master);

        if (setsid() < 0)
            _exit(111);
        if (ioctl(slave, TIOCSCTTY, 0) < 0)
            _exit(112);
        if (dup2(slave, STDIN_FILENO) < 0 ||
            dup2(slave, STDOUT_FILENO) < 0 ||
            dup2(slave, STDERR_FILENO) < 0)
            _exit(113);

        close(slave);

        if (chdir(temp) < 0)
            _exit(114);

        execl(cs_path, cs_path, (char *)NULL);
        _exit(115);
    }

    close(slave);

    char prompt[4096] = {0};
    size_t prompt_length = 0;
    bool prompt_seen = false;

    for (int ticks = 0; ticks < 100; ++ticks) {
        struct pollfd pfd = {.fd = master, .events = POLLIN | POLLHUP};
        int ready = poll(&pfd, 1, 100);

        if (ready > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
            char buffer[512];
            ssize_t n = read(master, buffer, sizeof(buffer));

            if (n > 0) {
                size_t available = sizeof(prompt) - prompt_length - 1;
                if ((size_t)n > available)
                    n = (ssize_t)available;

                memcpy(prompt + prompt_length, buffer, (size_t)n);
                prompt_length += (size_t)n;
                prompt[prompt_length] = 0;

                if (strstr(prompt, "cs> ")) {
                    prompt_seen = true;
                    break;
                }
            }
        }
    }

    if (!prompt_seen)
        Fail("shell did not present its initial prompt");

    size_t input_length = strlen(input);
    size_t sent = 0;
    while (sent < input_length) {
        ssize_t n = write(master, input + sent, input_length - sent);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            Fail("write to PTY failed");
        }
        sent += (size_t)n;
    }

    size_t capacity = 8192;
    size_t length = 0;
    char *output = malloc(capacity);
    if (!output)
        Fail("malloc failed");

    int status = 0;
    bool exited = false;

    for (int ticks = 0; ticks < 100; ++ticks) {
        struct pollfd pfd = {.fd = master, .events = POLLIN | POLLHUP};
        int ready = poll(&pfd, 1, 100);

        if (ready > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
            char buffer[4096];
            ssize_t n = read(master, buffer, sizeof(buffer));
            if (n > 0) {
                if (length + (size_t)n + 1 > capacity) {
                    while (length + (size_t)n + 1 > capacity)
                        capacity *= 2;
                    output = realloc(output, capacity);
                    if (!output)
                        Fail("realloc failed");
                }
                memcpy(output + length, buffer, (size_t)n);
                length += (size_t)n;
                output[length] = 0;
            }
        }

        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            exited = true;
            break;
        }
    }

    if (!exited) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        free(output);
        Fail("interactive shell timed out");
    }

    for (;;) {
        char buffer[4096];
        ssize_t n = read(master, buffer, sizeof(buffer));
        if (n <= 0)
            break;
        if (length + (size_t)n + 1 > capacity) {
            while (length + (size_t)n + 1 > capacity)
                capacity *= 2;
            output = realloc(output, capacity);
            if (!output)
                Fail("realloc failed");
        }
        memcpy(output + length, buffer, (size_t)n);
        length += (size_t)n;
        output[length] = 0;
    }

    close(master);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "interactive-test: cs exited unsuccessfully\n");
        fprintf(stderr, "%s\n", output);
        free(output);
        return 1;
    }

    Require(output, "cstest-command");
    Require(output, "echo \"multiline-ok\"");
    Require(output, "cat 'completion file.txt'");
    Require(output, "path-ok");
    Require(output, "Running");
    Require(output, "alias-ok");

    free(output);
    unlink(command);
    unlink(file);
    rmdir(bin);
    rmdir(temp);
    return 0;
}
