#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fs.h>

#define DISKNAME "test_disk.fs"
#define DATA_BLOCK_COUNT 50 

/** Change any string to be outputted in the color specified
 * @str: string to be changed color
 * @color: color number to be used in formatting
 * 
 * return: string formatted in @color
*/
char * color(const char *str, int color) {
    // allocate space for the string and the extra chars to make it red
    char *res = malloc((strlen(str) + 20) * sizeof(char));

    // add chars to make the text red and then reset formatting
    sprintf(res, "\e[0;%dm%s\e[0m", color, str);
    return res;
}

// change to specific colors
#define grey(str) color(str, 30)
#define red(str) color(str, 31)
#define green(str) color(str, 32)

// assert function
#define ASSERT(cond, func)                                      \
do {                                                            \
	if (!(cond)) {                                              \
		fprintf(stderr, red("Function '%s' failed\n"), func);   \
		exit(EXIT_FAILURE);                                     \
	}                                                           \
    else if (cond && func != NULL) {                            \
        fprintf(stderr, grey("Passed '%s'!\n"), func);          \
    }                                                           \
} while (0)    

// delete and create a new disk
void reset_disk(char *diskname, int data_block_count) {
    int system_ret;
    char cmd[150];

    // erase disk if exists
    sprintf(cmd, "rm %s", diskname);
    system_ret = system(cmd);

    // create disk
    sprintf(cmd, "./fs_make.x %s %d", diskname, data_block_count);
    system_ret = system(cmd);
    ASSERT(system_ret == 0, "setup");
}


/* START OF TESTS (register tests to run in function main()) */

void simple_test_everythig()
{
	int ret;
	int fd;
	char data[10] = "0123456789";
    char read_buf[10];
    fprintf(stderr, "%s", color("\n------TESTING simple_test_everythig------\n", 33));

    reset_disk(DISKNAME, DATA_BLOCK_COUNT);

	/* Mount disk */
	ret = fs_mount(DISKNAME);
	ASSERT(!ret, "fs_mount");

    /* Create file */
	ret = fs_create("myfile");
	ASSERT(ret == 0, "fs_create");

	/* Open file */
	fd = fs_open("myfile");
	ASSERT(fd >= 0, "fs_open");

    /* Check file size */
	ret = fs_stat(fd);
	ASSERT(ret == 0, "fs_stat 1");

    /* Write some data */
	ret = fs_write(fd, data, 10);
	ASSERT(ret == 10, "fs_write");

    /* Check file size */
	ret = fs_stat(fd);
	ASSERT(ret == strlen(data), "fs_stat 2");

	/* Read some data */
	ret = fs_read(fd, read_buf, 10);
	ASSERT(ret == 10, "fs_read ret");
	ASSERT(!strncmp(read_buf, data, 10), "fs_read data");

    /* Check file size */
	ret = fs_stat(fd);
	ASSERT(ret == 10, "fs_stat 3");

	/* Close file and unmount */
	fs_close(fd);
	fs_umount();

    /* Re Mount disk */
	ret = fs_mount(DISKNAME);
	ASSERT(!ret, "fs_mount (persistant)");

    /* Open file */
	fd = fs_open("myfile");
	ASSERT(fd >= 0, "fs_open (persistant)");

    /* Create file (should fail) */
	ret = fs_create("myfile");
	ASSERT(ret == -1, "fs_create duplicate (persistant)");

    /* Check file size */
	ret = fs_stat(fd);
	ASSERT(ret == 10, "fs_stat (persistant)");

	/* Read some data */
	ret = fs_read(fd, read_buf, 10);
	ASSERT(ret == 10, "fs_read ret (persistant)");
	ASSERT(!strncmp(read_buf, data, 10), "fs_read data (persistant)");

    /* Close file and unmount */
	fs_close(fd);
	fs_umount();

    fprintf(stderr, "%s", green("...PASSED THE WHOLE TEST!\n"));
}

