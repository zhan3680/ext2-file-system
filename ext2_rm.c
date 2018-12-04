#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include "ext2.h"
#include "ext2_helper.c"

unsigned char *disk;
unsigned char *block_bit_map;
unsigned char *inode_bit_map;
struct ext2_inode *inode_table;
int table_size;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;
int table_start;
int dblock_size;


/*This program takes two command line arguments. 
 The first is the name of an ext2 formatted virtual disk, 
 and the second is an absolute path to a file or link 
 (not a directory) on that disk. The program should work like rm,
 removing the specified file from the disk. If the file does not 
 exist or if it is a directory, then your program should return
 the appropriate error. Once again, please read the specifications 
 of ext2 carefully, to figure out what needs to actually happen 
 when a file or link is removed (e.g., no need to zero out data
 blocks, must set i_dtime in the inode, removing a directory entry 
 need not shift the directory entries after the one being deleted,
 etc.).

 BONUS: Implement an additional "-r" flag (after the disk 
 image argument), which allows removing directories as well. 
 In this case, you will have to recursively remove all the
 contents of the directory specified in the last argument.
 If "-r" is used with a regular file or link, then it should
 be ignored (the ext2_rm operation should be carried out as 
 if the flag had not been entered). If you decide to do the 
 bonus, make sure first that your ext2_rm works, then create
 a new copy of it and rename it to ext2_rm_bonus.c, and implement
 the additional functionality in this separate source file.
 */
 
/* Need to check if all dirs on path exists. */
int check_path(char* path){
	/* Modify the origin input. */
	if(path[0] == '.' && path[1] == '/'){
		path = &path[2];
	} else if(path[0] == '/'){
		path = &path[1];
	}
    int length = strlen(path);
	char path_to_check[length + 1];
	memset(path_to_check, '\0', sizeof(path_to_check));
	int level, res;
	strncpy(path_to_check, path, length);
	
	/* ./A/B/C/// */
	/* Remove the trailing back slashes*/
	int finished = 0;
	length = length - 1;
	while(! finished){
		if(path_to_check[length] == '/'){
			path_to_check[length] = '\0';
			length -= 1;
		} else {
			finished = 1;
		}
	}
	/* Handle the special case. */
	if(path_to_check == "lost+found"){
	    return -1;
	}
	length = strlen(path_to_check) - 1;
	/* Retrive the last directory name to be created. */
	/* Special case when want to create a dir at the root.*/
	finished = 0;
	while( !finished && length >= 1){
		if(path_to_check[length] == '/'){
			length -= 1;
			finished = 1;
		} else {
			length -= 1;
		}
	}
	/* If length is 0, want to return the inode number of the root.*/
	if(length == 0){
	    return 2;
	}
	level = compute_level(path_to_check) - 1;
	printf("Table %d\n", gd->bg_inode_table);
	res = cd(&(path_to_check[0]), gd->bg_inode_table, level, 'd');
	return res;
}

 
 /* A helper function that checks whether the input is valid. */
int check_syntax(char* path){
    if(path[0] == '.' && path[1] == '/'){
	    path = &path[2];
	} else if(path[0] == '/'){
	    path = &path[1];
	}
	int length = strlen(path);
	char path_to_check[length + 1];
	memset(path_to_check, '\0', sizeof(path_to_check));
	int level, res;
	strncpy(path_to_check, path, length);
	if(strlen(path_to_check) == 0){
	    printf("Invalid input, empty file name.\n");
		return -1;
	}
    length = strlen(path_to_check);
	/* Then check if the name is too long. */
	int finished = 0;
	length = length - 1;
	while(! finished){
		if(path_to_check[length] == '/'){
			path_to_check[length] = '\0';
			length -= 1;
		} else {
			finished = 1;
		}
	}

	length = strlen(path_to_check) - 1;
	finished = 0;
	while( !finished && length >= 1){
		if(path_to_check[length] == '/'){
			path_to_check[length] = '\0';
			length -= 1;
			finished = 1;
		} else {
			length -= 1;
		}
	}
	if(strlen(path_to_check) - length + 1 > EXT2_NAME_LEN){
		printf("Name of new directory too long.\n");
		return -1;
	}
	return 0;
}
 
/* A function checks if there already exists a file in here. */
int check_duplicate(char* path, char type){
	if(path[0] == '.' && path[1] == '/'){
		path = &path[2];
	} else if (path[0] == '/'){
		path = &path[1];
	}
	int length = strlen(path);
	int level, res;
	char path_to_check[length];
	memset(path_to_check, '\0', sizeof(path_to_check));
    strncpy(path_to_check, path, length);

	/* Remove the trailing back slashes*/
	int finished = 0;
	length = length - 1;
	while(! finished && length >= 1){
		if(path_to_check[length] == '/'){
			path_to_check[length] = '\0';
			length -= 1;
		} else {
			finished = 1;
		}
	}
    
	level = compute_level(path_to_check);
	res = cd(&(path_to_check[0]), gd->bg_inode_table, level, type);
	if(res < 0 && type != 'd'){
		res = cd(&(path_to_check[0]), gd->bg_inode_table, level, 'l');
	}
	return res;
}

