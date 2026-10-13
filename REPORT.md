# Operating Systems Programming Assignment 02 - Report
**Student Roll Number:** BSDSF24A015  
**Course:** Operating Systems  
**Instructor:** Muhammad Arif Butt, PhD  

---

## Feature-2: ls-v1.1.0 - Complete Long Listing Format

### 1. Crucial Difference Between `stat()` and `lstat()` System Calls
- **Behavior with Symbolic Links:**
  - The `stat()` system call retrieves file metadata for the specified path. However, when the specified path refers to a symbolic link, `stat()` automatically follows (dereferences) the link and returns the metadata of the target file that the link points to.
  - The `lstat()` system call is identical to `stat()`, except that when the named file is a symbolic link, `lstat()` does **not** follow the link. Instead, it returns metadata regarding the symbolic link itself (such as its own link size, permissions, and file type `S_IFLNK`).
- **Context in the `ls` Command:**
  - In `ls` (specifically `ls -l`), using `lstat()` is essential because users want to inspect the link itself: they need to see that the file is indeed a symbolic link (indicated by the `l` file type character), inspect the link's own size (which is the length of the pathname string it contains), and print the link's arrow destination (`link_name -> target_path`).
  - If `stat()` were used instead, a symbolic link pointing to a directory would be erroneously displayed as a directory (`d`), and a broken/dangling symbolic link whose target does not exist would cause `stat()` to return `-1` with `ENOENT`, preventing `ls` from displaying the link at all. Therefore, `lstat()` is the correct and appropriate system call.

### 2. Extracting File Type and Permission Bits from `st_mode`
- **Structure of `st_mode`:**
  - In `struct stat`, `st_mode` is a bitmask of type `mode_t` (typically a 16-bit or 32-bit unsigned integer) that packs two distinct sets of information:
    1. **File Type:** Stored in the upper bits (masked by `S_IFMT`, typically `0170000` in octal).
    2. **Special Execution Bits & Access Permissions:** Stored in the lower 12 bits (SUID `04000`, SGID `02000`, Sticky `01000`, and user/group/other `rwx` permissions `0777`).
- **Extracting File Type:**
  - To extract the file type, we perform a bitwise AND between `st_mode` and the file type mask `S_IFMT`, and compare the result against predefined file type constants:
    ```c
    (st_mode & S_IFMT) == S_IFDIR  // Directory
    (st_mode & S_IFMT) == S_IFREG  // Regular file
    (st_mode & S_IFMT) == S_IFLNK  // Symbolic link
    ```
  - POSIX also provides convenience macros that perform this operation internally:
    ```c
    S_ISDIR(st_mode)  // evaluates ((st_mode & S_IFMT) == S_IFDIR)
    S_ISREG(st_mode)  // evaluates ((st_mode & S_IFMT) == S_IFREG)
    S_ISLNK(st_mode)  // evaluates ((st_mode & S_IFMT) == S_IFLNK)
    ```
- **Extracting Permission Bits:**
  - Each individual permission corresponds to a single bit in the lower 9 bits. Using the bitwise AND operator `&` with predefined macros extracts whether a specific permission is granted:
    - User/Owner: `st_mode & S_IRUSR` (read), `st_mode & S_IWUSR` (write), `st_mode & S_IXUSR` (execute).
    - Group: `st_mode & S_IRGRP` (read), `st_mode & S_IWGRP` (write), `st_mode & S_IXGRP` (execute).
    - Others: `st_mode & S_IROTH` (read), `st_mode & S_IWOTH` (write), `st_mode & S_IXOTH` (execute).
  - Special bits are extracted similarly:
    - Set-UID: `st_mode & S_ISUID` (modifies user execute position to `s` or `S`).
    - Set-GID: `st_mode & S_ISGID` (modifies group execute position to `s` or `S`).
    - Sticky bit: `st_mode & S_ISVTX` (modifies others execute position to `t` or `T`).

---

## Feature-3: ls-v1.2.0 – Column Display (Down Then Across)

### 1. General Logic for Printing in "Down Then Across" Columnar Format
- **Row-Major Output Mechanism vs. Columnar Data Layout:**
  - Standard terminals output text strictly sequentially from left to right, line by line (row-major order). Once a newline character (`\n`) is written to stdout, the terminal cursor advances to the next line and cannot backtrack to modify previous lines.
  - In a "down then across" layout, directory entries are sorted vertically within each column: column 0 contains entries $0$ to $R-1$, column 1 contains $R$ to $2R-1$, and so forth (where $R$ is the number of rows).
  - Therefore, to print the first visible row across all columns, the program must print entry $0$, then jump across by $R$ elements to print entry $R$, then jump by $R$ again to print $2R$, and so on across all columns $C$. For any row $r \in [0, R-1]$ and column $c \in [0, C-1]$, the linear index in the filename array is:
    $$\text{index}(r, c) = r + c \times R$$
- **Why a Simple Single Loop is Insufficient:**
  - A single loop iterates linearly through the filename array ($0, 1, 2, \dots, N-1$). If printed sequentially as encountered, items naturally flow horizontally across columns before wrapping down to the next row (row-major / horizontal layout).
  - To achieve "down then across" (column-major order), items must be grouped and indexed non-sequentially across rows. A program must therefore:
    1. Read and buffer **all** entries into an in-memory array beforehand so random access by index is possible.
    2. Determine the total entry count $N$, find the maximum filename length, and compute the number of columns $C$ and rows $R = \lceil N / C \rceil$.
    3. Use nested loops (`for r in 0..R-1` outer, `for c in 0..C-1` inner) jumping by strides of $R$ across columns for each line printed.

