// Author:      Tim Kuelker
// Date:        September 1, 2019
// Course:      CMPSCI 4760 - Operating Systems
// Description: This program gets launched when the user passes in the [-i] option
// to display information on the number of links in inode table.


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

void handle_root(const char * , int );
void show_links_depth_first(const char * , int , int );


int main(int argc, char *argv[]) {
	int indent = atol(argv[1]);
	int spacing = atol(argv[2]);

        fprintf(stderr,"\nDepth-First-Search will now display link count in inode table:\n\n");
        
	handle_root(argv[0], spacing);
        show_links_depth_first(argv[0], indent, spacing);

        return 0;
}


void handle_root(const char *root, int spacing){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;

    if ((dir = opendir(root)) == NULL)
        return;

    spacing -= strlen(root);

    lstat(root, &sb);

    fprintf(stderr, "[%s]%*d\n", root, spacing,  (int)sb.st_nlink);
    closedir(dir);
}


void show_links_depth_first(const char *root, int indent, int spacing){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    int space_holder = spacing;

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

                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s[%s]%*d\n", indent, "", entry->d_name, spacing,  (int)sb.st_nlink);

                show_links_depth_first(path, indent + indent, space_holder);
                break;

            case S_IFLNK:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s{%s}%*d\n", indent, "", entry->d_name, spacing,  (int)sb.st_nlink);
                break;

            case S_IFREG:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s- %s%*d\n", indent, "", entry->d_name, spacing, (int)sb.st_nlink);
                break;

            default:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s*%s*%*d\n", indent, "", entry->d_name, spacing, (int)sb.st_nlink);
                break;
        }
    }
    closedir(dir);
}

