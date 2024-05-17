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
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_DIRS 10 // Numarul maxim de directoare acceptate in linia de comanda

char cale[1024];

// Structura pentru stocarea informatiilor despre un fisier sau director
typedef struct Info {
    char name[256]; // numele directorului sau fisierului
    ino_t inode; // inode  
    off_t size; // dimensiune totala in bytes 
    time_t modified_time; // data si ora ultimei modificari
} INFO;

int arePermisiuni(const char *cale) {
    struct stat statbuf;

    if (stat(cale, &statbuf) != 0) {
        perror("Eroare la accesarea fisierului");
        return -1; // Nu se poate accesa informatia
    }

    // Verificam permisiunile de citire, scriere si executie pentru utilizator, grup si altii
    if (!(statbuf.st_mode & (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH))) {
        printf("%d\n", statbuf.st_mode);
        return 1; // Nu are nicio permisiune
    } else {
        return 0; // Are permisiuni
    }
}

void mutaInIzolare(const char *cale, const char *isolated_dir) {
    char caleNoua[1024];
    char *nume_fisier = basename(strdup(cale)); // Extrage numele fisierului din cale

    // Construieste calea noua in directorul de izolare
    snprintf(caleNoua, sizeof(caleNoua), "./%s/%s", isolated_dir, nume_fisier);
    
    // Muta fisierul in directorul de izolare
    if (rename(cale, caleNoua) != 0) {
        perror("Eroare la mutarea fisierului in izolare");
        exit(EXIT_FAILURE);
    } else {
        printf("Fișierul %s a fost mutat cu succes in izolare la %s.\n", cale, caleNoua);
    }
}

void malitios(const char *cale, const char *isolated_dir) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Eroare la crearea pipe-ului");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("Eroare la fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Procesul copil care va executa scriptul
        close(pipefd[0]); // Inchide capatul de citire al pipe-ului
        dup2(pipefd[1], STDOUT_FILENO); // Redirectioneaza stdout catre pipe
        dup2(pipefd[1], STDERR_FILENO); // Redirectioneaza stderr catre pipe
        close(pipefd[1]); // Inchide capatul de scriere al pipe-ului

        execl("./verifica.sh", "./verifica.sh", cale, NULL);
        perror("Eroare la exec");
        exit(EXIT_FAILURE);  // Terminam procesul copil daca execl esueaza
    }

    close(pipefd[1]); // Inchide capatul de scriere al pipe-ului in parinte

    // Procesul parinte poate astepta terminarea copilului daca este necesar
    int status;
    waitpid(pid, &status, 0); // Asteapta finalizarea procesului copil

    char buffer[256];
    ssize_t bytesRead;
    int is_dangerous = 0;

    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0'; // Null-terminate the string
        printf("Output from script: %s\n", buffer); // Process the output if necessary
        if (strstr(buffer, "PERICOL") != NULL) {
            is_dangerous = 1;
        }
    }
    close(pipefd[0]);

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        printf("Scriptul s-a terminat cu codul de iesire: %d\n", exit_code);
        if (is_dangerous) {
            // Scriptul a indicat ca fișierul este periculos
            mutaInIzolare(cale, isolated_dir);
        }
    } else {
        printf("Procesul copil nu s-a terminat normal.\n");
    }
}

void createSnapshot(const char *directory, const char *snapshot) {
    struct stat buff; 
    INFO dir_info;

    // Obtine informatii despre directorul insusi
    if (lstat(directory, &buff) == -1) {
        printf("Eroare la obtinerea informatiilor despre director.\n");
        exit(EXIT_FAILURE);
    }

    // Deschide fisierul de snapshot
    int snaps = open(snapshot, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP | S_IWOTH);
    if (snaps == -1) {
        printf("Nu am putut crea fisierul de snapshot.\n");
        exit(EXIT_FAILURE);
    }

    // Introducem informatiile despre director in structura
    strncpy(dir_info.name, directory, sizeof(dir_info.name));
    dir_info.inode = buff.st_ino;
    dir_info.size = buff.st_size;
    dir_info.modified_time = buff.st_mtime;
    const char *modified = ctime(&dir_info.modified_time);

    strncpy(cale, dir_info.name, sizeof(cale) - 1);
    cale[sizeof(cale) - 1] = '\0';

    printf("%s\n", cale);

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

    close(snaps);
}

void populateSnapshot(const char *directory, const char *snapshot, const char *cale, const char *isolated_dir) {
    struct stat buff;
    INFO dir_info;

    int snaps = open(snapshot, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP | S_IWOTH);
    if (snaps == -1) {
        printf("Nu am putut deschide fisierul de snapshot.\n");
        exit(EXIT_FAILURE);
    }

    // Deschide directorul pentru a citi continutul sau
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        printf("Nu am putut deschide directorul.\n");
        close(snaps);
        exit(EXIT_FAILURE);
    }

    // Parcurge fiecare intrare in director
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignora "." si ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construieste calea completa pentru fiecare fisier / director
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);

        // Obtine informatii despre fisier / director
        if (lstat(path, &buff) == -1) {
            printf("Eroare la obtinerea informatiilor despre %s.\n", path);
            closedir(dir);
            close(snaps);
            exit(EXIT_FAILURE);
        }

        // Introducem informatiile despre fisier / director in structura
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

        // Daca este un director, parcurge recursiv
        if (S_ISDIR(buff.st_mode)) {
            populateSnapshot(path, snapshot, cale, isolated_dir);
        } else if (S_ISREG(buff.st_mode)) {
            printf("%s\n", path);
            if (arePermisiuni(path) == 1) {
            malitios(path, isolated_dir); // Verifica si executa analiza daca fisierul nu are permisiuni
            }
        }
    }

    // Inchide directorul si fisierul de snapshot
    closedir(dir);
    close(snaps);
}

