// banker - deadlock avoidance with the banker's algorithm

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_THREADS   16
#define MAX_RESOURCES 8

#define HOLD_US 10000
#define WORK_US 50000

#define REQ_GRANTED     0
#define REQ_ERROR       1
#define REQ_UNAVAILABLE 2
#define REQ_UNSAFE      3

typedef struct {
    int n;
    int m;

    int total[MAX_RESOURCES];
    int available[MAX_RESOURCES];

    int max[MAX_THREADS][MAX_RESOURCES];
    int allocation[MAX_THREADS][MAX_RESOURCES];
    int need[MAX_THREADS][MAX_RESOURCES];

    int pending[MAX_THREADS][MAX_RESOURCES];
} bank_t;

bank_t bank;

pthread_mutex_t bank_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bank_cond = PTHREAD_COND_INITIALIZER;

void print_vector(const int vec[]) {
    for (int j = 0; j < bank.m; j++) {
        printf(" %d", vec[j]);
    }
}

void show(void) {
    printf("Available:");
    print_vector(bank.available);
    printf("\n");

    for (int i = 0; i < bank.n; i++) {
        printf("T%d Allocation:", i);
        print_vector(bank.allocation[i]);

        printf(" Max:");
        print_vector(bank.max[i]);

        printf(" Need:");
        print_vector(bank.need[i]);

        printf("\n");
    }
}

int parse_thread_vector(char **args, int arg_count,
                        int *i, int vec[]) {
    if (arg_count != bank.m + 2) {
        return 0;
    }

    *i = atoi(args[1]);

    if (*i < 0 || *i >= bank.n) {
        return 0;
    }

    for (int j = 0; j < bank.m; j++) {
        vec[j] = atoi(args[j + 2]);

        if (vec[j] < 0) {
            return 0;
        }
    }

    return 1;
}

int vec_leq(const int a[], const int b[]) {
    for (int j = 0; j < bank.m; j++) {
        if (a[j] > b[j]) {
            return 0;
        }
    }

    return 1;
}

int is_safe(int sequence[]) {
    int work[MAX_RESOURCES];
    int finish[MAX_THREADS] = {0};
    int count = 0;

    for (int j = 0; j < bank.m; j++) {
        work[j] = bank.available[j];
    }

    while (1) {
        int progress = 0;

        for (int i = 0; i < bank.n; i++) {
            if (!finish[i] && vec_leq(bank.need[i], work)) {

                for (int j = 0; j < bank.m; j++) {
                    work[j] += bank.allocation[i][j];
                }

                finish[i] = 1;
                sequence[count++] = i;
                progress = 1;
            }
        }

        if (!progress) {
            break;
        }
    }

    return count == bank.n;
}

void allocate(int i, const int req[]) {
    for (int j = 0; j < bank.m; j++) {
        bank.available[j] -= req[j];
        bank.allocation[i][j] += req[j];
        bank.need[i][j] -= req[j];
    }
}

void deallocate(int i, const int rel[]) {
    for (int j = 0; j < bank.m; j++) {
        bank.available[j] += rel[j];
        bank.allocation[i][j] -= rel[j];
        bank.need[i][j] += rel[j];
    }
}

int try_request(int i, const int req[]) {
    if (!vec_leq(req, bank.need[i])) {
        return REQ_ERROR;
    }

    if (!vec_leq(req, bank.available)) {
        return REQ_UNAVAILABLE;
    }

    allocate(i, req);

    int sequence[MAX_THREADS];

    if (is_safe(sequence)) {
        return REQ_GRANTED;
    }

    deallocate(i, req);

    return REQ_UNSAFE;
}

int release_resources(int i, const int rel[]) {
    if (!vec_leq(rel, bank.allocation[i])) {
        return 0;
    }

    deallocate(i, rel);

    return 1;
}

int acquire_one(int i, int j) {
    int req[MAX_RESOURCES] = {0};

    req[j] = 1;

    pthread_mutex_lock(&bank_lock);

    while (1) {
        int result = try_request(i, req);

        if (result == REQ_GRANTED) {
            printf("T%d granted R%d\n", i, j);

            pthread_mutex_unlock(&bank_lock);

            return 1;
        }

        if (result == REQ_ERROR) {
            fprintf(stderr,
                    "customer T%d requested beyond its maximum claim\n",
                    i);

            pthread_mutex_unlock(&bank_lock);

            return 0;
        }

        printf("T%d waiting R%d\n", i, j);

        pthread_cond_wait(&bank_cond, &bank_lock);
    }
}

void release_all(int i) {
    int rel[MAX_RESOURCES];

    pthread_mutex_lock(&bank_lock);

    for (int j = 0; j < bank.m; j++) {
        rel[j] = bank.allocation[i][j];
    }

    deallocate(i, rel);

    printf("T%d released all\n", i);

    pthread_cond_broadcast(&bank_cond);

    pthread_mutex_unlock(&bank_lock);
}

void *customer(void *arg) {
    int i = *(int *)arg;

    for (int j = 0; j < bank.m; j++) {

        while (1) {
            pthread_mutex_lock(&bank_lock);

            int remaining = bank.need[i][j];

            pthread_mutex_unlock(&bank_lock);

            if (remaining <= 0) {
                break;
            }

            if (!acquire_one(i, j)) {
                return NULL;
            }

            usleep(HOLD_US);
        }
    }

    usleep(WORK_US);

    release_all(i);

    return NULL;
}

void run_customers(void) {
    pthread_t threads[MAX_THREADS];
    int ids[MAX_THREADS];

    for (int i = 0; i < bank.n; i++) {
        ids[i] = i;

        pthread_create(
            &threads[i],
            NULL,
            customer,
            &ids[i]
        );
    }

    for (int i = 0; i < bank.n; i++) {
        pthread_join(threads[i], NULL);
    }
}

