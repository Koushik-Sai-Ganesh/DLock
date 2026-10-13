#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(void)
{
    const char *message = "DLock file system demonstration\n";
    char buffer[100];

    // Create or open the file for writing
    int fd = open("dlock_demo.txt",
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1) {
        perror("open");
        return 1;
    }

    // Write the message to the file
    if (write(fd, message, strlen(message)) == -1) {
        perror("write");
        close(fd);
        return 1;
    }

    close(fd);

    // Open the file for reading
    fd = open("dlock_demo.txt", O_RDONLY);

    if (fd == -1) {
        perror("open");
        return 1;
    }

    // Read the file contents
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);

    if (n == -1) {
        perror("read");
        close(fd);
        return 1;
    }

    buffer[n] = '\0';
    printf("File contents: %s", buffer);

    close(fd);
    return 0;
}
