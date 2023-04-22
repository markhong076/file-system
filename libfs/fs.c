#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

// superblock macros
#define SIGNATURE_LENGTH 8

// FAT macros
#define FAT_START_IDX 1
#define FAT_EOC 0xFFFF

// Root Directory macros
#define ROOT_ENTRY_SIZE 32


int ceil_but_better(double input) {
	int rounded_down = (int) input;

	if (input == (double) rounded_down) 
		return rounded_down;
	
	return rounded_down + 1;
}

/** Takes the minimum of 2 integers
 * 
*/
int min(int num1, int num2) {
	return num1 < num2 ? num1 : num2;
}

/** Takes the maximum of 2 integers
 * 
*/
int max(int num1, int num2) {
	return num1 > num2 ? num1 : num2;
}

typedef struct superblock {
	char signature[SIGNATURE_LENGTH];
	uint16_t block_count;
	uint16_t root_block_idx;
	uint16_t data_block_start_idx;
	uint16_t amt_data_blocks;
	uint8_t num_blocks_for_FAT;
} * superblock_t;

typedef struct FAT {
	int curr_pos;
	size_t num_blocks_taken;
	uint16_t *blocks;
} * FAT_t;

typedef struct file {
	char filename[FS_FILENAME_LEN];
	uint32_t file_size;
	uint16_t first_block_idx;
	char padding[10];
} file;

typedef struct rootDir {
	size_t num_files;
	file files[FS_FILE_MAX_COUNT];
} * rootDir_t;

typedef struct openFile {
	int fd;
	int file_num;
	size_t file_offset;
} openFile;

typedef struct FS {
	superblock_t superblock;
	FAT_t FAT;
	rootDir_t rootDir;
	openFile open_files[FS_OPEN_MAX_COUNT];
	size_t num_open_files;
	bool is_mounted;
} FS;


/** Creates and allocates memory for a FS instance
 * 
 * returns: pointer of FS struct
*/
FS * fs_init(void) {
	FS *fs = malloc(sizeof(*fs));
	fs->is_mounted = false;
	fs->num_open_files = 0;

	// malloc error handling
	if (!fs)
		return NULL;

	// init sub structs for filesystem var
	fs->superblock = malloc(BLOCK_SIZE);
	fs->FAT = malloc(sizeof(*fs->FAT));
	fs->rootDir = malloc(sizeof(*fs->rootDir));

	// malloc error handling
	if (!fs->superblock || !fs->FAT || !fs->rootDir)
		return NULL;

	// fill openFiles array
	fs->num_open_files = 0;
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
		fs->open_files[i] = (openFile){.file_num = -1};

	return fs;
}

/* HELPER FUNCTIONS FOR FILESYSTEM */

/** Validate a filename 
 * @filename: filename to validate
 * 
 * returns: true if the filename is valid, 
 * 			false otherwise
*/
bool fs_validate_filename(const char *filename) {
	if (filename[0] == '\0' || filename == NULL || strlen(filename) > FS_FILENAME_LEN || strlen(filename) == 0)
		return false;

	return true;
}

/** Validate a file descriptor for a given filesystem 
 * @fs: FS* - filesystem pointer
 * @fd: int - file descriptor of target open file
 * 
 * returns: 1 if the fd is valid, 
 * 			0 otherwise
*/
bool fs_validate_fd(FS *fs, int fd) {
	// invalid file descriptor
	if (fd < 0 || fd > FS_OPEN_MAX_COUNT)
		return false;

	// file descriptor is not currently open
	if (fs->open_files[fd].file_num == -1) 
		return false;

	return true;
}

/** Validate a file number for a given filesystem 
 * @fs: FS* - filesystem pointer
 * @file_num: int - file number of target file
 * 
 * returns: 1 if the file is valid, 
 * 			0 otherwise
*/
bool fs_validate_file_num(FS *fs, int file_num) {
	// invalid file descriptor
	if (file_num < 0 || file_num > FS_FILE_MAX_COUNT)
		return false;

	// get file
	file target_file = fs->rootDir->files[file_num];

	// file descriptor is not currently open
	if (target_file.filename[0] == '\0')
		return false;

	return true;
}

/** Find a file number in the root directory 
 * @filename: string containing filename to search for
 * 
 * returns: index of the file if it exists, 
 * 			-1 if the filename is invalid
 * 			-2 if the filename is not in the directory
*/
int fs_file_num_from_filename(FS *fs, const char *filename) {
	// validate filename
	if (!fs_validate_filename(filename))
		return -1;

	// loop through files
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		// if filename matches return index
		if (strcmp(fs->rootDir->files[i].filename, filename) == 0)
			return i;
	}

	// if filename not found
	return -2;
}

