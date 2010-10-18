/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * shellcmd.h - PSPLINK shell command definitions
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.pspdev.org/psp/branches/psplinkusb/psplink/psplink.h $
 * $Id: psplink.h 2021 2006-10-12 22:22:21Z tyranid $
 */

#ifndef __SHELLCMD_H
#define __SHELLCMD_H

#define SHELL_CMD_BEGIN     0xFF
#define SHELL_CMD_END       0xFE
#define SHELL_CMD_SUCCESS 	0xFD
#define SHELL_CMD_ERROR     0xFC
#define SHELL_CMD_CWD 		0xFB
#define SHELL_CMD_TAB		0xFA
#define SHELL_CMD_LASTMOD   0xF9
#define SHELL_CMD_DISASM    0xF8
#define SHELL_CMD_SYMLOAD   0xF7

#ifdef _PCTERM
/* Structure to hold a single command entry */
struct sh_command 
{
	const char *name;		/* Normal name of the command */
	const char *syn;		/* Synonym of the command */
	int (*func)(int argc, char **argv);
	int min_args;
	const char *desc;		/* Textual description */
	const char *detail;
	const char *help;		/* Command usage */
};

#define SHELL_CAT(name, desc) \
   { name, NULL, NULL, 0, desc, "", NULL },
#define SHELL_CMD(name, syn, func, min_args, desc, detail, help) \
   { name, syn, NULL, min_args, desc, detail, help },
#define SHELL_CMD_SHARED(name, syn, func, min_args, desc, detail, help)  \
   { name, syn, func, min_args, desc, detail, help },
#define SHELL_CMD_PCTERM SHELL_CMD_SHARED
#define SHELL_CMD_PSP(name, syn, func, min_args, desc, detail, help)

#else
/* Structure to hold a single command entry */
struct sh_command 
{
	const char *name;		/* Normal name of the command */
	const char *syn;		/* Synonym of the command */
	int (*func)(int argc, char **argv, unsigned int *vRet);	/* Pointer to the command function */
	int min_args;
};

/* Help category, empty on psp side */
#define SHELL_CAT(name, desc)
/* Normal command */
#define SHELL_CMD(name, syn, func, min_args, desc, detail, help) \
   { name, syn, func, min_args },
/* Command that is used on both PSP and PCTERM side (to do special processing) */
#define SHELL_CMD_SHARED SHELL_CMD
/* Command that is only used on the PCTERM */
#define SHELL_CMD_PCTERM(name, syn, func, min_args, desc, detail, help)
/* Command that is only used on the PSP */
#define SHELL_CMD_PSP SHELL_CMD

#endif

