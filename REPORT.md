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

---

## Feature-5: ls-v1.4.0 – Alphabetical Sort

### 1. Necessity of Buffering Directory Entries Before Sorting & Potential Drawbacks
- **Why In-Memory Buffering is Mandatory:**
  - The POSIX directory reading interface (`opendir()`, `readdir()`) operates as a forward-only stream. `readdir()` yields directory entries in arbitrary, unsorted order dictated by the file system's internal structures (e.g., ext4 directory hash trees or directory entry blocks).
  - Standard comparison-based sorting algorithms (such as Quicksort) require arbitrary random access to compare and swap pairs of elements across the collection. Because an active directory stream cannot be randomly indexed or swapped in-place on disk, the application must read and store all filenames into an in-memory random-access container (a dynamic array of pointers `char **`) before invoking `qsort()`.
- **Potential Drawbacks for Massive Directories (Millions of Files):**
  - **Memory Exhaustion (High RAM Footprint):** Storing millions of filename strings and pointer arrays consumes substantial heap memory. For $10^6$ files with an average name length of 32 bytes, holding pointers (8 bytes each) plus dynamically allocated strings with `malloc` heap overhead can consume hundreds of megabytes of RAM, risking Out-Of-Memory (`OOM`) crashes or heavy swapping.
  - **Latency and User Delay (No Streaming Feedback):** The program cannot display a single entry until all millions of entries have been traversed and sorted ($O(N \log N)$ complexity). This introduces significant execution latency where the user experiences a frozen terminal.
  - **Alternative Production Approaches:** For directories with millions of items, high-performance utilities use streaming external merge sorts (spilling sorted chunks to temporary files) or present an unsorted stream immediately unless the user explicitly requests sorting.

### 2. Purpose and Signature of the `qsort()` Comparison Function
- **Signature of the Comparison Callback:**
  - In standard C library `<stdlib.h>`, the `qsort` function is declared as:
    ```c
    void qsort(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *));
    ```
  - For sorting an array of strings (`char *names[]`), our comparison function is:
    ```c
    int compare_filenames(const void *a, const void *b) {
        const char *str1 = *(const char * const *)a;
        const char *str2 = *(const char * const *)b;
        return strcmp(str1, str2);
    }
    ```
- **How It Works and Contract:**
  - `qsort()` passes pointers to the two elements being compared to the callback. Because the array elements being sorted are pointers to strings (`char *`), `a` and `b` point to those array elements, meaning they have the effective type `char **`.
  - The function dereferences `a` and `b` to retrieve the actual string pointers `char *`, and calls `strcmp(str1, str2)`:
    - Returns `< 0` if `str1` lexicographically precedes `str2`.
    - Returns `0` if `str1` and `str2` are identical.
    - Returns `> 0` if `str1` lexicographically follows `str2`.
- **Why It Must Take `const void *` Arguments:**
  - **Polymorphic Type Generality:** C lacks templates or generics. The standard C library achieves polymorphism by using `void *` (generic untyped pointer). This allows `qsort()` to sort arbitrary arrays of any data type (integers, structs, floats, pointers) without knowing the underlying element type.
  - **Const-Correctness:** The `const` qualifier guarantees that the comparison callback will inspect the referenced data without mutating it, preserving data integrity during sorting operations.

---

## Feature-6: ls-v1.5.0 – Colorized Output Based on File Type

