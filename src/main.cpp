#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <SDL2/SDL.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "pico8.h"

#define WINDOW_TITLE "pico-c — PICO-8 Compatible Emulator"

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *screen   = NULL;
static uint32_t pixels[PICO8_SCREEN_W * PICO8_SCREEN_H];
static int scale = 3;

static int running = 1;
static int cart_loaded = 0;
static int has_init = 0;
static int has_draw = 0;
static int has_update = 0;
static int has_update60 = 0;

static int rel_mode = 0;        /* SDL relative mouse mode is active */
static int text_input_on = 0;   /* SDL text input is active */

/* Boot / shell / run state. */
static int      app_mode = PICO8_APP_BOOT;
static uint32_t boot_start = 0;
static int      boot_skipped = 0;
static const char *pending_cart = NULL;   /* cartridge passed on the command line */

static void enter_shell(void);
static void start_cart(void);
static void stop_cart_to_shell(void);

/* ------------------------------------------------------------------ */
/*  Key translation for the shell                                      */
/* ------------------------------------------------------------------ */

static int translate_shell_key(SDL_Keycode sym)
{
    switch (sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  return '\r';
        case SDLK_BACKSPACE: return 8;
        case SDLK_DELETE:    return 127;
        case SDLK_ESCAPE:    return 27;
        case SDLK_TAB:       return PICO8_KEY_TAB;
        case SDLK_UP:        return PICO8_KEY_UP;
        case SDLK_DOWN:      return PICO8_KEY_DOWN;
        case SDLK_LEFT:      return PICO8_KEY_LEFT;
        case SDLK_RIGHT:     return PICO8_KEY_RIGHT;
        case SDLK_HOME:      return PICO8_KEY_HOME;
        case SDLK_END:       return PICO8_KEY_END;
        default:             return 0;
    }
}

/* Convert host window pixel coordinates to PICO-8 screen pixels (0..127). */
static void mouse_screen_pos(int wx, int wy, int *sx, int *sy)
{
    int ww = PICO8_SCREEN_W, wh = PICO8_SCREEN_H;
    if (window) SDL_GetWindowSize(window, &ww, &wh);
    if (ww < 1) ww = 1;
    if (wh < 1) wh = 1;
    int x = wx * PICO8_SCREEN_W / ww;
    int y = wy * PICO8_SCREEN_H / wh;
    if (x < 0) x = 0; else if (x > PICO8_SCREEN_W - 1) x = PICO8_SCREEN_W - 1;
    if (y < 0) y = 0; else if (y > PICO8_SCREEN_H - 1) y = PICO8_SCREEN_H - 1;
    *sx = x;
    *sy = y;
}

/* Track POKE(0x5f2d, flags): flag 0x4 enables pointer lock (relative mouse),
   flag 0x1 additionally enables keyboard text capture while running. */
static void update_devkit_modes(void)
{
    int devkit = (app_mode == PICO8_APP_RUN) ? pico8_ram[0x5f2d] : 0;

    int want_rel = (devkit & 4) ? 1 : 0;
    if (want_rel != rel_mode) {
        SDL_SetRelativeMouseMode(want_rel ? SDL_TRUE : SDL_FALSE);
        rel_mode = want_rel;
    }

    int want_text = (app_mode == PICO8_APP_SHELL) ||
                    (app_mode == PICO8_APP_RUN && (devkit & 1));
    if (want_text != text_input_on) {
        if (want_text) SDL_StartTextInput();
        else           SDL_StopTextInput();
        text_input_on = want_text;
    }
}

