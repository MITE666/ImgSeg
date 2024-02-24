#ifndef PGRAPH_H
#define PGRAPH_H

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define SIGMA 0.2

struct pfeat {
    double value;
    double sin_hue;
    double cos_hue;
};

struct neigh {
    int pos;
    double w;
};

struct node {
    struct pfeat feat;
    struct neigh *neighbors;
    int neigh_count;
};

struct pfeat rgb_to_pfeat(int r, int g, int b) {
    struct pfeat p_feat;
    
    double h, s, v;

    double rd = r / 255.0;
    double gd = g / 255.0;
    double bd = b / 255.0;
    double cmax = fmax(rd, fmax(gd, bd));
    double cmin = fmin(rd, fmin(gd, bd));
    double delta = cmax - cmin;

    if (delta == 0) {
        h = 0;
    } else if (cmax == rd) {
        h = 60 * fmod(((gd - bd) / delta), 6);
    } else if (cmax == gd) {
        h = 60 * (((bd - rd) / delta) + 2);
    } else if (cmax == bd) {
        h = 60 * (((rd - gd) / delta) + 4);
    }
    if (h < 0) {
        h += 360;
    }
     
    if (cmax == 0) {
        s = 0;
    } else {
        s = delta / cmax;
    }

    v = cmax;

    p_feat.value = v;
    p_feat.sin_hue = v * s * sin(h);
    p_feat.cos_hue = v * s * cos(h);

    return p_feat;
}

double eulerian_dif(struct pfeat f1, struct pfeat f2) {
    return (sqrt(pow(f1.value - f2.value, 2) + pow(f1.sin_hue - f2.sin_hue, 2) + pow(f1.cos_hue - f2.cos_hue, 2)));
}

struct node *build_graph(Uint32 *pixels, SDL_Surface *surface, int img_width, int img_height) {
    int di[4] = {-1, 0, 1, 0};
    int dj[4] = {0, 1, 0, -1};
 
    struct node *graph = (struct node *)(malloc(sizeof(struct node) * img_height * img_width + 2));
    if (graph == NULL) {
        perror("Malloc error\n");
        exit(EXIT_FAILURE);
    }

    #pragma omp parallel for collapse(2)
    for (int y = 0; y < img_height; ++y) {
        for (int x = 0; x < img_width; ++x) {
            Uint32 p = pixels[y * img_width + x];

            Uint8 r, g, b;
            SDL_GetRGB(p, surface->format, &r, &g, &b);

            graph[y * img_height + x].feat = rgb_to_pfeat(r, g, b);
            
            int n = 4;
            if (x == 0 || x == img_width - 1)
                --n;
            if (y == 0 || y == img_height - 1)
                --n;
            graph[y * img_height + x].neigh_count = n;
        }
    }

    #pragma omp parallel for collapse(2)
    for (int y = 0; y < img_height; ++y) {
        for (int x = 0; x < img_width; ++x) {
            struct neigh *neighbors = (struct neigh *)(malloc(sizeof(struct neigh) * graph[y * img_height + x].neigh_count));
            if (neighbors == NULL) {
                perror("Malloc error\n");
                exit(EXIT_FAILURE);
            }
            int k = 0;
            for (int i = 0; i < 4; ++i) {
                int new_x = x + di[i];
                int new_y = y + dj[i];
                if (new_x < 0 || new_x >= img_width || new_y < 0 || new_y >= img_height)
                    break;
                
                struct neigh n;
                n.pos = new_y * img_height + new_x;
                n.w = exp(-(eulerian_dif(graph[y * img_height + x].feat, graph[new_y * img_height + new_x].feat)) / (2 * pow(SIGMA, 2)));
                neighbors[k++] = n;
            }
            graph[y * img_height + x].neighbors = neighbors;
        }
    }

    return graph;
}

#endif