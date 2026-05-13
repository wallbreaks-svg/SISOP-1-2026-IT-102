#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

/* Menyimpan path absolut ke direktori sumber */
static char dir_sumber[PATH_MAX];

/* Nama file yang hanya ada di mount, tidak di sumber */
#define NAMA_VIRTUAL "tujuan.txt"
#define JUMLAH_FILE 7

/* ------------------------------------------------------------------
 * Fungsi bantu: rangkai path sumber + path relatif dari FUSE
 * ------------------------------------------------------------------ */
static void rangkai_path(char *hasil, size_t ukuran, const char *rel)
{
    snprintf(hasil, ukuran, "%s%s", dir_sumber, rel);
}

/* ------------------------------------------------------------------
 * Fungsi bantu: baca fragmen KOORD dari setiap file, gabungkan
 * hasilnya menjadi isi tujuan.txt yang dibuat saat diminta (on-the-fly)
 * ------------------------------------------------------------------ */
static char *buat_isi_tujuan(size_t *panjang)
{
    char gabungan[4096];
    memset(gabungan, 0, sizeof(gabungan));
    int pertama = 1;

    for (int nomor = 1; nomor <= JUMLAH_FILE; nomor++)
    {
        char lokasi[PATH_MAX];
        snprintf(lokasi, sizeof(lokasi), "%s/%d.txt", dir_sumber, nomor);

        FILE *fp = fopen(lokasi, "r");
        if (!fp)
            continue;

        char baris[1024];
        while (fgets(baris, sizeof(baris), fp))
        {
            /* Cari baris yang dimulai dengan "KOORD:" */
            if (strncmp(baris, "KOORD:", 6) != 0)
                continue;

            char *penggalan = baris + 6;

            /* Buang spasi/tab di awal */
            while (*penggalan == ' ' || *penggalan == '\t')
                penggalan++;

            /* Buang karakter newline di akhir */
            size_t pjg = strlen(penggalan);
            while (pjg > 0 &&
                   (penggalan[pjg - 1] == '\n' || penggalan[pjg - 1] == '\r'))
                penggalan[--pjg] = '\0';

            /* Tambahkan pemisah spasi antar fragmen */
            if (!pertama)
                strncat(gabungan, " ", sizeof(gabungan) - strlen(gabungan) - 1);

            strncat(gabungan, penggalan, sizeof(gabungan) - strlen(gabungan) - 1);
            pertama = 0;
            break;
        }
        fclose(fp);
    }

    /* Bentuk baris akhir sesuai format yang diminta soal */
    char *keluaran = malloc(4096);
    if (!keluaran)
        return NULL;

    int ditulis = snprintf(keluaran, 4096, "Tujuan Mas Amba: %s\n", gabungan);
    *panjang = (size_t)ditulis;
    return keluaran;
}

/* ------------------------------------------------------------------
 * Callback getattr: kembalikan informasi stat sebuah path
 * ------------------------------------------------------------------ */
static int cb_getattr(const char *path, struct stat *info)
{
    memset(info, 0, sizeof(struct stat));

    /* Root mount directory */
    if (strcmp(path, "/") == 0)
    {
        info->st_mode = S_IFDIR | 0755;
        info->st_nlink = 2;
        return 0;
    }

    /* File virtual: stat-nya dihitung dari panjang konten yang dibuat */
    if (strcmp(path, "/" NAMA_VIRTUAL) == 0)
    {
        size_t pjg = 0;
        char *isi = buat_isi_tujuan(&pjg);
        if (!isi)
            return -ENOMEM;
        free(isi);

        info->st_mode = S_IFREG | 0444;
        info->st_nlink = 1;
        info->st_size = (off_t)pjg;
        return 0;
    }

    /* File biasa: teruskan ke direktori sumber */
    char penuh[PATH_MAX];
    rangkai_path(penuh, sizeof(penuh), path);

    if (lstat(penuh, info) == -1)
        return -errno;

    return 0;
}

