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

// Structura pentru stocarea informatiilor despre un fisier
typedef struct Info {
    char name[256];
  ino_t inode; // inode  
  off_t size; // dimensiune totala in bytes 
    time_t modified_time;
} INFO;

// Functie pentru a crea un snapshot si a scrie informatiile intr-un fisier care se creeaza la momentul executiei
void createSnapshot(const char *directory, const char *snapshot) {
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        printf("Nu am putut deschide directorul.\n");
        exit(EXIT_FAILURE);
    }

    printf("Director deschis cu succes: %s\n", directory);

    
    int snaps = open(snapshot, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP | S_IWOTH);
    if (snaps == -1) {
        printf("Nu am putut crea fișierul de snapshot.\n");
        closedir(dir);
        exit(EXIT_FAILURE);
    }

    printf("Fișierul de snapshot creat cu succes: %s\n", snapshot);
    
    struct dirent *in;
    struct stat buff;
    INFO file_info;

    // Parcurge fiecare intrare in director
    while ((in = readdir(dir)) != NULL) {
       // Ignora "." și ".."
        if (strcmp(in->d_name, ".") == 0 || strcmp(in->d_name, "..") == 0) {
            continue;
        }

        // Construieste calea completa pentru fiecare fisier / director
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, in->d_name);
	printf("Cale: %s\n", path);
 
        if (lstat(path, &buff) == -1) {
            printf("Eroare la obtinerea informatiilor.\n");
            exit(EXIT_FAILURE);
        }

        // Introducerea informatiilor in structura
        strncpy(file_info.name, in->d_name, sizeof(file_info.name));
        file_info.inode = buff.st_ino;
        file_info.size = buff.st_size;
        file_info.modified_time = buff.st_mtime;
        
        // Scrierea informatiilor in fisierul de snapshot
        char buffer[1024];
        int len = snprintf(buffer, sizeof(buffer), "Nume: %s Inode: %lu Dimensiune: %lld Modificat: %ld\n", file_info.name, (unsigned long)file_info.inode, (long long)file_info.size, (long)file_info.modified_time);

	int bytes_written = write(snaps, buffer, len);
if (bytes_written == -1) {
    printf("Eroare la scrierea in fisierul de snapshot.\n");
    close(snaps);
    closedir(dir);
    exit(EXIT_FAILURE);
} else if (bytes_written != len) {
    printf("Eroare la scrierea in fisierul de snapshot: Numar incorect de octeti scrise.\n");
    close(snaps);
    closedir(dir);
    exit(EXIT_FAILURE);
}

        // Verifica daca este director si apeleaza recursiv functia
        if (S_ISDIR(buff.st_mode)) {
            createSnapshot(path, snapshot);
        }
    }

    close(snaps);
    closedir(dir);
}

int main(int argc, char *argv[]) {
    // Verifica daca calea directorului a fost data in linia de comanda si daca este singurul argument
    if (argc != 2) {
        printf("Argumente insuficiente sau prea multe\n");
        exit(EXIT_FAILURE);
    }

    // Verifica daca argumentul este un director
    struct stat statbuf;
    if (lstat(argv[1], &statbuf) == -1 || (S_ISDIR(statbuf.st_mode) == 0)) {
        printf("Argumentul trebuie sa fie director.\n");
        exit(EXIT_FAILURE);
    }

    // Creeaza snapshot-ul
    createSnapshot(argv[1], "snapshot.txt");

    printf("Snapshot-ul a fost creat cu succes.\n");
    return 0;
}