### 1. How ANSI Escape Codes Produce Color in a Standard Linux Terminal
- **Mechanism of In-Band Terminal Control:**
  - Modern Linux terminal emulators follow the ANSI/VT100 control sequence standards (ECMA-48). Rather than providing separate out-of-band APIs for styling, control information is transmitted **in-band** within the standard byte stream using special "escape sequences".
  - An escape sequence begins with the ASCII Escape character (`ESC`, hex `0x1B`, octal `\033`, decimal `27`), followed by the left bracket `[` (forming the Control Sequence Introducer, or **CSI**).
  - The format for Select Graphic Rendition (SGR) is:
    $$\text{ESC} \ [ \ \langle \text{parameters} \rangle \ \text{m}$$
  - When the terminal emulator's parser encounters `\033[`, it stops rendering characters directly to the screen grid and instead switches into an internal state machine to interpret the following parameter codes.
- **Specific Code Sequence for Printing Text in Green:**
  - The parameter `32` sets foreground color to green:
    - Normal Green: `"\033[0;32m"` (`0` = normal attribute, `32` = green foreground).
    - Bold/Bright Green: `"\033[1;32m"` (`1` = bold/bright attribute).
  - To print a green string safely without bleeding color into subsequent terminal commands, the color must be reset using the reset sequence `"\033[0m"`:
    ```c
    printf("\033[0;32m%s\033[0m", filename);
    ```

### 2. Checking Executable Permission Bits in `st_mode`
- **Bitwise Layout of Execution Permissions in `st_mode`:**
  - The file mode field `st_mode` in `struct stat` is a bitmask where specific bits encode read, write, and execute permissions partitioned into three distinct classes of users:
    1. **Owner/User Execution (`S_IXUSR`):**
       - Defined as octal `00100` (`0x0040`).
       - Bit test: `(st_mode & S_IXUSR) != 0`
    2. **Group Execution (`S_IXGRP`):**
       - Defined as octal `00010` (`0x0008`).
       - Bit test: `(st_mode & S_IXGRP) != 0`
    3. **Others/World Execution (`S_IXOTH`):**
       - Defined as octal `00001` (`0x0001`).
       - Bit test: `(st_mode & S_IXOTH) != 0`
- **Determining If a File Is Executable:**
  - To verify whether a file is executable by **any** entity (owner, group, or others), we combine the three bits using the bitwise OR operator `|` into a composite mask, and apply a bitwise AND `&` against `st_mode`:
    ```c
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        /* File is a regular file and has executable permissions */
    }
    ```
  - It is critical to first verify that the file is a regular file (`S_ISREG(mode)`), because directory entries also have execute bits (`x` on directories grants permission to search or traverse into the directory). Verifying `S_ISREG(mode)` ensures that directories are colored with the directory color (blue) rather than being mistakenly classified as green executable binaries.

---

## Feature-7: ls-v1.6.0 – Recursive Listing (-R Option)

### 1. Base Case in Recursive Functions and in Recursive `ls`
- **Definition and Role of a Base Case:**
  - In computer science and algorithm design, a **base case** is a termination condition that produces a direct result without making further recursive calls. It serves as the bottom of the recursion stack, ensuring that the function terminates cleanly and avoids infinite recursion (which would cause a stack overflow error and segment fault).
- **The Base Case in Recursive `ls`:**
  - A file system directory tree is a hierarchical, directed acyclic graph (ignoring directory hard links, which Unix disallows).
  - In our recursive implementation of `do_ls(dir)`:
    1. **Leaf Directories (No Subdirectories):** When `do_ls()` is called on a directory that contains only regular files (or is completely empty), the loop checking directory entries (`S_ISDIR(st.st_mode)`) finds no child directories. Since no child directories exist to invoke `do_ls()`, the function returns without spawning new recursive calls. This constitutes the natural base case of the tree traversal.
    2. **Filtering Loop References (`.` and `..`):** Every Unix directory entry contains self (`.`) and parent (`..`) references. If these were followed recursively, `.` would immediately cause an infinite loop recursing into the same directory forever. By explicitly skipping names starting with `.` (or filtering `strcmp(name, ".") == 0 || strcmp(name, "..") == 0`), these cyclical references are discarded, preventing infinite recursion loops.
    3. **Preventing Symlink Recursion via `lstat()`:** If a symbolic link points to a parent or ancestor directory (a circular symlink), using `lstat()` ensures that `st.st_mode` reflects `S_IFLNK` rather than `S_IFDIR`. Since `S_ISDIR()` evaluates to false for symbolic links, the traversal does not descend into symlinked directories, preventing infinite cyclical recursion.
    4. **Unreadable / Inaccessible Directories:** If a subdirectory cannot be opened (`opendir() == NULL`), `do_ls` prints an error message to stderr and returns immediately, acting as a defensive terminal base case for permission-denied subtrees.

### 2. Importance of Full Path Construction in Recursive Directory Traversal
- **Difference Between Relative Filenames and Path Resolution:**
  - When `readdir()` reads entries inside a directory, `entry->d_name` contains only the base filename of the item (e.g., `"child"`), not its location relative to the process's current working directory (CWD).
  - System calls such as `opendir()`, `stat()`, and `lstat()` interpret relative path arguments with respect to the **current working directory (CWD) of the process**, not with respect to the directory currently being processed by the function.
- **What Happens If `do_ls("subdir")` Is Called Without Constructing the Full Path:**
  - Suppose a user launches `ls -R /tmp/test_dir` while the process CWD is `/home/user`.
  - Inside `/tmp/test_dir`, there exists a subdirectory called `subdir`.
  - If the program merely calls `do_ls("subdir")`:
    - The OS kernel attempts to resolve `"subdir"` relative to the process's working directory (`/home/user/subdir`).
    - Since `/home/user/subdir` does not exist, `opendir("subdir")` fails with `ENOENT` ("No such file or directory"), or worse, opens an unrelated directory if a folder named `subdir` happens to exist in `/home/user`!
  - Likewise, running `lstat("subdir", &st)` before recursing would attempt to inspect `./subdir` rather than `/tmp/test_dir/subdir`, returning `-1` or inspecting the wrong file entirely.
- **Why Full Path Construction (`parent_dir/subdir`) Is Essential:**
  - Constructing the complete relative or absolute path (e.g., using `snprintf(sub_path, sizeof(sub_path), "%s/%s", dir, names[i])`) ensures that every subsequent system call (`lstat`, `opendir`) receives an unambiguous path that the kernel can resolve correctly regardless of the process's active CWD.
  - This preserves path context throughout arbitrary levels of nested recursion.

