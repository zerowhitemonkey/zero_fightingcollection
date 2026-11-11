// Download of logos / backgrounds from SteamGridDB (user key in steamgriddb_key.txt)
#ifndef ARTDL_H
#define ARTDL_H
typedef struct {
    const char *id;        // file names: artwork/<id>_logo.png, artwork/<id>_bg.jpg
    const char *title;     // SteamGridDB search
    const char *search;    // optional better search name (NULL = title)
    int has_logo, has_bg;  // already available (assets or artwork): not downloaded
} artdl_game_t;
typedef struct {
    int running, done, total, current, downloaded, missing, has_key;
    char title[96], message[96];
} artdl_status_t;
int artdl_start(const char *basedir, const artdl_game_t *list, int n);   // 0 = already running / failed
void artdl_get_status(artdl_status_t *out);
#endif
