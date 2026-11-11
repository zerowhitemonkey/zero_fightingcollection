// Download of game logos and backgrounds from public sources (ROM CHECK -> X).
// Source: SteamGridDB (needs the user's own free API key in steamgriddb_key.txt): transparent logo + hero image.
// Files are written to <app>/artwork/<id>_logo.png and <id>_bg.jpg; existing files are never overwritten.
// Runs in a background thread, one request at a time.
#include "artdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>
#include "../ext/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb_image_write.h"
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static artdl_status_t st;
static SDL_mutex *mtx;
static SDL_Thread *thr;
static char base[1024], key[256];
static int ngames;
static artdl_game_t games[64];

void artdl_get_status(artdl_status_t *out) {
    if (!mtx) { memset(out, 0, sizeof *out); return; }
    SDL_LockMutex(mtx); *out = st; SDL_UnlockMutex(mtx);
}
static void set_status(int cur, const char *title, const char *msg) {
    SDL_LockMutex(mtx);
    st.current = cur;
    if (title) snprintf(st.title, sizeof st.title, "%s", title);
    if (msg) snprintf(st.message, sizeof st.message, "%s", msg);
    SDL_UnlockMutex(mtx);
}

// ------------------------------------------------------------------ HTTP (WinHTTP)
#ifdef _WIN32
static unsigned char *http_get(const char *url, const char *auth, size_t *len, int *status) {
    *len = 0; *status = 0;
    wchar_t wurl[2048]; MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048);
    URL_COMPONENTS uc; memset(&uc, 0, sizeof uc); uc.dwStructSize = sizeof uc;
    wchar_t host[256], path[1800];
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 1800;
    wchar_t extra[1024]; uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return NULL;
    wcsncat(path, extra, 1800 - wcslen(path) - 1);
    HINTERNET s = WinHttpOpen(L"FightingCollection/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(L"FightingCollection/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return NULL;
    WinHttpSetTimeouts(s, 10000, 10000, 20000, 30000);
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : NULL;
    unsigned char *buf = NULL; size_t cap = 0;
    if (r) {
        wchar_t hdr[400] = L"";
        if (auth && *auth) { wchar_t wa[300]; MultiByteToWideChar(CP_UTF8, 0, auth, -1, wa, 300); swprintf(hdr, 400, L"Authorization: Bearer %ls\r\n", wa); }
        if (WinHttpSendRequest(r, hdr[0] ? hdr : WINHTTP_NO_ADDITIONAL_HEADERS, hdr[0] ? (DWORD)-1L : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(r, NULL)) {
            DWORD code = 0, sz = sizeof code;
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
            *status = (int)code;
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(r, &avail) || !avail) break;
                if (*len + avail + 1 > cap) { cap = (*len + avail + 1) * 2; buf = (unsigned char*)realloc(buf, cap); }
                DWORD rd = 0;
                if (!WinHttpReadData(r, buf + *len, avail, &rd) || !rd) break;
                *len += rd;
                if (*len > (64u << 20)) break;   // limit 64 MB
            }
            if (buf) buf[*len] = 0;
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return buf;
}
#else
// non Windows: nessuna rete; per il banco di prova ARTDL_FAKE=<cartella> legge le risposte da file (URL con '/' -> '_')
static unsigned char *http_get(const char *url, const char *auth, size_t *len, int *status) {
    (void)auth; *len = 0; *status = 0;
    const char *d = getenv("ARTDL_FAKE");
    if (!d) return NULL;
    char p[2048]; snprintf(p, sizeof p, "%s/", d); size_t o = strlen(p);
    for (const char *s = url; *s && o + 1 < sizeof p; ++s) p[o++] = (*s == '/' || *s == '?' || *s == '&' || *s == ':') ? '_' : *s;
    p[o] = 0;
    FILE *f = fopen(p, "rb");
    if (!f) { fprintf(stderr, "fake miss: %s\n", p); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char*)malloc(n + 1); *len = fread(b, 1, n, f); b[*len] = 0; fclose(f);
    *status = 200; return b;
}
#endif

// ------------------------------------------------------------------ minimal JSON helpers
// value of the first "name":"..." after p (unescaped); returns pointer after the value or NULL
static const char *json_str(const char *p, const char *name, char *out, size_t n) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", name);
    const char *q = p ? strstr(p, pat) : NULL;
    if (!q) return NULL;
    q += strlen(pat);
    while (*q == ' ' || *q == ':') ++q;
    if (*q != '"') { if (n) out[0] = 0; return q; }
    ++q;
    size_t o = 0;
    while (*q && *q != '"') {
        char ch = *q++;
        if (ch == '\\' && *q) { ch = *q++; if (ch == 'u') { q += 4; ch = '?'; } else if (ch == 'n') ch = ' '; }
        if (o + 1 < n) out[o++] = ch;
    }
    if (n) out[o] = 0;
    return *q ? q + 1 : q;
}
static long json_num(const char *p, const char *name, const char **after) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", name);
    const char *q = p ? strstr(p, pat) : NULL;
    if (!q) return -1;
    q += strlen(pat);
    while (*q == ' ' || *q == ':') ++q;
    long v = strtol(q, (char**)&q, 10);
    if (after) *after = q;
    return v;
}
static void norm(const char *s, char *o, size_t n) {   // lowercase, letters and digits only
    size_t k = 0;
    for (; *s && k + 1 < n; ++s) if (isalnum((unsigned char)*s)) o[k++] = (char)tolower((unsigned char)*s);
    o[k] = 0;
}
static void urlenc(const char *s, char *o, size_t n) {
    size_t k = 0;
    for (; *s && k + 4 < n; ++s) {
        unsigned char ch = (unsigned char)*s;
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.') o[k++] = (char)ch;
        else k += snprintf(o + k, n - k, "%%%02X", ch);
    }
    o[k] = 0;
}

