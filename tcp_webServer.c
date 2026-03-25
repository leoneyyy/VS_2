#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>

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
    char buffer[1024];
    char path[512];
    char line[1024];
    char response_header[1024];
    int n;

    n = read(sockfd, buffer, sizeof(buffer) - 1);
    if (n <= 0) return;
    buffer[n] = '\0';

    if (sscanf(buffer, "%*s %s", path) != 1) return;

    // Pfad-Logik (Vermeidung von //)
    char fullpath[1024];
    int root_len = strlen(docroot);
    if (root_len > 0 && docroot[root_len - 1] == '/' && path[0] == '/') {
        snprintf(fullpath, sizeof(fullpath), "%s%s", docroot, path + 1);
    } else {
        snprintf(fullpath, sizeof(fullpath), "%s%s", docroot, path);
    }

    struct stat path_stat;
    if (stat(fullpath, &path_stat) < 0) {
        // DEBUG: Zeigt uns im Terminal den genauen Systemfehler (z.B. No such file or directory)
        perror("Fehler bei stat()");
        printf("Versuchter Zugriff auf: '%s'\n", fullpath);

        const char *not_found = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 - Datei nicht gefunden</h1>";
        write(sockfd, not_found, strlen(not_found));
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        DIR *d = opendir(fullpath);
        struct dirent *entry;
        if (d) {
            const char *header = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n";
            write(sockfd, header, strlen(header));

            write(sockfd, "<html><body><h1>Index</h1><ul>\n", 31);
            while ((entry = readdir(d)) != NULL) {
                sprintf(line, "<li><a href=\"%s\">%s</a></li>\n", entry->d_name, entry->d_name);
                write(sockfd, line, strlen(line));
            }
            write(sockfd, "</ul></body></html>\n", 20);
            closedir(d);
            return;
        }
    }

    // DATEI-MODUS
    FILE *file = fopen(fullpath, "rb");
    if (file == NULL) {
        const char *not_found = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>Datei konnte nicht geoeffnet werden</h1>";
        write(sockfd, not_found, strlen(not_found));
        return;
    }

    const char *mime_type = "text/plain";
    if (strstr(path, ".html")) mime_type = "text/html";
    else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) mime_type = "image/jpeg";
    else if (strstr(path, ".png")) mime_type = "image/png";

    sprintf(response_header, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", mime_type);
    write(sockfd, response_header, strlen(response_header));

    int bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        write(sockfd, buffer, bytes_read);
    }
    fclose(file);
}