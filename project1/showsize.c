// Author:      Tim Kuelker
// Date:        September 1, 2019
// Course:      CMPSCI 4760 - Operating Systems
// Description: This program gets launched when the user passes in the [-s] option
// to display information on the directories size in B, KB, MB, GB...et.


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

char *bytes_to_string(long );
void handle_root(const char * , int );
void show_links_depth_first(const char * , int , int );


int main(int argc, char *argv[]) {
	int indent = atol(argv[1]);
	int spacing = atol(argv[2]);

        fprintf(stderr,"\nDepth-First-Search will now display size:\n\n");
        
	handle_root(argv[0], spacing);
        show_links_depth_first(argv[0], indent, spacing);

        return 0;
}


char *bytes_to_string(long byteCount)
{
    char* suffix[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
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


void handle_root(const char *root, int spacing){
    DIR *dir;
    struct dirent *entry;
    struct stat sb;

    if ((dir = opendir(root)) == NULL)
        return;

    spacing -= strlen(root);

    lstat(root, &sb);

    fprintf(stderr, "[%s]%*s\n", root, spacing,  bytes_to_string((long)sb.st_size));
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
                fprintf(stderr, "%*s[%s]%*s\n", indent, "", entry->d_name, spacing, bytes_to_string((long)sb.st_size));

                show_links_depth_first(path, indent + indent, space_holder);
                break;

            case S_IFLNK:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s{%s}%*s\n", indent, "", entry->d_name, spacing, bytes_to_string((long)sb.st_size));
                break;

            case S_IFREG:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s- %s%*s\n", indent, "", entry->d_name, spacing, bytes_to_string((long)sb.st_size));
                break;

            default:
                spacing -= (indent + strlen(entry->d_name));
                fprintf(stderr, "%*s*%s*%*s\n", indent, "", entry->d_name, spacing, bytes_to_string((long)sb.st_size));
                break;
        }
    }
    closedir(dir);
}


