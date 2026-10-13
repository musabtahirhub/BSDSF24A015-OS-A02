/*
 * Programming Assignment 02: ls
 * Version 1.6.0: Recursive Listing (-R Option)
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

/* ANSI Escape Codes for Terminal Colors */
#define COLOR_RESET "\033[0m"
#define COLOR_BLUE "\033[0;34m"  /* Directory */
#define COLOR_GREEN "\033[0;32m" /* Executable */
#define COLOR_RED "\033[0;31m"   /* Archive / Tarball */
#define COLOR_PINK "\033[0;35m"  /* Symbolic Link */
#define COLOR_REVERSE "\033[7m"  /* Special Files */

typedef enum {
  MODE_VERTICAL,  /* Default: down then across */
  MODE_LONG,      /* -l */
  MODE_HORIZONTAL /* -x */
} DisplayMode;

static DisplayMode display_mode = MODE_VERTICAL;
static int recursive_flag = 0;
static int printed_any_dir = 0;

/* Helper to safely build paths without double slashes */
static void build_path(char *dest, size_t size, const char *dir,
                       const char *name) {
  size_t len = strlen(dir);
  if (len > 0 && dir[len - 1] == '/') {
    snprintf(dest, size, "%s%s", dir, name);
  } else {
    snprintf(dest, size, "%s/%s", dir, name);
  }
}

/* Check file extension for archives */
int is_archive(const char *name) {
  const char *dot = strrchr(name, '.');
  if (!dot)
    return 0;
  return (strcmp(dot, ".zip") == 0 || strcmp(dot, ".tar") == 0 ||
          strcmp(dot, ".gz") == 0 || strcmp(dot, ".tgz") == 0 ||
          strcmp(dot, ".bz2") == 0 || strcmp(dot, ".xz") == 0 ||
          strcmp(dot, ".7z") == 0);
}

/* Return color based on st_mode */
const char *get_file_color(const char *name, mode_t mode) {
  if (S_ISLNK(mode))
    return COLOR_PINK;
  if (S_ISDIR(mode))
    return COLOR_BLUE;
  if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISFIFO(mode) || S_ISSOCK(mode))
    return COLOR_REVERSE;
  if (is_archive(name))
    return COLOR_RED;
  if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
    return COLOR_GREEN;
  return NULL;
}

void print_colored_name(const char *name, mode_t mode) {
  const char *color = get_file_color(name, mode);
  if (color) {
    printf("%s%s%s", color, name, COLOR_RESET);
  } else {
    printf("%s", name);
  }
}

int compare_filenames(const void *a, const void *b) {
  const char *str1 = *(const char *const *)a;
  const char *str2 = *(const char *const *)b;
  return strcmp(str1, str2);
}

int get_terminal_width(void) {
  struct winsize ws;
  if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
      ws.ws_col > 0) {
    return ws.ws_col;
  }
  return 80;
}

void mode_to_letters(mode_t mode, char str[]) {
  strcpy(str, "----------");

  if (S_ISDIR(mode))
    str[0] = 'd';
  else if (S_ISCHR(mode))
    str[0] = 'c';
  else if (S_ISBLK(mode))
    str[0] = 'b';
  else if (S_ISLNK(mode))
    str[0] = 'l';
  else if (S_ISFIFO(mode))
    str[0] = 'p';
  else if (S_ISSOCK(mode))
    str[0] = 's';

  if (mode & S_IRUSR)
    str[1] = 'r';
  if (mode & S_IWUSR)
    str[2] = 'w';
  if (mode & S_ISUID)
    str[3] = (mode & S_IXUSR) ? 's' : 'S';
  else if (mode & S_IXUSR)
    str[3] = 'x';

  if (mode & S_IRGRP)
    str[4] = 'r';
  if (mode & S_IWGRP)
    str[5] = 'w';
  if (mode & S_ISGID)
    str[6] = (mode & S_IXGRP) ? 's' : 'S';
  else if (mode & S_IXGRP)
    str[6] = 'x';

  if (mode & S_IROTH)
    str[7] = 'r';
  if (mode & S_IWOTH)
    str[8] = 'w';
  if (mode & S_ISVTX)
    str[9] = (mode & S_IXOTH) ? 't' : 'T';
  else if (mode & S_IXOTH)
    str[9] = 'x';
}

