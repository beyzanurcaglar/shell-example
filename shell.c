/* 
 * File: shell.c
 * Authors: Bahar Kayhan, Beyza Nur Caglar
 * Date: 31 Mart 2024
 * Description: Bu dosya bir shell ornegidir. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MAX_KOMUT_BOYUTU 100
#define LOG_FILE "log.txt"

/* Fonksiyon prototipleri */
void komut_calistir(char *komut);
void komut_kaydet(char *komut);
char *komut_yolunu_bul(char *komut);

int main() {  
    char komut[MAX_KOMUT_BOYUTU];

    while (1) {
        /* Program baslarken sadece "$" yaz */
        if (write(1, "$ ", 2) == -1) {
            perror("ERROR");
            exit(EXIT_FAILURE);
        }

        /* Kullanicidan caliştirilmak istenen komutu al */
        ssize_t byte_oku = read(1, komut, MAX_KOMUT_BOYUTU);
        if (byte_oku == -1) {
            perror("ERROR READING COMMAND");
            continue;
        }

        /* Komutun sonunda bulunan "\n" karakterini kaldir */
        komut[strcspn(komut, "\n")] = '\0';

        /* Komutu calistir */
        komut_calistir(komut);
    }

    return 0;
}

/* Kullanicisinin girdigi komutun calismasini saglayan fonksiyon */
void komut_calistir(char *komut) {
    /* "exit" komutu girildiyse programdan cikis yap */
    if (strcmp(komut, "exit") == 0) {           
        exit(EXIT_SUCCESS);
    }

    pid_t pid;
    int status;

    /* Yeni bir process olustur */
    pid = fork();

    if (pid < 0) {
        perror("FORK FAILED");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { /* Child process */

        /* "./" ile baslaniyorsa manual olarak ekle */
        if (strncmp(komut, ",", 2) != 0) {
            char *temp = malloc(strlen(komut) + 3);
            if (temp == NULL) {
                perror("ERROR");
                exit(EXIT_FAILURE);
            }
            strcpy(temp, "./");
            strcat(temp, komut);
            execlp(temp, komut, NULL);
            free(temp);
        } else {
            execlp(komut, komut, NULL);
        }
                  
        /* "whereis" komutu girilmesi durumu */
	/* whereis komutu icin stackoverflowdan destek alindi */
        if (strstr(komut, "whereis ") == komut) {
            char *yol = komut_yolunu_bul(komut + strlen("whereis "));
            if (yol != NULL) {
                printf("%s\n", yol);
                free(yol);
            } else {
                printf("%s: COMMAND NOT FOUND!!\n", komut);
            }
            exit(EXIT_SUCCESS);
        } 

        /* "cat" komutunun nasil girildigini kontrol et */
        if (strcmp(komut, "cat") == 0) {
            char dosya_adi[MAX_KOMUT_BOYUTU];
            if (scanf("%s", dosya_adi) != 1) {
                perror("ERROR READING INPUT");
                exit(EXIT_FAILURE);
            }
            printf("%s\n", dosya_adi);
            exit(EXIT_SUCCESS);
        } else if (strstr(komut, "cat ") == komut) {	 /* "cat" komutu ile baslaniyorsa ve dosya adi varsa */

            /* "cat" kismini atla */
            char *dosya_adi = komut + 4;        
            int file_fd = open(dosya_adi, O_RDONLY);
            if (file_fd == -1) {
                perror("ERROR OPENING FILE");
                exit(EXIT_FAILURE);
            }

            char buffer[MAX_KOMUT_BOYUTU];
            int byte_oku;
            while ((byte_oku = read(file_fd, buffer, MAX_KOMUT_BOYUTU)) > 0) {
                if (write(1, buffer, byte_oku) == -1) {
                    perror("ERROR WRITING THE STDOUT");
                    exit(EXIT_FAILURE);
                }
            }

            if (byte_oku == -1) {
                perror("ERROR READING FILE");
                exit(EXIT_FAILURE);
            }

            /* Dosyayi kapat */
            close(file_fd);
            exit(EXIT_SUCCESS);
        } else {
            /* Diger komutlari exec ile calistir */
            if (execlp(komut, komut, NULL) == -1) {
                printf("%s: Command not found \n", komut);
                perror("EXEC FAILED");
                exit(EXIT_FAILURE);
            }
        }
    } else { /* Parent process */
        /* Parent process baslamadan önce child process'in bitmesini bekle */
        waitpid(pid, &status, 0);
    }

    /* Sadece calistirilabilinen komutlari log.txt dosyasina yaz */
    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS && strcmp(komut, "exit") != 0) {
        komut_kaydet(komut);
    }
}

/* Komutun PATH icindeki yolunu bul */
char *komut_yolunu_bul(char *komut) {
    char *yol = getenv("PATH");

    if (yol == NULL) {
        perror("ERROR GETTING PATH VARIABLE");
        return NULL;
    }

    char *token = strtok(yol, ":");

    while (token != NULL) {
        char path_buffer[MAX_KOMUT_BOYUTU];
        snprintf(path_buffer, MAX_KOMUT_BOYUTU, "%s/%s", token, komut);

        if (access(path_buffer, F_OK) != -1) {
            char *result = strdup(path_buffer);
            if (result == NULL) {
                perror("ERROR");
            }
            return result;
        }
        token = strtok(NULL, ":");
    }
    return NULL;
}

/* Komutlari log.txt dosyasina kaydeder */
void komut_kaydet(char *komut) {
    int log_fd = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (log_fd == -1) {
        perror("ERROR OPENING LOG FILE");
        return;
    }

    /* Zaman bilgisi al */
    /* Zaman ayarlamasi icin chatgptden yardim alindi */ 
    struct timeval zaman;
    gettimeofday(&zaman, NULL);
    long milisaniye = zaman.tv_usec / 1000;

    /* Zamani dosyaya yaz */
    char log_entry[MAX_KOMUT_BOYUTU + 50];    
    int boyut = sprintf(log_entry, "%ld\t%s\n", milisaniye, komut);

    if (write(log_fd, log_entry, boyut) == -1) {
        perror("ERROR WRITING TO LOG FILE");
        close(log_fd);
        return;
    }
   
    close(log_fd);
}

