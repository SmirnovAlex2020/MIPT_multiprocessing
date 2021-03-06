#include <stdio.h>
#include <mpi.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

int reallocation_factor = 2;
int messaging_interval = 200;

typedef int bool;
#define FALSE 0
#define TRUE 1



typedef struct {
	int rank;
    int size;
    int l;
    int a;
    int b;
    int n;
    int N;
    float pl;
    float pr;
    float pu;
    float pd;
    
} dats;

typedef enum {
    DEFAULT,
    LEFT,
    RIGHT,
    UP,
    DOWN
} dir_t;

typedef struct {
    int x;
    int y;
} coords_t;

typedef struct {
    coords_t coords;
    int n;
    int seed;
} particle_t;

coords_t get_coordinates(int rank, int a, int b) {
    return (coords_t) { rank % a, rank / a};
}

int get_rank(coords_t coords, int a, int b) {
    return coords.y * a + coords.x;
}


void push_back(particle_t** array, int* n, int* capacity, particle_t* element) {
    if (*n < *capacity) {
        (*array)[*n] = *element;
        (*n)++;
    } 
    else {
        *array = realloc(*array, *capacity * reallocation_factor * sizeof(particle_t));
        *capacity *= reallocation_factor;
        (*array)[*n] = *element;
        (*n)++;
    }
}

void pop(particle_t** array, int* n, int index) {
    (*array)[index] = (*array)[(*n) - 1];
    (*n)--;
}

int get_adjacent_rank(coords_t coords, int a, int b, dir_t dir) {
	if (dir == UP) {
        coords.y -= 1;
        if (coords.y < 0) {
            coords.y = b - 1;
        }
    }
    if (dir == DOWN) {
        coords.y += 1;
        if (coords.y >= b) {
            coords.y = 0;
        }
    }
    if (dir == LEFT) {
        coords.x -= 1;
        if (coords.x < 0) {
            coords.x = a - 1;
        }
    }
    if (dir == RIGHT) {
        coords.x += 1;
        if (coords.x >= a) {
            coords.x = 0;
        }
    }
    return get_rank(coords, a, b);
}

dir_t decide(float dir_pr_1, float dir_pr_2, float dir_pr_3, float dir_pr_4) {
    if (dir_pr_1 >= dir_pr_2 && dir_pr_1 >= dir_pr_3 && dir_pr_1 >= dir_pr_4) {
        return LEFT;
    } 
    else if (dir_pr_2 >= dir_pr_1 && dir_pr_2 >= dir_pr_3 && dir_pr_2 >= dir_pr_4) {
        return RIGHT;
    } 
    else if (dir_pr_3 >= dir_pr_1 && dir_pr_3 >= dir_pr_2 && dir_pr_3 >= dir_pr_4) {
        return UP;
    } 
    else {
        return DOWN;
    }
}

