/*
 * @file csh_config.h
 * @brief CherrySH configuration for the RK3568 test console.
 *
 * Adapted from the upstream stm32 FreeRTOS sample; polling single-task
 * console: blocking sget, no login, inline command execution.
 *
 * @author zhugengyu
 * @date 10.09.2026
 */

#ifndef CSH_CONFIG_H
#define CSH_CONFIG_H

#define CONFIG_CSH_DEBUG 0

#define CONFIG_CSH_DFTROW 25
#define CONFIG_CSH_DFTCOL 80

#define CONFIG_CSH_HISTORY 1
#define CONFIG_CSH_COMPLETION 1
#define CONFIG_CSH_MAX_COMPLETION 40
#define CONFIG_CSH_PROMPTEDIT 1
#define CONFIG_CSH_PROMPTSEG 7
#define CONFIG_CSH_XTERM 0
#define CONFIG_CSH_NEWLINE "\r\n"
#define CONFIG_CSH_SPACE 4
#define CONFIG_CSH_CTRLMAP 0
#define CONFIG_CSH_ALTMAP 0
#define CONFIG_CSH_REFRESH_PROMPT 1

/* sget blocks until at least one character is available. */
#define CONFIG_CSH_NOBLOCK 0

#define CONFIG_CSH_HELP ""
#define CONFIG_CSH_MAXLEN_PATH 128
#define CONFIG_CSH_MAXSEG_PATH 16
#define CONFIG_CSH_MAX_USER 1
#define CONFIG_CSH_MAX_ARG 8
#define CONFIG_CSH_LNBUFF_STATIC 1
#define CONFIG_CSH_LNBUFF_SIZE 256

/* commands run inline in the REPL task */
#define CONFIG_CSH_MULTI_THREAD 0
#define CONFIG_CSH_SIGNAL_HANDLER 0
#define CONFIG_CSH_USER_CALLBACK 1
#define CONFIG_CSH_SYMTAB 1
#define CONFIG_CSH_PRINT_BUFFER_SIZE 512

#endif
