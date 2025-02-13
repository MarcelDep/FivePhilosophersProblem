#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>

// Semafory
sem_t semaphores;

// Filozofowie 
char* philosopher_names[5] = {"Sokrates", "Platon", "Arystoteles", "Pitagoras", "Heraklit"};
int philosophers_id[5] = {0, 1, 2, 3, 4};

// Ilość pałeczek
int CHOPSTICK_AMOUNT = 5;

// Tutaj będą przechowywane PID procesów child 
int pid_child_array[5] = {0, 0, 0 ,0 ,0};

int eating(char* philosopher_name) { // Funckja zwracająca czas przez który filozof będzie jadł
    unsigned int seed = time(0);
    int eating_time = ( rand_r(&seed) % (4 - 1 + 1)) * 10000 + 10000; // Losujemy czas jedzenia z zakresu od 1 do 4 sekund
    printf("%s zaczyna jeść przez %d mikro sekund.\n", philosopher_name, eating_time);
    usleep(eating_time);
    return eating_time;
}

int thinking(char* philosopher_name) { // Funckja zwracająca czas przez który filozof będzie myślał
    unsigned int seed = time(0);
    int thinking_time = ( rand_r(&seed) % (5 - 2 + 1)) * 10000 + 20000; // Losujemy czas jedzenia z zakresu od 2 do 5 sekund
    printf("%s filozofuje przez %d mikro sekund.\n", philosopher_name, thinking_time);
    usleep(thinking_time);
    return thinking_time;
}

// Porównywanie zawartości dwóch tablic
int array_copare(int* array1, int* array2) {
    for (int i = 0; i < 5; i++) {
        if (!(array1[i] == 1 && array2[i] == 0)) {
            return -1;
        }
    }
    return 0; 
}

int philosophizing(int id, int *left_chopsticks, int *right_chopsticks, sem_t* semaphores) { // Funckja określająca co będzie robił filozof
    // Zidentyfikuj jakie pałki ma próbować podnieść filozof 
    int taken_chopsticks = 0;
    int eaten_meals = 1;
    char* philosopher_name = philosopher_names[id]; 
    printf("Mam na imię %s \n", philosopher_name);

    while(1) {
        int thinking_time = thinking(philosopher_name);
        int right_semaphore_value = 0;
        int left_semaphore_value = 0;
        printf("%s skończył filozofować i wycięczony zasiada do stołu aby zjeść.\n", philosopher_name);
        // Wartość semafora po lewej i prawej stronie od filozofa
        sem_getvalue(&semaphores[id], &left_semaphore_value);
        sem_getvalue(&semaphores[id], &right_semaphore_value);
        // Jeżeli semafor jest równy 1 to spróbuj wziąć pałkę po lewej stronie i zajmij semafor
        if (left_semaphore_value == 1) {
            sem_wait(&semaphores[id]);
            taken_chopsticks++;
            left_chopsticks[id] = 1;
            printf("%s wziął lewą pałeczkę nr. %d.\n", philosopher_name, id);
            // Jeżeli każdy filozof weźmie po jednej lewej pałce i dojdzie do zakleszczenia to filozof, który jako ostatni wziął pałeczkę odda ją
            if (array_copare(left_chopsticks, right_chopsticks) == 0) {
                printf("Mężny %s zapobiega zakleszczeniu oddając pałeczkę nr. %d\n",philosopher_name, id);
                sem_post(&semaphores[id]);
                taken_chopsticks--;
                left_chopsticks[id] = 0;
                usleep(10000);
                }
            }
            // Jeżeli uda się to spróbuj wziąć pałkę prawej stronie i zając semafor
            if (right_semaphore_value == 1 && left_chopsticks[id] == 1) {
                sem_wait(&semaphores[((id + 1) % 5)]);
                taken_chopsticks++;
                right_chopsticks[((id + 1) % 5)] = 1;
                printf("%s wziął prawą pałeczkę nr. %d.\n", philosopher_name, ((id + 1) % 5));
                // Jeżeli masz obie pałki to zacznij jeść, następnie odłóż pałeczki, zwolnij semafor i wróć do myślenia
                if (taken_chopsticks == 2) {
                    int eating_time = eating(philosopher_name);
                    printf("Po pysznym %d posiłku pełen energii %s wraca do filozofowania odkładając lewą pałeczkę nr. %d i prawą nr. %d.\n", eaten_meals, philosopher_name, id, ((id +1) % 5));
                    left_chopsticks[id] = 0;
                    right_chopsticks[((id + 1) % 5)] = 0;
                    sem_post(&semaphores[id]);
                    sem_post(&semaphores[((id +1) % 5)]);
                    taken_chopsticks = 0;
                    eaten_meals++;
                }
            }
        }
    }   

// Tworzenie pięciu procesów child, z których każdy będzie innym filozofem
int* createChildren(int *left_chopsticks, int *right_chopsticks, sem_t* semaphores) {
    int pid_child;
    for (int i = 0; i < 5; i++) {
        pid_child = fork(); 
        if (pid_child == 0) {
            printf("Dziecko nr. %d\n", i);
            pid_child_array[i] = pid_child;
            int id = philosophers_id[i];
            philosophizing(id, left_chopsticks, right_chopsticks, semaphores); 
            }
        }
    // Rodzic musi czekać na każde dziecko
    for (int i = 0; i < 5; i++) {
        wait(NULL); 
    }
    return pid_child_array;
} 

int main() {
    // Tworzenie tablic przechowujących stan pałeczek  i semafory w pamięci współdzielonej
    sem_t* semaphores = mmap(NULL, CHOPSTICK_AMOUNT*sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    int* left_chopsticks = mmap(NULL, CHOPSTICK_AMOUNT*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    int* right_chopsticks = mmap(NULL, CHOPSTICK_AMOUNT*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

    // Inicjalizacja semaforów i tablic
    for (int i = 0; i < 5; i++) {
        sem_init(&semaphores[i], 1, 1);
    }
    for (int i = 0; i < CHOPSTICK_AMOUNT; i++) {
        left_chopsticks[i] = 0;
    }
    for (int i = 0; i < CHOPSTICK_AMOUNT; i++) {
        right_chopsticks[i] = 0;
    }    

    createChildren(left_chopsticks, right_chopsticks, semaphores);

    // Usuwanie semaforów 
    for (int i = 0; i < 5; i++) {
        sem_destroy(&semaphores[i]);
    }
    return 0;
}