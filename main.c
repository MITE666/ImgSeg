#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <omp.h>
#include "pgraph.h"
#include "vector.h"

#define RADIUS 10

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

    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Could not fork\n");
        return -1;
    } else if (pid == 0) {        
        struct node *graph = build_graph(pixels, surface, img_width, img_height);
        double max_w = 0;
        for (int y = 0; y < img_height; ++y) {
            for (int x = 0; x < img_width; ++x) {
                printf("Val: %f, Sin_H: %f, Cos_H: %f\n", graph[y * img_height + x].feat.value, graph[y * img_height + x].feat.sin_hue, graph[y * img_height + x].feat.cos_hue);
                for (int i = 0; i < graph[y * img_height + x].neigh_count; ++i) {
                    if (graph[y * img_height + x].neighbors[i].w < 1 && graph[y * img_height + x].neighbors[i].w > 0)
                        printf("N%d: pos - %d, w - %f\n", i, graph[y * img_height + x].neighbors[i].pos, graph[y * img_height + x].neighbors[i].w);
                        if (graph[y * img_height + x].neighbors[i].w > max_w)
                            max_w = graph[y * img_height + x].neighbors[i].w;
                }
            }
        printf("%f\n", max_w);
        }

        free_graph(graph, img_width, img_height);

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
