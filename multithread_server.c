#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define MAX_CLIENTS 64

int clients[MAX_CLIENTS] = {0};
int num_clients = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_thread(void *);

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        return 1;
    }

    while (1)
    {
        int client = accept(listener, NULL, NULL);
        if (client == -1)
        {
            perror("accept() failed");
            continue;
        }
        pthread_mutex_lock(&clients_mutex);
        if (num_clients < MAX_CLIENTS)
        {
            printf("New client connected: %d\n", client);
            clients[num_clients] = client;
            num_clients++;
        }
        else
        {
            printf("Too many connections\n");
            close(client);
        }
        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_thread, &client);
        pthread_detach(thread_id);
    }

    close(listener);

    return 0;
}

void *client_thread(void *param)
{
    int client = *(int *)param;

    char buf[256];
    char message[1024];

    int i = 0;
    int ii = 0;
    for (int j = 0; j < MAX_CLIENTS; j++)
    {
        if (clients[j] == client)
        {
            i = j;
            break;
        }
        if (j == MAX_CLIENTS - 1)
        {
            printf("Failed at line 11x");
            exit(0);
        }
    }
    if (i % 2 == 0)
        ii = i + 1;
    else
        ii = i - 1;

    while (1)
    {
        // Receive data from the client
        int ret = recv(client, buf, sizeof(buf), 0);
        if (ret <= 0)
            break;

        buf[strcspn(buf, "\n")] = '\0';
        printf("Received from client %d: %s\n", clients[i], buf);

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        snprintf(message, sizeof(message), "%4d-%2d-%2d %2d:%2d:%2d %s: %s\n",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec, clients[i], buf);

        ret = send(clients[ii], message, strlen(message), 0);
        if (ret == -1)
        {
            perror("send() failed");
            return NULL;
        }
    }
    close(client);
}