void create_errors_basic()
{
    const char *filename = "myfile";
	int ret;
    fprintf(stderr, "%s", color("\n------TESTING create_errors_basic------\n", 33));

    /* Reset disk file */
    reset_disk(DISKNAME, DATA_BLOCK_COUNT);

    /* Create file before mounting */
	ret = fs_create(filename);
	ASSERT(ret == -1, "fs_create before mount");

	/* Mount disk */
	ret = fs_mount(DISKNAME);
	ASSERT(!ret, "fs_mount");

    /* Create file (should work) */
	ret = fs_create(filename);
	ASSERT(ret == 0, "fs_create initial");

    /* Create with duplicate filename (should error) */
	ret = fs_create(filename);
	ASSERT(ret == -1, "fs_create duplicate");

    /* Create with invalid filename (should error) */
	ret = fs_create("\0myfile");
	ASSERT(ret == -1, "fs_create invald filename 1");
    ret = fs_create("");
	ASSERT(ret == -1, "fs_create invald filename 2");
    ret = fs_create(";SLKDFJ;ASKDJF;SKDJF;AKSJDF;LAKSJDF;KAJS;DFKJAS;DLKFJAKDJFSKDJF;ASKJDF;ADJF;ASKJDF;ALKSJDF;LAKSJDF;LKASJDF");
	ASSERT(ret == -1, "fs_create invald filename 3");

    /* Create files until file limit has been reached */
    // start at 1 because 1 file has already been added
    for (int i = 1; i < FS_FILE_MAX_COUNT; i++) {
        char new_name[FS_FILENAME_LEN];
        sprintf(new_name, "%s (%d)", filename, i);
        ret = fs_create(new_name);
        ASSERT(ret == 0, NULL);                     // only output here on failure
    }
    // should get through whole for loop and last one should be -1
    ret = fs_create("file over limit!");
	ASSERT(ret == -1, "fs_create too many files");

    // delete 1 file
    ret = fs_delete(filename);
	ASSERT(ret == 0, "fs_create delete original file");

    // create file again (should succeed)
    ret = fs_create(filename);
	ASSERT(ret == 0, "fs_create create original file again");

    // finish
	fs_umount();
    fprintf(stderr, "%s", green("...PASSED THE WHOLE TEST!\n"));
}

void delete_errors_basic()
{
    const char *filename = "myfile";
	int ret;
    fprintf(stderr, "%s", color("\n------TESTING delete_errors_basic------\n", 33));

    /* Reset disk file */
    reset_disk(DISKNAME, DATA_BLOCK_COUNT);

    /* Delete before mounting */
	ret = fs_delete(filename);
	ASSERT(ret == -1, "fs_delete before mounting");

	/* Mount disk */
	ret = fs_mount(DISKNAME);
	ASSERT(!ret, "fs_mount");

    /* Delete non existant file */
	ret = fs_delete(filename);
	ASSERT(ret == -1, "fs_delete non existant");

    /* Create file (should work) */
	ret = fs_create(filename);
	ASSERT(ret == 0, "fs_create file 1");

    /* Delete file (should work) */
	ret = fs_delete(filename);
	ASSERT(ret == 0, "fs_delete file 1");

    /* Delete now that there are 0 files left */
	ret = fs_delete(filename);
	ASSERT(ret == -1, "fs_delete non existant part 2");

    // finish
	fs_umount();
    fprintf(stderr, "%s", green("...PASSED THE WHOLE TEST!\n"));
}

void open_close_basic()
{
    const char *filename = "myfile";
	char *buf = "0123456789";
    int fd1, fd2;
	int ret1, ret2;
    fprintf(stderr, "%s", color("\n------TESTING open_close_basic------\n", 33));

    /* Reset disk file */
    reset_disk(DISKNAME, DATA_BLOCK_COUNT);

    /* Open before mounting */
	fd1 = fs_open(filename);
	ASSERT(fd1 == -1, "fs_open before mounting");

    /* Close before mounting */
	ret1 = fs_close(0);
	ASSERT(ret1 == -1, "fs_close before mounting");

	/* Mount disk */
	ret1 = fs_mount(DISKNAME);
	ASSERT(!ret1, "fs_mount");

	/* close error */
	ret1 = fs_close(0);
	ASSERT(ret1 == -1, "fs_close after mounting");

	/* open file that doesnt exist */
	ret1 = fs_open("invalid file");
	ASSERT(ret1 == -1, "fs_open file doesnt exist");

	ret1 = fs_create(filename);
	ASSERT(ret1 == 0, "fs_create create filename");

	/* open fd1 successfully */
	fd1 = fs_open(filename);
	ASSERT(fd1 >= 0, "fs_open fd1");

	/* open fd2 successfully */
	fd2 = fs_open(filename);
	ASSERT(fd2 >= 0, "fs_open success");
	ASSERT(fd1 != fd2, "fd1 != fd2");

	/* check that they point to the same file */
	ret1 = fs_stat(fd1);
	ret2 = fs_stat(fd2);
	ASSERT(ret1 == ret2, "size1 == size2");

	/* write to one file descriptor and check that both have updated sizes */
	ret1 = fs_write(fd1, buf, 10);
	ASSERT(ret1 == 10, "wrote 10 bytes to fd1");

	ret1 = fs_stat(fd1);
	ret2 = fs_stat(fd2);
	ASSERT(ret1 == ret2, "size1 == size2");

    // finish
	fs_umount();
    fprintf(stderr, "%s", green("...PASSED THE WHOLE TEST!\n"));
}

