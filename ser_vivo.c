#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main() {

    int fd;
    char *dir = "ninho";
    char *file = "ninho/semente.c";

    char *code =
        "#include <stdio.h>\n"
        "int main(){ printf(\"O Vazio ecoa no Cheio.\\n\"); return 42; }\n";

    mkdir(dir, 0755);

    fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write(fd, code, 120);
    close(fd);

    pid_t p = fork();

    if (p == 0) {
        char *args[] = {
            "clang",
            "-o",
            "ninho/semente",
            "ninho/semente.c",
            NULL
        };
        execvp("clang", args);
    }

    wait(NULL);

    char *run[] = { "ninho/semente", NULL };
    execv("ninho/semente", run);

    return 0;
}
