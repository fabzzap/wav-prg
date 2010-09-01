struct plugin_tree;

void digest_list(const char** list, struct plugin_tree** tree);
unsigned char are_all_dependencies_ok(const char* loader);
