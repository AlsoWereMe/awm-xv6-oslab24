#include "user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

// Find match name
char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

void find(char *path, char *target_name) {
    char buf[512];
    char *p;
    int fd;
    
    struct dirent de;
    struct stat st;

    // Open the path.
    if((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // Get the path state such as type.
    if(fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // Compare the current path name with the target name
    if(strcmp(fmtname(path), target_name) == 0) {
        printf("%s\n", path);
    }

    if (st.type == T_DIR)
    {
        // Ensure path can be contained.
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            fprintf(2, "find: path too long\n");
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
            // If no file in dirc, just continue.
            if(de.inum == 0)
                continue;
            // Copy and null-terminate the name
            char name[DIRSIZ+1];
            memmove(name, de.name, DIRSIZ);
            name[DIRSIZ] = '\0';

            // Get the state of file or dirc.
            // Skip "." and ".."
            if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;
            
            // Build the new path
            strcpy(p, name);

            if(stat(buf, &st) < 0) {
                fprintf(2, "find: cannot stat %s\n", buf);
                continue;
            }

            if(strcmp(name, target_name) == 0) {
                printf("%s\n", buf);
            }
            
            // Recurse into the directory
            if(st.type == T_DIR) {
                find(buf, target_name);
            }
        }
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Too few arguments.\n");
        exit(1);
    } else if (argc > 3)
    {
        printf("Too much arguments.\n");
        exit(1);
    }
    
    find(argv[1], argv[2]);
    return 0;
}