/* A helper function, given a path, return the name of the last not null string.*/
void get_name(char** path){
	char* target = *path;
	int finished = 0;
	int length = strlen(*path) - 1;
	if((*path)[0] == '.' && (*path)[1] == '/'){
	    *path = &(*path)[2];
	} else if ((*path)[0] == '/'){
	    *path = &(*path)[1];
	}
	
	if(strlen(*path) <= 2){
	    if(strcmp(*path, ".") == 0 || strcmp(*path, "..") == 0){
	        return;
	    }
	}
	/* Retrive the following '/' */
	while(! finished){
		if((*path)[length] == '/'){
			(*path)[length] = '\0';
			length -= 1;
		} else {
			finished = 1;
		}
	}
	
	/* Retrive the last directory name to be created. */
	finished = 0;
	while( !finished && length > 0){
		if((*path)[length] == '/'){
            (*path) = &(*path)[length + 1];
    	    finished = 1;
		} else {
			length -= 1;
		}
	}
}

/* Helper function to switch the dir_entry contents. */
void switch_contents(struct ext2_dir_entry* e1, struct ext2_dir_entry* e2, int start, int del){
    int length, temp_inode;
	char temp_name_len, temp_file_type;
	short temp_rec;
	int e1_len, e2_len;
	e1_len = 8 + e1->name_len + 4 - e1->name_len%4;
	e2_len = 8 + e2->name_len + 4 - e2->name_len%4;
	if((e1->name_len) > (e2->name_len)){
	    length = e1->name_len + 4 - e1->name_len%4;
	} else {
		length = e2->name_len + 4 - e2->name_len%4;
	}
	// Copy to temp
	char temp_name[length];
	temp_inode = e1->inode;
	temp_rec = e1->rec_len;
	temp_name_len = e1->name_len;
	temp_file_type = e1->file_type;
	strncpy(temp_name, e1->name, length);
	
	// Copy e2 to e1.
	e1->inode = e2->inode;
	e1->rec_len = e2->rec_len;
	e1->file_type = e2->file_type;
	e1->name_len = e2->name_len;
	strncpy(e1->name, e2->name, e2->name_len + 4 -  e2->name_len%4);
	e2 = (struct ext2_dir_entry*)(disk + del * EXT2_BLOCK_SIZE + start - e1_len + e2_len);
	// Copy temp to e2.
	e2->inode = temp_inode;
	e2->rec_len = temp_rec;
	e2->file_type = temp_file_type;
	e2->name_len = temp_name_len;
	strncpy(e2->name, temp_name, length);
}

/* A helper function to free the entry block.
 *
 */
int free_dir_entry(int del, char* name, char d_type){
	struct ext2_dir_entry* del_dblock = (struct ext2_dir_entry*)(disk + 
	EXT2_BLOCK_SIZE * (del));
	struct ext2_dir_entry* next;
 	struct ext2_dir_entry* cur;
	struct ext2_dir_entry* previous;
	int read_count = 0;
	int left, cur_len, next_len;
	/* First search for the entry block, will skip the first two reserved
     * ones while searching. 
	 */
    
	next = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * (del));
	/* Continue search while does not find */
	int check, check2;
	if(d_type == 'f' || d_type == 'l'){
	    check = EXT2_FT_DIR;
        check2 = EXT2_FT_DIR;
	} else if (d_type == 'd'){
	    check = EXT2_FT_REG_FILE;
	    check2 = EXT2_FT_SYMLINK;
	} 
	if(strcmp(next->name, name) == 0 && next->rec_len > EXT2_NAME_LEN + 8){
	    //memset(next, '0', EXT2_BLOCK_SIZE);
		// Note that do not need to zero out the data block.
		return 0;
	}
	//BREAK NAME NOT EQUALS PROBLEM
	while(strcmp(next->name, name) != 0 || (next->file_type == check || next->file_type == check2)){
	    read_count += next->rec_len;
		previous = next;
    	next = (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE * (del) + read_count);
		next->name[next->name_len] = '\0';
	}
	previous->rec_len += next->rec_len;
	/* Update next's rec_len is it is not the last entry. */
    if(read_count + next->rec_len != EXT2_BLOCK_SIZE){
	    next->rec_len = calculate_reclen(next);
	}
	
	return 0;
	//read_count += next->rec_len;
	//cur = next;
	//next = (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE * (del) + read_count);
	/* Now next points to the dir that has name wanted. */
	//if(cur->rec_len > EXT2_NAME_LEN + 8){
	//   previous->rec_len += cur->rec_len;
	//	left = cur->rec_len;
	//    memset(cur, '0', left);
	//	return 0;
	//}
	/* There are still some other blocks in current map. Shift them back.*/
	//while(read_count < EXT2_BLOCK_SIZE){
	//   cur_len = cur->rec_len;
	//   next_len = next->rec_len;
	//    switch_contents(cur, next, read_count, del);
	//    previous = cur;
	//    cur = (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE * (del) + read_count - cur_len + next_len);
	//    read_count += next_len;
	//    next = (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE * (del) + read_count);
	//}
	/* Then update the rec_len of previous file. */
	//previous->rec_len += cur_len;	
	//return 0;
}