/* Define all the shell commands so it can be shared between psplink and pcterm */
#define SHELL_COMMANDS \
	SHELL_CAT("thread", "Commands to manipulate threads") \
	SHELL_CMD("thlist", "tl", thlist_cmd, 0, "List the threads in the system.", \
			"If v is specified as an argument the output will be verbose.", "[v]") \
	SHELL_CMD("thsllist", NULL, thsllist_cmd, 0, "List the sleeping threads in the system.", \
		    "If v is specified as an argument the output will be verbose.", "[v]") \
	SHELL_CMD("thdelist", NULL, thdelist_cmd, 0, "List the delayed threads in the system", \
		    "If v is specified as an argument the output will be verbose.", "[v]") \
	SHELL_CMD("thsulist", NULL, thsulist_cmd, 0, "List the suspended threads in the system", \
		    "If v is specified as an argument the output will be verbose.", "[v]") \
	SHELL_CMD("thdolist", NULL, thdolist_cmd, 0, "List the dormant threads in the system", \
		    "If v is specified as an argument the output will be verbose.", "[v]") \
	SHELL_CMD("thinfo", "ti", thinfo_cmd, 1, "Print info about a thread", \
			"Prints more detailed information about a thread. uid specifies a numeric unique identifier, "\
			"@name is the name of the thread.", "uid|@name") \
	SHELL_CMD("thsusp", "ts", thsusp_cmd, 1, "Suspend a thread", \
			"Suspends a thread (so it stops executing). To resume the thread use the `thresm` command. " \
		    "uid specifies a numeric unique identifier, @name is the name of the thread.", "uid|@name") \
	SHELL_CMD("thspuser", NULL, thspuser_cmd, 0, "Suspend all user threads", "", "") \
	SHELL_CMD("thresm", "tr", thresm_cmd, 1, "Resume a thread", "", "uid|@name") \
	SHELL_CMD("thwake", "tw", thwake_cmd, 1, "Wakeup a thread", "", "uid|@name") \
	SHELL_CMD("thterm", "tt", thterm_cmd, 1, "Terminate a thread", "", "uid|@name") \
	SHELL_CMD("thdel", "td", thdel_cmd, 1, "Delete a thread", "", "uid|@name") \
	SHELL_CMD("thtdel", "tx", thtdel_cmd, 1, "Terminate and delete a thread", "", "uid|@name") \
	SHELL_CMD("thctx",  NULL, thctx_cmd, 1, "Find and print the full thread context", "", "uid|@name") \
	SHELL_CMD("thpri",  "tp", thpri_cmd, 2, "Change a threads current priority", "", "uid|@name pri") \
	SHELL_CMD("thcreat", "tc", thcreat_cmd, 5, "Create a new thread", "", "name entry pri stack attr" )\
	SHELL_CMD("evlist", "el", evlist_cmd, 0, "List the event flags in the system", "", "[v]") \
	SHELL_CMD("evinfo", "ei", evinfo_cmd, 1, "Print info about an event flag", "", "uid|@name") \
	SHELL_CMD("evdel", "ed", evdel_cmd, 1, "Delete an event flag", "", "uid|@name") \
	SHELL_CMD("evset", "es", evset_cmd, 2, "Set event flag bits", "", "uid|@name pattern") \
	SHELL_CMD("evclr", "ec", evclr_cmd, 2, "Clear event flag bits", "", "uid|@name pattern") \
	SHELL_CMD("smlist", "sl", smlist_cmd, 0, "List the semaphores in the system", "", "[v]") \
	SHELL_CMD("sminfo", "si", sminfo_cmd, 1, "Print info about a semaphore", "", "uid|@name") \
	SHELL_CMD("smdel",  "sd", smdel_cmd, 1, "Delete a semaphore", "", "uid|@name") \
	SHELL_CMD("mxlist", "xl", mxlist_cmd, 0, "List the message boxes in the system", "", "[v]") \
	SHELL_CMD("mxinfo", "xi", mxinfo_cmd, 1, "Print info about a message box", "", "uid|@name") \
	SHELL_CMD("mxdel",  "xd", mxdel_cmd, 1, "Delete a messagebox", "", "uid|@name") \
	SHELL_CMD("cblist", "cl", cblist_cmd, 0, "List the callbacks in the system", "", "[v]") \
	SHELL_CMD("cbinfo", "ci", cbinfo_cmd, 1, "Print info about a callback", "", "uid|@name") \
	SHELL_CMD("cbdel",  "cbd", cbdel_cmd, 1, "Delete a callback", "", "uid|@name") \
	SHELL_CMD("vtlist", "zl", vtlist_cmd, 0, "List the virtual timers in the system", "", "[v]") \
	SHELL_CMD("vtinfo", "zi", vtinfo_cmd, 1, "Print info about a virtual timer", "", "uid|@name") \
	SHELL_CMD("vtdel",  "zd", vtdel_cmd, 1, "Delete a virtual timer", "", "uid|@name") \
	SHELL_CMD("vpllist","vl", vpllist_cmd, 0, "List the variable pools in the system", "", "[v]") \
	SHELL_CMD("vplinfo","vi", vplinfo_cmd, 1, "Print info about a variable pool", "", "uid|@name") \
	SHELL_CMD("vpldel",  "vd", vpldel_cmd, 1, "Delete a variable pool", "", "uid|@name") \
	SHELL_CMD("fpllist","fl", fpllist_cmd, 0, "List the fixed pools in the system", "", "[v]") \
	SHELL_CMD("fplinfo","fi", fplinfo_cmd, 1, "Print info about a fixed pool", "", "uid|@name") \
	SHELL_CMD("fpldel", "fd", fpldel_cmd, 1, "Delete a fixed pool", "", "uid|@name") \
	SHELL_CMD("mpplist","pl", mpplist_cmd, 0, "List the message pipes in the system", "", "[v]") \
	SHELL_CMD("mppinfo","pi", mppinfo_cmd, 1, "Print info about a message pipe", "", "uid|@name") \
	SHELL_CMD("mppdel", "pd", mppdel_cmd, 1, "Delete a message pipe", "", "uid|@name") \
	SHELL_CMD("thevlist","tel", thevlist_cmd, 0, "List the thread event handlers in the system", "", "[v]") \
	SHELL_CMD("thevinfo","tei", thevinfo_cmd, 1, "Print info about a thread event handler", "", "uid|@name") \
	SHELL_CMD("thevdel", "ted", thevdel_cmd, 1, "Delete a thread event", "", "uid|@name") \
	SHELL_CMD("thmon", "tm", thmon_cmd, 1, "Monitor thread events", "", "u|k|a [csed]") \
	SHELL_CMD("thmonoff", NULL, thmonoff_cmd, 0, "Disable the thread monitor", "", "") \
	SHELL_CMD("sysstat", NULL, sysstat_cmd, 0, "Print the system status", "", "") \
	 \
	SHELL_CAT("module", "Commands to handle modules") \
	SHELL_CMD("modlist","ml", modlist_cmd, 0, "List the currently loaded modules", \
			"[opts] is a string of optional arguments\n" \
			"v - Verbose mode (print all module information)\n" \
			"t - Print the module's threads\n" \
			"c - Print the module's callbacks\n" \
			, "[opts]") \
	SHELL_CMD("modinfo","mi", modinfo_cmd, 1, "Print info about a module", \
			"[opts] is a string of optional arguments\n" \
			"t - Print the module's threads\n" \
			"c - Print the module's callbacks\n" \
			, "uid|@name [opts]") \
	SHELL_CMD("modstop","ms", modstop_cmd, 1, "Stop a running module", "", "uid|@name") \
	SHELL_CMD("modunld","mu", modunld_cmd, 1, "Unload a module (must be stopped)", "", "uid|@name") \
	SHELL_CMD("modstun","mn", modstun_cmd, 1, "Stop and unload a module", "", "uid|@name") \
	SHELL_CMD("modload","md", modload_cmd, 1, "Load a module", "", "path") \
	SHELL_CMD("modstart","mt", modstart_cmd, 1, "Start a module", "", "uid|@name [args]") \
	SHELL_CMD("modexec","me", modexec_cmd, 1, "LoadExec a module", \
			"@key is the exec key, can be @game, @vsh or @updater" \
			, "[@key] path [args]") \
	SHELL_CMD("modaddr","ma", modaddr_cmd, 1, "Display info about the module at a specified address", \
			"[opts] is a string of optional arguments\n" \
			"t - Print the module's threads\n" \
			"c - Print the module's callbacks\n" \
			, "addr [opts]") \
	SHELL_CMD("ldstart","ld", ldstart_cmd, 1, "Load and start a module", "", "path [args]") \
	SHELL_CMD("kill", NULL, kill_cmd, 1, "Kill a module and all it's threads", "", "uid|@name") \
	SHELL_CMD("debug", "d", debug_cmd, 1, "Start a module under GDB", "", "program.elf [args]") \
	SHELL_CMD("modexp", "mp", modexp_cmd, 1, "List the exports from a module", "", "uid|@name") \
	SHELL_CMD("modimp", NULL, modimp_cmd, 1, "List the imports in a module", "", "uid|@name") \
	SHELL_CMD("modfindx", "mfx", modfindx_cmd, 3, "Find a module's export address", "", "uid|@name library nid|@name") \
	SHELL_CMD("modfindi", "mfi", modfindi_cmd, 3, "Find a module's import address", "", "uid|@name library nid|@name") \
	SHELL_CMD("apihook", NULL, apihook_cmd, 4, "Hook a user mode API call", "", "uid|@name library nid|@name ret [param]") \
	SHELL_CMD("apihooks", NULL, apihooks_cmd, 4, "Hook a user mode API call with sleep", "", "uid|@name library nid|@name ret [param]") \
	SHELL_CMD("apihp", NULL, apihp_cmd, 0, "Print the user mode API hooks", "", "") \
	SHELL_CMD("apihd", NULL, apihd_cmd, 1, "Delete an user mode API hook", "", "id") \
	 \
	SHELL_CAT("memory", "Commands to manipulate memory") \
	SHELL_CMD("meminfo", "mf", meminfo_cmd, 0, "Print free memory info", "", "[partitionid]") \
	SHELL_CMD("memreg",  "mr", memreg_cmd, 0, "Print available memory regions (for other commands)", "", "") \
	SHELL_CMD("memdump", "dm", memdump_cmd, 0, "Dump memory to screen", "", "[addr|-] [b|h|w]") \
	SHELL_CMD("memblocks", "mk", memblocks_cmd, 0, "Dump the sysmem block table", "", "[f|t]") \
	SHELL_CMD("malloc", NULL, malloc_cmd, 4, "Alloc from a mem partition", "", "pid name l|h size") \
	SHELL_CMD("mfree", NULL, mfree_cmd, 1, "Free from a mem partition", "", "uid") \
	SHELL_CMD("mhead", NULL, mhead_cmd, 1, "Get head pointer for a mem allocation", "", "uid") \
	SHELL_CMD("savemem", "sm", savemem_cmd, 3, "Save memory to a file", "", "addr size path") \
	SHELL_CMD("loadmem", "lm", loadmem_cmd, 2, "Load memory from a file", "", "addr path [maxsize]") \
	SHELL_CMD("pokew",   "pw", pokew_cmd, 2, "Poke words into memory", "", "addr val1 [val2..valN]") \
	SHELL_CMD("pokeh",   "ph", pokeh_cmd, 2, "Poke half words into memory", "", "addr val1 [val2..valN]") \
	SHELL_CMD("pokeb",   "pb", pokeb_cmd, 2, "Poke bytes into memory", "", "addr val1 [val2..valN]") \
	SHELL_CMD("pokes",   "ps", pokes_cmd, 2, "Poke a string into memory", "", "addr str") \
	SHELL_CMD("peekw",   "kw", peekw_cmd, 1, "Peek the word at address", "", "addr [o|b|x|f]") \
	SHELL_CMD("peekh",   "kh", peekh_cmd, 1, "Peek the half word at address", "", "addr [o|b|x]") \
	SHELL_CMD("peekb",   "kb", peekb_cmd, 1, "Peek the byte at address", "", "addr [o|b|x]") \
	SHELL_CMD("fillw",   "fw", fillw_cmd, 3, "Fill a block of memory with a word value", "", "addr size val") \
	SHELL_CMD("fillh",   "fh", fillh_cmd, 3, "Fill a block of memory with a half value", "", "addr size val") \
	SHELL_CMD("fillb",   "fb", fillb_cmd, 3, "Fill a block of memory with a byte value", "", "addr size val") \
	SHELL_CMD("copymem", "cm", copymem_cmd, 3, "Copy a block of memory", "", "srcaddr destaddr size") \
	SHELL_CMD("findstr", "ns", findstr_cmd, 3, "Find an ASCII string", "", "addr size str") \
	SHELL_CMD("findhex", "nx", findhex_cmd, 3, "Find an hexstring string", "", "addr size hexstr [mask]") \
	SHELL_CMD("findw",   "nw", findw_cmd, 3, "Find a list of words", "", "addr size val1 [val2..valN]") \
	SHELL_CMD("findh",   "nh", findh_cmd, 3, "Find a list of half words", "", "addr size val1 [val2..valN]") \
	SHELL_CMD("dcache",  "dc", dcache_cmd, 1, "Perform a data cache operation", \
			"w  - Writeback the cache\n" \
			"i  - Invalidate the cache\n" \
			"wi - Writeback and invalidate the cache\n" \
			"addr and size is an optional range. If not specified will perform\n" \
			"the action over the entire cache" \
			, "w|i|wi [addr size]") \
	SHELL_CMD("icache",  "ic", icache_cmd, 0, "Invalidate the instruction cache", "", "[addr size]") \
	SHELL_CMD("disasm",  "di", disasm_cmd, 1, "Disassemble instructions", "", "address [count]") \
	SHELL_CMD_PCTERM("asm", NULL, asm_cmd, 1, "Assemble instructions to a memory address", "", "addr [inst]") \
	SHELL_CMD_PCTERM("disopts", NULL, disopts_cmd, 0, "Print/set/clear the current disassembler options", "", "[+opts|-opts]") \
	SHELL_CMD("memprot", NULL, memprot_cmd, 1, "Set memory protection on or off", "", "on|off") \
	 \
	SHELL_CAT("fileio", "Commands to handle file io") \
	SHELL_CMD("ls",  "dir", ls_cmd,    0, "List the files in a directory", "", "[path1..pathN]") \
	SHELL_CMD("chdir", "cd", chdir_cmd, 1, "Change the current directory", "", "path") \
	SHELL_CMD("cp",  "copy", cp_cmd, 2, "Copy a file", "", "source destination") \
	SHELL_CMD("mkdir", NULL, mkdir_cmd, 1, "Make a Directory", "", "dir") \
	SHELL_CMD("rm", "del", rm_cmd, 1, "Removes a File", "", "file") \
	SHELL_CMD("rmdir", "rd", rmdir_cmd, 1, "Removes a Directory", "", "dir") \
	SHELL_CMD("rename", "ren", rename_cmd, 2, "Renames a File", "", "src dst") \
	SHELL_CMD("remap", NULL, remap_cmd, 2, "Remaps a device to another", "", "devfrom: devto:") \
	SHELL_CMD("pwd",   NULL, pwd_cmd, 0, "Print the current working directory", "", "") \
 \
	SHELL_CAT("debugger", "Debug commands") \
	SHELL_CMD("exprint", "ep", exprint_cmd, 0, "Print the current exception info", "", "[ex]") \
	SHELL_CMD("exlist",  "exl", exlist_cmd, 0, "List the exception contexts", "", "") \
	SHELL_CMD("exctx",   "ecx", exctx_cmd, 1, "Set the current exception context", "", "ex") \
	SHELL_CMD("exresume", "c", exresume_cmd, 0, "Resume from the exception", "", "[addr]") \
	SHELL_CMD("exprfpu", "ef", exprfpu_cmd, 0, "Print the current FPU registers", "", "[ex]") \
	SHELL_CMD("exprvfpu", "ev", exprvfpu_cmd, 0, "Print the current VFPU registers", "", "[s|c|r|m|e] [ex]") \
	SHELL_CMD("setreg", "str", setreg_cmd, 2, "Set the value of an exception register", "", "$reg value") \
	SHELL_CMD("bpset", "bp", bpset_cmd, 1, "Set a break point", \
			"addr is the address at which the breakpoint should be set.\nOpts is a list of options for the "\
			"breakpoint.\n1 - specifies that this will be a one shot breakpoint\n" \
			"h - specifies this is a hardware breakpoint (if available)\n", \
			"addr [opts]") \
	SHELL_CMD("bpdel", "bc", bpdel_cmd, 1, "Delete a breakpoint", "", "num|addr") \
	SHELL_CMD("bpdis", "bd", bpdis_cmd, 1, "Disable a breakpoint", "", "num|addr") \
	SHELL_CMD("bpena", "be", bpena_cmd, 1, "Enable a breakpoint", "", "num|addr") \
	SHELL_CMD("bpth", NULL, bpth_cmd, 1, "Break a thread at the first available point", "", "uid|@name") \
	SHELL_CMD("bpprint", "bt", bpprint_cmd, 0, "Print the current breakpoints", "", "") \
	SHELL_CMD("hwprint", NULL, hwprint_cmd, 0, "Print the hardware debugger registers", "", "") \
	SHELL_CMD("step", "s", step_cmd, 0, "Step the next instruction", "", "") \
	SHELL_CMD("skip", "k", skip_cmd, 0, "Skip the next instruction (i.e. jump over jals)", "", "") \
	SHELL_CMD("call", NULL, call_cmd, 1, "Issue a function call", "", "addr [arg0...arg5]") \
 \
	SHELL_CAT("misc", "Miscellaneous commands (e.g. USB, exit)") \
	SHELL_CMD("usbstat", "us", usbstat_cmd, 0, "Display the status of the USB connection", "", "") \
    SHELL_CMD("uidlist","ul", uidlist_cmd, 0, "List the system UIDS", "", "[root]") \
	SHELL_CMD("uidinfo", "ui", uidinfo_cmd, 1, "Print info about a UID", "", "uid|@name [parent]") \
	SHELL_CMD("cop0", "c0", cop0_cmd, 0, "Print the cop0 registers", "", "") \
	SHELL_CMD_SHARED("exit", "quit", exit_cmd, 0, "Exit the shell", "", "") \
	SHELL_CMD_PCTERM("close", NULL, close_cmd, 0, "Close the shell (leave PSP running)", "", "") \
	SHELL_CMD("scrshot", "ss", scrshot_cmd, 1, "Take a screen shot", "", "file [pri]") \
	SHELL_CMD("calc", NULL, calc_cmd, 1, "Do a simple address calculation", "", "addr [d|o|x]") \
	SHELL_CMD("reset", "r", reset_cmd, 0, "Reset", "", "[key]") \
	SHELL_CMD("ver", "v", version_cmd, 0, "Print version of psplink", "", "") \
	SHELL_CMD("pspver", NULL, pspver_cmd, 0, "Print the version of PSP", "", "") \
	SHELL_CMD("config", NULL, config_cmd, 0, "Print the configuration file settings", "", "") \
	SHELL_CMD("confset", NULL, confset_cmd, 1, "Set a configuration value", "", "name [value]") \
	SHELL_CMD("confdel", NULL, confdel_cmd, 1, "Delete a configuration value", "", "name") \
	SHELL_CMD("power", NULL, power_cmd, 0, "Print power information", "", "") \
	SHELL_CMD("poweroff", NULL, poweroff_cmd, 0, "Power off the PSP", "", "") \
	SHELL_CMD("clock", NULL, clock_cmd, 3, "Set the clock frequencies", "", "cpu ram bus") \
	SHELL_CMD_PCTERM("tty", NULL, tty_cmd, 0, "Enter TTY mode.", \
			"In this mode all input goes to stdin on the PSP.\nTo exit tty mode type ~. at " \
			"the start of the line.", "") \
	SHELL_CMD("tonid", NULL, tonid_cmd, 1, "Calculate the NID from a name", "", "name") \
	SHELL_CMD("profmode", NULL, profmode_cmd, 0, "Set or display the current profiler mode", "", "[t|g|o]") \
	SHELL_CMD("debugreg", NULL, debugreg_cmd, 0, "Set or display the current debug register", "", "[val]") \
	SHELL_CMD("irqs", NULL, irqs_cmd, 0, "Display the current IRQ list (or a specific int plus subs)", "", "[intno]")\
	SHELL_CMD("iena", NULL, iena_cmd, 2, "Enable or disable an interrupt or sub-interrupt", "", "d|e intno [sub]") \
	SHELL_CMD("irel", NULL, irel_cmd, 1, "Release an interrupt or sub-interrupt", "", "intno [sub]") \
	SHELL_CMD_PCTERM("env", NULL, env_cmd, 0, "Display the environment settings", "", "") \
	SHELL_CMD_PCTERM("set", NULL, set_cmd, 1, "Set an environment variable", "", "name=value") \
	SHELL_CMD_PCTERM("unset", NULL, unset_cmd, 1, "Unset an environment variable", "", "name") \
	SHELL_CMD_PCTERM("echo", NULL, echo_cmd, 0, "Echo text to the screen", "", "[args...]") \
	SHELL_CMD_PCTERM("error", NULL, error_cmd, 1, "Print the name of a known kernel error", "", "no") \
	SHELL_CMD_PCTERM("strlen", NULL, strlen_cmd, 1, "Get length of a string", "", "str") \
	SHELL_CMD_PSP("tab", NULL, tab_cmd, 1, "Tab Completion", "", "dir [file]") \
	SHELL_CMD_SHARED("symload", NULL, symload_cmd, 0, "Load a symbol set", \
			"Load a set of symbols. Can either specify a module name or" \
			"a local path to an ELF file. If no argument is supplied it will" \
			"issue load commands for all current modules. PSP symbol location is " \
			"specified with the PSP_SYMBOL_PATH environment variable.",\
		   	"symload [@module|file]") \
	SHELL_CMD_PCTERM("help", "?", help_cmd, 0, "Print help about a command", \
			"If a category is specified print all commands underneath. If a command is specified " \
			"prints specific help.", "[category|command]") \
	SHELL_CMD(NULL, NULL, NULL, 0, NULL, NULL, NULL)

#endif