int compareSnapshots(const char *snapshot1, const char *snapshot2) {
    FILE *file1 = fopen(snapshot1, "r");
    FILE *file2 = fopen(snapshot2, "r");

    if (!file1 || !file2) {
        printf("Nu am putut deschide fisierele pentru comparare.\n");
        return -1;
    }

    char line1[1024], line2[1024];
    int differences = 0;

    while (fgets(line1, sizeof(line1), file1) && fgets(line2, sizeof(line2), file2)) {
        if (strcmp(line1, line2) != 0) {
            differences++;
            printf("Diferenta gasita:\n%s\n%s\n", line1, line2);
        }
    }

    // Verifica daca unul dintre fisiere are linii suplimentare
    while (fgets(line1, sizeof(line1), file1)) {
        differences++;
        printf("Linie suplimentara in snapshot anterior:\n%s\n", line1);
    }

    while (fgets(line2, sizeof(line2), file2)) {
        differences++;
        printf("Linie suplimentara in snapshot curent:\n%s\n", line2);
    }

    fclose(file1);
    fclose(file2);

    return differences;
}

void updateSnapshot(const char *snapshot1, const char *snapshot2) {
    FILE *src = fopen(snapshot2, "r");
    FILE *dest = fopen(snapshot1, "w");

    if (!src || !dest) {
        printf("Nu am putut deschide fisierele pentru actualizare.\n");
        if (src) fclose(src);
        if (dest) fclose(dest);
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), src)) {
        fputs(buffer, dest);
    }

    fclose(src);
    fclose(dest);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s -o output_dir dir1 [dir2 ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char output_dir[256] = "";
    char isolated_dir[256] = "";
    char *dirs[MAX_DIRS];
    int num_dirs = 0;
    int i;

    // Gaseste directorul de output
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

    // Gaseste directorul de izolare

    for (i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            strncpy(isolated_dir, argv[i + 1], sizeof(isolated_dir) - 1);
            break;
        }
    }

    printf("%s\n", isolated_dir);

    if (strlen(isolated_dir) == 0) {
        fprintf(stderr, "Directorul de izolare trebuie specificat cu optiunea -s.\n");
        exit(EXIT_FAILURE);
    }

    // Parcurge sarind peste -o, -s si argumentele lor

    for (int j = 1; j < argc; j++) {
        if ((strcmp(argv[j], "-o") == 0) || (strcmp(argv[j], "-s") == 0)) {
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

    // Creare procese copil
    for (i = 0; i < num_dirs; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Eroare la fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Codul procesului copil
            printf("Creare snapshot pt directorul: %s\n", dirs[i]);
            char snapshot_path[512], previous_snapshot_path[512];
            snprintf(snapshot_path, sizeof(snapshot_path), "%s/snapshot_%d.txt", output_dir, i);
            snprintf(previous_snapshot_path, sizeof(previous_snapshot_path), "%s/previous_snapshot_%d.txt", output_dir, i);

            // Creare snapshot curent
            createSnapshot(dirs[i], snapshot_path);
            populateSnapshot(dirs[i], snapshot_path, dirs[i], isolated_dir);
            printf("Snapshotul pentru directorul %d a fost creat cu succes.\n", i + 1);

            // Comparare snapshot curent cu cel anterior
            if (access(previous_snapshot_path, F_OK) == 0) {
                int differences = compareSnapshots(previous_snapshot_path, snapshot_path);
                if (differences > 0) {
                    printf("Au fost gasite %d diferente intre snapshot-uri. Actualizare snapshot anterior.\n", differences);
                    updateSnapshot(previous_snapshot_path, snapshot_path);
                } else {
                    printf("Nu au fost gasite diferente intre snapshot-uri.\n");
                }
            } else {
                // Dacă nu există un snapshot anterior, creează unul
                printf("Nu a fost gasit un snapshot anterior. Creare unul nou.\n");
                updateSnapshot(previous_snapshot_path, snapshot_path);
            }

            exit(EXIT_SUCCESS);
        }
    }

    // Codul procesului parinte
    int status;
    pid_t child_pid;
    while ((child_pid = wait(&status)) > 0) {
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("Procesul copil cu PID-ul %d s-a terminat cu codul %d\n", child_pid, exit_code);
        } else {
            printf("Procesul copil cu PID-ul %d nu s-a terminat normal.\n", child_pid);
        }
    }

    return 0;
}




