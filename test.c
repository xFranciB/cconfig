#include "util.h"
#include <ini_string.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>

#ifdef _WIN32
#    include <direct.h>
#    include <io.h>
#    define chdir _chdir
#    define access _access
#    define F_OK 0
#elif defined(unix)
#    include <unistd.h>
#    include <sys/wait.h>
#    include <sys/ioctl.h>
#endif

#define TEST_BASEPATH "./test/"
#define LIBRARY_ARCHIVE_NAME "ini.a"
#define LIBRARY_PATH "../"

// Compilation flags
// NOTE: All paths are relative to the TEST_BASEPATH directory
#define CC "cc"
#define CFLAGS "-fsanitize=address -ggdb -Wall -Wextra -pedantic -I../include/ -L" LIBRARY_PATH
// NOTE: LIBRARY_ARCHIVE NAME has to be added after the sources,
// so it's not specified here

// Other constants
#define SUBPROCESS_BUFFER_SIZE 256
#define COPY_BUFFER 256

CREATE_DA(IniString*, IniString)

typedef u8 CMD_ID;
typedef enum {
	COMMAND_ID_RECORD = 0,
	COMMAND_ID_TEST,

	COMMAND_ID_AMOUNT
} COMMAND_IDS;

typedef struct {
	char *filepath;
	char *name;
	u32 number;
} Testcase;

CREATE_DA(Testcase, Testcase)

int testcase_compare(const void *t1, const void *t2) {
	return ((const Testcase*)t1)->number - ((const Testcase*)t2)->number;
}

static inline char *mystrcat(char *dst, char *src) {
	while (*dst) dst++;
	while ((*dst++ = *src++)) {}
	return dst - 1;
}

static inline bool file_exists(const char *filepath) {
	if (access(filepath, F_OK) == -1) {
		if (errno == ENOENT) {
			return false;
		}

		ERROR_EXIT_ERRNO("ERROR: Error when calling access()");
	}

	return true;
}

static inline void print_usage(const char *filename) {
	fprintf(stderr,
		"Usage: %s <command>\n"
		"\n"
		"Available commands:\n"
		"  RECORD    record all tests\n"
		"  TEST      test against recorded tests\n",
		filename
	);
}

static inline void cstr_to_lower(char *str) {
	while (*str) {
		*str = tolower(*str);
		str++;
	}
}

static inline void copy_file(char *src, char *dst) {
#ifdef _WIN32
	assert(0 && "Not implemented");
#elif defined(unix)
	u8 buf[COPY_BUFFER];
	FILE *fin, *fout;
	size_t bytes_read;

	if ((fin = fopen(src, "rb")) == NULL) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fopen()");
	}

	if ((fout = fopen(dst, "wb")) == NULL) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fopen()");
	}

	while ((bytes_read = fread(buf, sizeof(u8), COPY_BUFFER, fin)) > 0) {
		fwrite(buf, sizeof(u8), bytes_read, fout);
	}

	if (fclose(fin) == EOF) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fclose()");
	}

	if (fclose(fout) == EOF) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fclose()");
	}
#endif
}

static inline void delete_file(const char *filename) {
#ifdef _WIN32
	assert(0 && "Not implemented");
#elif defined(unix)
	unlink(filename);
#endif
}

static inline bool files_contents_equal(const char *p1, const char *p2) {
	FILE *f1, *f2;
	bool status;
	char copybuf1[COPY_BUFFER];
	char copybuf2[COPY_BUFFER];

	if ((f1 = fopen(p1, "r")) == NULL) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fopen()");
	}

	if ((f2 = fopen(p2, "r")) == NULL) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fopen()");
	}

	while (true) {
		size_t r1 = fread(copybuf1, sizeof(u8), COPY_BUFFER, f1);
		size_t r2 = fread(copybuf2, sizeof(u8), COPY_BUFFER, f2);

		if (r1 != r2) {
			RETURN_DEFER(false);
		}

		if (r1 == 0) {
			RETURN_DEFER(true);
		}

		if (memcmp(copybuf1, copybuf2, r1) != 0) {
			RETURN_DEFER(false);
		}
	}

defer:
	if (fclose(f1) == EOF) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fclose()\n");
	}

	if (fclose(f2) == EOF) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fclose()\n");
	}

	return status;
}

static inline void free_subprocess(void **out) {
#ifdef _WIN32
	assert(0 && "Not implemented");
#elif defined(unix)
	int *fds = (int*)(*out);
	close(fds[0]);
	close(fds[1]);
	free(fds);
#endif
}