/** Find a file number in the root directory 
 * @fs: FS* - filesystem pointer
 * @fd: int - file descriptor of target open file
 * 
 * returns: index of the file if it exists, 
 * 			-1 if the file does not exist or if the fd is invalid
*/
int fs_file_num_from_fd(FS *fs, int fd) {
	if (!fs_validate_fd(fs, fd))
		return -1;

	int file_num = fs->open_files[fd].file_num;

	if (!fs_validate_file_num(fs, file_num))
		return -1;

	return file_num;
}

// Save superblock to disk
int fs_save_superblock(FS *fs) {
	// write superblock values
	if (block_write(0, fs->superblock) == -1)
		return -1;

	return 0;
}

// Save rootDir to disk
int fs_save_rootDir(FS *fs) {
	if (block_write(fs->superblock->root_block_idx, fs->rootDir->files) == -1)
		return -1;

	return 0;
}

// Save FAT to disk
int fs_save_FAT(FS *fs) {
	// write array to disk
	for (int i = 0; i < fs->superblock->num_blocks_for_FAT; i++) {
		size_t FAT_ptr_offset = BLOCK_SIZE * i / sizeof(uint16_t);
		block_write(FAT_START_IDX + i, fs->FAT->blocks + FAT_ptr_offset);
	}

	return 0;
}

// returns true if fs is open
// returns false if fs has not been opened
bool is_mounted(FS *fs) {
	return block_disk_count() != -1 && fs->is_mounted;
}


// global filesystem var
FS *fs;

int fs_mount(const char *diskname)
{
	// init global filesystem var
	fs = fs_init();

	// open virtual disk
	if (block_disk_open(diskname) == -1)
		return -1;

	// assign superblock values
	block_read(0, fs->superblock);

	// init FAT array
	fs->FAT->curr_pos = 0;
	fs->FAT->blocks = malloc(fs->superblock->num_blocks_for_FAT * BLOCK_SIZE);

	// malloc error handling
	if (!fs->FAT->blocks)
		return -1;

	// read and assign values to array
	for (int i = 0; i < fs->superblock->num_blocks_for_FAT; i++) {
		size_t FAT_ptr_offset = BLOCK_SIZE * i / sizeof(uint16_t);
		block_read(FAT_START_IDX + i, fs->FAT->blocks + FAT_ptr_offset);
	}

	// read into block buffer
	block_read(fs->superblock->root_block_idx, fs->rootDir->files);

	// calculate number of blocks taken
	fs->rootDir->num_files = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++){
		// calculate blocks taken and increment counter
		int file_num_blocks = ceil_but_better(fs->rootDir->files[i].file_size / (double) BLOCK_SIZE);
		fs->FAT->num_blocks_taken += file_num_blocks;

		// increment number of files counter if filename is not null
		if (fs->rootDir->files[i].filename[0] != '\0')
			fs->rootDir->num_files++;
	}

	fs->is_mounted = true;
	return 0;
}

int fs_umount(void)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// save superblock
	if (!fs_save_superblock(fs) == -1)
		return -1;

	// save rootDir
	if (!fs_save_rootDir(fs) == -1)
		return -1;

	// save FAT
	if (!fs_save_FAT(fs) == -1)
		return -1;

	// free pointer memory
	free(fs);

	// close block disk
	if (block_disk_close() == -1)
		return -1;

	return 0;
}

int fs_info(void)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", fs->superblock->block_count);
	printf("fat_blk_count=%d\n", fs->superblock->num_blocks_for_FAT);
	printf("rdir_blk=%d\n", fs->superblock->root_block_idx);
	printf("data_blk=%d\n", fs->superblock->data_block_start_idx);
	printf("data_blk_count=%d\n", fs->superblock->amt_data_blocks);
	printf("fat_free_ratio=%ld/%d\n", fs->superblock->amt_data_blocks - fs->FAT->num_blocks_taken - 1, fs->superblock->amt_data_blocks);
	printf("rdir_free_ratio=%ld/%d\n", FS_FILE_MAX_COUNT - fs->rootDir->num_files, FS_FILE_MAX_COUNT);
}

