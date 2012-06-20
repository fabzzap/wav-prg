#include "observers.h"
#include "wav2prg_api.h"

#include <stdlib.h>
#include <string.h>

static struct wav2prg_observed {
  const char *name;
  const struct wav2prg_observer_loaders **observers;
} *observed_list = NULL;

static void add_observer_to_list(const struct wav2prg_observer_loaders ***observers, const struct wav2prg_observer_loaders *new_observer){
  int number_of_observers;

  for(number_of_observers = 0; (*observers)[number_of_observers] != NULL; number_of_observers++);
  *observers = realloc(*observers, sizeof(**observers) * (2 + number_of_observers));
  (*observers)[number_of_observers + 1] = NULL;
  /* recognition of Kernal loaders is added at end,
     recognition of anything else is added at beginning */
  if (strcmp(new_observer->loader, "Default C64")
   && strcmp(new_observer->loader, "Default C16")
   && strcmp(new_observer->loader, "Kernal data chunk")
   && strcmp(new_observer->loader, "Kernal data chunk C16")
   ){
    memmove((*observers) + 1, *observers, sizeof(**observers) * number_of_observers);
    number_of_observers = 0;
  }
  (*observers)[number_of_observers] = new_observer;
}

static const struct wav2prg_observer_loaders*** get_list_of_observers_maybe_adding_observed(const char *observed_name, enum wav2prg_bool add_if_missing){
  struct wav2prg_observed *current_observed;
  int number_of_observed = 0;

  if (observed_list == NULL)
    observed_list = calloc(sizeof(struct wav2prg_observed), 1);

  for(current_observed = observed_list; current_observed->name != NULL; current_observed++, number_of_observed++){
    if (!strcmp(observed_name, current_observed->name))
      return &current_observed->observers;
  }
  if (add_if_missing)
  {
    observed_list = realloc(observed_list, sizeof(observed_list[0]) * (2 + number_of_observed));
    observed_list[number_of_observed + 1].name = NULL;
    observed_list[number_of_observed + 1].observers = NULL;
    observed_list[number_of_observed].name = observed_name;
    observed_list[number_of_observed].observers = calloc(sizeof(struct wav2prg_observer_loaders*), 1);
    return &observed_list[number_of_observed].observers;
  }
  return NULL;
}

void add_observed(const char *observed_name, const struct wav2prg_observer_loaders *observed){
  const struct wav2prg_observer_loaders*** list = get_list_of_observers_maybe_adding_observed(observed_name, wav2prg_true);

  add_observer_to_list(list, observed);
}

const struct wav2prg_observer_loaders** get_observers(const char *observed_name){
  const struct wav2prg_observer_loaders ***observer = get_list_of_observers_maybe_adding_observed(observed_name, wav2prg_false);
  return observer ? *observer : NULL;
}

void free_observers(void){
  struct wav2prg_observed *current_observed;

  if (observed_list == NULL)
    return;

  for(current_observed = observed_list; current_observed->observers != NULL; current_observed++)
    free(current_observed->observers);
  free(observed_list);
  observed_list = NULL;
}
