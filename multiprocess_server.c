#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <wait.h>
#include <dirent.h>

void signalHandler(int signo)
{
    int pid = wait(NULL);
    printf("Child %d terminated.\n", pid);
}

int main()
{
    // Create a socket for the server
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    // Set up the server address
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    // Bind the socket to the server address
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }

    // Listen for incoming connections
    if (listen(listener, 5))
    {
        perror("listen() failed");
        return 1;
    }

    signal(SIGCHLD, signalHandler);

    while (1)
    {
        // Accept a new client connection
        int client = accept(listener, NULL, NULL);
        if (client == -1)
        {
            perror("accept() failed");
            continue;
        }
        printf("New client connected: %d\n", client);

        if (fork() == 0)
        {
            close(listener);
            char buf[256];

            // Receive data from the client
            DIR *dir;
            struct dirent *entry;

            dir = opendir(".");
            if (dir == NULL)
            {
                send(client, "ERROR No files to download \r\n", strlen("ERROR No files to download \r\n"), 0);
                close(client);
            }
            char list_file[256] = "";
            int count_file = 0;
            while ((entry = readdir(dir)) != NULL)
            {
                if (entry->d_type == DT_REG)
                {
                    strcat(list_file, entry->d_name);
                    strcat(list_file, " \r\n");
                    count_file++;
                }
            }
            list_file[strlen(list_file)] = '\0';

            int result_size = 256 + (count_file * (256 + 4)); // 256 for the header, 256 for each file name, and 4 for additional characters
            char *result = malloc(result_size);

            sprintf(result, "OK %d\r\n", count_file);
            strcat(result, list_file);
            strcat(result, " \r\n");

            send(client, result, strlen(result), 0);
            while (1)
            {
                int ret = recv(client, buf, sizeof(buf), 0);
                if (ret <= 0)
                    break;

                buf[strcspn(buf, "\n")] = '\0';
                printf("Received from %d: %s\n", client, buf);

                FILE *f = fopen(buf, "rb");
                if (f == NULL)
                {
                    send(client, "ERROR File not found\r\n", strlen("ERROR File not found\r\n"), 0);
                    continue;
                }

                // Move the file pointer to the end of the file
                fseek(f, 0, SEEK_END);

                // Get the current position of the file pointer
                long numBytes = ftell(f);
                fseek(f, 0, SEEK_SET);

                sprintf(result, "OK %ld\r\n", numBytes);

                send(client, result, ret, 0);

                char file_buf[2048];
                while (!feof(f))
                {
                    ret = fread(file_buf, 1, sizeof(file_buf), f);
                    if (ret <= 0)
                        break;
                    send(client, file_buf, ret, 0);
                }
            }

            // Close the client socket
            close(client);
            exit(0);
        }

        close(client);
    }

    // Close the listener socket
    close(listener);

    return 0;
}