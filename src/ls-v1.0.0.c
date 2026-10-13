/*
 * Programming Assignment 02: ls
 * Version 1.2.0: Column Display (Down Then Across)
 */
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

extern int errno;

static int opt_l = 0;

/* Detect current terminal column width using ioctl */
int get_terminal_width(void) {
    struct winsize ws;
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 80; /* Fallback to standard 80 columns when stdout is not a terminal */
}

/* Convert st_mode into a 10-character permissions string */
void mode_to_letters(mode_t mode, char str[]) {
    strcpy(str, "----------");

    /* File type */
    if (S_ISDIR(mode))  str[0] = 'd';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';

    /* User (owner) permissions */
    if (mode & S_IRUSR) str[1] = 'r';
    if (mode & S_IWUSR) str[2] = 'w';
    if (mode & S_ISUID) {
        str[3] = (mode & S_IXUSR) ? 's' : 'S';
    } else if (mode & S_IXUSR) {
        str[3] = 'x';
    }

    /* Group permissions */
    if (mode & S_IRGRP) str[4] = 'r';
    if (mode & S_IWGRP) str[5] = 'w';
    if (mode & S_ISGID) {
        str[6] = (mode & S_IXGRP) ? 's' : 'S';
    } else if (mode & S_IXGRP) {
        str[6] = 'x';
    }

    /* Others permissions */
    if (mode & S_IROTH) str[7] = 'r';
    if (mode & S_IWOTH) str[8] = 'w';
    if (mode & S_ISVTX) {
        str[9] = (mode & S_IXOTH) ? 't' : 'T';
    } else if (mode & S_IXOTH) {
        str[9] = 'x';
    }
}

/* Display filenames in multiple columns: formatted "down then across" */
void display_columns_down_across(char **names, size_t count) {
    if (count == 0) return;

    int term_width = get_terminal_width();
    size_t max_len = 0;
    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(names[i]);
        if (len > max_len) {
            max_len = len;
        }
    }

    int spacing = 2;
    int col_width = (int)max_len + spacing;
    int num_cols = term_width / col_width;
    if (num_cols < 1) num_cols = 1;
    if ((size_t)num_cols > count) num_cols = (int)count;

    int num_rows = ((int)count + num_cols - 1) / num_cols;

    for (int r = 0; r < num_rows; r++) {
        for (int c = 0; c < num_cols; c++) {
            int idx = r + c * num_rows;
            if ((size_t)idx < count) {
                int next_idx = r + (c + 1) * num_rows;
                if (c + 1 < num_cols && (size_t)next_idx < count) {
                    printf("%-*s", col_width, names[idx]);
                } else {
                    printf("%s", names[idx]);
                }
            }
        }
        printf("\n");
    }
}

/* Default listing with column display (v1.2.0 behavior) */
void do_ls(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        if (count >= capacity) {
            capacity = (capacity == 0) ? 16 : capacity * 2;
            char **new_names = realloc(names, capacity * sizeof(char *));
            if (!new_names) {
                perror("realloc failed");
                closedir(dp);
                for (size_t i = 0; i < count; i++) free(names[i]);
                free(names);
                return;
            }
            names = new_names;
        }
        names[count] = strdup(entry->d_name);
        count++;
    }

    if (errno != 0) {
        perror("readdir failed");
    }

    closedir(dp);

    display_columns_down_across(names, count);

    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

