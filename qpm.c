#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <time.h>

#define VERSION "QPM v1.1.1 (Clean Animation)"
#define CONFIG_PATH "/etc/qpm.conf"
#define LOG_PATH "/var/log/qpm/history.log"
#define CACHE_PATH "/var/cache/qpm/mirrorlist.txt"

#define MAX_MIRRORS 32
#define MAX_URL_LEN 256

char mirrors[MAX_MIRRORS][MAX_URL_LEN];
int mirror_count = 0;

/* ────────────────────────────── */
/* Spinner animation (cleaned) */
void spinner(const char *msg, int cycles, int delay_ms) {
    const char symbols[] = "|/-\\";
    printf("%s ", msg);
    fflush(stdout);
    for (int i = 0; i < cycles; i++) {
        printf("\r%s %c", msg, symbols[i % 4]);
        fflush(stdout);
        usleep(delay_ms * 1000);
    }
    printf("\r%s ✓\n", msg);
    fflush(stdout);
}

/* CURL memory */
struct Memory { char *data; size_t size; };
size_t write_cb(void *c, size_t s, size_t n, void *u) {
    size_t r = s * n;
    struct Memory *m = (struct Memory *)u;
    char *p = realloc(m->data, m->size + r + 1);
    if (!p) return 0;
    m->data = p;
    memcpy(&(m->data[m->size]), c, r);
    m->size += r;
    m->data[m->size] = 0;
    return r;
}

/* ────────────────────────────── */
void ensure_dirs() {
    mkdir("/var/log/qpm", 0755);
    mkdir("/var/cache/qpm", 0755);
}

void ensure_root() {
    if (getuid() != 0) {
        fprintf(stderr, "Root privileges required.\n");
        exit(1);
    }
}

void log_event(const char *event) {
    ensure_dirs();
    FILE *f = fopen(LOG_PATH, "a");
    if (!f) return;
    fprintf(f, "%s\n", event);
    fclose(f);
}

/* ────────────────────────────── */
void load_config() {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) {
        printf("%s not found, creating default...\n", CONFIG_PATH);
        f = fopen(CONFIG_PATH, "w");
        if (f) {
            fprintf(f, "# QPM config\nmirror=https://mirrors.qyzyl.xyz/qpm/\n");
            fclose(f);
            f = fopen(CONFIG_PATH, "r");
        } else {
            perror("Failed to create config");
            exit(1);
        }
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strlen(line) < 10)
            continue;
        if (strncmp(line, "mirror=", 7) == 0) {
            char *url = line + 7;
            url[strcspn(url, "\n")] = 0;
            strncpy(mirrors[mirror_count++], url, MAX_URL_LEN);
        }
    }
    fclose(f);
}

/* ────────────────────────────── */
int download_file(const char *url, const char *out_path) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    FILE *f = fopen(out_path, "wb");
    if (!f) { perror("Cannot open file"); return 0; }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    spinner("Downloading", 12, 60);

    CURLcode res = curl_easy_perform(curl);
    fclose(f);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) return 1;
    printf("Download failed for %s\n", url);
    return 0;
}

/* ────────────────────────────── */
void update_mirrorlist() {
    ensure_root();
    ensure_dirs();
    load_config();

    CURL *curl = curl_easy_init();
    if (!curl) return;
    struct Memory chunk = {malloc(1), 0};

    printf("Checking mirrors...\n");
    spinner("Connecting", 10, 70);

    int success = 0;
    for (int i = 0; i < mirror_count; i++) {
        printf("→ Trying: %s\n", mirrors[i]);
        curl_easy_setopt(curl, CURLOPT_URL, mirrors[i]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK && chunk.size > 0) {
            FILE *out = fopen(CACHE_PATH, "w");
            if (!out) { perror("Cannot write cache"); break; }

            int count = 0;
            char *line = strtok(chunk.data, "\n");
            while (line) {
                if (strstr(line, ".qbf")) {
                    fprintf(out, "%s\n", line);
                    count++;
                }
                line = strtok(NULL, "\n");
            }
            fclose(out);
            printf("✔ Mirror OK — %d packages found.\n", count);
            log_event("Mirror list updated.");
            success = 1;
            break;
        } else {
            printf("✗ Mirror failed: %s\n", mirrors[i]);
        }
        free(chunk.data);
        chunk.data = malloc(1);
        chunk.size = 0;
    }

    curl_easy_cleanup(curl);
    if (!success)
        printf("No reachable mirrors.\n");
}

