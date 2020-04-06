// Author:      Tim Kuelker
// Date:        September 1, 2019
// Course:      CMPSCI 4760 - Operating Systems
// Description: This program gets launched when the user passes in the [-t] option
// to display information on the file type; directory, link, regular file, etc....


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

void handle_root(const char * , int );
void file_type_depth_first(const char * , int , int );


int main(int argc, char *argv[]) {
	int indent = atol(argv[1]);
	int spacing = atol(argv[2]);

	fprintf(stderr,"\nDepth-First-Search will now display file type:\n\n");

        handle_root(argv[0], spacing);
        file_type_depth_first(argv[0], indent, spacing);

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

    fprintf(stderr, "[%s]%*sd\n", root, spacing, "");
    closedir(dir);
}


void file_type_depth_first(const char *root, int indent, int spacing){
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
                fprintf(stderr, "%*s[%s]%*sd\n", indent, "", entry->d_name, spacing, "");;
                file_type_depth_first(path, indent + indent, space_holder);
                break;

            case S_IFLNK:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s{%s}%*sl\n", indent, "", entry->d_name, spacing, "");
                break;

            case S_IFREG:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s- %s%*s-\n", indent, "", entry->d_name, spacing, "");
                break;

            default:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s*%s*%*s-\n", indent, "", entry->d_name, spacing, "");
                break;
        }
    }
    closedir(dir);
}


