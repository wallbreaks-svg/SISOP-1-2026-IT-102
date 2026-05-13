FROM ubuntu:latest

# Install dependensi yang dibutuhkan
RUN apt-get update && apt-get install -y \
    libfuse-dev \
    fuse \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set direktori kerja
WORKDIR /app

# Salin semua file ke dalam image
COPY . .

# Buka port untuk koneksi TCP server
EXPOSE 9000

# Jalankan server saat container dinyalakan
CMD ["./server"]