void detect_deadlock(void) {
    int work[MAX_RESOURCES];
    int finish[MAX_THREADS];

    for (int j = 0; j < bank.m; j++) {
        work[j] = bank.available[j];
    }

    for (int i = 0; i < bank.n; i++) {
        int holds_any = 0;

        for (int j = 0; j < bank.m; j++) {
            if (bank.allocation[i][j] != 0) {
                holds_any = 1;
                break;
            }
        }

        finish[i] = !holds_any;
    }

    while (1) {
        int progress = 0;

        for (int i = 0; i < bank.n; i++) {

            if (!finish[i] &&
                vec_leq(bank.pending[i], work)) {

                for (int j = 0; j < bank.m; j++) {
                    work[j] += bank.allocation[i][j];
                }

                finish[i] = 1;
                progress = 1;
            }
        }

        if (!progress) {
            break;
        }
    }

    int any = 0;

    for (int i = 0; i < bank.n; i++) {

        if (!finish[i]) {

            if (!any) {
                printf("DEADLOCK:");
            }

            printf(" T%d", i);

            any = 1;
        }
    }

    if (any) {
        printf("\n");
    } else {
        printf("NO DEADLOCK\n");
    }
}

int main(void) {
    char line[1024];
    char *args[64];
    int arg_count;

    setvbuf(stdout, NULL, _IOLBF, 0);

    while (fgets(line, sizeof(line), stdin) != NULL) {

        char *newline = strchr(line, '\n');

        if (newline != NULL) {
            *newline = '\0';
        }

        arg_count = 0;

        args[arg_count] = strtok(line, " \t\r");

        while (args[arg_count] != NULL &&
               arg_count < 63) {

            arg_count++;

            args[arg_count] =
                strtok(NULL, " \t\r");
        }

        if (arg_count == 0 ||
            args[0][0] == '#') {

            continue;
        }

        if (strcmp(args[0], "threads") == 0) {

            if (arg_count != 2) {
                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            int n = atoi(args[1]);

            if (n < 1 || n > MAX_THREADS) {
                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            bank.n = n;

        } else if (strcmp(args[0],
                          "resources") == 0) {

            int m = arg_count - 1;

            if (m < 1 || m > MAX_RESOURCES) {
                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            bank.m = m;

            for (int j = 0; j < bank.m; j++) {
                bank.total[j] =
                    atoi(args[j + 1]);

                bank.available[j] =
                    bank.total[j];
            }

        } else if (strcmp(args[0],
                          "max") == 0) {

            int i;
            int vec[MAX_RESOURCES];

            if (!parse_thread_vector(
                    args,
                    arg_count,
                    &i,
                    vec)) {

                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            for (int j = 0; j < bank.m; j++) {
                bank.max[i][j] = vec[j];

                bank.need[i][j] =
                    bank.max[i][j] -
                    bank.allocation[i][j];
            }

        } else if (strcmp(args[0],
                          "alloc") == 0) {

            int i;
            int vec[MAX_RESOURCES];

            if (!parse_thread_vector(
                    args,
                    arg_count,
                    &i,
                    vec)) {

                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            for (int j = 0; j < bank.m; j++) {

                bank.available[j] +=
                    bank.allocation[i][j];

                bank.allocation[i][j] =
                    vec[j];

                bank.available[j] -=
                    bank.allocation[i][j];

                bank.need[i][j] =
                    bank.max[i][j] -
                    bank.allocation[i][j];
            }

        } else if (strcmp(args[0],
                          "pending") == 0) {

            int i;
            int vec[MAX_RESOURCES];

            if (!parse_thread_vector(
                    args,
                    arg_count,
                    &i,
                    vec)) {

                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            for (int j = 0; j < bank.m; j++) {
                bank.pending[i][j] = vec[j];
            }

        } else if (strcmp(args[0],
                          "show") == 0) {

            show();

        } else if (strcmp(args[0],
                          "safe") == 0) {

            int sequence[MAX_THREADS];

            if (is_safe(sequence)) {

                printf("SAFE:");

                for (int i = 0;
                     i < bank.n;
                     i++) {

                    printf(" T%d",
                           sequence[i]);
                }

                printf("\n");

            } else {

                printf("UNSAFE\n");
            }

        } else if (strcmp(args[0],
                          "request") == 0) {

            int i;
            int req[MAX_RESOURCES];

            if (!parse_thread_vector(
                    args,
                    arg_count,
                    &i,
                    req)) {

                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            int result =
                try_request(i, req);

            if (result == REQ_GRANTED) {

                printf("GRANTED\n");

            } else if (result == REQ_ERROR) {

                printf(
                    "ERROR: exceeds maximum claim\n"
                );

            } else if (result ==
                       REQ_UNAVAILABLE) {

                printf(
                    "WAIT: resources unavailable\n"
                );

            } else {

                printf(
                    "WAIT: unsafe state\n"
                );
            }

        } else if (strcmp(args[0],
                          "release") == 0) {

            int i;
            int rel[MAX_RESOURCES];

            if (!parse_thread_vector(
                    args,
                    arg_count,
                    &i,
                    rel)) {

                printf("ERROR: bad arguments to %s\n",
                       args[0]);

                continue;
            }

            if (release_resources(i, rel)) {

                printf("RELEASED\n");

            } else {

                printf(
                    "ERROR: exceeds allocation\n"
                );
            }

        } else if (strcmp(args[0],
                          "run") == 0) {

            run_customers();

        } else if (strcmp(args[0],
                          "detect") == 0) {

            detect_deadlock();

        } else {

            printf(
                "ERROR: unknown command %s\n",
                args[0]
            );
        }
    }

    return 0;
}