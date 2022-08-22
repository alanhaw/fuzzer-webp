#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

int ERR_ON_STR1 = 20008;
int ERR_ON_STR2 = 20009;
int ERR_ON_TRUC = 20010;
int ERR_ON_OPEN = 20011;
int ERR_ON_WRITE = 20012;
int ERR_WRITE_NOT_FULL = 20013;
int ERR_ON_FORK = 20014;
int ERR_ON_EXECL = 20015;
int ERR_ON_WAIT = 20016;
int ERR_ON_SETPGID = 20017;

sigjmp_buf  JumpBuffer;

static const int str_size = 256;
char *data_file_name = (char *) "TEMP_FILE";
char *bin_file_name = (char *) "webp-pixbuf-loader/builddir/tests/t4_gtk3_fuzz_exe";

void myexit(int tmpstatus) {
        fprintf(stdout, "myexit status %d\n", tmpstatus);
        //abort();        // Do not use exit().
        siglongjmp(JumpBuffer, tmpstatus);
}

int run_binary(char *tmp_file_name) {
        pid_t   pid;
        pid_t	childpid;
        int	err = 0;
        size_t	initialSize = 128;
        char    binname[256];

        strcpy (binname, bin_file_name);
        //fprintf(stdout, "Initial pid: %d\n", getpid());
        if ( (pid = fork()) < 0) {
                err = ERR_ON_FORK;
                return err;
        }
        else if (pid == 0) {
                // in child.
                childpid = getpid();
                int res1 = setpgid(childpid, childpid);
                fprintf(stdout, "child pid: %d\n", childpid);
                if (res1 != 0) {
                        fprintf(stdout, "setpgid error %d\n", errno);
                        return 0;       //TAG myexit (ERR_ON_SETPGID);
                }
                if (execl(binname, binname, tmp_file_name, (char *) NULL)) {
                        // check errno
                        return 0; //TAG myexit(ERR_ON_EXECL);
                }
                else {
                        // in child. Execution will never run through here.
                        fprintf(stdout, "In client\n");
                }
        }
        else {
                // in parent.
                int childres = 0;
                int status = 0;
                //fprintf(stdout, "Parent pid: %d\n", getpid());
                //fprintf(stdout, "calling waitpid\n");
                if ((childres = waitpid(pid, &status, 0)) < 0) {
                        err = ERR_ON_WAIT;
                        fprintf(stdout, "parent waitpid error: %d\n", err);
                        return err;
                }
                if (WIFEXITED(status)) {
                        fprintf(stdout, "child exit status: %d\n", WEXITSTATUS(status));
                        err = WEXITSTATUS(status) + 10000;
                }
                else if (WIFSIGNALED(status)) {
                        fprintf(stdout, "child signal exit: %d\n", WTERMSIG(status));
                        err = WTERMSIG(status) + 11000;
                        return WTERMSIG(status);     //TAG propogate the signal.
                }
                //sleep(1);
                fprintf(stdout, "parent finished, result: %d\n", err);
        }
        return 0;       //TAG err
}

int get_new_file_name(char *out_file_name, size_t asize, char *in_file_name) {
	pid_t apid = getpid();
	int ret = snprintf(out_file_name, asize, "%s_%d", in_file_name, apid);
	if ( ret == asize)
		ret = -1;
	
	return ret;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
        char    tmp_file_name[str_size] = "";
        int     ret = 0;

        int     r = sigsetjmp(JumpBuffer,1);
        if (r == 0) {
                int res = get_new_file_name (tmp_file_name, str_size, data_file_name);
                if (res <= 0) { myexit (ERR_ON_STR1); }

                int fd = open (tmp_file_name, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
                if (fd < 0) {
                        if (errno == EEXIST) {
                                fd = open (tmp_file_name, O_TRUNC | O_RDWR);
                                if (fd < 0) { myexit (ERR_ON_TRUC); }
                        } else {
                                myexit (ERR_ON_OPEN);
                        }
                }

                int len = write(fd, Data, Size);
                if (len <= 0) {myexit(ERR_ON_WRITE);}
                if (len != Size) {myexit(ERR_WRITE_NOT_FULL);}
                close(fd);

                int ret = run_binary(tmp_file_name);

                // The file is not needed for later inspection.
                remove(tmp_file_name);
                return ret;     //TAG
        }
        else {
                ret = r;
                if (strlen(tmp_file_name) > 0) {
                        remove(tmp_file_name);
                }
        }
        return 0;        //TAG ret;
}


/*
int main() {

  char buf [10] = {0};
  LLVMFuzzerTestOneInput(buf, 10);

}*/