### 2. Purpose and Necessity of the `ioctl` System Call (`TIOCGWINSZ`)
- **Detecting Dynamic Terminal Dimensions:**
  - The `ioctl()` (Input/Output Control) system call allows user-space applications to query or configure underlying device parameters that cannot be expressed via standard `read()` and `write()` operations.
  - Passing `TIOCGWINSZ` (Terminal I/O Control Get Window Size) with a pointer to `struct winsize` allows the program to retrieve the current terminal dimensions dynamically from the terminal driver:
    ```c
    struct winsize ws;
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        term_width = ws.ws_col;
    }
    ```
- **Limitations of a Fixed-Width Fallback (e.g., 80 columns):**
  - **Screen Space Underutilization:** On modern widescreen monitors, terminal emulators frequently have widths of 120, 160, 200, or more columns. A hardcoded 80-column limit forces filenames into unnecessarily few columns and long vertical lists, wasting over 50% of the available display space.
  - **Ugly Line Wrapping on Narrow Screens:** If the terminal window is tiled, split, or resized below 80 columns (e.g., 60 columns on a split pane or mobile SSH session), an 80-column layout will overshoot the right edge of the terminal. The terminal emulator will wrap the overflowing text onto the subsequent line, corrupting column alignment and rendering the output unreadable.
  - **Adaptive Usability:** Using `ioctl` guarantees that the layout adapts in real time to whatever terminal size the user configures, providing a clean, professional command-line experience identical to GNU `ls`.

---

## Feature-4: ls-v1.3.0 – Horizontal Column Display (-x Option)

### 1. Implementation Complexity Comparison: Vertical ("Down Then Across") vs. Horizontal ("Across") Logic
- **Horizontal Printing Logic ("Across"):**
  - **Sequential Streaming:** Horizontal output directly follows the natural streaming nature of terminal text. The program can process filenames sequentially from index $0$ to $N-1$.
  - **Minimal Pre-Calculation:** The only required pre-calculations are determining the maximum filename length to fix the column width, and retrieving the terminal width. As files are printed, the program maintains a running horizontal cursor position counter (`current_pos`). When adding the next column width would exceed the terminal boundary, it outputs `\n` and resets the counter to zero.
  - **Single Linear Loop:** A single `for (size_t i = 0; i < count; i++)` loop suffices without needing multidimensional coordinate math.
- **Vertical Printing Logic ("Down Then Across"):**
  - **Non-Sequential Strided Indexing:** Because terminal lines cannot be revisited after printing, the program must calculate the 2D layout geometry ($R$ rows by $C$ columns) before printing any text.
  - **Extensive Pre-Calculation:** The program must pre-compute:
    1. Maximum entry length and column slot width ($W = \text{max\_len} + \text{spacing}$).
    2. Column count: $C = \lfloor \text{term\_width} / W \rfloor$.
    3. Row count: $R = \lceil N / C \rceil$.
    4. Two-dimensional index mapping for each grid cell: $\text{index} = r + c \times R$.
  - **Nested 2D Traversal:** Requires nested loops (`for r in 0..R-1` and `for c in 0..C-1`) and bounds checking to ensure stride jumps do not read past array boundaries.
- **Conclusion:**
  - The "down then across" (vertical) layout requires significantly more pre-calculation because the physical output medium (row-by-row stream) is orthogonal to the logical ordering of the data (column-by-column).

### 2. Strategy for Managing Different Display Modes (`-l`, `-x`, and Default)
- **State Management via `enum DisplayMode`:**
  - Rather than scattering multiple uncoordinated boolean flags across the codebase, a clean `enum` was introduced to encapsulate mutually exclusive display behaviors:
    ```c
    typedef enum {
        MODE_VERTICAL,   /* Default: down then across */
        MODE_LONG,       /* -l: detailed long listing */
        MODE_HORIZONTAL  /* -x: horizontal row-major listing */
    } DisplayMode;
    static DisplayMode display_mode = MODE_VERTICAL;
    ```
- **Command-Line Option Parsing with `getopt()`:**
  - In `main()`, `getopt(argc, argv, "lx")` parses the flags and sets `display_mode` accordingly. Subsequent flags override or set the active mode cleanly.
- **Unified Gathering and Single-Point Dispatching:**
  - In `do_ls()`, directory reading is completely decoupled from rendering. `do_ls()` collects all valid directory entries into a dynamically allocated array `char **names`.
  - Once reading is complete, a concise `switch (display_mode)` statement directs the collected array to the appropriate specialized rendering function:
    - `case MODE_LONG`: calls `display_long_listing(dir, names, count)`
    - `case MODE_HORIZONTAL`: calls `display_columns_horizontal(names, count)`
    - `case MODE_VERTICAL`: calls `display_columns_down_across(names, count)`
  - This architecture ensures high modularity, eliminates duplicated directory reading code, and ensures that features like sorting (Feature-5) and coloring (Feature-6) apply uniformly to all display modes with zero code duplication.
