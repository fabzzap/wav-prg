#include <stdio.h>

struct simple_block_list_element** add_all_entries_from_file(struct simple_block_list_element **block, FILE *fd);
void put_filename_in_entryname(const char *filename, char *entryname);