static inline void log_subprocess(void **out, int status_code, FILE *f) {
#ifdef _WIN32
	assert(0 && "Not implemented");
#elif defined(unix)
	int *fds = (int*)(*out);
	char buf[SUBPROCESS_BUFFER_SIZE];
	ssize_t nbytes;

	int out_size;
	int err_size;

	if (ioctl(fds[0], FIONREAD, &out_size) < 0) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling ioctl()");
	}

	if (ioctl(fds[1], FIONREAD, &err_size) < 0) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling ioctl()");
	}

	fprintf(f, "exit %d\nstdout %d\n", status_code, out_size);

	while (true) {
		nbytes = read(fds[0], buf, SUBPROCESS_BUFFER_SIZE);

		if (nbytes < 0) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling read()");
		}

		if (nbytes == 0) {
			break;
		}

		fprintf(f, "%.*s", (int)nbytes, buf);
	}

	fprintf(f, "stderr %d\n", err_size);

	while (true) {
		nbytes = read(fds[1], buf, SUBPROCESS_BUFFER_SIZE);

		if (nbytes < 0) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling read()");
		}

		if (nbytes == 0) {
			break;
		}

		fprintf(f, "%.*s", (int)nbytes, buf);
	}
#endif
}

static inline int run_subprocess(const char *name, const char **argv, size_t argc, void **out) {
#ifdef _WIN32
	assert(0 && "Not implemented");
#elif defined(unix)
	int outpipe[2];
	if (pipe(outpipe) == -1) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling pipe()");
	}

	int errpipe[2];
	if (pipe(errpipe) == -1) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling pipe()");
	}

	pid_t cpid = fork();

	if (cpid == -1) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling fork()");
	}

	if (cpid == 0) {
		if (dup2(outpipe[1], STDOUT_FILENO) == -1) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling dup2(stdout)");
		}

		if (dup2(errpipe[1], STDERR_FILENO) == -1) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling dup2(stderr)");
		}

		close(outpipe[0]);
		close(outpipe[1]);
		close(errpipe[0]);
		close(errpipe[1]);

		IniString_da argvnull;
		IniString_da_init(&argvnull, argc + 2);

		IniString *arg = ini_string_new(name);
		IniString_da_append(&argvnull, arg);

		for (size_t i = 0; i < argc; i++) {
			arg = ini_string_new(argv[i]);
			IniString_da_append(&argvnull, arg);
		}

		IniString_da_append(&argvnull, NULL);

		if (execvp(
			name, argvnull.items
	  	) == -1) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling execvp()");
		}

		ERROR_EXIT_MSG("ERROR: Error when calling execvp()");
	} else {
		int wstatus;

		close(outpipe[1]);
		close(errpipe[1]);

		if (out != NULL) {
			*out = malloc(sizeof(outpipe[0]) + sizeof(errpipe[0]));
			((int*)*out)[0] = outpipe[0];
			((int*)*out)[1] = errpipe[0];
		} else {
			close(outpipe[0]);
			close(errpipe[0]);
		}

		waitpid(cpid, &wstatus, 0);

		return WEXITSTATUS(wstatus);
	}
#endif
}

static inline void compile_testcase(Testcase tc) {
	static IniString_da command = { 0 };
	static size_t base_count = 0;

	// First time running this function, create
	// base compilation command
	if (command.capacity == 0) {
		char *p = CFLAGS;
		char *start = p;
		IniString *str;

		IniString_da_init(&command, 4);

		while (*p) {
			if (*p == ' ') {
				str = ini_string_from_sized_string(start, p - start);
				IniString_da_append(&command, str);

				start = p + 1;
			}

			p++;
		}

		if (start != p) {
			str = ini_string_from_sized_string(start, p - start);
			IniString_da_append(&command, str);
		}
		
		base_count = command.count;
	}

	char buf[256];
	char *end;

	buf[0] = 0;

	end = mystrcat(buf, tc.filepath);
	end = mystrcat(end, "/");
	mystrcat(end, "main.c");

	IniString *inp = ini_string_new(buf);

	IniString_da_append(&command, inp);
	IniString_da_append(&command, "-o");

	*end = 0;
	mystrcat(end, "main");
	IniString_da_append(&command, buf);
	IniString_da_append(&command, "-l:" LIBRARY_ARCHIVE_NAME);

	void *out;
	int status_code = run_subprocess(
		CC, (const char**)command.items, command.count, &out
	);

	if (status_code != 0) {
		fprintf(stderr,
			"\n"
			"ERROR: Error while compiling testcase [%u] %s\n"
			"       Compilation command:\n"
			"       %s",
			tc.number, tc.name, CC
		);

		for (size_t i = 0; i < command.count; i++) {
			fprintf(stderr, " %s", command.items[i]);
		}

		fputc('\n', stderr);

		log_subprocess(&out, status_code, stderr);
		free_subprocess(&out);

		ERROR_EXIT();
	}

	free_subprocess(&out);
	ini_string_free(inp);

	command.count = base_count;
}

