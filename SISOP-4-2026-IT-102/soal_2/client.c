#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define ALAMAT_SERVER "127.0.0.1"
#define PORT_SERVER 9000
#define UKURAN_BUF 4096

int main(void)
{
    /* Buat socket TCP */
    int soket = socket(AF_INET, SOCK_STREAM, 0);
    if (soket < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in info_server;
    memset(&info_server, 0, sizeof(info_server));
    info_server.sin_family = AF_INET;
    info_server.sin_port = htons(PORT_SERVER);
    info_server.sin_addr.s_addr = inet_addr(ALAMAT_SERVER);

    /* Hubungkan ke server */
    if (connect(soket, (struct sockaddr *)&info_server, sizeof(info_server)) < 0)
    {
        perror("connect");
        close(soket);
        return 1;
    }

    printf("Terhubung ke server %s:%d\n", ALAMAT_SERVER, PORT_SERVER);
    printf("Ketik perintah (ketik 'exit' untuk keluar):\n");

    char perintah[UKURAN_BUF];
    char balasan[UKURAN_BUF];

    while (1)
    {
        printf("> ");
        fflush(stdout);

        /* Baca input dari pengguna */
        if (!fgets(perintah, sizeof(perintah), stdin))
            break;

        /* Hapus newline */
        perintah[strcspn(perintah, "\n")] = '\0';

        if (strcmp(perintah, "exit") == 0)
            break;

        /* Kirim ke server dengan newline sebagai penanda akhir */
        strcat(perintah, "\n");
        if (send(soket, perintah, strlen(perintah), 0) < 0)
        {
            perror("send");
            break;
        }

        /* Terima balasan dari server */
        memset(balasan, 0, sizeof(balasan));
        int diterima = recv(soket, balasan, sizeof(balasan) - 1, 0);
        if (diterima <= 0)
        {
            printf("Koneksi ke server terputus.\n");
            break;
        }
        balasan[diterima] = '\0';
        printf("%s", balasan);
    }

    close(soket);
    return 0;
}