int fs_create(const char *filename)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// validate filename
	if (!fs_validate_filename(filename))
		return -1;

	// makesure filename does not exist already
	if (fs_file_num_from_filename(fs, filename) != -2)
		return -1;
	
	// make sure there is space for another file
	if (fs->rootDir->num_files >= FS_FILE_MAX_COUNT)
		return -1;

	// get file list pointer
	file *files_list = fs->rootDir->files;

	// initalize new file instance
	file new_file = {.file_size = 0, .first_block_idx = FAT_EOC};
	strcpy(new_file.filename, filename);

	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		// if cell is empty in directory...
		if (files_list[i].filename[0] == '\0') {
			// fill cell with new file and return successful
			files_list[i] = new_file;
			fs->rootDir->num_files++;
			
			// save updated rootDir
			if (!fs_save_rootDir(fs) == -1)
				return -1;

			return 0;
		}
	}

	return -1;
}

int fs_delete(const char *filename)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	file *files_list = fs->rootDir->files;
	file EMPTY_FILE_CELL = {.filename = '\0', .file_size = 0, .first_block_idx = FAT_EOC};

	// get file_num
	int file_num = fs_file_num_from_filename(fs, filename);

	// invalid filename or couldnt find
	if (file_num < 0) 
		return -1;

	// TODO: first, free blocks in FAT associated with file
	uint16_t block_idx = files_list[file_num].first_block_idx;
	while (block_idx != FAT_EOC) {
		// save next block idx and set value to FAT_EOC
		uint16_t next_block_idx = fs->FAT->blocks[block_idx];
		fs->FAT->blocks[block_idx] = FAT_EOC;

		// update current block idx and continue
		block_idx = next_block_idx;
	}

	// delete entry in 
	files_list[file_num] = EMPTY_FILE_CELL;
	fs->rootDir->num_files--;

	// save updated rootDir
	if (!fs_save_rootDir(fs) == -1)
		return -1;
	
	// save updated FAT
	if (!fs_save_FAT(fs) == -1)
		return -1;

	return 0;
}

int fs_ls(void)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	file *files_list = fs->rootDir->files;

	// print header
	printf("FS Ls:\n");

	// print each file information
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		// file is not empty...
		if (files_list[i].filename[0] != '\0')
			printf("file: %s, size: %d, data_blk: %d\n", files_list[i].filename, files_list[i].file_size, files_list[i].first_block_idx);
	}
}

int fs_open(const char *filename)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// check if max amount of open files has been reached
	if (fs->num_open_files >= FS_OPEN_MAX_COUNT)
		return -1;
	
	// find open file descriptor
	int fd = -1;
	for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
		// file_num of -1 means fd is available to use
		if (fs->open_files[i].file_num == -1) {
			fd = i;
			break;
		}
	}

	// ERROR: no file descriptor available
	if (fd == -1)
		return -1;

	// get file number
	int file_num = fs_file_num_from_filename(fs, filename);
	if (file_num < 0)
		return -1;

	// set open file
	fs->open_files[fd] = (openFile){.fd = fd, .file_num = file_num, .file_offset = 0};
	fs->num_open_files++;

	return fd;
}

int fs_close(int fd)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// make sure file descriptor is valid
	if (!fs_validate_fd(fs, fd))
		return -1;

	// close file descriptor
	fs->open_files[fd] = (openFile){.file_num = -1};
	fs->num_open_files--;

	return 0;
}

int fs_stat(int fd)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// get file number from fd
	int file_num = fs_file_num_from_fd(fs, fd);
	if (file_num == -1)
		return -1;

	// return file size
	return fs->rootDir->files[file_num].file_size;
}

int fs_lseek(int fd, size_t offset)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// offset is too large
	if (offset > fs_stat(fd))
		return -1;

	// invalid fd
	if (!fs_validate_fd(fs, fd))
		return -1;

	// set offset
	fs->open_files[fd].file_offset = offset;

	return 0;
}

/** Find the block and block offset of the file in question 
 * @fs: pointer to filesystem
 * @fd: file descriptor of open file in question
 * @block_idx: block index where the overall offset lays
 * @block_offset: offset of the block where the overall offset lays
 * 
 * returns: 0 on successful, -1 otherwise
*/
int fs_get_block_offset(FS *fs, int fd, size_t *block_idx, size_t *block_offset) {
	// get file number
	int file_num = fs_file_num_from_fd(fs, fd);
	if (file_num == -1) 
		return -1;

	// get open file descriptor
	openFile open_file = fs->open_files[fd];
	// get target file
	file target_file = fs->rootDir->files[file_num];

	// get start values before looping to update them
	*block_idx = target_file.first_block_idx;
	*block_offset = open_file.file_offset;

	// loop until block_offset is inside a the current block
	// or until we hit FAT_EOC
	while (*block_offset >= BLOCK_SIZE && fs->FAT->blocks[*block_idx] != FAT_EOC) {
		// decrement block_offset by entire block size
		*block_offset -= BLOCK_SIZE;

		// go to next block in file
		*block_idx = fs->FAT->blocks[*block_idx];
	}

	return 0;
}

