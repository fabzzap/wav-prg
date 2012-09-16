struct audiotap;

void write_cleaned_tap(struct block_list_element* blocks, struct audiotap *input_handle, enum wav2prg_bool need_v2, const char* filename, uint8_t machine, uint8_t videotype, struct wav2prg_display_interface *wav2prg_display_interface, struct display_interface_internal *display_interface_internal);
