#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void str_echo(int sockfd, char *docroot);
void err_abort(char *str);


int main(int argc, char *argv[]) {

    struct sockaddr_in srv_addr;
    int sockfd, newsockfd, pid;
    if(argc < 3){
        fprintf(stderr, "Syntax: %s <docroot> <port>\n", argv[0]);
        exit(1);
    }

    if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        err_abort((char*) "Kann Stream-Socket nicht oeffnen!");
    }

    memset((void *)&srv_addr, '\0', sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //Hier wird der richtige Port übergeben, damit der Server auf diesem Port startet
    srv_addr.sin_port = htons(atoi(argv[2]));
    if(bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0){
        err_abort((char*) "Kann lokale Adresse nicht binden, laeuft fremder Server?");
    }

    listen(sockfd, 5);

    for(;;) {
        struct sockaddr_in cli_addr;
        socklen_t alen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &alen);
        if (newsockfd < 0) {
            err_abort((char *) "Fehler beim Verbindungsaufbau!");
        }

        if ((pid = fork()) < 0) {
            err_abort((char *) "Fehler beim Fork!");
        } else if (pid == 0) {
            close(sockfd);
            str_echo(newsockfd, argv[1]);
            exit(0);
        } else {
            close(newsockfd);
        }
    }
}

void err_abort(char *str) {
    fprintf(stderr, "TCP Echo-Server: %s\n", str);
    fflush(stdout);
    fflush(stderr);
    exit(1);
}

void str_echo(int sockfd, char *docroot) {
    char buffer[512];
    char path[512];
    int n;

    n = read(sockfd, buffer, sizeof(buffer));
    if (n < 0) {
        err_abort((char *) "Fehler beim Lesen vom Socket!");
    }

    sscanf(buffer, "%*s %s", path);
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", docroot, path);

    FILE *file = fopen(fullpath, "r");
    if (file == NULL) {
        const char *not_found = "HTTP/1.0 404 Not Found\r\n\r\n";
        write(sockfd, not_found, strlen(not_found));
        return;
    }

    const char *ok_response = "HTTP/1.0 200 OK\r\n\r\n";
    write(sockfd, ok_response, strlen(ok_response));

    while (fread(buffer, sizeof(buffer), file) != NULL) {
        write(sockfd, buffer, strlen(buffer));
    }

    fclose(file);
}
