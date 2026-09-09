/* Local diagnostic player. This is translated VS C, not the Neo Geo cartridge. */
#include "vs_machine.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef VS_HOST_SDL
#include <SDL.h>
#endif

static void load(const char *dir, const char *name, void *data, size_t n) {
    char path[4096];
    if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path)) exit(2);
    FILE *f = fopen(path, "rb");
    if (!f || fread(data, 1, n, f) != n || fgetc(f) != EOF) {
        fprintf(stderr, "Invalid/missing private asset: %s\n", name); exit(2);
    }
    fclose(f);
}
int main(int argc, char **argv) {
    static uint8_t prg[32768], chr[16384], pal[192], rgb[256 * 240 * 3];
    static VsMachine m;
    if (argc < 2) { fprintf(stderr, "usage: %s ASSET_DIR [frames [output.ppm]]\n", argv[0]); return 2; }
    load(argv[1], "vs-program.bin", prg, sizeof(prg));
    load(argv[1], "vs-chr.bin", chr, sizeof(chr));
    load(argv[1], "vs-palette.bin", pal, sizeof(pal));
    for (unsigned i = 0; i < sizeof(pal); ++i)
        pal[i] = (uint8_t)((pal[i] << 5) | (pal[i] << 2) | (pal[i] >> 1));
    vs_machine_init(&m, prg, chr);
    printf("Boot: pc=%04x idle=%u fault=%u ctrl=%02x steps=%u\n", m.cpu.pc, m.cpu.idle, m.cpu.fault, m.ctrl, m.cpu.instructions);
    unsigned limit = argc >= 3 ? (unsigned)strtoul(argv[2], NULL, 10) : 600;
#ifdef VS_HOST_SDL
    SDL_Window *window = NULL; SDL_Renderer *renderer = NULL; SDL_Texture *texture = NULL;
    if (limit == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return 2;
        window = SDL_CreateWindow("VS Super Mario Bros. — C prototype (not Neo Geo yet)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 768, 720, SDL_WINDOW_RESIZABLE);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (renderer) texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 256, 240);
        if (!window || !renderer || !texture) { fprintf(stderr, "%s\n", SDL_GetError()); return 2; }
        SDL_RenderSetLogicalSize(renderer, 256, 240);
    }
#endif
    for (unsigned frame = 0; !limit || frame < limit; ++frame) {
        uint8_t p1 = frame >= 360 && frame < 364 ? 4 : 0;
        uint8_t p2 = 0, coin = frame >= 240 && frame < 244 ? 0x20 : 0;
#ifdef VS_HOST_SDL
        uint32_t began = SDL_GetTicks();
        if (!limit) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) goto done;
            const uint8_t *k = SDL_GetKeyboardState(NULL);
            if (k[SDL_SCANCODE_ESCAPE]) goto done;
            /* SMB uses the non-reversed MAME cabinet mapping. */
            p1 = (k[SDL_SCANCODE_RETURN] ? 4 : 0)
               | (k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_SPACE] ? 1 : 0)
               | (k[SDL_SCANCODE_X] || k[SDL_SCANCODE_LSHIFT] ? 2 : 0)
               | (k[SDL_SCANCODE_UP] ? 16 : 0) | (k[SDL_SCANCODE_DOWN] ? 32 : 0)
               | (k[SDL_SCANCODE_LEFT] ? 64 : 0) | (k[SDL_SCANCODE_RIGHT] ? 128 : 0);
            p2 = k[SDL_SCANCODE_2] ? 4 : 0;
            coin = k[SDL_SCANCODE_5] ? 0x20 : 0;
        }
#endif
        if (!vs_machine_frame(&m, p1, p2, coin)) {
            fprintf(stderr, "Stopped at frame %u pc=%04x fault=%u nmi=%u\n", frame, m.cpu.pc, m.cpu.fault, m.cpu.in_nmi); return 1;
        }
#ifdef VS_HOST_SDL
        if (!limit) {
            vs_machine_render(&m, pal, rgb);
            SDL_UpdateTexture(texture, NULL, rgb, 256 * 3);
            SDL_RenderClear(renderer); SDL_RenderCopy(renderer, texture, NULL, NULL); SDL_RenderPresent(renderer);
            uint32_t elapsed = SDL_GetTicks() - began;
            if (elapsed < 17) SDL_Delay(17 - elapsed);
        }
#endif
    }
#ifdef VS_HOST_SDL
done:
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
#endif
    printf("Frames=%u pc=%04x instructions=%u coin_edges=%u apu_writes=%u credits=%02x\n",
           m.frames, m.cpu.pc, m.cpu.instructions, m.bus.coin_counter, m.apu_writes, m.bus.extra_ram[0x610]);
    if (argc >= 4) {
        vs_machine_render(&m, pal, rgb);
        FILE *f = fopen(argv[3], "wb");
        if (!f) return 2;
        fprintf(f, "P6\n256 240\n255\n");
        if (fwrite(rgb, 1, sizeof(rgb), f) != sizeof(rgb) || fclose(f)) return 2;
    }
    if (argc >= 5) {
        FILE *f = fopen(argv[4], "wb");
        if (!f) return 2;
        if (fwrite(m.bus.ram, 1, 2048, f) != 2048 || fwrite(m.bus.extra_ram, 1, 2048, f) != 2048 || fclose(f)) return 2;
    }
    return m.cpu.fault ? 1 : 0;
}
