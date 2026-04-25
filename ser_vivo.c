#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    static const char dir[] = "ninho";
    static const char file[] = "ninho/semente.c";
    static const char bin[] = "ninho/semente";
    static const char code[] =
        "#include <stdio.h>\n"
        "int main(void){ puts(\"O Vazio ecoa no Cheio.\"); return 42; }\n";

    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        perror("mkdir");
        return 1;
    }

    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    size_t total = sizeof(code) - 1U;
    size_t off = 0;
    while (off < total) {
        ssize_t w = write(fd, code + off, total - off);
        if (w < 0) {
            perror("write");
            close(fd);
            return 1;
        }
        off += (size_t)w;
    }

    if (close(fd) < 0) {
        perror("close");
        return 1;
    }

    pid_t p = fork();
    if (p < 0) {
        perror("fork");
        return 1;
    }

    if (p == 0) {
        char *const args[] = {"clang", "-O2", "-Wall", "-Wextra", "-std=c11", "-o", (char *)bin, (char *)file, NULL};
        execvp("clang", args);
        perror("execvp clang");
        _exit(127);
    }

    int status = 0;
    if (waitpid(p, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "clang falhou (status=%d)\n", status);
        return 1;
    }

    char *const run[] = {(char *)bin, NULL};
    execv(bin, run);
    perror("execv");
    return 1;
}
