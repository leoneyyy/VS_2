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
#include <signal.h>

void str_echo(int sockfd, char *docroot);
void err_abort(char *str);


int main(int argc, char *argv[]) {

    struct sockaddr_in srv_addr;
    int sockfd, newsockfd, pid;
    signal(SIGCHLD, SIG_IGN);
    if(argc < 3){
        fprintf(stderr, "Syntax: %s <docroot> <port>\n", argv[0]);
        exit(1);
    }
    // Hier wird ein TCP-Socket erstellt. Es wird überprüft, ob der Socket erfolgreich erstellt wurde.
    if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        err_abort((char*) "Kann Stream-Socket nicht oeffnen!");
    }

    // Hier wird die Serveradresse (srv_addr) mit den entsprechenden Werten initialisiert.
    memset((void *)&srv_addr, '\0', sizeof(srv_addr));
    // Steht für die Adressfamilie (IPv4) des Servers. Es gibt an, dass der Server IPv4-Adressen verwenden wird.
    srv_addr.sin_family = AF_INET;
    // Steht für die IP-Adresse des Servers. INADDR_ANY bedeutet, dass der Server auf allen verfügbaren Netzwerkinterfaces lauschen wird.
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //Hier wird der richtige Port übergeben, damit der Server auf diesem Port startet
    srv_addr.sin_port = htons(atoi(argv[2]));
    // Hier wird der Socket an die Serveradresse gebunden. Es wird überprüft, ob die Bindung erfolgreich war.
    if(bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0){
        err_abort((char*) "Kann lokale Adresse nicht binden, laeuft fremder Server?");
    }
    // Hier wird der Socket in den Listenmodus versetzt, damit er Verbindungsanfragen von Clients akzeptieren kann.
    listen(sockfd, 5);

    for(;;) {

        // Hier wird auf eine eingehende Verbindungsanfrage eines Clients gewartet.
        struct sockaddr_in cli_addr;
        socklen_t alen = sizeof(cli_addr);
        // Wenn eine Verbindungsanfrage eingeht, wird ein neuer Socket (newsockfd) erstellt, um die Verbindung mit dem Client zu handhaben.
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &alen);
        if (newsockfd < 0) {
            err_abort((char *) "Fehler beim Verbindungsaufbau!");
        }
        // Hier wird ein neuer Prozess erstellt, um die Verbindung mit dem Client zu handhaben. Der Kindprozess wird die Funktion str_echo aufrufen,
        // um die Anfragen des Clients zu bearbeiten, während der Elternprozess weiterhin auf neue Verbindungsanfragen wartet.
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

    char buffer[2048];
    char path[512], line[2048], fullpath[1024], response_header[1024];
    int n;
    // Hier wird die Anfrage des Clients gelesen und in den Buffer geschrieben. Es wird darauf geachtet, dass der Buffer nicht überläuft.
    n = read(sockfd, buffer, sizeof(buffer) - 1);
    if (n <= 0) return;
    buffer[n] = '\0';

    printf("\n--- NEUE ANFRAGE VON BROWSER ---\n%s\n-------------------------------\n", buffer);

    // Es wird nach einer GET-Anfrage gesucht und der Pfad extrahiert. Wenn die Anfrage nicht im richtigen Format ist, wird die Funktion verlassen.
    if (sscanf(buffer, "GET %s", path) != 1) return;

    // Hier wird überprüft, ob der Pfad mit einem '/' endet. Wenn ja, wird das '/' entfernt, um später die korrekte Dateipfad zu erstellen.
    int path_len = strlen(path);
    if (path_len > 1 && path[path_len - 1] == '/') {
        path[path_len - 1] = '\0';
    }

    // Hier wird der vollständige Pfad zur angeforderten Ressource erstellt, indem der Dokumentenstamm (docroot) mit dem angeforderten Pfad kombiniert wird.
    // Es wird darauf geachtet, dass kein doppeltes '/' entsteht.
    char *p = path;
    if (p[0] == '/') p++;
    int root_len = strlen(docroot);
    if (docroot[root_len - 1] == '/') {
        snprintf(fullpath, sizeof(fullpath), "%s%s", docroot, p);
    } else {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", docroot, p);
    }

    // Hier wird überprüft, ob die angeforderte Ressource existiert und ob es sich um ein Verzeichnis oder eine Datei handelt.
    // Wenn die Ressource nicht existiert, wird eine 404-Fehlerantwort gesendet.
    struct stat path_stat;
    if (stat(fullpath, &path_stat) < 0) {
        perror("stat-Fehler");
        const char *not_found = "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
        write(sockfd, not_found, strlen(not_found));
        return;
    }

    // Wenn die Ressource ein Verzeichnis ist, wird eine HTML-Seite mit einer Auflistung der Dateien und Unterverzeichnisse erstellt.
    // Es werden nur Verzeichnisse und Dateien mit den Endungen .html, .jpg und .png angezeigt.
    if (S_ISDIR(path_stat.st_mode)) {
        DIR *d = opendir(fullpath);
        if (d) {
            write(sockfd, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n", 44);
            write(sockfd, "<html><body><h1>Index von Verzeichnis</h1><ul>\n", 47);

            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                struct stat entry_stat;
                char entry_fullpath[2048];
                snprintf(entry_fullpath, sizeof(entry_fullpath), "%s/%s", fullpath, entry->d_name);
                stat(entry_fullpath, &entry_stat);

                if (S_ISDIR(entry_stat.st_mode) || strstr(entry->d_name, ".html") ||
                    strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".png")) {

                    snprintf(line,sizeof(line) ,"<li><a href=\"%s/%s\">%s</a></li>\n",
                            strcmp(path, "/") == 0 ? "" : path, entry->d_name, entry->d_name);
                    write(sockfd, line, strlen(line));
                }
            }
            write(sockfd, "</ul></body></html>", 19);
            closedir(d);
            return;
        }
    }
    
    FILE *file = fopen(fullpath, "rb");
    if (file == NULL) return;

    const char *mime_type = "text/plain";
    if (strstr(fullpath, ".html") || strstr(fullpath, ".htm")) mime_type = "text/html";
    else if (strstr(fullpath, ".jpg") || strstr(fullpath, ".jpeg")) mime_type = "image/jpeg";
    else if (strstr(fullpath, ".png")) mime_type = "image/png";

    sprintf(response_header, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", mime_type);
    write(sockfd, response_header, strlen(response_header));

    int bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        write(sockfd, buffer, bytes_read);
    }
    fclose(file);
}