// ------------------------------------------------------------------ images
static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }
// resample RGBA to fit within mw x mh (only shrinks), box filter
static unsigned char *shrink(const unsigned char *src, int w, int h, int mw, int mh, int *ow, int *oh) {
    double s = 1.0;
    if (w > mw) s = (double)mw / w;
    if (h * s > mh) s = (double)mh / h;
    int nw = (int)(w * s + 0.5), nh = (int)(h * s + 0.5);
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    unsigned char *d = (unsigned char*)malloc((size_t)nw * nh * 4);
    for (int y = 0; y < nh; ++y) {
        int y0 = (int)((double)y * h / nh), y1 = (int)((double)(y + 1) * h / nh); if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < nw; ++x) {
            int x0 = (int)((double)x * w / nw), x1 = (int)((double)(x + 1) * w / nw); if (x1 <= x0) x1 = x0 + 1;
            unsigned acc[4] = {0}, cnt = 0;
            for (int yy = y0; yy < y1; ++yy) for (int xx = x0; xx < x1; ++xx) {
                const unsigned char *p = src + ((size_t)yy * w + xx) * 4;
                acc[0] += p[0] * p[3]; acc[1] += p[1] * p[3]; acc[2] += p[2] * p[3]; acc[3] += p[3]; ++cnt;
            }
            unsigned char *q = d + ((size_t)y * nw + x) * 4;
            for (int c = 0; c < 3; ++c) q[c] = acc[3] ? (unsigned char)(acc[c] / acc[3]) : 0;
            q[3] = (unsigned char)(acc[3] / cnt);
        }
    }
    *ow = nw; *oh = nh;
    return d;
}
// download an image and save it: logo = PNG (alpha kept, max 720x330, cropped to content), bg = JPG (max 1920x1080)
static int save_image(const char *url, const char *auth, const char *out, int logo) {
    size_t len; int code;
    unsigned char *data = http_get(url, auth, &len, &code);
    if (!data || code != 200 || len < 100) { free(data); return 0; }
    int w, h, n;
    unsigned char *px = stbi_load_from_memory(data, (int)len, &w, &h, &n, 4);
    free(data);
    if (!px) return 0;
    if (logo) {   // crop to the non-transparent content
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) if (px[((size_t)y * w + x) * 4 + 3] > 8) {
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
        if (x1 >= x0 && (x0 > 0 || y0 > 0 || x1 < w - 1 || y1 < h - 1)) {
            int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
            unsigned char *c = (unsigned char*)malloc((size_t)cw * ch * 4);
            for (int y = 0; y < ch; ++y) memcpy(c + (size_t)y * cw * 4, px + ((size_t)(y + y0) * w + x0) * 4, (size_t)cw * 4);
            stbi_image_free(px); px = c; w = cw; h = ch;
        }
    }
    int ow, oh;
    unsigned char *o = shrink(px, w, h, logo ? 720 : 1920, logo ? 330 : 1080, &ow, &oh);
    stbi_image_free(px);
    char tmp[1200]; snprintf(tmp, sizeof tmp, "%s.tmp", out);
    int ok;
    if (logo) ok = stbi_write_png(tmp, ow, oh, 4, o, ow * 4);
    else {
        unsigned char *rgb = (unsigned char*)malloc((size_t)ow * oh * 3);
        for (size_t i = 0; i < (size_t)ow * oh; ++i) memcpy(rgb + i * 3, o + i * 4, 3);
        ok = stbi_write_jpg(tmp, ow, oh, 3, rgb, 90);
        free(rgb);
    }
    free(o);
    if (ok) { remove(out); ok = rename(tmp, out) == 0; }
    if (!ok) remove(tmp);
    return ok;
}

