/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 *
 * Copyright (c) Fabrizio Gennari, 2010-2012
 *
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 *
 * yet_another_getopt.c : reads command-line options, and makes them self-documenting
 */

#include "yet_another_getopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct option_names {
  char* option_name;
  uint32_t option_index;
};

static enum wav2prg_bool search_option(const struct option_names *names, const char *name, uint32_t *option){
  for(; names->option_name != NULL; names++){
    if(!strcmp(names->option_name, name)){
      if (option)
        *option = names->option_index;
      return wav2prg_true;
    }
  }
  return wav2prg_false;
}

static enum wav2prg_bool find_single_character_match(const struct option_names *names, const char option, uint32_t *option_index, char **last_matched_option){
  free(*last_matched_option);
  *last_matched_option = malloc(2);
  (*last_matched_option)[0] = option;
  (*last_matched_option)[1] = 0;
  return search_option(names, *last_matched_option, option_index);
}

static enum wav2prg_bool find_multiple_character_match(const struct option_names *names, const char *option, uint32_t *option_index, char **last_matched_option, uint32_t *consumed_chars){
  char *equal_pos;

  free(*last_matched_option);
  *last_matched_option = strdup(option);

  equal_pos = strchr(*last_matched_option, '=');
  if (equal_pos)
    *equal_pos = 0;

  *consumed_chars = strlen(*last_matched_option) - 1;    

  return search_option(names, *last_matched_option, option_index);
}

enum wav2prg_bool yet_another_getopt(const struct get_option *options, uint32_t *argc, char **argv){
  uint32_t total_options = 0, total_names = 0, i;
  struct option_names* names = calloc(1, sizeof(struct option_names));
  uint32_t current_arg_num = 0, current_arg_pos = 0, num_arg_non_options = 0;
  uint32_t *arg_non_options = malloc(*argc * sizeof(uint32_t));
  enum {
    beginning_of_one_arg,
    single_dash_at_beginning,
    double_dash_at_beginning,
    one_letter_arg,
    multi_letter_arg,
    may_be_argument_option,
    must_be_argument_option,
    next_is_argument_option,
    after_multi_letter_no_argument
  } state = beginning_of_one_arg;
  uint32_t found_option;
  enum wav2prg_bool ok_so_far = wav2prg_true;
  const struct get_option *options_to_get_names_from;
  char *last_matched_option = NULL;
  
  for(options_to_get_names_from = options; options_to_get_names_from->names; options_to_get_names_from++){
    const char **name;
    for (name = options_to_get_names_from->names; *name != NULL; name++){
      if(!search_option(names, *name, NULL)){
        names = realloc(names, sizeof(struct option_names) * (total_names + 2));
        names[total_names  ].option_name  = strdup(*name) ;
        names[total_names++].option_index = total_options;
        names[total_names  ].option_name  = NULL;
      }
    }
    total_options++;
  }
  
