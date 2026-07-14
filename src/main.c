/* Pocket Frame — a picture frame for Onion OS (Miyoo Mini+).
 * Fetches a remote image and displays it fullscreen, refreshing every N seconds.
 *
 * Config: settings.conf next to the binary — one or more url= lines (sources),
 * interval= (seconds) and selected= (active source). The welcome menu edits
 * everything on-device and saves back to the file on every change.
 *
 * Welcome: UP/DOWN select option, LEFT/RIGHT change value (hold to accelerate),
 *          X edit the URL, A start, B/MENU exit
 * Editor:  d-pad navigates the keyboard, A types, B deletes, START saves,
 *          MENU cancels
 * Running: A refresh now, LEFT/RIGHT switch source, B back to welcome, MENU exit
 *
 * ponytail: downloads via Onion's own `curl` (fork/exec), no libcurl linked.
 * ponytail: no scaling — serve images at 640x480; other sizes are centered.
 * ponytail: editor is lowercase-only and appends at the end (no mid-string
 *           cursor); L1/R1 cursor movement is the upgrade if ever needed.
 * ponytail: SDL_GetTicks wraps at ~49 days, so intervals up to ~1 month are
 *           safe; beyond that you'd need a wall-clock time source.
 */
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define FONT_PATH "/mnt/SDCARD/miyoo/app/Helvetica-Neue-2.ttf" /* shipped with Onion */
#define W 640
#define H 480
#define TMP_IMG "/tmp/pocketframe_download"
#define MAX_URLS 8

static char urls[MAX_URLS][512];
static int n_urls = 0;
static int sel = 0;
static int interval = 60;

/* URL editor keyboard */
static const char *KB_ROWS[] = {
    "abcdefghijklm",
    "nopqrstuvwxyz",
    "0123456789.-_",
    ":/?=&%#@+~",
};
#define KB_NROWS (int)(sizeof KB_ROWS / sizeof *KB_ROWS)

static const char *cur_url(void)
{
    return n_urls ? urls[sel] : "";
}

static void load_config(void)
{
    FILE *f = fopen("settings.conf", "r");
    char line[600];
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = '\0';
        if (!strncmp(line, "url=", 4) && line[4] && n_urls < MAX_URLS)
            snprintf(urls[n_urls++], sizeof urls[0], "%s", line + 4);
        else if (!strncmp(line, "interval=", 9))
            interval = atoi(line + 9);
        else if (!strncmp(line, "selected=", 9))
            sel = atoi(line + 9);
    }
    fclose(f);
    if (interval < 5) interval = 5;
    if (sel < 0 || sel >= n_urls) sel = 0;
}

static void save_config(void)
{
    FILE *f = fopen("settings.conf", "w");
    int i;
    if (!f) return;
    for (i = 0; i < n_urls; i++)
        fprintf(f, "url=%s\n", urls[i]);
    fprintf(f, "interval=%d\nselected=%d\n", interval, sel);
    fclose(f);
}

/* +/-5s per press, min 5, no ceiling; holding the key accelerates the step
 * (5s -> 1m -> 1h) while keeping the value a multiple of 5 */
static void step_interval(int dir, int repeats)
{
    int step = repeats < 12 ? 5 : repeats < 36 ? 60 : 3600;
    interval += dir * step;
    if (interval < 5) interval = 5;
}

static void format_interval(char *out, size_t n)
{
    if (interval < 60)
        snprintf(out, n, "every %ds", interval);
    else if (interval < 3600)
        snprintf(out, n, "every %dm %ds", interval / 60, interval % 60);
    else
        snprintf(out, n, "every %dh %dm", interval / 3600, (interval % 3600) / 60);
}

/* spawn curl in the background; the main loop stays alive (and blinking) */
static pid_t start_download(void)
{
    char cmd[700];
    snprintf(cmd, sizeof cmd,
             "curl -sfL --connect-timeout 5 --max-time 25 -o %s '%s'",
             TMP_IMG, cur_url());
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    return pid;
}

static void fill_circle(SDL_Surface *s, int cx, int cy, int r, Uint32 col)
{
    int x, y;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) {
                SDL_Rect p = { (Sint16)(cx + x), (Sint16)(cy + y), 1, 1 };
                SDL_FillRect(s, &p, col);
            }
}

/* ponytail: the Miyoo Mini panel is mounted 180° rotated and SDL doesn't
 * compensate; we compose into a buffer and copy it rotated to the framebuffer. */