/* ────────────────────────────── */
void install_package(const char *pkg) {
    ensure_root();
    ensure_dirs();
    load_config();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    char qbf_url[512];
    char local_qbf[128];
    snprintf(local_qbf, sizeof(local_qbf), "/tmp/qpm-%s.qbf", pkg);

    int success = 0;
    for (int i = 0; i < mirror_count; i++) {
        snprintf(qbf_url, sizeof(qbf_url), "%s/%s.qbf", mirrors[i], pkg);
        printf("Fetching %s...\n", pkg);
        if (download_file(qbf_url, local_qbf)) {
            printf("Fetched metadata: %s.qbf\n", pkg);
            success = 1;
            break;
        }
    }
    if (!success) {
        printf("Package not found: %s\n", pkg);
        curl_global_cleanup();
        return;
    }

    FILE *qbf = fopen(local_qbf, "r");
    if (!qbf) { perror("Cannot read QBF"); curl_global_cleanup(); return; }

    FILE *log = fopen(LOG_PATH, "a");
    fprintf(log, "[INSTALL] %s\n", pkg);

    char line[512];
    while (fgets(line, sizeof(line), qbf)) {
        if (strstr(line, "install")) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            char *inst = eq + 1;
            inst[strcspn(inst, "\n")] = 0;

            char *src = strtok(inst, ":");
            char *dst = strtok(NULL, ":");
            if (!src || !dst) continue;

            while (*src == ' ' || *src == '\t') src++;
            while (*dst == ' ' || *dst == '\t') dst++;
            for (int i = strlen(src)-1; i >= 0 && (src[i]==' '||src[i]=='\t'); i--) src[i]='\0';
            for (int i = strlen(dst)-1; i >= 0 && (dst[i]==' '||dst[i]=='\t'); i--) dst[i]='\0';

            char src_url[512];
            snprintf(src_url, sizeof(src_url), "%s/%s", mirrors[0], src);
            printf("Installing → %s\n", dst);
            spinner("Transferring", 10, 60);

            char tmp[128];
            snprintf(tmp, sizeof(tmp), "/tmp/qpm-tmpfile");
            if (!download_file(src_url, tmp)) continue;

            char cmd[512];
            snprintf(cmd, sizeof(cmd), "install -Dm755 %s %s", tmp, dst);
            system(cmd);
            printf("✔ Installed: %s\n", dst);
            fprintf(log, "%s\n", dst);
        }
    }
    fclose(qbf);
    fclose(log);
    curl_global_cleanup();
}

/* ────────────────────────────── */
void remove_package(const char *pkg) {
    ensure_root();
    ensure_dirs();

    FILE *in = fopen(LOG_PATH, "r");
    if (!in) { printf("No log file.\n"); return; }

    int found = 0;
    char line[512];
    printf("Removing %s...\n", pkg);
    spinner("Processing", 10, 60);

    while (fgets(line, sizeof(line), in)) {
        if (strstr(line, "[INSTALL]") && strstr(line, pkg)) {
            found = 1;
            continue;
        }
        if (found) {
            if (line[0] == '[') break;
            line[strcspn(line, "\n")] = 0;
            unlink(line);
            printf("Removed: %s\n", line);
        }
    }
    fclose(in);

    if (found) {
        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg), "[REMOVE] %s", pkg);
        log_event(logmsg);
    } else printf("Package not found in logs.\n");
}

/* ────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: qpm <command> [package]\n");
        printf("Commands: update-mirrorlist, install, remove, --version\n");
        return 0;
    }

    if (!strcmp(argv[1], "--version")) printf("%s\n", VERSION);
    else if (!strcmp(argv[1], "update-mirrorlist")) update_mirrorlist();
    else if (!strcmp(argv[1], "install")) {
        if (argc < 3) { printf("Package name required.\n"); return 1; }
        install_package(argv[2]);
    }
    else if (!strcmp(argv[1], "remove")) {
        if (argc < 3) { printf("Package name required.\n"); return 1; }
        remove_package(argv[2]);
    }
    else printf("Unknown command: %s\n", argv[1]);

    return 0;
}