void display_columns_down_across(const char *dir, char **names, size_t count) {
  if (count == 0)
    return;

  int term_width = get_terminal_width();
  size_t max_len = 0;
  for (size_t i = 0; i < count; i++) {
    size_t len = strlen(names[i]);
    if (len > max_len)
      max_len = len;
  }

  int col_width = (int)max_len + 2;
  int num_cols = term_width / col_width;
  if (num_cols < 1)
    num_cols = 1;
  if ((size_t)num_cols > count)
    num_cols = (int)count;

  int num_rows = ((int)count + num_cols - 1) / num_cols;

  for (int r = 0; r < num_rows; r++) {
    for (int c = 0; c < num_cols; c++) {
      int idx = r + c * num_rows;
      if ((size_t)idx < count) {
        char full_path[1024];
        build_path(full_path, sizeof(full_path), dir, names[idx]);
        struct stat st;
        memset(&st, 0, sizeof(st));
        lstat(full_path, &st);

        print_colored_name(names[idx], st.st_mode);

        int next_idx = r + (c + 1) * num_rows;
        if (c + 1 < num_cols && (size_t)next_idx < count) {
          int pad = col_width - (int)strlen(names[idx]);
          if (pad > 0)
            printf("%*s", pad, "");
        }
      }
    }
    printf("\n");
  }
}

void display_columns_horizontal(const char *dir, char **names, size_t count) {
  if (count == 0)
    return;

  int term_width = get_terminal_width();
  size_t max_len = 0;
  for (size_t i = 0; i < count; i++) {
    size_t len = strlen(names[i]);
    if (len > max_len)
      max_len = len;
  }

  int col_width = (int)max_len + 2;
  int num_cols = term_width / col_width;
  if (num_cols < 1)
    num_cols = 1;

  for (size_t i = 0; i < count; i++) {
    char full_path[1024];
    build_path(full_path, sizeof(full_path), dir, names[i]);
    struct stat st;
    memset(&st, 0, sizeof(st));
    lstat(full_path, &st);

    print_colored_name(names[i], st.st_mode);

    int is_last_in_row = ((i + 1) % num_cols == 0) || (i + 1 == count);
    if (is_last_in_row) {
      printf("\n");
    } else {
      int pad = col_width - (int)strlen(names[i]);
      if (pad > 0)
        printf("%*s", pad, "");
    }
  }
}