// ------------------------------------------------------------------ sources
// SteamGridDB: search by title; prefers the oldest exact-name match (e.g. the 1993 Samurai Shodown, not the 2019 one)
static long sgdb_find(const char *title) {
    char q[300], url[600], want[200];
    urlenc(title, q, sizeof q);
    norm(title, want, sizeof want);
    snprintf(url, sizeof url, "https://www.steamgriddb.com/api/v2/search/autocomplete/%s", q);
    size_t len; int code;
    char *js = (char*)http_get(url, key, &len, &code);
    if (!js || code != 200) { free(js); return -1; }
    long best = -1, first = -1, bestdate = 0;
    const char *p = strstr(js, "\"data\"");
    while (p) {
        const char *a;
        long id = json_num(p, "id", &a);
        if (id < 0) break;
        char name[200], nn[200];
        const char *b = json_str(p, "name", name, sizeof name);
        if (!b) break;
        long date = json_num(p, "release_date", NULL);
        norm(name, nn, sizeof nn);
        if (first < 0) first = id;
        if (!strcmp(nn, want) && (best < 0 || (date > 0 && date < bestdate))) { best = id; bestdate = date > 0 ? date : bestdate; }
        p = strchr(b, '}');   // next object
        if (p && !strstr(p, "\"id\"")) break;
    }
    free(js);
    return best >= 0 ? best : first;
}
static int sgdb_image(long gid, const char *kind, const char *mimes, const char *out, int logo) {
    char url[300];
    snprintf(url, sizeof url, "https://www.steamgriddb.com/api/v2/%s/game/%ld?mimes=%s&types=static&nsfw=false", kind, gid, mimes);
    size_t len; int code;
    char *js = (char*)http_get(url, key, &len, &code);
    if (!js || code != 200) { free(js); return 0; }
    char img[1024] = "";
    const char *p = strstr(js, "\"data\"");
    if (p) json_str(p, "url", img, sizeof img);
    free(js);
    return img[0] && save_image(img, NULL, out, logo);
}
static int run(void *u) {
    (void)u;
    char dir[1100]; snprintf(dir, sizeof dir, "%s/artwork", base); MKDIR(dir);
    int got = 0, missing = 0;
    for (int i = 0; i < ngames; ++i) {
        artdl_game_t *g = &games[i];
        set_status(i + 1, g->title, "");
        char lp[1200], bp[1200];
        snprintf(lp, sizeof lp, "%s/%s_logo.png", dir, g->id);
        snprintf(bp, sizeof bp, "%s/%s_bg.jpg", dir, g->id);
        int need_logo = !g->has_logo && !file_exists(lp), need_bg = !g->has_bg && !file_exists(bp);
        if (!need_logo && !need_bg) continue;
        if (key[0]) {
            set_status(i + 1, NULL, "SteamGridDB");
            long gid = sgdb_find(g->search ? g->search : g->title);
            if (gid >= 0) {
                if (need_logo && sgdb_image(gid, "logos", "image/png", lp, 1)) { need_logo = 0; ++got; }
                if (need_bg && sgdb_image(gid, "heroes", "image/jpeg,image/png", bp, 0)) { need_bg = 0; ++got; }
            }
        }
        missing += need_logo + need_bg;
    }
    SDL_LockMutex(mtx);
    st.running = 0; st.done = 1; st.downloaded = got; st.missing = missing;
    snprintf(st.message, sizeof st.message, "%s", key[0] ? "" : "no SteamGridDB key");
    SDL_UnlockMutex(mtx);
    return 0;
}

int artdl_start(const char *basedir, const artdl_game_t *list, int n) {
    if (!mtx) mtx = SDL_CreateMutex();
    if (st.running) return 0;
    if (thr) { SDL_WaitThread(thr, NULL); thr = NULL; }
    snprintf(base, sizeof base, "%s", basedir);
    key[0] = 0;
    char kp[1100]; snprintf(kp, sizeof kp, "%s/steamgriddb_key.txt", basedir);
    FILE *f = fopen(kp, "r");
    if (f) {
        if (fgets(key, sizeof key, f)) { char *e = key + strlen(key); while (e > key && (unsigned char)e[-1] <= ' ') *--e = 0; }
        fclose(f);
    }
    ngames = n > 64 ? 64 : n;
    memcpy(games, list, sizeof(artdl_game_t) * ngames);
    memset(&st, 0, sizeof st);
    st.total = ngames; st.has_key = key[0] != 0;
    if (!key[0]) { st.done = 1; snprintf(st.message, sizeof st.message, "no SteamGridDB key"); return 0; }   // senza chiave non si scarica
    st.running = 1;
    thr = SDL_CreateThread(run, "artdl", NULL);
    if (!thr) { st.running = 0; return 0; }
    return 1;
}