int free_indirect_dblock(int level, int block_num, char type){
	int i;
    if(level == 2){
	// Indirect block.
	    unsigned int* blocks = (int*)(disk + EXT2_BLOCK_SIZE * (block_num));
		for(i = 0; i < 256; i++){
			if(blocks[i] == 0){
			    return 0;
			}
		    free_map(type, blocks[i]);
		}
	} else if(level == 3){
	// Double indirect block.
	    unsigned int* indir_blocks = (int*)(disk + EXT2_BLOCK_SIZE * (block_num));
		for(i = 0; i < 256; i++){
			if(indir_blocks[i] == 0){
			    return 0;
			}
			unsigned int* blocks = (int*)(disk + EXT2_BLOCK_SIZE * (indir_blocks[i]));
			for(int j = 0; j < 256; j++){
			   if(blocks[j] == 0){
			        return 0;
			    } 
				free_map(type, blocks[j]);
			}
		    
		}
	} else if (level == 4){
	    int* b1 =  (int*)(disk + EXT2_BLOCK_SIZE * (block_num));
		for(i = 0; i < 256; i++){
			if(b1[i] == 0){
			    return 0;
			}
			unsigned int* b2 = (int*)(disk + EXT2_BLOCK_SIZE * (b1[i]));
			for(int j = 0; j < 256; j++){
				if(b2[j] == 0){
			        return 0;
			    }
				unsigned int* b3 = (int*)(disk + EXT2_BLOCK_SIZE * (b2[j]));
				for(int k = 0; k < 256; k++){
			        if(b3[k] == 0){
			            return 0;
			        } 
					free_map(type, b3[k]);
				}
			}
		    
		}
	}
}


int delete(int parent_inode_num, int del_file_inum, char* path, char d_type){
	struct ext2_inode* parent_inode= &inode_table[parent_inode_num - 1];
	struct ext2_inode* del_inode = &inode_table[del_file_inum - 1];
	int del_dblock;
	int db_index, i, num_blocks;
	/* Get the name of given path. */
	get_name(&path);
	num_blocks = del_inode->i_blocks/2;
	/* Go to the parent's directory block to delete the path. */
	/* del_dblock is the data block number that contains del file. */
	del_dblock = sen_in_inode(path, strlen(path) - 1, *parent_inode, d_type);
	if(del_dblock < 0 && d_type == 'f'){
		del_dblock = sen_in_inode(path, strlen(path) - 1, *parent_inode, 'l');
	}
	free_dir_entry(del_dblock, path, d_type);
	/* Decrease the count of the link of del_inode by 1. */
	del_inode -> i_links_count -= 1;
	if(del_inode -> i_links_count > 0){
		return 0;
	}
	char type;
	/* Want to increase the number of free blocks by 1. */
	/* Free bitmap if neceessary. */
    for(i = 0; i < num_blocks; i++){
	    type = 'd';
        free_map(type, (del_inode->i_block)[i]);
		if(i >= 12){
			/* Free data blocks. */
		    free_indirect_dblock(i - 10, del_inode->i_block[i], type);
			/* Free the indirect blocks. */
			free_map(type, (del_inode->i_block)[i]);
		} 
		increase_free_blocks();
	}
	if(del_inode->i_links_count == 0){
	    type = 'i';
	    free_map(type, del_file_inum);
		increase_free_inodes();
	}
	 
	return 0;
}

int main(int argc, char **argv) {
	int new_inode, parent_inode, del_file_inode;
	if(argc != 3){
		printf("Usage:<disk> <absolute path to the deleted file/link>\n");
		exit(1);
	}
	char* path = argv[2];
	init_disk(argc, argv);
	
	/* Check if input is valid. */
	if(check_syntax(path) < 0){
		return -EINVAL;
	}
	
	/* Call cd to check if path is valid. */
	parent_inode = check_path(path);
	if(parent_inode == -1){
		printf("Some parent inodes does not exists.\n");
		return -ENOENT;
	}
	printf("Finish path check. \n");
	
	/* Get the inode of required file/link. */
	del_file_inode = check_duplicate(path, 'f');
	if(del_file_inode < 0){
		printf("The delted_file does not exists.\n");
		exit(1);
	}
	printf("Now ready to delte the file required.\n");
	/* Delete the dir_entry. */
	delete(parent_inode, del_file_inode, path , 'f');
	
	return 0;
}
 
 