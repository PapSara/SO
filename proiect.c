#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#define MAX_DIRS 10 // Numarul maxim de directoare acceptate in linia de comanda

char cale[1024];

// Structura pentru stocarea informatiilor despre un fisier sau director
typedef struct Info {
    char name[256]; // numele directorului sau fisierului
    ino_t inode; // inode  
    off_t size; // dimensiune totala in bytes 
    time_t modified_time; // data si ora ultimei modificari
} INFO;

void createSnapshot(const char *directory, const char *snapshot) {
    struct stat buff; 
    INFO dir_info;

    // Obtine informatii despre directorul insusi
    if (lstat(directory, &buff) == -1) {
        printf("Eroare la obtinerea informatiilor despre director.\n");
        exit(EXIT_FAILURE);
    }

    // Deschide fisierul de snapshot
    int snaps = open(snapshot, O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP | S_IWOTH);
    if (snaps == -1) {
        printf("Nu am putut crea fișierul de snapshot.\n");
        exit(EXIT_FAILURE);
    }

    // Introdu informatiile despre director in structura
    strncpy(dir_info.name, directory, sizeof(dir_info.name));
    dir_info.inode = buff.st_ino;
    dir_info.size = buff.st_size;
    dir_info.modified_time = buff.st_mtime;
    const char *modified = ctime(&dir_info.modified_time);

    strncpy(cale, dir_info.name, sizeof(cale) - 1);
    cale[sizeof(cale) - 1] = '\0';

    // Scrie informatiile despre director in fisierul de snapshot
    char dir_buffer[2048];
    int dir_len = snprintf(dir_buffer, sizeof(dir_buffer), "Nume director: %s\nInode: %lu\nDimensiune: %lld\nModificat: %s\n\n",
                           dir_info.name, (unsigned long)dir_info.inode, (long long)dir_info.size, modified);
    int dir_bytes_written = write(snaps, dir_buffer, dir_len);
    if (dir_bytes_written == -1 || dir_bytes_written != dir_len) {
        printf("Eroare la scrierea informatiilor despre director in fisierul de snapshot.\n");
        close(snaps);
        exit(EXIT_FAILURE);
    }
}

// Functie pentru a crea un snapshot si a scrie informatiile intr-un fisier care se creeaza la momentul executiei
void populateSnapshot(const char *directory, const char *snapshot, const char *cale) {
    struct stat buff;
    INFO dir_info;

    int snaps = open(snapshot, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP | S_IWOTH);
    if (snaps == -1) {
        printf("Nu am putut deschide fișierul de snapshot.\n");
        exit(EXIT_FAILURE);
    }

    // Deschide directorul pentru a citi continutul sau
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        printf("Nu am putut deschide directorul.\n");
        close(snaps);
        exit(EXIT_FAILURE);
    }

    // Parcurge fiecare intrare în director
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignoră "." si ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construieste calea completa pentru fiecare fisier / director
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);

        // Obține informatii despre fisier / director
        if (lstat(path, &buff) == -1) {
            printf("Eroare la obtinerea informatiilor despre %s.\n", path);
            closedir(dir);
            close(snaps);
            exit(EXIT_FAILURE);
        }

        // Introdu informatiile despre fisier / director in structura
        strncpy(dir_info.name, entry->d_name, sizeof(dir_info.name));
        dir_info.inode = buff.st_ino;
        dir_info.size = buff.st_size;
        dir_info.modified_time = buff.st_mtime;
        const char *modif = ctime(&dir_info.modified_time);

        // Scrie informatiile despre fisier / director in fisierul de snapshot
        char entry_buffer[1024];
        int entry_len = snprintf(entry_buffer, sizeof(entry_buffer), "Cale: %s\nNume: %s\nInode: %lu\nDimensiune: %lld\nModificat: %s\n\n", path,
                                 dir_info.name, (unsigned long)dir_info.inode, (long long)dir_info.size, modif);
        int entry_bytes_written = write(snaps, entry_buffer, entry_len);
        if (entry_bytes_written == -1 || entry_bytes_written != entry_len) {
            printf("Eroare la scrierea informatiilor despre %s in fisierul de snapshot.\n", path);
            closedir(dir);
            close(snaps);
            exit(EXIT_FAILURE);
        }

        // Dacă este un director, procesează-l recursiv
        if (S_ISDIR(buff.st_mode)) {
            populateSnapshot(path, snapshot, cale);
        }
    }

    // inchide directorul si fisierul de snapshot
    closedir(dir);
    close(snaps);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -o output_dir dir1 [dir2 ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char output_dir[256] = "";
    char *dirs[MAX_DIRS];
    int num_dirs = 0;
    int i;

    // gaseste directorul de output
    for (i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            strncpy(output_dir, argv[i + 1], sizeof(output_dir) - 1);
            break;
        }
    }
    
    if (strlen(output_dir) == 0) {
        fprintf(stderr, "Directorul de output trebuie specificat cu optiunea -o\n");
        exit(EXIT_FAILURE);
    }

    // parcurge sarind peste -o si argumentul sau
    for (int j = 1; j < argc; j++) {
        if (strcmp(argv[j], "-o") == 0) {
            j++; 
            continue;
        }
        if (num_dirs >= MAX_DIRS) {
            fprintf(stderr, "Prea multe directoare, maximul este %d\n", MAX_DIRS);
            exit(EXIT_FAILURE);
        }
        dirs[num_dirs++] = argv[j];
    }

    if (num_dirs == 0) {
        fprintf(stderr, "Nu sunt directoare.\n");
        exit(EXIT_FAILURE);
    }

    // Proceed with snapshot creation
    for (i = 0; i < num_dirs; i++) {
        printf("Creare snapshot pt directorul: %s\n", dirs[i]);
        char snapshot_path[512];
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/snapshot_%d.txt", output_dir, i);
        createSnapshot(dirs[i], snapshot_path);
        printf("Snapshot creat cu succes pentru: %s\n", dirs[i]);
        populateSnapshot(dirs[i], snapshot_path, dirs[i]);  // Aici trebuie să trecem calea inițială
    }

    return 0;
}




