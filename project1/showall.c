// Author:      Tim Kuelker
// Date:        September 1, 2019
// Course:      CMPSCI 4760 - Operating Systems
// Description: This program gets launched when the user passes in the [-l] option
// to display all information about directory.


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

char *gid_to_name(gid_t);
char *uid_to_name(uid_t );
void format_permissions(int , char []);
char *bytes_to_string(long );
void print_info(struct stat );
void handle_root(const char * , int );
void gid_depth_first(const char * , int , int );


int main(int argc, char *argv[]) {
	int indent = atol(argv[1]);
	int spacing = atol(argv[2]);

        fprintf(stderr,"\nDepth-First-Search will now display all information like using options tpiugsd:\n\n");
        
	handle_root(argv[0], spacing);
        gid_depth_first(argv[0], indent, spacing);

        return 0;
}


void format_permissions(int mode, char permissions[]){

        strcpy(permissions, "----------");

	// User Permissions
        if(S_ISDIR(mode)) permissions[0] = 'd';
        if(S_ISCHR(mode)) permissions[0] = 'c';
        if(S_ISBLK(mode)) permissions[0] = 'b';
        if(S_ISLNK(mode)) permissions[0] = 'l';

	// User Permissions
        if(mode & S_IRUSR) permissions[1] = 'r';
        if(mode & S_IWUSR) permissions[2] = 'w';
        if(mode & S_IXUSR) permissions[3] = 'x';

	// Group Permissions
        if(mode & S_IRGRP) permissions[4] = 'r';
        if(mode & S_IWGRP) permissions[5] = 'w';
        if(mode & S_IXGRP) permissions[6] = 'x';

	// Other Permissions
        if(mode & S_IROTH) permissions[7] = 'r';
        if(mode & S_IWOTH) permissions[8] = 'w';
        if(mode & S_IXOTH) permissions[9] = 'x';
}


// Converts current GID format to string
char *gid_to_name(gid_t gid){
        struct group *getgrgid(), *grp_ptr;
        static char numstr[10];

        if((grp_ptr = getgrgid(gid)) == NULL){
                sprintf(numstr, "%d", gid);
                return numstr;
        }
        else
                return grp_ptr->gr_name;
}


// Converts current UID format to string
char *uid_to_name(uid_t uid){
        struct passwd *getpwuid(), *pw_ptr;
        static char numstr[10];

        if((pw_ptr = getpwuid(uid)) == NULL){
                sprintf(numstr, "%d", uid);
                return numstr;
        }
        else
                return pw_ptr->pw_name;
}


// Function that will convert Byte size to respective KB, MB, GB
char *bytes_to_string(long byteCount){
    char* suffix[] = {"B", "KB", "MB", "GB", "TB"};
    char length = sizeof(suffix) / sizeof(suffix[0]);

    int i = 0;
    double size = byteCount;

    if (byteCount > 1024){
        for (i = 0; (byteCount/1024) > 0 && i < length - 1; i++, byteCount /= 1024)
                size = byteCount / 1024.0;
    }

    static char output[200];
    sprintf(output, "%.02lf %s", size, suffix[i]);
    return output;

}


void print_info(struct stat sb){
        char permissions[11];
        format_permissions(sb.st_mode, permissions);
        fprintf(stderr, "%s", permissions);
        fprintf(stderr, "%3d%2s", (int)sb.st_nlink, "");
        fprintf(stderr, "%-8s%2s", uid_to_name(sb.st_uid), "");
        fprintf(stderr, "%-8s%1s", gid_to_name(sb.st_gid), "");
        fprintf(stderr, "%10s%3s", bytes_to_string((long)sb.st_size), "");
        fprintf(stderr, "%12s", ctime(&sb.st_mtime));
}


void handle_root(const char *root, int spacing){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;


    if ((dir = opendir(root)) == NULL)
        return;

    spacing -= strlen(root);

    lstat(root, &sb);

    fprintf(stderr, "[%s]%*s", root, spacing, "");
    print_info(sb);
    closedir(dir);
}


void gid_depth_first(const char *root, int indent, int spacing){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    int space_holder = spacing;
    char permissions[11];

    if ((dir = opendir(root)) == NULL)
        return;

    while ((entry = readdir(dir)) != NULL){
        char path[1024];
        spacing = space_holder;

        snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);

        lstat(path, &sb);

        switch(sb.st_mode & S_IFMT) {

            case S_IFDIR:
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) // || strcmp(entry->d_name, ".git") == 0)
                    continue;

                format_permissions(sb.st_mode, permissions);
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s[%s]%*s", indent, "", entry->d_name, spacing, "");
                print_info(sb);
                gid_depth_first(path, indent + indent, space_holder);
                break;

            case S_IFLNK:
                format_permissions(sb.st_mode, permissions);
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s{%s}%*s", indent, "", entry->d_name, spacing,  "");
                print_info(sb);
                break;

            case S_IFREG:
                format_permissions(sb.st_mode, permissions);
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s- %s%*s", indent, "", entry->d_name, spacing,  "");
                print_info(sb);
                break;

            default:
                format_permissions(sb.st_mode, permissions);
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s*%s*%*s", indent, "", entry->d_name, spacing,  "");
                print_info(sb);
                break;
        }
    }
    closedir(dir);
}