/* Long listing format (-l) */
void do_ls_l(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* First pass: count entries and gather names */
    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        if (count >= capacity) {
            capacity = (capacity == 0) ? 16 : capacity * 2;
            char **new_names = realloc(names, capacity * sizeof(char *));
            if (!new_names) {
                perror("realloc failed");
                closedir(dp);
                for (size_t i = 0; i < count; i++) free(names[i]);
                free(names);
                return;
            }
            names = new_names;
        }
        names[count] = strdup(entry->d_name);
        count++;
    }

    if (errno != 0) {
        perror("readdir failed");
    }
    closedir(dp);

    if (count == 0) {
        free(names);
        return;
    }

    /* Gather metadata and determine column widths for alignment */
    struct stat *stats = malloc(count * sizeof(struct stat));
    char **owners = malloc(count * sizeof(char *));
    char **groups = malloc(count * sizeof(char *));

    int max_link_len = 1;
    int max_owner_len = 1;
    int max_group_len = 1;
    int max_size_len = 1;
    long long total_blocks = 0;

    char full_path[1024];
    for (size_t i = 0; i < count; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, names[i]);
        if (lstat(full_path, &stats[i]) == -1) {
            perror(full_path);
            memset(&stats[i], 0, sizeof(struct stat));
            owners[i] = strdup("unknown");
            groups[i] = strdup("unknown");
            continue;
        }

        total_blocks += stats[i].st_blocks;

        struct passwd *pw = getpwuid(stats[i].st_uid);
        if (pw) {
            owners[i] = strdup(pw->pw_name);
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", stats[i].st_uid);
            owners[i] = strdup(buf);
        }

        struct group *gr = getgrgid(stats[i].st_gid);
        if (gr) {
            groups[i] = strdup(gr->gr_name);
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", stats[i].st_gid);
            groups[i] = strdup(buf);
        }

        char num_buf[64];
        snprintf(num_buf, sizeof(num_buf), "%lu", (unsigned long)stats[i].st_nlink);
        if ((int)strlen(num_buf) > max_link_len) max_link_len = strlen(num_buf);

        if ((int)strlen(owners[i]) > max_owner_len) max_owner_len = strlen(owners[i]);
        if ((int)strlen(groups[i]) > max_group_len) max_group_len = strlen(groups[i]);

        snprintf(num_buf, sizeof(num_buf), "%lld", (long long)stats[i].st_size);
        if ((int)strlen(num_buf) > max_size_len) max_size_len = strlen(num_buf);
    }

    printf("total %lld\n", total_blocks / 2);

    /* Print aligned long listing rows */
    char perm_str[11];
    char time_str[64];
    char link_target[1024];

    for (size_t i = 0; i < count; i++) {
        mode_to_letters(stats[i].st_mode, perm_str);

        /* Format modification time: Mmm dd hh:mm */
        char *ct = ctime(&stats[i].st_mtime);
        if (ct) {
            /* Slice "Mmm dd hh:mm" from ctime */
            snprintf(time_str, sizeof(time_str), "%.12s", ct + 4);
        } else {
            strcpy(time_str, "??? ?? ??:??");
        }

        printf("%s %*lu %-*s %-*s %*lld %s %s",
               perm_str,
               max_link_len, (unsigned long)stats[i].st_nlink,
               max_owner_len, owners[i],
               max_group_len, groups[i],
               max_size_len, (long long)stats[i].st_size,
               time_str,
               names[i]);

        /* If symbolic link, print target */
        if (S_ISLNK(stats[i].st_mode)) {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, names[i]);
            ssize_t len = readlink(full_path, link_target, sizeof(link_target) - 1);
            if (len != -1) {
                link_target[len] = '\0';
                printf(" -> %s", link_target);
            }
        }
        printf("\n");

        free(names[i]);
        free(owners[i]);
        free(groups[i]);
    }

    free(names);
    free(stats);
    free(owners);
    free(groups);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "l")) != -1) {
        switch (opt) {
            case 'l':
                opt_l = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    int num_dirs = argc - optind;
    if (num_dirs == 0) {
        if (opt_l) {
            do_ls_l(".");
        } else {
            do_ls(".");
        }
    } else {
        for (int i = optind; i < argc; i++) {
            if (num_dirs > 1) {
                printf("%s:\n", argv[i]);
            }
            if (opt_l) {
                do_ls_l(argv[i]);
            } else {
                do_ls(argv[i]);
            }
            if (num_dirs > 1 && i < argc - 1) {
                puts("");
            }
        }
    }

    return 0;
}