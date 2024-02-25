#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <float.h>
#include <sys/wait.h>
#include <omp.h>
#include "pgraph.h"
#include "vector.h"

#define RADIUS 5
#define INF 1.0e30

void send_vector(int fd, struct vector vec) {
    write(fd, &(vec.size), sizeof(size_t));
    write(fd, vec.data, sizeof(int) * vec.size);
}

void receive_vector(int fd, struct vector *vec) {
    read(fd, &(vec->size), sizeof(size_t));
    vec->data = (int*)malloc(sizeof(int) * vec->size);
    read(fd, vec->data, sizeof(int) * vec->size);
}

void draw_circle(SDL_Renderer *renderer, int center_x, int center_y, int radius) {
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderDrawPoint(renderer, center_x + x, center_y + y);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    const char *dir_path = "./image_src/";
    char img_path[strlen(dir_path) + strlen(argv[argc - 1]) + 1];
    sprintf(img_path, "%s%s", dir_path, argv[argc - 1]);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Surface *surface = IMG_Load(img_path);
    if (surface == NULL) {
        printf("Surface could not be loaded! SDL_Img Error: %s\n", IMG_GetError());
        SDL_Quit();
        return -1;
    }

    Uint32 *pixels = (Uint32 *)surface->pixels; 
    int img_width = surface->w;
    int img_height = surface->h;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe\n");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Could not fork\n");
        return -1;
    } else if (pid == 0) {        
        struct node *graph = build_graph(pixels, surface, img_width, img_height);

        close(pipefd[1]);

        struct vector vec_f, vec_b;

        receive_vector(pipefd[0], &vec_f);
        receive_vector(pipefd[0], &vec_b);

        size_t vec_size = vec_f.size;
        
        for (size_t i = 0; i < vec_size; ++i) {
            for (int y = -RADIUS; y <= RADIUS; ++y) {
                for (int x = -RADIUS; x <= RADIUS; ++x) {
                    if (x * x + y * y <= RADIUS * RADIUS && (x != 0 && y != 0)) {
                        push(&vec_f, (vec_f.data[i] / img_height + y) * img_height + (vec_f.data[i] % img_height + x));
                    }
                }
            }
        }

        graph[img_height * img_width].neigh_count = vec_f.size;
        graph[img_height * img_width].neighbors = (struct neigh *)(malloc(sizeof(struct neigh) * vec_f.size));

        for (size_t i = 0; i < vec_f.size; ++i) {
            graph[img_height * img_width].neighbors[i].pos = vec_f.data[i];
            graph[img_height * img_width].neighbors[i].w = INF;
        }

        vec_size = vec_b.size;

        for (size_t i = 0; i < vec_size; ++i) {
            for (int y = -RADIUS; y <= RADIUS; ++y) {
                for (int x = -RADIUS; x <= RADIUS; ++x) {
                    if (x * x + y * y <= RADIUS * RADIUS && (x != 0 && y != 0)) {
                        push(&vec_b, (vec_b.data[i] / img_height + y) * img_height + (vec_b.data[i] % img_height + x));
                    }
                }
            }
        }

        for (size_t i = 0; i < vec_b.size; ++i) {
            graph[vec_b.data[i]].neighbors[graph[vec_b.data[i]].neigh_count - 1].pos = img_height * img_width + 1;
            graph[vec_b.data[i]].neighbors[graph[vec_b.data[i]].neigh_count - 1].w = INF;
        }

        for (int i = 0; i < graph[img_height * img_width].neigh_count; ++i) {
            struct neigh n = graph[img_height * img_width].neighbors[i];
            printf("S: x = %d, y = %d, w = %f\n", n.pos % img_height, n.pos / img_height, n.w);
        }

        for (int i = 0; i < img_height * img_width; ++i) {
            struct neigh n = graph[i].neighbors[graph[i].neigh_count - 1];
            if (n.w > 100)
                printf("T: x = %d, y = %d, w = %f\n", i % img_height, i / img_height, n.w);
        }

        close(pipefd[0]);

        free_graph(graph, img_width, img_height);

        free_vec(&vec_f);
        free_vec(&vec_b);

        exit(EXIT_SUCCESS);

    } else {
        SDL_Window *window = SDL_CreateWindow("img", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, img_width, img_height, SDL_WINDOW_SHOWN);
        if (window == NULL) {
            printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
            SDL_FreeSurface(surface);
            SDL_Quit();
            return -1;
        }

        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == NULL) {
            printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
            SDL_FreeSurface(surface);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture == NULL) {
            printf("Texture could not be loaded! SDL Error: %s\n", IMG_GetError());
            SDL_FreeSurface(surface);
            SDL_DestroyWindow(window);
            SDL_DestroyRenderer(renderer);
            SDL_Quit();
            return -1;
        }

        bool quit = false;
        SDL_Event event;

        bool fg = true;
        struct vector vec_f, vec_b;
        init(&vec_f);
        init(&vec_b);

        bool sent = false;

        close(pipefd[0]);

        while (!quit) {
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT) {
                    quit = true;
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        if (fg) {
                            push(&vec_f, event.button.y * img_height + event.button.x);
                        } else {
                            push(&vec_b, event.button.y * img_height + event.button.x);
                        }
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        fg = !fg;
                    }
                }  else if (event.type == SDL_KEYDOWN && !sent) {
                    if (event.key.keysym.sym == SDLK_RETURN) {
                        sent = true;
                        
                        send_vector(pipefd[1], vec_f);
                        send_vector(pipefd[1], vec_b);
                    }
                }
            }

            SDL_RenderClear(renderer);

            SDL_RenderCopy(renderer, texture, NULL, NULL);  

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            for (size_t i = 0; i < vec_f.size; ++i) {
                draw_circle(renderer, vec_f.data[i] % img_height, vec_f.data[i] / img_height, RADIUS);
            }

            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            for (size_t i = 0; i < vec_b.size; ++i) {
                draw_circle(renderer, vec_b.data[i] % img_height, vec_b.data[i] / img_height, RADIUS);
            }

            SDL_RenderPresent(renderer);
        }

        close(pipefd[1]);

        free_vec(&vec_f);
        free_vec(&vec_b);

        SDL_DestroyTexture(texture);
        texture = NULL;

        SDL_DestroyRenderer(renderer);
        renderer = NULL;

        SDL_DestroyWindow(window);
        window = NULL;

        wait(NULL);
    }

    SDL_FreeSurface(surface);
    SDL_Quit();

    return 0;
}