void display_long_listing(const char *dir, char **names, size_t count) {
  if (count == 0) {
    printf("total 0\n");
    return;
  }

  struct stat *stats = calloc(count, sizeof(struct stat));
  char **owners = calloc(count, sizeof(char *));
  char **groups = calloc(count, sizeof(char *));

  int max_link_len = 1;
  int max_owner_len = 1;
  int max_group_len = 1;
  int max_size_len = 1;
  long long total_blocks = 0;

  char full_path[1024];
  for (size_t i = 0; i < count; i++) {
    build_path(full_path, sizeof(full_path), dir, names[i]);
    if (lstat(full_path, &stats[i]) == -1) {
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
    if ((int)strlen(num_buf) > max_link_len)
      max_link_len = strlen(num_buf);

    if ((int)strlen(owners[i]) > max_owner_len)
      max_owner_len = strlen(owners[i]);
    if ((int)strlen(groups[i]) > max_group_len)
      max_group_len = strlen(groups[i]);

    snprintf(num_buf, sizeof(num_buf), "%lld", (long long)stats[i].st_size);
    if ((int)strlen(num_buf) > max_size_len)
      max_size_len = strlen(num_buf);
  }

  printf("total %lld\n", total_blocks / 2);

  char perm_str[11];
  char time_str[64];
  char link_target[1024];

  for (size_t i = 0; i < count; i++) {
    mode_to_letters(stats[i].st_mode, perm_str);

    char *ct = ctime(&stats[i].st_mtime);
    if (ct) {
      snprintf(time_str, sizeof(time_str), "%.12s", ct + 4);
    } else {
      strcpy(time_str, "??? ?? ??:??");
    }

    printf("%s %*lu %-*s %-*s %*lld %s ", perm_str, max_link_len,
           (unsigned long)stats[i].st_nlink, max_owner_len, owners[i],
           max_group_len, groups[i], max_size_len, (long long)stats[i].st_size,
           time_str);

    print_colored_name(names[i], stats[i].st_mode);

    if (S_ISLNK(stats[i].st_mode)) {
      build_path(full_path, sizeof(full_path), dir, names[i]);
      ssize_t len = readlink(full_path, link_target, sizeof(link_target) - 1);
      if (len != -1) {
        link_target[len] = '\0';
        printf(" -> %s", link_target);
      }
    }
    printf("\n");

    free(owners[i]);
    free(groups[i]);
  }

  free(stats);
  free(owners);
  free(groups);
}

void do_ls(const char *dir) {
  DIR *dp = opendir(dir);
  if (dp == NULL) {
    fprintf(stderr, "Cannot open directory : %s\n", dir);
    return;
  }

  if (recursive_flag) {
    if (printed_any_dir > 0)
      printf("\n%s:\n", dir);
    else
      printf("%s:\n", dir);
    printed_any_dir = 1;
  }

  char **names = NULL;
  size_t count = 0;
  size_t capacity = 0;

  errno = 0;
  struct dirent *entry;
  while ((entry = readdir(dp)) != NULL) {
    if (entry->d_name[0] == '.')
      continue;
    if (count >= capacity) {
      capacity = (capacity == 0) ? 16 : capacity * 2;
      char **new_names = realloc(names, capacity * sizeof(char *));
      if (!new_names) {
        perror("realloc failed");
        closedir(dp);
        for (size_t i = 0; i < count; i++)
          free(names[i]);
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

  if (count > 1) {
    qsort(names, count, sizeof(char *), compare_filenames);
  }

  switch (display_mode) {
  case MODE_LONG:
    display_long_listing(dir, names, count);
    break;
  case MODE_HORIZONTAL:
    display_columns_horizontal(dir, names, count);
    break;
  case MODE_VERTICAL:
  default:
    display_columns_down_across(dir, names, count);
    break;
  }

  if (recursive_flag) {
    for (size_t i = 0; i < count; i++) {
      char sub_path[2048];
      build_path(sub_path, sizeof(sub_path), dir, names[i]);

      struct stat st;
      if (lstat(sub_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (strcmp(names[i], ".") != 0 && strcmp(names[i], "..") != 0) {
          do_ls(sub_path);
        }
      }
    }
  }

  for (size_t i = 0; i < count; i++) {
    free(names[i]);
  }
  free(names);
}

int main(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "lxR")) != -1) {
    switch (opt) {
    case 'l':
      display_mode = MODE_LONG;
      break;
    case 'x':
      display_mode = MODE_HORIZONTAL;
      break;
    case 'R':
      recursive_flag = 1;
      break;
    default:
      fprintf(stderr, "Usage: %s [-l] [-x] [-R] [directory...]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  int num_dirs = argc - optind;
  if (num_dirs == 0) {
    do_ls(".");
  } else {
    for (int i = optind; i < argc; i++) {
      if (!recursive_flag && num_dirs > 1) {
        printf("%s:\n", argv[i]);
      }
      do_ls(argv[i]);
      if (!recursive_flag && num_dirs > 1 && i < argc - 1) {
        puts("");
      }
    }
  }
  return 0;
}