static inline bool run_testcase(Testcase tc, CMD_ID command_id) {
	static_assert(COMMAND_ID_AMOUNT == 2, "Incorrect command id amount");
	bool status = true;

	char buf[256];
	char *end;
	void *out;
	IniString *outini;
	IniString_da argv;
	FILE *fout;

	buf[0] = 0;

	// Put run.ini in `outini`
	end = mystrcat(buf, tc.filepath);
	end = mystrcat(end, "/");
	mystrcat(end, "run.ini");
	outini = ini_string_new(buf);

	// Put test.ini in `buf`
	*end = 0;
	mystrcat(end, "test.ini");

	// Copy `buf` to `outini`
	copy_file(buf, outini);

	// Put main in `buf`
	*end = 0;
	mystrcat(end, "main");

	// Create argv array from `outini`
	IniString_da_init(&argv, 1);
	IniString_da_append(&argv, outini);

	int status_code = run_subprocess(buf, (const char**)argv.items, argv.count, &out);

	if (command_id == COMMAND_ID_RECORD) {
		// Put expected.txt in `buf`
		*end = 0;
		mystrcat(end, "expected.txt");

		if ((fout = fopen(buf, "w")) == NULL) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling fopen()");
		}
	} else if (command_id == COMMAND_ID_TEST) {
		// Put actual.txt in `buf`
		*end = 0;
		mystrcat(end, "actual.txt");

		if ((fout = fopen(buf, "w")) == NULL) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling fopen()");
		}
	}

	{
		FILE *runini;
		long len;
		char copybuf[COPY_BUFFER];
		size_t bytes_read;

		log_subprocess(&out, status_code, fout);

		if ((runini = fopen(outini, "r")) == NULL) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling fopen()");
		}

		if (fseek(runini, 0, SEEK_END) < 0) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling fseek()");
		}

		if ((len = ftell(runini)) < 0) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling ftell()");
		}

		if (fseek(runini, 0, SEEK_SET) < 0) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling fseek()");
		}

		fprintf(fout, "ini %ld\n", len);

		while ((bytes_read = fread(copybuf, sizeof(u8), COPY_BUFFER, runini)) > 0) {
			fwrite(copybuf, sizeof(u8), bytes_read, fout);
		}

		if (fclose(runini) == EOF) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling fclose()");
		}

		if (fclose(fout) == EOF) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling fclose()\n");
		}
	}

	delete_file(outini);

	if (command_id == COMMAND_ID_RECORD) {
		// ...

	} else if (command_id == COMMAND_ID_TEST) {
		// Put actual.txt in `outini`
		ini_string_free(outini);
		outini = ini_string_new(buf);

		// Put expected.txt in `buf`
		*end = 0;
		mystrcat(end, "expected.txt");

		if (!files_contents_equal(buf, outini)) {
			RETURN_DEFER(false);
		}

		delete_file(outini);
	}

	status = true;
defer:
	free_subprocess(&out);
	ini_string_free(outini);
	IniString_da_free(&argv);
	return status;
}

static inline void validate_testcase(Testcase tc, CMD_ID command_id) {
	static_assert(COMMAND_ID_AMOUNT == 2, "Incorrect command id amount");
	char buf[256];
	u8 found_magic = 0;

	buf[0] = 0;

	char *end = mystrcat(buf, tc.filepath);
	end = mystrcat(end, "/");
	mystrcat(end, "main.c");

	if (file_exists(buf)) {
		found_magic |= 1 << 0;
	}

	*end = 0;
	mystrcat(end, "test.ini");

	if (file_exists(buf)) {
		found_magic |= 1 << 1;
	}

	if (command_id == COMMAND_ID_TEST) {
		*end = 0;
		mystrcat(end, "expected.txt");

		if (file_exists(buf)) {
			found_magic |= 1 << 2;
		}
	} else {
		found_magic |= 1 << 2;
	}
	
	if (found_magic == 7) {
		return;
	}

	fprintf(stderr,
		"ERROR: Found invalid testcase!\n"
		"       The testcase\n"
		"       - [%u] %s\n"
		"       does not contain all necessary files!\n"
		"\n"
		" NOTE: The required files are:\n",
		tc.number, tc.name
	);

	fputs("       - main.c ", stderr);

	if (found_magic & (1 << 0)) {
		fputs("FOUND\n", stderr);
	} else {
		fputs("NOT FOUND\n", stderr);
	}

	fputs("       - test.ini ", stderr);

	if (found_magic & (1 << 1)) {
		fputs("FOUND\n", stderr);
	} else {
		fputs("NOT FOUND\n", stderr);
	}

	fputs("       - expected.txt (autogenerated, only required by the TEST command) ", stderr);

	if (found_magic & (1 << 2)) {
		fputs("FOUND\n", stderr);
	} else {
		fputs("NOT FOUND\n", stderr);
	}

	ERROR_EXIT();
}

