#include "user.h"
#include "types.h"

int main(void) {
    printf("=== testproc: start ===\n");

    int pid = fork();
    if (pid < 0) {
        printf("fork failed!\n");
        exit();
    }

    if (pid == 0) {
        // 子进程
        printf("child: pid=%d\n", getpid());
        for (int i = 0; i < 5; i++) {
            printf("child counting %d\n", i);
        }
        exit();
    } else {
        // 父进程
        printf("parent: pid=%d, waiting for child=%d\n", getpid(), pid);
        wait(); // 等待子进程退出
        printf("parent: child exited\n");
        exit();
    }

    return 0;
}