static void process_events(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_TEXTINPUT:
                if (app_mode == PICO8_APP_SHELL) {
                    pico8_console_shell_text(ev.text.text);
                } else if (app_mode == PICO8_APP_RUN && (pico8_ram[0x5f2d] & 1)) {
                    for (const char *p = ev.text.text; *p; p++)
                        pico8_keyboard_char((unsigned char)*p);
                }
                break;

            case SDL_MOUSEMOTION:
                if (app_mode != PICO8_APP_RUN) break;
                pico8_mouse_relative(ev.motion.xrel, ev.motion.yrel);
                if (!rel_mode) {
                    int sx, sy;
                    mouse_screen_pos(ev.motion.x, ev.motion.y, &sx, &sy);
                    pico8_mouse_motion(sx, sy);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if (app_mode != PICO8_APP_RUN) break;
                pico8_mouse_button(ev.button.button,
                                   ev.type == SDL_MOUSEBUTTONDOWN);
                break;

            case SDL_MOUSEWHEEL:
                if (app_mode != PICO8_APP_RUN) break;
                pico8_mouse_wheel_add(ev.wheel.y);
                break;

            case SDL_KEYDOWN:
                if (ev.key.repeat) break;

                if (app_mode == PICO8_APP_BOOT) {
                    boot_skipped = 1;
                    break;
                }

                if (app_mode == PICO8_APP_SHELL) {
                    int k = translate_shell_key(ev.key.keysym.sym);
                    if (k) pico8_console_shell_key(k);
                    break;
                }

                /* PICO8_APP_RUN */
                if (pico8_ram[0x5f2d] & 1) {
                    /* Non-printable keys are not delivered as text input. */
                    switch (ev.key.keysym.sym) {
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:  pico8_keyboard_char('\r'); break;
                        case SDLK_BACKSPACE: pico8_keyboard_char(8);    break;
                        case SDLK_TAB:       pico8_keyboard_char(9);    break;
                        case SDLK_DELETE:    pico8_keyboard_char(127);  break;
                    }
                }
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE: stop_cart_to_shell(); break;
                    case SDLK_LEFT:   pico8_btn_state[0] |= (1 << PICO8_BTN_LEFT);   break;
                    case SDLK_RIGHT:  pico8_btn_state[0] |= (1 << PICO8_BTN_RIGHT);  break;
                    case SDLK_UP:     pico8_btn_state[0] |= (1 << PICO8_BTN_UP);     break;
                    case SDLK_DOWN:   pico8_btn_state[0] |= (1 << PICO8_BTN_DOWN);   break;
                    case SDLK_z:
                    case SDLK_c:      pico8_btn_state[0] |= (1 << PICO8_BTN_O);      break;
                    case SDLK_x:
                    case SDLK_v:      pico8_btn_state[0] |= (1 << PICO8_BTN_X);      break;
                }
                break;

            case SDL_KEYUP:
                if (app_mode != PICO8_APP_RUN) break;
                switch (ev.key.keysym.sym) {
                    case SDLK_LEFT:   pico8_btn_state[0] &= ~(1 << PICO8_BTN_LEFT);   break;
                    case SDLK_RIGHT:  pico8_btn_state[0] &= ~(1 << PICO8_BTN_RIGHT);  break;
                    case SDLK_UP:     pico8_btn_state[0] &= ~(1 << PICO8_BTN_UP);     break;
                    case SDLK_DOWN:   pico8_btn_state[0] &= ~(1 << PICO8_BTN_DOWN);   break;
                    case SDLK_z:
                    case SDLK_c:      pico8_btn_state[0] &= ~(1 << PICO8_BTN_O);      break;
                    case SDLK_x:
                    case SDLK_v:      pico8_btn_state[0] &= ~(1 << PICO8_BTN_X);      break;
                }
                break;

            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int w = ev.window.data1;
                    int h = ev.window.data2;
                    scale = w / PICO8_SCREEN_W;
                    if (scale < 1) scale = 1;
                    if (h / PICO8_SCREEN_H < scale)
                        scale = h / PICO8_SCREEN_H;
                    if (scale < 1) scale = 1;
                }
                break;
        }
    }
}

