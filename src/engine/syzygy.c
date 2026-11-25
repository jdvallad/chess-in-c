#include "engine/syzygy.h"

#include <dirent.h>
#include <string.h>

static int count_pieces_in_tb_name(const char *fn) {
    // Syzygy filenames look like "KRvK.rtbw", "KRPvKR.rtbz", "KQvKP.rtbw"
    const char *dot = strrchr(fn, '.');
    if (!dot || (strcmp(dot, ".rtbw") != 0 && strcmp(dot, ".rtbz") != 0)) {
        return 0;
    }
    int pieces = 0;
    for (const char *p = fn; p < dot; p++) {
        if (*p == 'K' || *p == 'Q' || *p == 'R' || *p == 'B' || *p == 'N' || *p == 'P') {
            pieces++;
        }
    }
    return pieces;
}

int syzygy_scan_dir(const char *dir_path) {
    if (!dir_path || dir_path[0] == '\0') {
        return 0;
    }
    DIR *d = opendir(dir_path);
    if (!d) {
        return 0;
    }
    int max_p = 0;
    int files_found = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        int p = count_pieces_in_tb_name(de->d_name);
        if (p >= 3 && p <= 7) {
            files_found++;
            if (p > max_p) {
                max_p = p;
            }
        }
    }
    closedir(d);
    return (files_found > 0) ? max_p : 0;
}

int syzygy_installed_pieces(void) {
    return syzygy_scan_dir(SYZYGY_DEFAULT_DIR);
}
