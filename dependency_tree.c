#include "wav2prg_api.h"
#include "loaders.h"

#include <malloc.h>
#include <string.h>

static const char* get_plugin_this_is_dependent_on(const struct wav2prg_plugin_functions* this_plugin) {
  const struct wav2prg_plugin_conf *model_conf = this_plugin->get_new_plugin_state();
  int use_first_copy=1;

  if (!model_conf->dependency)
    return NULL;
  if (model_conf->dependency->other_dependency)
    return model_conf->dependency->other_dependency;
  if (model_conf->dependency->kernal_dependency == wav2prg_kernal_header)
    return use_first_copy ? "Kernal header chunk 1st copy" : "Kernal header chunk 2nd copy";
  return use_first_copy ? "Kernal data chunk 1st copy" : "Kernal data chunk 2nd copy";
}

static struct plugin_tree **is_this_plugin_already_in_tree(const char* this_plugin_name,struct plugin_tree **tree) {
  if (this_plugin_name == NULL)
    return tree;
  if (!*tree)
    return NULL;
  if (!strcmp((*tree)->node, this_plugin_name))
    return &((*tree)->first_child);
  if ((*tree)->first_child){
    struct plugin_tree **child_return = is_this_plugin_already_in_tree(this_plugin_name, &((*tree)->first_child));
    if(child_return)
      return child_return;
  }
  return ((*tree)->first_sibling) ?
    is_this_plugin_already_in_tree(this_plugin_name, &((*tree)->first_sibling)) : NULL;
}

void delete_tree(struct plugin_tree** tree) {
  if(*tree != NULL) {
    delete_tree(&(*tree)->first_child);
    delete_tree(&(*tree)->first_sibling);
    free(*tree);
    *tree = NULL;
  }
}

void digest_list(const char** list, struct plugin_tree** tree) {
  for(;*list; list++) {
    const char* this_plugin_name = *list;
    struct plugin_tree*  minitree = NULL;
    struct plugin_tree** where_to_add;
    while((where_to_add = is_this_plugin_already_in_tree(this_plugin_name, tree)) == NULL) {
      struct plugin_tree *new_minitree = NULL;
      const struct wav2prg_plugin_functions* next_in_chain = get_loader_by_name(this_plugin_name);

      if(next_in_chain == NULL) {
        delete_tree(&minitree);
        return;
      }

      new_minitree = malloc(sizeof(struct plugin_tree));
      new_minitree->node = this_plugin_name;
      new_minitree->first_sibling = NULL;
      new_minitree->first_child = minitree;

      minitree = new_minitree;
      this_plugin_name = get_plugin_this_is_dependent_on(next_in_chain);
    }
    while(*where_to_add != NULL)
      where_to_add = &((*where_to_add)->first_sibling);

    *where_to_add = minitree;
  }
}

unsigned char are_all_dependencies_ok(const char* loader) {
  while(loader) {
    const struct wav2prg_plugin_functions* this_plugin = get_loader_by_name(loader);
    if (!this_plugin)
      return 0;
    if (this_plugin->get_block_info)
      return 1;
    loader = get_plugin_this_is_dependent_on(this_plugin);
  }
  return 1;
}

/*int main() {
  const char *list[] = {
    "Kernal data chunk 1st copy",
    "Kernal header chunk 1st copy",
    NULL
  };
  struct plugin_tree* tree = NULL;

  register_loaders();
  digest_list(list, &tree);
  delete_tree(&tree);
  return 0;
}*/