  while(current_arg_num < *argc && ok_so_far){
    enum {
      keep_going,
      argument_consumed,
      argument_is_not_option
    } state_of_current_arg = keep_going;
    const char* current_arg = argv[current_arg_num] + current_arg_pos;
    
    switch(state){
    case beginning_of_one_arg:
      switch(current_arg[0]){
      case '-':
        state = single_dash_at_beginning;
        break;
      default:
        state_of_current_arg = argument_is_not_option;
        break;
      }
      break;
    case single_dash_at_beginning:
      switch(current_arg[0]){
      case '-':
        state = double_dash_at_beginning;
        break;
      case 0:
        state_of_current_arg = argument_is_not_option;
      default:
        ok_so_far = find_single_character_match(names, current_arg[0], &found_option, &last_matched_option);
        if (ok_so_far){
          switch(options[found_option].arguments){
          case option_no_argument:
            ok_so_far = options[found_option].callback(NULL, options[found_option].callback_parameter);
            state = one_letter_arg;
            break;
          case option_may_have_argument:
            state = may_be_argument_option;
            break;
          case option_must_have_argument:
            state = must_be_argument_option;
            break;
          }
        }
        else
          printf("Option %c unknown\n", current_arg[0]);
        break;
      }
      break;
    case one_letter_arg:
      switch(current_arg[0]){
      case 0:
        state = beginning_of_one_arg;
        break;
      default:
        ok_so_far = find_single_character_match(names, current_arg[0], &found_option, &last_matched_option);
        if (ok_so_far){
          switch(options[found_option].arguments){
          case option_no_argument:
            ok_so_far = options[found_option].callback(NULL, options[found_option].callback_parameter);
            state = one_letter_arg;
            break;
          case option_may_have_argument:
            state = may_be_argument_option;
            break;
          case option_must_have_argument:
            state = must_be_argument_option;
            break;
          }
        }
        else
          printf("Option %c unknown\n", current_arg[0]);
        break;
      }
      break;
    case may_be_argument_option:
      switch(current_arg[0]){
      case 0:
        ok_so_far = options[found_option].callback(NULL, options[found_option].callback_parameter);
        state = beginning_of_one_arg;
        break;
      case '=':
        current_arg_pos++;
        current_arg    ++;
        /* fallback */
      default:
        ok_so_far = options[found_option].callback(current_arg, options[found_option].callback_parameter);
        state_of_current_arg = argument_consumed;
        state = beginning_of_one_arg;
        break;
      }
      break;
    case must_be_argument_option:
      switch(current_arg[0]){
      case 0:
        state = next_is_argument_option;
        break;
      case '=':
        current_arg_pos++;
        current_arg    ++;
        /* fallback */
      default:
        ok_so_far = options[found_option].callback(current_arg, options[found_option].callback_parameter);
        state_of_current_arg = argument_consumed;
        state = beginning_of_one_arg;
        break;
      }
      break;
    case next_is_argument_option:
      ok_so_far = options[found_option].callback(current_arg, options[found_option].callback_parameter);
      state_of_current_arg = argument_consumed;
      state = beginning_of_one_arg;
      break;
    case double_dash_at_beginning:
      switch(current_arg[0]){
      case '-':
        state_of_current_arg = argument_is_not_option;
        state = beginning_of_one_arg;
        argv[current_arg_num] += 2;
        break;
      default:
        {
          uint32_t matched;
          ok_so_far = find_multiple_character_match(names, current_arg, &found_option, &last_matched_option, &matched);
          if(ok_so_far){
            current_arg_pos += matched;
            current_arg     += matched;
            switch(options[found_option].arguments){
            case option_no_argument:
              ok_so_far = options[found_option].callback(NULL, options[found_option].callback_parameter);
              state = after_multi_letter_no_argument;
              break;
            case option_may_have_argument:
              state = may_be_argument_option;
              break;
            case option_must_have_argument:
              state = must_be_argument_option;
              break;
            }
          }
          else
            printf("Option %s unknown\n", current_arg);
        }
        break;
      }
      break;
    case after_multi_letter_no_argument:
      switch(current_arg[0]){
      case 0:
        state = beginning_of_one_arg;
        break;
      default:
        printf("Argument given to option %s\n", last_matched_option);
        ok_so_far = wav2prg_false;
        break;
      }
      break;
    }
    if (state_of_current_arg != keep_going){
      if(state_of_current_arg == argument_is_not_option)
        arg_non_options[num_arg_non_options++] = current_arg_num;
      current_arg_pos += strlen(current_arg);
      current_arg += strlen(current_arg);
    }
    if (current_arg[0] == 0){
      current_arg_num++;
      current_arg_pos = 0;
    }
    else
      current_arg_pos++;
  }
  if (state == next_is_argument_option){
    printf("No argument given to option %s\n", last_matched_option);
    ok_so_far = wav2prg_false;
  }

  for(i = 0; names[i].option_name; i++)
    free(names[i].option_name);
  free(names);
  
  for (i = 0; i < num_arg_non_options; i++)
    argv[i] = argv[arg_non_options[i]];
  *argc = num_arg_non_options;
  free(arg_non_options);
  
  free(last_matched_option);

  return ok_so_far;
}

void list_options(const struct get_option *options)
{
  const struct get_option *option;

  for(option = options; option->names != NULL; option++){
    const char **names;
    enum wav2prg_bool first = wav2prg_true;
    for(names = option->names; *names != NULL; names++){
      if (first)
        first = wav2prg_false;
      else
        printf(", ");
      if(strlen(*names) == 1)
        printf("-");
      else
        printf("--");
      printf("%s", *names);
    }
    switch(option->arguments){
    case option_no_argument:
      break;
    case option_may_have_argument:
      printf(" [argument]");
      break;
    case option_must_have_argument:
      printf(" <argument>");
      break;
    }
    printf(":\n\t%s\n", option->description);
  }
}

/*
static enum wav2prg_bool c1(const char* arg, void* a)
{
  return wav2prg_true;
}

int main(int argc, char **argv)
{
  const char *o1names[]={"a", "b", "ccc", NULL};
  const char *o2names[]={"e", "ciao", NULL};
  const char *o3names[]={"f", "ddd", NULL};
  struct get_option x[] ={
    {
      o1names,
      "Does something",
      c1,
      NULL,
      wav2prg_true,
      option_no_argument
    },
    {
      o2names,
      "Does more",
      c1,
      NULL,
      wav2prg_true,
      option_must_have_argument
    },
    {
      o3names,
      "Does less",
      c1,
      NULL,
      wav2prg_true,
      option_may_have_argument
    },
    {NULL}
  };
  yet_another_getopt(x, &argc, argv);
  return 0;
}
*/