static inline Testcase_da get_testcases(CMD_ID command_id) {
	Testcase_da tcs;
	DIR *dir;
	struct dirent *testcase_dir;

	{
		dir = opendir(".");

		if (dir == NULL) {
			ERROR_EXIT_ERRNO("ERROR: Error when calling opendir()");
		}
	}

	{
		Testcase_da_init(&tcs, 2);
	}

	while ((testcase_dir = readdir(dir))) {
		if (
			strcmp(testcase_dir->d_name, ".") == 0 ||
			strcmp(testcase_dir->d_name, "..") == 0
		) {
			continue;
		}

		size_t namelen = strlen(testcase_dir->d_name);

		Testcase tc;
		tc.filepath = (char*)malloc(namelen + 1);
		tc.name = (char*)malloc(namelen);

		if (sscanf(testcase_dir->d_name, "%u.%s", &tc.number, tc.name) < 2) {
			ERROR_EXIT_MSG(
				"ERROR: Unknown directory in the \"" TEST_BASEPATH "\" directory.\n"
				"       The test directories must have the following format:\n"
				"       <testnumber>.<testname>\n"
				"\n"
				" NOTE: Found: %s\n",
				testcase_dir->d_name
			);
		}

		memcpy(tc.filepath, testcase_dir->d_name, namelen + 1);

		validate_testcase(tc, command_id);
		Testcase_da_append(&tcs, tc);
	}

	if (errno != 0) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling readdir()");
	}

	if (closedir(dir) == -1) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling closedir()");
	}

	qsort(
		tcs.items, tcs.count, sizeof(tcs.items[0]),
		testcase_compare
	);

	return tcs;
}

static inline void free_testcases(Testcase_da *tcs) {
	for (size_t i = 0; i < tcs->count; i++) {
		free(tcs->items[i].filepath);
		free(tcs->items[i].name);
	}

	Testcase_da_free(tcs);
}

int main(int argc, const char **argv) {
	if (argc < 2) {
		print_usage(argv[0]);
		ERROR_EXIT();
	}

	char *command = (char*)argv[1];
	cstr_to_lower(command);

	static_assert(COMMAND_ID_AMOUNT == 2, "Incorrect command id amount");
	CMD_ID command_id;

	if (strcmp(command, "record") == 0) {
		command_id = COMMAND_ID_RECORD;
	} else if (strcmp(command, "test") == 0) {
		command_id = COMMAND_ID_TEST;
	} else {
		fprintf(stderr, "ERROR: Unknown command \"%s\"\n", command);
		print_usage(argv[0]);
		ERROR_EXIT();
	}

	if (chdir(TEST_BASEPATH) == -1) {
		ERROR_EXIT_ERRNO("ERROR: Error when calling chdir()");
	}

	if (!file_exists(LIBRARY_PATH LIBRARY_ARCHIVE_NAME)) {
		ERROR_EXIT_MSG(
			"ERROR: Cannot find library file\n"
			"       Did you forget to build? If so, run `make`"
		);
	}

	Testcase_da tcs = get_testcases(command_id);

	printf("INFO: Found %zu valid testcases, compiling\n", tcs.count);

	for (size_t i = 0; i < tcs.count; i++) {
		compile_testcase(tcs.items[i]);
		printf("    - Compiled [%u] %s\n", tcs.items[i].number, tcs.items[i].name);
	}

	printf("\nINFO: Successfully compiled all testcases, running\n");

	if (command_id == COMMAND_ID_RECORD) {
		for (size_t i = 0; i < tcs.count; i++) {
			if (run_testcase(tcs.items[i], command_id)) {
				printf("    - OK [%u] %s\n", tcs.items[i].number, tcs.items[i].name);
			} else {
				printf("    - ERROR [%u] %s\n", tcs.items[i].number, tcs.items[i].name);

				ERROR_EXIT_MSG("\nSTOP: Testcase [%u] %s failed\n", tcs.items[i].number, tcs.items[i].name);
			}
		}
	} else if (command_id == COMMAND_ID_TEST) {
		for (size_t i = 0; i < tcs.count; i++) {
			if (run_testcase(tcs.items[i], command_id)) {
				printf("    - PASS [%u] %s\n", tcs.items[i].number, tcs.items[i].name);
			} else {
				printf("    - FAIL [%u] %s\n", tcs.items[i].number, tcs.items[i].name);

				ERROR_EXIT_MSG(
					"\n"
					"STOP: Testcase [%u] %s failed\n"
					"      The file `actual.txt` was generated in the failed\n"
					"      testcase's folder with the wrong returned output\n",
					tcs.items[i].number, tcs.items[i].name
				);
			}
		}
	}

	free_testcases(&tcs);
	return 0;
}