static int init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL2 init failed: %s\n", SDL_GetError());
        return -1;
    }

    window = SDL_CreateWindow(WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        PICO8_SCREEN_W * scale, PICO8_SCREEN_H * scale,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_RenderSetLogicalSize(renderer, PICO8_SCREEN_W * scale, PICO8_SCREEN_H * scale);

    screen = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        PICO8_SCREEN_W, PICO8_SCREEN_H);
    if (!screen) {
        fprintf(stderr, "Texture creation failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    return 0;
}

static void render_frame(void)
{
    pico8_render_screen(pixels, PICO8_SCREEN_W * 4, 1);

    SDL_UpdateTexture(screen, NULL, pixels, PICO8_SCREEN_W * 4);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, screen, NULL, NULL);
    SDL_RenderPresent(renderer);
}

/* ------------------------------------------------------------------ */
/*  Cartridge control                                                  */
/* ------------------------------------------------------------------ */

static void enter_shell(void)
{
    app_mode = PICO8_APP_SHELL;
    cart_loaded = 0;
    pico8_reset_input();
    pico8_console_shell_reset();
}

static void start_cart(void)
{
    pico8_reset_input();
    pico8_vm_halted = 0;
    pico8_frame_count = 0;

    has_init     = pico8_vm_has_function("_init");
    has_draw     = pico8_vm_has_function("_draw");
    has_update   = pico8_vm_has_function("_update");
    has_update60 = pico8_vm_has_function("_update60");

    if (has_init) pico8_vm_call_init();

    cart_loaded = 1;
    app_mode = PICO8_APP_RUN;
    pico8_console_shell_reset();
}

static void stop_cart_to_shell(void)
{
    app_mode = PICO8_APP_SHELL;
    cart_loaded = 0;
    pico8_reset_input();
    pico8_console_shell_reset();
}

static void handle_console_requests(void)
{
    if (pico8_console_request_reboot) {
        pico8_console_request_reboot = 0;
        pico8_vm_halted = 0;
        cart_loaded = 0;
        pending_cart = NULL;
        pico8_console_init();
        app_mode = PICO8_APP_BOOT;
        boot_start = SDL_GetTicks();
        boot_skipped = 0;
    }

    if (pico8_console_request_load) {
        pico8_console_request_load = 0;
        std::string path  = pico8_console_pending_path();
        std::string param = pico8_console_pending_param();

        cart_loaded = 0;
        pico8_vm_halted = 0;
        pico8_reset_memory();

        /* Restart the VM so globals from the previous cart do not leak. */
        pico8_vm_destroy();
        pico8_vm_init();
        pico8_vm_set_cmdline(param.empty() ? NULL : param.c_str());

        if (!path.empty() && pico8_console_load(path.c_str()) == 0) {
            start_cart();
        } else {
            pico8_console_print("load: failed");
            enter_shell();
        }
    }

    if (pico8_console_request_run && app_mode == PICO8_APP_SHELL) {
        pico8_console_request_run = 0;
        start_cart();
    }

    if (pico8_console_request_pause && app_mode == PICO8_APP_RUN) {
        pico8_console_request_pause = 0;
        stop_cart_to_shell();
    }
    pico8_console_request_pause = 0;

    if (pico8_console_request_quit)
        running = 0;
}

/* ------------------------------------------------------------------ */
/*  Main loop                                                          */
/* ------------------------------------------------------------------ */

