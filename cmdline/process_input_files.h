/* WAV-PRG: a program for converting C64 tapes into files suitable
 * for emulators and back.
 * 
 * Copyright (c) Fabrizio Gennari, 1998-2008
 * 
 * The program is distributed under the GNU General Public License.
 * See file LICENSE.TXT for details.
 * 
 * process_input_files.h : header file for process_input_files.c
 * 
 * This file belongs to the prg->wav part
 * This file is part of the command-line version of WAV-PRG
 */

struct simple_block_list_element *
process_input_files(int numarg
                    ,char **argo
                    ,char list_only
                    ,char use_filename_as_c64_name
                    ,char get_whole_t64
);
