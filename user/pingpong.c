#include "user.h"

int main() {
    int f2c[2]; // Pipe for father to child communication
    int c2f[2]; // Pipe for child to father communication

    if (pipe(f2c) == -1 || pipe(c2f) == -1) {
        printf("Failed to pipe.\n");
        exit(1);
    }

    int pid = fork();

    if (pid < 0) {
        printf("Failed to fork.\n");
        exit(1);
    }

    if (pid > 0) { // Parent process
        close(f2c[0]); // Close unused read end of f2c.
        close(c2f[1]); // Close unused write end of c2f.

        // Write message and parent PID to child.
        int ppid = getpid();  // Get parent process ID.
        write(f2c[1], &ppid, sizeof(ppid));
        close(f2c[1]); // Close write end of f2c.

        // Read message and child PID from c2f
        int child_pid;
        read(c2f[0], &child_pid, sizeof(child_pid));
        printf("%d: received pong from pid %d\n", ppid, child_pid);
        close(c2f[0]); // Close read end of c2f

        wait(0); // Wait for child process to finish
    } else { // Child process
        close(f2c[1]); // Close unused write end of f2c
        close(c2f[0]); // Close unused read end of c2f

        // Read message and parent PID from f2c
        int father_pid;
        read(f2c[0], &father_pid, sizeof(father_pid));
        printf("%d: received ping from pid %d\n", getpid(), father_pid);
        close(f2c[0]); // Close read end of f2c

        // Write message and child PID to parent.
        int child_pid = getpid();
        write(c2f[1], &child_pid, sizeof(child_pid));
        close(c2f[1]); // Close write end of c2f

        exit(0);
    }

    return 0;
}