void* process_region(void* data_void) {
    dats *data = (dats*) data_void;
    int l = data->l;
    int a = data->a;
    int b = data->b;
    int n = data->n;
    int N = data->N;
    int rank = data->rank;
    int size = data->size;
    float pl = data->pl;
    float pr = data->pr;
    float pu = data->pu;
    float pd = data->pd;
    coords_t coords = get_coordinates(rank, a, b);
    
    int up_rank = get_adjacent_rank(coords, a, b, UP);
    int down_rank = get_adjacent_rank(coords, a, b, DOWN);
    int left_rank = get_adjacent_rank(coords, a, b, LEFT);
    int right_rank = get_adjacent_rank(coords, a, b, RIGHT);


    int* seeds = malloc(sizeof(int) * size);
    int seed;
    if (rank == 0) {
        srand(time(NULL));
        for (int i = 0; i < size; i++)
            seeds[i] = (int) rand();
    }
    MPI_Scatter(seeds, 1, MPI_INT, &seed, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    int particles_size = N;
    int particles_capacity = N;
    particle_t* particles = (particle_t*) malloc(particles_capacity * sizeof(particle_t));
    srand(seed);
    for (int i = 0; i < N; i++) {
        particles[i].coords = (coords_t) {rand() % l, rand() % l};
        particles[i].n = n;
        particles[i].seed = rand();
    }
    
    int send_up_size = 0;
    int send_up_capacity = N;
    particle_t* send_up = (particle_t*) malloc(send_up_capacity * sizeof(particle_t));
    
    int send_down_size = 0;
    int send_down_capacity = N;
    particle_t* send_down = (particle_t*) malloc(send_down_capacity * sizeof(particle_t));
    
    int send_left_size = 0;
    int send_left_capacity = N;
    particle_t* send_left = (particle_t*) malloc(send_left_capacity * sizeof(particle_t));
    
    int send_right_size = 0;
    int send_right_capacity = N;
    particle_t* send_right = (particle_t*) malloc(send_right_capacity * sizeof(particle_t));
    
    int receive_up_capacity = 0;
    int receive_down_capacity = 0;
    int receive_left_capacity = 0;
    int receive_right_capacity = 0;
    
    
    int completed_size = 0;
    int completed_capacity = N;
    particle_t* completed = (particle_t*) malloc(completed_capacity * sizeof(particle_t));
    
    struct timeval t0, t1;
    assert(gettimeofday(&t0, NULL) == 0);
    
    while (TRUE) {
        int particle_index = 0;
        while (particle_index < particles_size) {
            particle_t* particle = particles + particle_index;
            bool inc_part_idx = TRUE;
            for (int t = 0; t < messaging_interval; t++) {
                if (particle->n == 0) {
                    push_back(&completed, &completed_size, &completed_capacity, particle);
                    pop(&particles, &particles_size, particle_index);
                    inc_part_idx = FALSE;
                    break;
                }
                float up_pr = rand_r((unsigned int*) &particle->seed) * pu;
                float down_pr = rand_r((unsigned int*) &particle->seed) * pd;
                float left_pr = rand_r((unsigned int*) &particle->seed) * pl;
                float right_pr = rand_r((unsigned int*) &particle->seed) * pr;
                
                dir_t dir = decide(left_pr, right_pr, up_pr, down_pr);
                if (dir == LEFT) {
                    particle->coords.x -= 1;
                } 
                else if (dir == RIGHT) {
                    particle->coords.x += 1;
                } 
                else if (dir == UP) {
                    particle->coords.y -= 1;
                } 
                else {
                    particle->coords.y += 1;
                }
                particle->n -= 1;
                if (particle->coords.y < 0) {
                    particle->coords.y = l - 1;
                    push_back(&send_up, &send_up_size, &send_up_capacity, particle);
                    pop(&particles, &particles_size, particle_index);
                    inc_part_idx = FALSE;
                    break;
                }
                if (particle->coords.y >= l) {
                    particle->coords.y = 0;
                    push_back(&send_down, &send_down_size, &send_down_capacity, particle);
                    pop(&particles, &particles_size, particle_index);
                    inc_part_idx = FALSE;
                    break;
                }
                if (particle->coords.x < 0) {
                    particle->coords.x = l - 1;
                    push_back(&send_left, &send_left_size, &send_left_capacity, particle);
                    pop(&particles, &particles_size, particle_index);
                    inc_part_idx = FALSE;
                    break;
                }
                if (particle->coords.x >= l) {
                    particle->coords.x = 0;
                    push_back(&send_right, &send_right_size, &send_right_capacity, particle);
                    pop(&particles, &particles_size, particle_index);
                    inc_part_idx = FALSE;
                    break;
                }
                
            }
            if (inc_part_idx) {
                particle_index += 1;
            }
        }
        
        MPI_Request* metadata = (MPI_Request*) malloc(sizeof(MPI_Request) * 8);
        MPI_Isend(&send_left_size, 1, MPI_INT, left_rank, 0, MPI_COMM_WORLD, metadata+0);
        MPI_Isend(&send_right_size, 1, MPI_INT, right_rank, 1, MPI_COMM_WORLD, metadata+1);
        MPI_Isend(&send_up_size, 1, MPI_INT, up_rank, 2, MPI_COMM_WORLD, metadata+2);
        MPI_Isend(&send_down_size, 1, MPI_INT, down_rank, 3, MPI_COMM_WORLD, metadata+3);

        MPI_Irecv(&receive_left_capacity, 1, MPI_INT, left_rank, 1, MPI_COMM_WORLD, metadata+4);
        MPI_Irecv(&receive_right_capacity, 1, MPI_INT, right_rank, 0, MPI_COMM_WORLD, metadata+5);
        MPI_Irecv(&receive_up_capacity, 1, MPI_INT, up_rank, 3, MPI_COMM_WORLD ,metadata+6);
        MPI_Irecv(&receive_down_capacity, 1, MPI_INT, down_rank, 2, MPI_COMM_WORLD, metadata+7);

        MPI_Waitall(8, metadata, MPI_STATUS_IGNORE);
        
        particle_t* receive_left = (particle_t*) malloc(receive_left_capacity * sizeof(particle_t));
        particle_t* receive_right = (particle_t*) malloc(receive_right_capacity * sizeof(particle_t));
        particle_t* receive_up = (particle_t*) malloc(receive_up_capacity * sizeof(particle_t));
        particle_t* receive_down = (particle_t*) malloc(receive_down_capacity * sizeof(particle_t));
        
        MPI_Request* data = (MPI_Request*) malloc(sizeof(MPI_Request) * 8);
        MPI_Issend(send_left, sizeof(particle_t) * send_left_size, MPI_BYTE, left_rank, 0, MPI_COMM_WORLD, data+0);
        MPI_Issend(send_right, sizeof(particle_t) * send_right_size, MPI_BYTE, right_rank, 1, MPI_COMM_WORLD, data+1);
        MPI_Issend(send_up, sizeof(particle_t) * send_up_size, MPI_BYTE, up_rank, 2, MPI_COMM_WORLD, data+2);
        MPI_Issend(send_down, sizeof(particle_t) * send_down_size, MPI_BYTE, down_rank, 3, MPI_COMM_WORLD, data+3);

        MPI_Irecv(receive_left, sizeof(particle_t) * receive_left_capacity, MPI_BYTE, left_rank, 1, MPI_COMM_WORLD, data+4);
        MPI_Irecv(receive_right, sizeof(particle_t) * receive_right_capacity, MPI_BYTE, right_rank, 0, MPI_COMM_WORLD, data+5);
        MPI_Irecv(receive_up, sizeof(particle_t) * receive_up_capacity, MPI_BYTE, up_rank, 3, MPI_COMM_WORLD, data+6);
        MPI_Irecv(receive_down, sizeof(particle_t) * receive_down_capacity, MPI_BYTE, down_rank, 2, MPI_COMM_WORLD, data+7);

        MPI_Waitall(8, data, MPI_STATUS_IGNORE);

        free(metadata);
        free(data);
        
        for (int j = 0; j < receive_up_capacity; j++) {
            push_back(&particles, &particles_size, &particles_capacity, receive_up + j);
        }
        for (int j = 0; j < receive_down_capacity; j++) {
            push_back(&particles, &particles_size, &particles_capacity, receive_down + j);
        }
        for (int j = 0; j < receive_left_capacity; j++) {
            push_back(&particles, &particles_size, &particles_capacity, receive_left + j);
        }
        for (int j = 0; j < receive_right_capacity; j++) {
            push_back(&particles, &particles_size, &particles_capacity, receive_right + j);
        }
        
        
        free(receive_left);
        free(receive_right);
        free(receive_up);
        free(receive_down);
        
        send_up_size = 0;
        send_down_size = 0;
        send_left_size = 0;
        send_right_size = 0;


        int* recv_buff = (int*) malloc(sizeof(int));
        MPI_Reduce(&completed_size, recv_buff, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        bool finished = FALSE;
        if (rank == 0) {
            if (*recv_buff == size * N) {
                finished = TRUE;
            } 
            else {
                finished = FALSE;
            }
        }
        MPI_Bcast(&finished, 1, MPI_INT, 0, MPI_COMM_WORLD);

        free(recv_buff);
        
        if (finished) {
            int* total = (int*) malloc(sizeof(int) * size);
            MPI_Gather(&completed_size, 1, MPI_INT, total, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (rank == 0) {
                assert(gettimeofday(&t1, NULL) == 0);
                double delta = ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) / 1000000.0;
                
                FILE *file;
                file = fopen("stats.txt", "w");
                fprintf(file, "%d %d %d %d %d %f %f %f %f %fs\n", l, a, b, n, N, pl, pr, pu, pd, delta);
                for (int rk = 0; rk < size; rk++) {
                    fprintf(file, "%d: %d\n", rk, total[rk]);
                }
                fclose(file);
            }
            free(total);
            break;
        }

        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    free(seeds);
    
    free(send_up);
    free(send_down);
    free(send_left);
    free(send_right);
    

    free(particles);
    free(completed);

    return NULL;
}

int main(int argc, char * argv[]) {
    
    MPI_Init(&argc, &argv);
    
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if(argc >= 10){
	  printf("Incorrect number of arguments.\n");
	  exit(1);
	}
    dats data = (dats) {
        atoi(argv[1]),
        atoi(argv[2]),
        atoi(argv[3]),
        atoi(argv[4]),
        atoi(argv[5]),
        atof(argv[6]),
        atof(argv[7]),
        atof(argv[8]),
        atof(argv[9]),
        world_rank,
        world_size
    };

    pthread_t thread;
    pthread_create(&thread, NULL, process_region, &data);
    pthread_join(thread, NULL);
    
    MPI_Finalize();
    return 0;
}