static void flip180(SDL_Surface *src, SDL_Surface *dst)
{
    int x, y;
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (y = 0; y < H; y++) {
        Uint32 *s = (Uint32 *)((Uint8 *)src->pixels + y * src->pitch);
        Uint32 *d = (Uint32 *)((Uint8 *)dst->pixels + (H - 1 - y) * dst->pitch);
        for (x = 0; x < W; x++)
            d[W - 1 - x] = s[x];
    }
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

static void draw_text(SDL_Surface *dst, TTF_Font *f, int x, int y,
                      const char *txt, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Color c = { r, g, b, 0 };
    SDL_Surface *s = TTF_RenderUTF8_Blended(f, txt, c);
    if (!s) return;
    SDL_Rect at = { (Sint16)x, (Sint16)y, 0, 0 };
    SDL_BlitSurface(s, NULL, dst, &at);
    SDL_FreeSurface(s);
}

/* key legend entry: key in green, description in gray, aligned by measure */
static void draw_key(SDL_Surface *buf, TTF_Font *f, int x, int y,
                     const char *key, const char *desc)
{
    int w = 0, h;
    draw_text(buf, f, x, y, key, 0x39, 0xd3, 0x53);
    TTF_SizeUTF8(f, key, &w, &h);
    draw_text(buf, f, x + w + 10, y, desc, 0x8d, 0x96, 0xa0);
}

/* welcome menu: row 0 = source, row 1 = refresh interval */
static void render_welcome(SDL_Surface *screen, SDL_Surface *buf,
                           TTF_Font *big, TTF_Font *small, int row)
{
    char line[600];
    SDL_FillRect(buf, NULL, SDL_MapRGB(buf->format, 13, 17, 23));
    if (big && small) {
        draw_text(buf, big, 40, 44, "POCKET FRAME", 0xe6, 0xed, 0xf3);

        snprintf(line, sizeof line, "%s SOURCE  %d/%d",
                 row == 0 ? ">" : " ", n_urls ? sel + 1 : 0, n_urls);
        if (row == 0) draw_text(buf, small, 40, 124, line, 0x39, 0xd3, 0x53);
        else          draw_text(buf, small, 40, 124, line, 0x8d, 0x96, 0xa0);
        snprintf(line, sizeof line, "%s",
                 n_urls ? cur_url() : "(no url= in settings.conf)");
        line[66] = '\0'; /* clamp huge URLs to one line */
        draw_text(buf, small, 58, 150, line, 0xe6, 0xed, 0xf3);

        snprintf(line, sizeof line, "%s REFRESH", row == 1 ? ">" : " ");
        if (row == 1) draw_text(buf, small, 40, 208, line, 0x39, 0xd3, 0x53);
        else          draw_text(buf, small, 40, 208, line, 0x8d, 0x96, 0xa0);
        format_interval(line, sizeof line);
        draw_text(buf, small, 58, 234, line, 0xe6, 0xed, 0xf3);

        draw_text(buf, small, 40, 268, "THIS MENU", 0x6e, 0x76, 0x81);
        draw_key(buf, small, 40, 294, "A", "start");
        draw_key(buf, small, 40, 320, "B", "exit");
        draw_key(buf, small, 190, 294, "^ v", "select");
        draw_key(buf, small, 190, 320, "< >", "change");
        draw_key(buf, small, 40, 346, "X", "edit url");

        draw_text(buf, small, 390, 268, "WHILE RUNNING", 0x6e, 0x76, 0x81);
        draw_key(buf, small, 390, 294, "A", "force refresh");
        draw_key(buf, small, 390, 320, "B", "back to menu");
        draw_key(buf, small, 390, 346, "< >", "switch source");

        draw_text(buf, big, 40, 400, "Press A to start", 0x39, 0xd3, 0x53);
    } else {
        /* no font available: signal with a green circle and carry on */
        fill_circle(buf, W / 2, H / 2, 12, SDL_MapRGB(buf->format, 0x39, 0xd3, 0x53));
    }
    flip180(buf, screen);
    SDL_Flip(screen);
}

static void render_edit(SDL_Surface *screen, SDL_Surface *buf,
                        TTF_Font *big, TTF_Font *small,
                        const char *text, int kb_r, int kb_c)
{
    char line[80];
    int r, c;
    size_t len = strlen(text);
    SDL_FillRect(buf, NULL, SDL_MapRGB(buf->format, 13, 17, 23));
    if (!big || !small) { /* can't edit without a font */
        flip180(buf, screen);
        SDL_Flip(screen);
        return;
    }
    draw_text(buf, big, 40, 28, "EDIT SOURCE", 0xe6, 0xed, 0xf3);

    /* show the tail of the URL (where typing happens), with a cursor */
    if (len > 62)
        snprintf(line, sizeof line, "\xe2\x80\xa6%s_", text + len - 61);
    else
        snprintf(line, sizeof line, "%s_", text);
    draw_text(buf, small, 40, 96, line, 0x39, 0xd3, 0x53);

    for (r = 0; r < KB_NROWS; r++) {
        for (c = 0; KB_ROWS[r][c]; c++) {
            int x = 40 + c * 44, y = 160 + r * 54;
            char ch[2] = { KB_ROWS[r][c], '\0' };
            if (r == kb_r && c == kb_c) {
                SDL_Rect hl = { (Sint16)(x - 10), (Sint16)(y - 8), 38, 42 };
                SDL_FillRect(buf, &hl, SDL_MapRGB(buf->format, 0x21, 0x26, 0x2d));
                draw_text(buf, big, x - 2, y - 6, ch, 0x39, 0xd3, 0x53);
            } else {
                draw_text(buf, small, x, y, ch, 0xe6, 0xed, 0xf3);
            }
        }
    }

    draw_key(buf, small, 40, 420, "A", "type");
    draw_key(buf, small, 148, 420, "B", "delete");
    draw_key(buf, small, 280, 420, "START", "save");
    draw_key(buf, small, 448, 420, "MENU", "cancel");
    flip180(buf, screen);
    SDL_Flip(screen);
}

/* top-right indicator: subtle gray blink while fetching; solid red if the
 * last fetch failed; nothing when all good */
static void render(SDL_Surface *screen, SDL_Surface *buf, SDL_Surface *img,
                   int fail, int pending, int blink_on)
{
    SDL_FillRect(buf, NULL, SDL_MapRGB(buf->format, 13, 17, 23));
    if (img) {
        SDL_Rect dst = { (Sint16)((W - img->w) / 2), (Sint16)((H - img->h) / 2), 0, 0 };
        SDL_BlitSurface(img, NULL, buf, &dst);
    }
    if (pending && blink_on)
        fill_circle(buf, W - 12, 12, 4, SDL_MapRGB(buf->format, 0x44, 0x4c, 0x56));
    else if (!pending && fail)
        fill_circle(buf, W - 12, 12, 4, SDL_MapRGB(buf->format, 0xda, 0x36, 0x33));
    flip180(buf, screen);
    SDL_Flip(screen);
}

int main(void)
{
    load_config();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 40); /* hold LEFT/RIGHT to accelerate; hold B to erase */
    SDL_Surface *screen = SDL_SetVideoMode(W, H, 32, SDL_SWSURFACE);
    if (!screen) {
        fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Surface *buf = SDL_CreateRGBSurface(SDL_SWSURFACE, W, H, 32,
                                            screen->format->Rmask, screen->format->Gmask,
                                            screen->format->Bmask, screen->format->Amask);
    if (!buf) {
        fprintf(stderr, "SDL_CreateRGBSurface: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    TTF_Font *font_big = NULL, *font_small = NULL;
    if (TTF_Init() == 0) {
        font_big = TTF_OpenFont(FONT_PATH, 30);
        font_small = TTF_OpenFont(FONT_PATH, 18);
    }
    if (!font_big || !font_small)
        fprintf(stderr, "font unavailable (%s): welcome without text\n", FONT_PATH);

    enum { WELCOME, RUN, EDIT } state = WELCOME;
    SDL_Surface *current = NULL;
    int fail = 0, force = 1, running = 1, pending = 0, blink_on = 0, row = 0;
    int kb_r = 0, kb_c = 0, repeats = 0;
    char edit_buf[512] = "";
    Uint32 last = 0, blink_t = 0;
    pid_t dl_pid = -1;

    if (!n_urls)
        fprintf(stderr, "settings.conf missing or has no url= line\n");

    render_welcome(screen, buf, font_big, font_small, row);

    while (running) {
        Uint32 now = SDL_GetTicks();

        if (state == RUN && n_urls && !pending &&
            (force || now - last >= (Uint32)interval * 1000)) {
            force = 0;
            last = now;
            dl_pid = start_download();
            pending = (dl_pid > 0);
            blink_on = 1;
            blink_t = now;
            render(screen, buf, current, fail, pending, blink_on);
        }

        if (pending) {
            int status;
            if (waitpid(dl_pid, &status, WNOHANG) == dl_pid) {
                pending = 0;
                fail = 1;
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    SDL_Surface *raw = IMG_Load(TMP_IMG); /* decoding IS the validation */
                    if (raw) {
                        SDL_Surface *conv = SDL_DisplayFormat(raw);
                        SDL_FreeSurface(raw);
                        if (conv) {
                            if (current) SDL_FreeSurface(current);
                            current = conv;
                            fail = 0;
                        }
                    }
                }
                if (state == RUN) /* download may finish while user is in welcome */
                    render(screen, buf, current, fail, 0, 0);
            } else if (state == RUN && now - blink_t >= 500) {
                blink_t = now;
                blink_on = !blink_on;
                render(screen, buf, current, fail, pending, blink_on);
            }
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYUP) {
                repeats = 0; /* interval acceleration resets on release */
            } else if (e.type == SDL_KEYDOWN && state == EDIT) {
                SDLKey k = e.key.keysym.sym;
                int rlen = (int)strlen(KB_ROWS[kb_r]);
                size_t elen = strlen(edit_buf);
                switch (k) {
                case SDLK_UP:
                case SDLK_DOWN:
                    kb_r = (kb_r + (k == SDLK_DOWN ? 1 : -1) + KB_NROWS) % KB_NROWS;
                    rlen = (int)strlen(KB_ROWS[kb_r]);
                    if (kb_c >= rlen) kb_c = rlen - 1;
                    break;
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    kb_c = (kb_c + (k == SDLK_RIGHT ? 1 : -1) + rlen) % rlen;
                    break;
                case SDLK_SPACE:  /* A: type */
                    if (elen < sizeof edit_buf - 1) {
                        edit_buf[elen] = KB_ROWS[kb_r][kb_c];
                        edit_buf[elen + 1] = '\0';
                    }
                    break;
                case SDLK_LCTRL:  /* B: delete */
                    if (elen) edit_buf[elen - 1] = '\0';
                    break;
                case SDLK_RETURN: /* START: save (if non-empty) */
                    if (edit_buf[0]) {
                        snprintf(urls[sel], sizeof urls[0], "%s", edit_buf);
                        save_config();
                        force = 1; /* source changed; refetch on start */
                    }
                    state = WELCOME;
                    render_welcome(screen, buf, font_big, font_small, row);
                    break;
                case SDLK_ESCAPE: /* MENU: cancel */
                    state = WELCOME;
                    render_welcome(screen, buf, font_big, font_small, row);
                    break;
                default:
                    break;
                }
                if (state == EDIT)
                    render_edit(screen, buf, font_big, font_small, edit_buf, kb_r, kb_c);
            } else if (e.type == SDL_KEYDOWN) {
                SDLKey k = e.key.keysym.sym;
                switch (k) {
                case SDLK_ESCAPE: /* MENU: exit from any screen */
                    running = 0;
                    break;
                case SDLK_LCTRL:  /* B: running -> welcome; welcome -> exit */
                    if (state == RUN) {
                        state = WELCOME;
                        render_welcome(screen, buf, font_big, font_small, row);
                    } else {
                        running = 0;
                    }
                    break;
                case SDLK_SPACE:  /* A: welcome -> start; running -> refresh */
                    if (state == WELCOME) {
                        state = RUN;
                        render(screen, buf, current, fail, pending, blink_on);
                        if (!pending && now - last >= (Uint32)interval * 1000)
                            force = 1;
                    } else {
                        force = 1;
                    }
                    break;
                case SDLK_UP:
                case SDLK_DOWN:
                    if (state == WELCOME) {
                        row = !row;
                        render_welcome(screen, buf, font_big, font_small, row);
                    }
                    break;
                case SDLK_LSHIFT: /* X: edit the selected source URL */
                    if (state == WELCOME && n_urls) {
                        snprintf(edit_buf, sizeof edit_buf, "%s", urls[sel]);
                        kb_r = kb_c = 0;
                        state = EDIT;
                        render_edit(screen, buf, font_big, font_small, edit_buf, kb_r, kb_c);
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_RIGHT: {
                    int dir = (k == SDLK_RIGHT) ? 1 : -1;
                    if (state == WELCOME) {
                        if (row == 0 && n_urls)
                            sel = (sel + dir + n_urls) % n_urls;
                        else if (row == 1)
                            step_interval(dir, repeats++);
                        save_config();
                        render_welcome(screen, buf, font_big, font_small, row);
                    } else if (n_urls > 1) { /* running: switch source live */
                        sel = (sel + dir + n_urls) % n_urls;
                        save_config();
                        force = 1;
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        SDL_Delay(100);
    }

    if (current) SDL_FreeSurface(current);
    SDL_FreeSurface(buf);
    if (font_big) TTF_CloseFont(font_big);
    if (font_small) TTF_CloseFont(font_small);
    if (TTF_WasInit()) TTF_Quit();
    SDL_Quit();
    return 0;
}
