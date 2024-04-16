/*Subiectul proiectului: monitorizarea modificarilor aparute in directoare de a lungul timpului.
Saptamana 1: Utilizatorul poate specifica directorul pe care doreste sa il monitorizeze in linia de comanda, ca prim argument. 
Se vor urmari atat modificarile aparute la nivelul directorului, cat si la nivelul subarborelui. Se va parcurge directorul si 
intregul sau subrbore si se face un SNAPSHOT. Acest snapshot poate fi un fisier, care contine toate datele pe care le furnizeaza directorul
si subarborele si care sunt considerate necesare.
Saptamana 2: comparam snapshot-ul anterior cu cel curent si primim mai multe directoare ca argumente
Se actualizeaza functionalitatea prog a.i sa primeasca mai multe arg, dar nu mai mult de 10, niciun arg nu se repeta.Prog proceseaza doar dir si ignora alte tipuri de arg, in cazul in care se inregistreaza modif comparam cel curent cu cel anterior, il inlocuiesti pe cel anterior cu cel curent sau il stergem pe cel anterior ca sa puteam compara snapshot-ul urmator cu ultima modificare.Functionalitatea codului va fi extinsa si va primi un arg suplimentar care primeste dir de iesire care primeste toate snapshot-urile
gcc -Wall -o exec prog.c
./exec arg1 arg2 -o iesire
./exec -o output input1 input2
*/
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define MAX_DIRS 10 // Numărul maxim de directoare acceptate în linia de comandă

// Structura pentru stocarea informatiilor despre un fisier sau director
typedef struct Info {
    char name[256];
    ino_t inode; // inode  
    off_t size; // dimensiune totala in bytes 
    time_t modified_time;
} INFO;

// Functie pentru a crea un snapshot si a scrie informatiile intr-un fisier care se creeaza la momentul executiei
void createSnapshot(const char *directory, const char *snapshot) {
    struct stat buff;
    INFO dir_info;

    // Obține informații despre directorul însuși
    if (lstat(directory, &buff) == -1) {
        printf("Eroare la obtinerea informatiilor despre director.\n");
        exit(EXIT_FAILURE);
    }

    // Introdu informațiile despre director în structură
    strncpy(dir_info.name, directory, sizeof(dir_info.name));
    dir_info.inode = buff.st_ino;
    dir_info.size = buff.st_size;
    dir_info.modified_time = buff.st_mtime;

    // Deschide fișierul de snapshot
    int snaps = open(snapshot, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP | S_IWOTH);
    if (snaps == -1) {
        printf("Nu am putut crea fișierul de snapshot.\n");
        exit(EXIT_FAILURE);
    }

    // Scrie informațiile despre director în fișierul de snapshot
    char dir_buffer[1024];
    int dir_len = snprintf(dir_buffer, sizeof(dir_buffer), "Nume director: %s Inode: %lu Dimensiune: %lld Modificat: %ld\n",
                           dir_info.name, (unsigned long)dir_info.inode, (long long)dir_info.size, (long)dir_info.modified_time);
    int dir_bytes_written = write(snaps, dir_buffer, dir_len);
    if (dir_bytes_written == -1 || dir_bytes_written != dir_len) {
        printf("Eroare la scrierea informatiilor despre director în fișierul de snapshot.\n");
        close(snaps);
        exit(EXIT_FAILURE);
    }

    // Deschide directorul pentru a citi conținutul său
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        printf("Nu am putut deschide directorul.\n");
        close(snaps);
        exit(EXIT_FAILURE);
    }

    // Parcurge fiecare intrare în director
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignoră "." și ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construiește calea completă pentru fiecare fișier / director
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);

        // Obține informații despre fișier / director
        if (lstat(path, &buff) == -1) {
            printf("Eroare la obtinerea informatiilor despre %s.\n", path);
            closedir(dir);
            close(snaps);
            exit(EXIT_FAILURE);
        }

        // Introdu informațiile despre fișier / director în structură
        strncpy(dir_info.name, entry->d_name, sizeof(dir_info.name));
        dir_info.inode = buff.st_ino;
        dir_info.size = buff.st_size;
        dir_info.modified_time = buff.st_mtime;

        // Scrie informațiile despre fișier / director în fișierul de snapshot
        char entry_buffer[1024];
        int entry_len = snprintf(entry_buffer, sizeof(entry_buffer), "Nume: %s Inode: %lu Dimensiune: %lld Modificat: %ld\n",
                                 dir_info.name, (unsigned long)dir_info.inode, (long long)dir_info.size, (long)dir_info.modified_time);
        int entry_bytes_written = write(snaps, entry_buffer, entry_len);
        if (entry_bytes_written == -1 || entry_bytes_written != entry_len) {
            printf("Eroare la scrierea informatiilor despre %s în fișierul de snapshot.\n", path);
            closedir(dir);
            close(snaps);
            exit(EXIT_FAILURE);
        }
        
        if (S_ISDIR(buff.st_mode)) {
            createSnapshot(path, snapshot);
        }
    }

    // Închide directorul și fișierul de snapshot
    closedir(dir);
    close(snaps);
}

int main(int argc, char *argv[]) {
    char output_dir[256] = ""; // Directorul de ieșire pentru snapshot-uri
    char *dirs[MAX_DIRS]; // Vector pentru a stoca directoarele primite în linia de comandă
    int num_dirs = 0; // Numărul de directoare primite în linia de comandă

    // Verificarea existenței argumentelor în linia de comandă
    if (argc < 3) {
        printf("Argumente insuficiente. Utilizare: %s -o director_iesire dir1 dir2 ... dirN\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parsarea opțiunii de director de ieșire
    if (strcmp(argv[1], "-o") != 0) {
        printf("Prima opțiune trebuie să fie '-o'.\n");
        exit(EXIT_FAILURE);
    }

    strncpy(output_dir, argv[2], sizeof(output_dir) - 1);

    // Parsarea directoarelor din argumentele de linie de comandă
    for (int i = 3; i < argc && num_dirs < MAX_DIRS; i++) {
        dirs[num_dirs++] = argv[i];
    }

    // Verificarea existenței cel puțin unui director în linia de comandă
    if (num_dirs == 0) {
        printf("Specificati cel putin un director.\n");
        exit(EXIT_FAILURE);
    }

    // Crearea snapshot-urilor pentru fiecare director
    for (int i = 0; i < num_dirs; i++) {
        printf("Creare snapshot pentru directorul: %s\n", dirs[i]);
        char snapshot_path[512];
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/snapshot_%d.txt", output_dir, i + 1);
        createSnapshot(dirs[i], snapshot_path);
        printf("Snapshot-ul pentru directorul %s a fost creat cu succes.\n", dirs[i]);
    }

    return 0;
}





