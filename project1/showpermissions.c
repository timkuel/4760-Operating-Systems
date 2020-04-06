// Author:      Tim Kuelker
// Date:        September 1, 2019
// Course:      CMPSCI 4760 - Operating Systems
// Description: This program gets launched when the user passes in the [-p] option
// to display information on the directories/files permissions.


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

void format_permissions(int , char[] );
void handle_root(const char * , int );
void gid_depth_first(const char * , int , int );


int main(int argc, char *argv[]) {
	int indent = atol(argv[1]);
	int spacing = atol(argv[2]);

        fprintf(stderr,"\nDepth-First-Search will now display the permission levels:\n\n");
        
	handle_root(argv[0], spacing);
        gid_depth_first(argv[0], indent, spacing);

        return 0;
}


void format_permissions(int mode, char permissions[]){

        strcpy(permissions, "---------");

	// User Permissions
        if(mode & S_IRUSR) permissions[0] = 'r';        
        if(mode & S_IWUSR) permissions[1] = 'w';
        if(mode & S_IXUSR) permissions[2] = 'x';

	// Group Permissions
        if(mode & S_IRGRP) permissions[3] = 'r';        
        if(mode & S_IWGRP) permissions[4] = 'w';
        if(mode & S_IXGRP) permissions[5] = 'x';

	// Other Permissions
        if(mode & S_IROTH) permissions[6] = 'r';        
        if(mode & S_IWOTH) permissions[7] = 'w';
        if(mode & S_IXOTH) permissions[8] = 'x';
}


void handle_root(const char *root, int spacing){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    char permissions[10];

    if ((dir = opendir(root)) == NULL)
        return;

    spacing -= strlen(root);

    lstat(root, &sb);

    format_permissions(sb.st_mode, permissions);

    fprintf(stderr, "[%s]%*s\n", root, spacing, permissions);
    closedir(dir);
}


void gid_depth_first(const char *root, int indent, int spacing){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    int space_holder = spacing;
    char permissions[10];

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
                fprintf(stderr, "%*s[%s]%*s\n", indent, "", entry->d_name, spacing, permissions);

                gid_depth_first(path, indent + indent, space_holder);
                break;

            case S_IFLNK:
                format_permissions(sb.st_mode, permissions);
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s{%s}%*s\n", indent, "", entry->d_name, spacing,  permissions);
                break;

            case S_IFREG:
                format_permissions(sb.st_mode, permissions);
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s- %s%*s\n", indent, "", entry->d_name, spacing,  permissions);
                break;

            default:
                format_permissions(sb.st_mode, permissions);
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s*%s*%*s\n", indent, "", entry->d_name, spacing,  permissions);
                break;
        }
    }
    closedir(dir);
}