/** Find the next open data block index in the FAT 
 * @fs: pointer to filesystem
 * 
 * returns: index of the next open block if it exists, 
 * 			-1 if there is no open block available
*/
int fs_find_open_data_block(FS *fs) {
	// errors
	if (fs->FAT->num_blocks_taken >= fs->superblock->amt_data_blocks)
		return -1;

	for (int i = 0; i < fs->superblock->amt_data_blocks; i++) {
		if (fs->FAT->blocks[i] == 0)
			return i;
	}

	// if no block is available
	return -1;
}

int fs_write(int fd, void *buf, size_t count)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// number of bytes already written from @buf
	size_t bytes_written = 0;

	// pointer to block sized buffer
	void *block_p = malloc(BLOCK_SIZE);

	// get target file
	int file_num = fs_file_num_from_fd(fs, fd);
	if (file_num == -1)
		return -1;
	
	// get open file descriptor
	openFile *open_file = &fs->open_files[fd];
	// get target file
	file *target_file = &fs->rootDir->files[file_num];

	// Pointer to block index we are currently on
	// This is a pointer so that we can change the value if we need
	// to change it to connect it to the end of a chain in the FAT
	uint16_t *block_idx_p = &target_file->first_block_idx;

	// loop until bytes are written
	while (bytes_written < count) {

		// allocate another block if necessary
		if (*block_idx_p == FAT_EOC) {

			// find open block
			size_t open_block = fs_find_open_data_block(fs);
			if (open_block == -1)
				return bytes_written;

			// connect new open block to chain
			*block_idx_p = open_block;
			fs->FAT->blocks[open_block] = FAT_EOC;
			fs->FAT->num_blocks_taken++;

			// save updated rootDir
			if (!fs_save_FAT(fs) == -1)
				return -1;
		}

		// calculate number of bytes to be written in this block
		size_t num_bytes_to_write = min(count - bytes_written, BLOCK_SIZE);

		// first read block we are about to overwrite
		if (num_bytes_to_write < BLOCK_SIZE)
			block_read(fs->superblock->data_block_start_idx + *block_idx_p, block_p);

		// bytes to block sized pointer
		memcpy(block_p, buf + bytes_written, num_bytes_to_write);

		// write to block
		block_write(fs->superblock->data_block_start_idx + *block_idx_p, block_p);

		// update bytes written and file size if necessary
		bytes_written += num_bytes_to_write;

		// update file size if necessary
		if (bytes_written > target_file->file_size) {
			target_file->file_size = bytes_written;

			// save updated rootDir to disk
			if (!fs_save_rootDir(fs) == -1)
				return -1;
		}

		// get next block
		block_idx_p = &fs->FAT->blocks[*block_idx_p];
	}

	return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	// make sure fs is properly mounted
	if (!is_mounted(fs))
		return -1;

	// number of bytes already read into @buf
	size_t bytes_read = 0;

	// pointer to block sized buffer
	void *block_p = malloc(BLOCK_SIZE);

	// get target file
	int file_num = fs_file_num_from_fd(fs, fd);
	if (file_num == -1)
		return -1;

	// get open file descriptor
	openFile *open_file = &fs->open_files[fd];

	// get target file
	file *target_file = &fs->rootDir->files[file_num];

	// vars to keep track of current block index and block offset
	size_t block_idx;
	size_t block_offset;

	int c = 0;
	while (bytes_read < count && open_file->file_offset < target_file->file_size) {

		// get block specific offset
		if (fs_get_block_offset(fs, fd, &block_idx, &block_offset) == -1)
			return -1;

		// failsafe
		if (block_idx == FAT_EOC)
			break;

		// read block
		block_read(fs->superblock->data_block_start_idx + block_idx, block_p);

		// find number of bytes after offset and before either EOF or end of block
		size_t valid_bytes_in_block = min(BLOCK_SIZE - block_offset, target_file->file_size - open_file->file_offset);

		// copy the smaller of the 2:
		// 1) number of valid bytes left in block
		// 2) count - amount of bytes already read
		size_t num_bytes_to_copy = min(count - bytes_read, valid_bytes_in_block);

		// fill string buffer
		memcpy(buf + bytes_read, block_p + block_offset, num_bytes_to_copy);

		// update count to contain how many bytes are left to be read
		bytes_read += num_bytes_to_copy;
		open_file->file_offset += num_bytes_to_copy;
	}

	// end reading
	return bytes_read;
}