static void main_loop(void)
{
    uint32_t frame_time = 1000 / PICO8_FPS;
    uint32_t next_frame = SDL_GetTicks();

    while (running) {
        process_events();
        handle_console_requests();
        update_devkit_modes();

        if (app_mode == PICO8_APP_BOOT) {
            uint32_t elapsed = SDL_GetTicks() - boot_start;
            if (boot_skipped || pico8_console_boot_finished(elapsed)) {
                if (pending_cart) {
                    pico8_reset_memory();
                    if (pico8_console_load(pending_cart) == 0) {
                        pending_cart = NULL;
                        start_cart();
                    } else {
                        fprintf(stderr, "Failed to load cart: %s\n", pending_cart);
                        pico8_console_print("failed to load cartridge");
                        pending_cart = NULL;
                        enter_shell();
                    }
                } else {
                    enter_shell();
                }
            } else {
                pico8_console_boot_draw(elapsed);
            }
        } else if (app_mode == PICO8_APP_SHELL) {
            pico8_console_shell_draw();
        } else { /* PICO8_APP_RUN */
            if (has_update60 || (pico8_frame_count % 2) == 0) {
                pico8_update_input(has_update60);
                pico8_vm_call_update();
                if (has_draw) pico8_vm_call_draw();
                pico8_input_frame_end();
            }
            pico8_frame_count++;

            if (pico8_vm_halted)
                stop_cart_to_shell();
        }

        render_frame();

        uint32_t now = SDL_GetTicks();
        if (now < next_frame)
            SDL_Delay(next_frame - now);
        next_frame += frame_time;
        if (next_frame < now) next_frame = now + frame_time;
    }
}

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    const char *cart_path = NULL;
    const char *cmdline_args = NULL;
    int headless = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            cmdline_args = argv[++i];
        } else if (strcmp(argv[i], "-headless") == 0) {
            headless = 1;
        } else if (cart_path == NULL && argv[i][0] != '-') {
            cart_path = argv[i];
        }
    }

    if (pico8_vm_init() != 0) {
        fprintf(stderr, "VM initialization failed\n");
        return 1;
    }

    pico8_vm_set_cmdline(cmdline_args);
    pico8_console_init();

    if (headless) {
        if (!cart_path) {
            fprintf(stderr, "Usage: pico-c -headless [-p <args>] <cart.p8>\n");
            pico8_vm_destroy();
            return 1;
        }
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (SDL_Init(SDL_INIT_TIMER) < 0) {
            fprintf(stderr, "SDL2 timer init failed: %s\n", SDL_GetError());
            pico8_vm_destroy();
            return 1;
        }

        pico8_reset_memory();
        if (pico8_console_load(cart_path) == 0) {
            has_init     = pico8_vm_has_function("_init");
            has_draw     = pico8_vm_has_function("_draw");
            has_update   = pico8_vm_has_function("_update");
            has_update60 = pico8_vm_has_function("_update60");
            if (has_init) pico8_vm_call_init();
            cart_loaded = 1;
        } else {
            fprintf(stderr, "Failed to load cart: %s\n", cart_path);
        }

        int frame_limit = 7200;
        uint32_t start = SDL_GetTicks();
        while (frame_limit > 0 &&
               (!pico8_vm_halted || pico8_console_request_load)) {
            if (pico8_console_request_load) {
                pico8_console_request_load = 0;
                std::string lpath = pico8_console_pending_path();
                std::string lparam = pico8_console_pending_param();
                pico8_vm_destroy();
                pico8_vm_init();
                pico8_vm_set_cmdline(lparam.empty() ? NULL : lparam.c_str());
                pico8_reset_memory();
                if (!lpath.empty() && pico8_console_load(lpath.c_str()) == 0) {
                    has_init     = pico8_vm_has_function("_init");
                    has_draw     = pico8_vm_has_function("_draw");
                    has_update   = pico8_vm_has_function("_update");
                    has_update60 = pico8_vm_has_function("_update60");
                    pico8_vm_halted = 0;
                    pico8_frame_count = 0;
                    if (has_init) pico8_vm_call_init();
                }
                frame_limit--;
                continue;
            }
            if (has_update60 || (pico8_frame_count % 2) == 0) {
                pico8_update_input(has_update60);
                pico8_vm_call_update();
                if (has_draw) pico8_vm_call_draw();
            }
            pico8_frame_count++;
            frame_limit--;
            if (frame_limit % 60 == 0 && SDL_GetTicks() - start > 120000)
                break;
        }

        pico8_vm_destroy();
        SDL_Quit();
        return 0;
    }

    if (init_sdl() != 0) {
        pico8_vm_destroy();
        return 1;
    }

    pico8_audio_init();

    /* Start on the boot splash. A cartridge given on the command line is
       queued and run once the splash finishes. */
    pending_cart = cart_path;
    app_mode = PICO8_APP_BOOT;
    boot_start = SDL_GetTicks();
    boot_skipped = 0;

    main_loop();

    pico8_audio_destroy();
    pico8_vm_destroy();

    if (screen)   SDL_DestroyTexture(screen);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
