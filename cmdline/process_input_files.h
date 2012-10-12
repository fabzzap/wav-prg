/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 * 
 * Copyright (c) Fabrizio Gennari, 2008-2012
 * 
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 * 
 * process_input_files.h : header file for process_input_files.c
 */

#include <stdint.h>

struct simple_block_list_element *
process_input_files(int numarg
                    ,char **argo
                    ,char list_only
                    ,uint8_t use_filename_as_c64_name
                    ,uint8_t get_whole_t64
);