void write_and_read()
{
    const char *filename = "myfile";
	char buf[10] = "0123456789";
	char read_buf[10];
    int fd;
	int ret;
    fprintf(stderr, "%s", color("\n------TESTING write_and_read------\n", 33));

    /* Reset disk file */
    reset_disk(DISKNAME, DATA_BLOCK_COUNT);

    /* Write before mounting */
	ret = fs_write(0, buf, 10);
	ASSERT(ret == -1, "fs_write before mounting");

    /* Read before mounting */
	ret = fs_read(0, read_buf, 10);
	ASSERT(ret == -1, "fs_read before mounting");

	/* Mount disk */
	ret = fs_mount(DISKNAME);
	ASSERT(!ret, "fs_mount");

	/* Write on non existent file */
	ret = fs_write(0, buf, 10);
	ASSERT(ret == -1, "fs_write after mounting");

    /* Read on non existent file */
	ret = fs_read(0, read_buf, 10);
	ASSERT(ret == -1, "fs_read after mounting");\

	/* create and open file */
	ret = fs_create(filename);
	fd = fs_open(filename);
	ASSERT(fd >= 0, "opened file");

	/* read from empty file */
	ret = fs_read(fd, read_buf, 10);
	ASSERT(ret == 0, "read only 0 bytes");
	ASSERT(fs_stat(fd) == 0, "fs_stat is 0 bytes");

    // finish
	fs_umount();
    fprintf(stderr, "%s", green("...PASSED THE WHOLE TEST!\n"));
}

void write_and_read_big_files()
{
    const char *filename = "myfile";
	int data[6000];
	for (int i = 0; i < 1000; i++) data[i] = 0;
	for (int i = 1000; i < 2000; i++) data[i] = 1;
	for (int i = 2000; i < 3000; i++) data[i] = 2;
	for (int i = 3000; i < 4000; i++) data[i] = 3;
	for (int i = 4000; i < 5000; i++) data[i] = 4;
	for (int i = 5000; i < 6000; i++) data[i] = 5;

	int read_buf[6000];
    int fd;
	int ret;
    fprintf(stderr, "%s", color("\n------TESTING write_and_read_big_files------\n", 33));

    /* Reset disk file */
    reset_disk(DISKNAME, DATA_BLOCK_COUNT);

	/* Mount disk */
	ret = fs_mount(DISKNAME);
	ASSERT(!ret, "fs_mount");

	/* create and open file */
	ret = fs_create(filename);
	fd = fs_open(filename);
	ASSERT(fd >= 0, "opened file");

	ret = fs_write(fd, data, sizeof(data));
	printf("ret = %d\n", ret);
	ASSERT(ret == sizeof(data), "write all 6000 integers");
	ASSERT(fs_stat(fd) == 24000, "fs_stat is 24000 bytes");

	/* read from empty file */
	ret = fs_read(fd, read_buf, fs_stat(fd));
	ASSERT(ret == fs_stat(fd), "read all bytes");
	// for (int i = 0; i < 6000; i++) {
	// 	printf("(%d, %d) ", read_buf[i], data[i]);
	// }


	/* compare data to read_buf */
	ASSERT(data[0] == read_buf[0], "data[0] == read_buf[0]");
	ASSERT(data[1000] == read_buf[1000], "data[1000] == read_buf[1000]");
	ASSERT(data[2000] == read_buf[2000], "data[2000] == read_buf[2000]");
	ASSERT(data[3000] == read_buf[3000], "data[3000] == read_buf[3000]");
	ASSERT(data[4000] == read_buf[4000], "data[4000] == read_buf[4000]");
	ASSERT(data[5000] == read_buf[5000], "data[5000] == read_buf[5000]");
	ASSERT(data[5999] == read_buf[5999], "data[5999] == read_buf[5999]");


    // finish
	fs_umount();
    fprintf(stderr, "%s", green("...PASSED THE WHOLE TEST!\n"));
}

void read_write_basic(){
	const char *filename = "myfile";
	int fd;
	int ret;
	int count = 17;

	// create big array
	char buf[6000];
	for (int i = 0; i < 6000; i++) buf[i] = i;
	
	// read buffer
	char read_buf[6000];

    /* Reset disk file */
	reset_disk(DISKNAME, DATA_BLOCK_COUNT);

    /* Mount disk */
	ret = fs_mount(DISKNAME);
	ret = fs_create(filename);
	fd = fs_open(filename);

    /* write to a file descriptor with 6000 bytes (spanning more than one block) */
	ret = fs_write(fd, buf, sizeof(buf));
	ASSERT(ret == 6000, "wrote 6000 bytes");

    /* read file 17 bytes at a time */
	count = 0;
	while(count < 6500){
		ret = fs_read(fd, read_buf + count, 170);
		// ASSERT(ret == 170, "read 170 bytes");
		count += 170;
    }

	for (int i = 0; i < 6000; i++) {
		if (buf[i] != read_buf[i])
			printf("(%d, %d)\n", buf[i], read_buf[i]);
	}
}

int main(int argc, char *argv[]) {
    reset_disk(DISKNAME, DATA_BLOCK_COUNT);

    // LIST OF TESTS
    simple_test_everythig();
    create_errors_basic();
    delete_errors_basic();
    open_close_basic();
	write_and_read();
	write_and_read_big_files();
	read_write_basic();
}
