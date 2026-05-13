#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <utime.h>

/* ============================================================
 * KONFIGURASI ENKRIPSI
 * ============================================================ */
#define KUNCI_XOR 0x76
#define EKSTENSI_ENC ".enc"
#define PANJANG_ENC 4

/* Path absolut ke encrypted_storage */
static char gudang[PATH_MAX];

/* ============================================================
 * FUNGSI BANTU
 * ============================================================ */

static int ada_ekstensi_enc(const char *nama)
{
    size_t pjg = strlen(nama);
    if (pjg <= PANJANG_ENC)
        return 0;
    return strcmp(nama + pjg - PANJANG_ENC, EKSTENSI_ENC) == 0;
}

static void path_ke_gudang(char *hasil, size_t ukuran, const char *fuse_p)
{
    if (strcmp(fuse_p, "/") == 0)
    {
        snprintf(hasil, ukuran, "%s", gudang);
        return;
    }

    char coba[PATH_MAX];
    snprintf(coba, sizeof(coba), "%s%s", gudang, fuse_p);
    struct stat st;
    if (stat(coba, &st) == 0 && S_ISDIR(st.st_mode))
    {
        snprintf(hasil, ukuran, "%s", coba);
        return;
    }

    if (ada_ekstensi_enc(fuse_p))
    {
        snprintf(hasil, ukuran, "%s%s", gudang, fuse_p);
        return;
    }

    snprintf(hasil, ukuran, "%s%s%s", gudang, fuse_p, EKSTENSI_ENC);
}

static void hapus_ekstensi(char *tujuan, size_t ukuran, const char *nama_enc)
{
    size_t pjg = strlen(nama_enc);
    if (ada_ekstensi_enc(nama_enc))
    {
        size_t baru = pjg - PANJANG_ENC;
        if (baru >= ukuran)
            baru = ukuran - 1;
        strncpy(tujuan, nama_enc, baru);
        tujuan[baru] = '\0';
    }
    else
    {
        strncpy(tujuan, nama_enc, ukuran - 1);
        tujuan[ukuran - 1] = '\0';
    }
}

static void terapkan_xor(char *buf, size_t pjg)
{
    for (size_t i = 0; i < pjg; i++)
        buf[i] ^= KUNCI_XOR;
}

/* ============================================================
 * CALLBACK FUSE
 * ============================================================ */

static int moo_getattr(const char *path, struct stat *info)
{
    memset(info, 0, sizeof(struct stat));
    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);
    if (lstat(ep, info) == -1)
        return -errno;
    return 0;
}

static int moo_access(const char *path, int mask)
{
    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);
    if (access(ep, mask) == -1)
        return -errno;
    return 0;
}

static int moo_readdir(const char *path, void *buf, fuse_fill_dir_t tambah,
                       off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;

    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);

    DIR *dir = opendir(ep);
    if (!dir)
        return -errno;

    tambah(buf, ".", NULL, 0);
    tambah(buf, "..", NULL, 0);

    struct dirent *entri;
    while ((entri = readdir(dir)) != NULL)
    {
        if (entri->d_name[0] == '.')
            continue;
        char nama_tampil[NAME_MAX + 1];
        hapus_ekstensi(nama_tampil, sizeof(nama_tampil), entri->d_name);
        tambah(buf, nama_tampil, NULL, 0);
    }
    closedir(dir);
    return 0;
}

static int moo_mkdir(const char *path, mode_t mode)
{
    char ep[PATH_MAX];
    snprintf(ep, sizeof(ep), "%s%s", gudang, path);
    if (mkdir(ep, mode) == -1)
        return -errno;
    return 0;
}

static int moo_rmdir(const char *path)
{
    char ep[PATH_MAX];
    snprintf(ep, sizeof(ep), "%s%s", gudang, path);
    if (rmdir(ep) == -1)
        return -errno;
    return 0;
}

static int moo_unlink(const char *path)
{
    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);
    if (unlink(ep) == -1)
        return -errno;
    return 0;
}

static int moo_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);
    int fd = open(ep, fi->flags | O_CREAT | O_TRUNC, mode);
    if (fd == -1)
        return -errno;
    fi->fh = (uint64_t)fd;
    return 0;
}

static int moo_open(const char *path, struct fuse_file_info *fi)
{
    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);
    int fd = open(ep, fi->flags);
    if (fd == -1)
        return -errno;
    fi->fh = (uint64_t)fd;
    return 0;
}

static int moo_read(const char *path, char *buf, size_t ukuran,
                    off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    int fd = (int)fi->fh;
    int res = (int)pread(fd, buf, ukuran, offset);
    if (res == -1)
        return -errno;
    terapkan_xor(buf, (size_t)res);
    return res;
}

static int moo_write(const char *path, const char *buf, size_t ukuran,
                     off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    int fd = (int)fi->fh;

    char *terenkripsi = malloc(ukuran);
    if (!terenkripsi)
        return -ENOMEM;
    memcpy(terenkripsi, buf, ukuran);
    terapkan_xor(terenkripsi, ukuran);

    int res = (int)pwrite(fd, terenkripsi, ukuran, offset);
    free(terenkripsi);
    if (res == -1)
        return -errno;
    return res;
}

static int moo_truncate(const char *path, off_t ukuran)
{
    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);
    if (truncate(ep, ukuran) == -1)
        return -errno;
    return 0;
}

static int moo_utimens(const char *path, const struct timespec ts[2])
{
    char ep[PATH_MAX];
    path_ke_gudang(ep, sizeof(ep), path);

    struct timeval tv[2];
    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    if (utimes(ep, tv) == -1)
        return -errno;
    return 0;
}

static int moo_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;
    close((int)fi->fh);
    return 0;
}

/* ============================================================
 * TABEL OPERASI FUSE
 * ============================================================ */
static struct fuse_operations operasi_moo = {
    .getattr = moo_getattr,
    .access = moo_access,
    .readdir = moo_readdir,
    .mkdir = moo_mkdir,
    .rmdir = moo_rmdir,
    .unlink = moo_unlink,
    .create = moo_create,
    .open = moo_open,
    .read = moo_read,
    .write = moo_write,
    .truncate = moo_truncate,
    .utimens = moo_utimens,
    .release = moo_release,
};

/* ============================================================
 * MAIN — fix: simpan gudang SEBELUM fuse_main dipanggil
 * ============================================================ */
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Penggunaan: %s <encrypted_storage> <fuse_mount>\n", argv[0]);
        return 1;
    }

    /* Simpan path absolut encrypted_storage langsung tanpa realpath
     * karena direktori sudah pasti ada */
    snprintf(gudang, sizeof(gudang), "%s", argv[1]);

    /* Susun argumen untuk fuse_main: buang argv[1], pakai argv[2] */
    char *arg_fuse[10];
    int n = 0;
    arg_fuse[n++] = argv[0]; /* nama program  */
    arg_fuse[n++] = argv[2]; /* mount point   */

    /* Teruskan opsi tambahan seperti -o allow_other */
    for (int i = 3; i < argc; i++)
        arg_fuse[n++] = argv[i];

    arg_fuse[n] = NULL;

    return fuse_main(n, arg_fuse, &operasi_moo, NULL);
}