/* ------------------------------------------------------------------
 * Callback readdir: tampilkan daftar isi direktori
 * ------------------------------------------------------------------ */
static int cb_readdir(const char *path, void *buf, fuse_fill_dir_t isi_buf,
                      off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    isi_buf(buf, ".", NULL, 0);
    isi_buf(buf, "..", NULL, 0);
    isi_buf(buf, NAMA_VIRTUAL, NULL, 0); /* tampilkan file virtual */

    /* Tampilkan semua file yang ada di direktori sumber */
    DIR *dir = opendir(dir_sumber);
    if (!dir)
        return -errno;

    struct dirent *entri;
    while ((entri = readdir(dir)) != NULL)
    {
        if (entri->d_name[0] == '.')
            continue;
        isi_buf(buf, entri->d_name, NULL, 0);
    }
    closedir(dir);

    return 0;
}

/* ------------------------------------------------------------------
 * Callback open: buka file untuk dibaca
 * ------------------------------------------------------------------ */
static int cb_open(const char *path, struct fuse_file_info *fi)
{
    /* File virtual hanya boleh dibuka dengan mode read-only */
    if (strcmp(path, "/" NAMA_VIRTUAL) == 0)
    {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;
        return 0;
    }

    /* Teruskan open ke direktori sumber */
    char penuh[PATH_MAX];
    rangkai_path(penuh, sizeof(penuh), path);

    int fd = open(penuh, fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = (uint64_t)fd;
    return 0;
}

/* ------------------------------------------------------------------
 * Callback read: baca isi file
 * ------------------------------------------------------------------ */
static int cb_read(const char *path, char *buf, size_t ukuran,
                   off_t offset, struct fuse_file_info *fi)
{
    /* File virtual: hasilkan konten saat diminta */
    if (strcmp(path, "/" NAMA_VIRTUAL) == 0)
    {
        size_t pjg = 0;
        char *isi = buat_isi_tujuan(&pjg);
        if (!isi)
            return -ENOMEM;

        int dibaca = 0;
        if ((size_t)offset < pjg)
        {
            size_t tersedia = pjg - (size_t)offset;
            size_t disalin = ukuran < tersedia ? ukuran : tersedia;
            memcpy(buf, isi + offset, disalin);
            dibaca = (int)disalin;
        }
        free(isi);
        return dibaca;
    }

    /* File biasa: teruskan baca ke file descriptor sumber */
    int fd = (int)fi->fh;
    int res = (int)pread(fd, buf, ukuran, offset);
    if (res == -1)
        return -errno;
    return res;
}

/* ------------------------------------------------------------------
 * Callback release: tutup file descriptor setelah selesai
 * ------------------------------------------------------------------ */
static int cb_release(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, "/" NAMA_VIRTUAL) == 0)
        return 0;

    close((int)fi->fh);
    return 0;
}

/* ------------------------------------------------------------------
 * Daftar operasi FUSE yang diimplementasikan
 * ------------------------------------------------------------------ */
static struct fuse_operations daftar_operasi = {
    .getattr = cb_getattr,
    .readdir = cb_readdir,
    .open = cb_open,
    .read = cb_read,
    .release = cb_release,
};

/* ------------------------------------------------------------------
 * Titik masuk program
 * ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Penggunaan: %s <direktori_sumber> <direktori_mount>\n",
                argv[0]);
        return 1;
    }

    /* Simpan path absolut direktori sumber */
    if (realpath(argv[1], dir_sumber) == NULL)
    {
        perror("realpath");
        return 1;
    }

    /*
     * fuse_main hanya butuh: nama_program + mountpoint
     * argv[1] (direktori sumber) sudah kita simpan, tidak diteruskan ke FUSE
     */
    char *argumen_fuse[3];
    argumen_fuse[0] = argv[0]; /* nama program        */
    argumen_fuse[1] = argv[2]; /* direktori mount     */
    argumen_fuse[2] = NULL;

    return fuse_main(2, argumen_fuse, &daftar_operasi, NULL);
}
