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
