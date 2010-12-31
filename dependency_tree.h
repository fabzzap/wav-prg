struct display_interface;
struct display_interface_internal;

struct plugin_tree {
  const char* node;
  struct plugin_tree* first_child;
  struct plugin_tree* first_sibling;
};

void digest_list(const char** list, struct plugin_tree** tree, struct display_interface* error_report, struct display_interface_internal *error_report_internal);
unsigned char are_all_dependencies_ok(const char* loader);
