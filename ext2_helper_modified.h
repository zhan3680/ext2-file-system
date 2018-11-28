#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

/* This is a helper function that helps with assignment 4. */
int search_in_db(char* target_name, int dir_block, char type);
int search_in_inode(char* file_name, int length, struct ext2_inode inode, char type);
int cd(char* path, int i_node_start, int level, char type);
int cd_revised(char* path, char type);
int find_new_block(char* type);
int allocate_inode();
int allocate_dblock();
int compute_level(char *path);
int init_disk(int argc, char** argv);
unsigned short calculate_reclen(struct ext2_dir_entry *entry);


extern unsigned char *disk;
extern unsigned char *block_bit_map;
extern unsigned char *inode_bit_map;
extern struct ext2_inode *inode_table;
extern int table_size;
extern int dblock_size;
extern int table_start;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gd;

/* Given a dir_block, search for a entry having name target_name. */
int search_in_db(char* target_name, int dir_block, char type){
	int read_count = 0;
	/* While not read the whole data block. */
	while(read_count < EXT2_BLOCK_SIZE){
		struct ext2_dir_entry* cur_entry= (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE * dir_block + read_count);
		char cur_name[cur_entry->name_len + 1];
		memset(cur_name, '0', cur_entry->name_len + 1);
		char cur_type;
		strncpy(cur_name, cur_entry->name, cur_entry->name_len);
		cur_name[cur_entry->name_len] = '\0';
		//printf("Current name is %s\n", cur_name);
		if(cur_entry->file_type & EXT2_FT_REG_FILE){
			cur_type = 'f';
		} else if (cur_entry->file_type & EXT2_FT_DIR){
		    cur_type = 'd';
		} else if (cur_entry->file_type & EXT2_FT_SYMLINK){
			cur_type = 'l';
		} else {
			cur_type = '0';
		}
		if(!strcmp(&cur_name[0], target_name) && (type == cur_type)){
			return cur_entry->inode;
		}
		read_count += cur_entry->rec_len;
	}
	/* If not found in current dir block. */
	return -1;
}

/*
 * Given a file name, return the correspond 
 */
int search_in_inode(char* file_name, int length, struct ext2_inode inode, char type){
	int i, j, res;
	char target_name[length + 1]; //modified from length+2 to length+1!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	memset(target_name, '\0', sizeof(target_name));
	strncpy(target_name, file_name, length);
	
	
	/* Search for direct blocks. */
	for(i = 0; i < 12; i++){
	    res = search_in_db(target_name, inode.i_block[i], type);
	    if(res > 0){
	            return res;
	    } else if (inode.i_block[i + 1] == 0){
		    return -1;
	    }
	}
	/* Search for indirect pointers. */
	if(res < 0){
		int *indirect_block = (int *)(disk + (inode.i_block)[12] * EXT2_BLOCK_SIZE);
		for(i = 0; i < 256; i++){
			if(indirect_block[i] == 0){
				return -1;
			}
			res = search_in_db(target_name, indirect_block[i], type);
		
		        if(res > 0){
			        return res;
		        }
		}
	}
	
	/* Search for double indirect pointers. */
        if(res < 0){
		int *double_indirect = (int *)(disk + (inode.i_block)[13] * EXT2_BLOCK_SIZE);
		for(i = 0; i < 256; i++){
			int *indirect_block = (int *)(disk + (double_indirect[i]) * EXT2_BLOCK_SIZE);
			for(j = 0; j < 256; j++){
				if(indirect_block[i] == 0){
				    return -1;
			        }
			        res = search_in_db(target_name, indirect_block[i], type);
		
		                if(res > 0){
			                return res;
		                }
			}
		}
	}
	
	/* Search for triple indirect pointers. */
	if(res < 0){
		int *tri_indirect = (int *)(disk + ((inode.i_block)[14] * EXT2_BLOCK_SIZE));
		for(i = 0; i < 256; i++){
			int *db_indirect = (int *)(disk + (tri_indirect[i]) * EXT2_BLOCK_SIZE);
			    for(j = 0; j < 256; j++){
					int *indirect_block = (int *)(disk + db_indirect[j] * EXT2_BLOCK_SIZE);
					if(indirect_block[i] == 0){
				                return -1;
			                }
			                res = search_in_db(target_name, indirect_block[i], type);
		
		                        if(res > 0){
			                        return res;
		                        }
			    }
		}
	}
	
	/* Go over all but still not find. */
	return -1;
}

/*
 * A helper file for changing directory.
 * If such directory exists, return the inode number of the last directory.
 */
int cd(char* path, int i_node_start, int level, char type){
	/* The root inode of all. */
	int end = 0;
	char* target_dir = path;
	int cur_level = 0;
	int i, byte_index, bit_index, target_inode;
	while( end < strlen(target_dir) - 1 && (path)[end + 1] != '/'){
		end += 1;
	}
	if(level == 1){
		return  search_in_inode(target_dir, end, inode_table[1], type);
	}
	target_inode = search_in_inode(target_dir, end, inode_table[1], 'd');
	target_dir = &target_dir[end + 2];
	if(target_inode > 0){
		cur_level += 1;
		i = target_inode;
	} else {
		i = 11;   //why don't we return -1 here???????????????????????????????????????????????????
	}
	while( end < strlen(target_dir) - 1 && (path)[end + 1] != '/'){
		end += 1;
	}
	while(cur_level < level && i < table_size){
                byte_index = (i - 1)/8;
		bit_index = (i - 1)%8;
		if(inode_bit_map[byte_index] & 1<<bit_index){
		        if(inode_table[i - 1].i_mode & EXT2_S_IFDIR){
				if(cur_level == level -1){
			                target_inode = search_in_inode(target_dir, end, inode_table[i - 1], type);
		                } else {
					/* Still need to go over some internal subdirectories. */
					target_inode = search_in_inode(target_dir, end, inode_table[i - 1], 'd');
			        }
			        /* Case when some directory is missing. */
		                if(target_inode < 0){
				        i += 1;
			        } else {
				        /* Then find the next subdirectory. */
     		                        cur_level += 1;
	        	                target_dir = &target_dir[end + 2];
		                        end = 0;
				        if(cur_level < level){
    	                                         while( end < strlen(target_dir) && (path)[end + 1] != '/'){
		                                          end += 1;
	                                         }
		                        }
		                        i = target_inode;
			       }
			} else {
				i += 1;
			}
	        } else {
		        i += 1;
		}
		
	}
	/* Case does not find all the subdirectories. */
	if(level != cur_level){
		return -1;
	}
	return target_inode;
}

/*
 * A helper file for changing directory: revised version, changed the way it deals with paths;
 * 1. comsequtive '/'; 2. length problem
 * If such directory exists, return the inode number of the last directory.
 */
int cd_revised(char* path, char type){
        if(strlen(path) == 0){
            return -ENOENT;
        }
	/* The root inode of all. */
	int head = 0; //keep track of where current dir starts (used to process subdirs)
        int length = 1; //length of current dir
	int cur_level = 0; //how many levels have we processed?
        int total_level = compute_level(path); 
        char *target_dir = path; 
        int target_inode = EXT2_ROOT_INO;
        while(cur_level < total_level){
	       	while( head < strlen(path) - 1 && (path)[head + 1] != '/'){
			head += 1;
		        length++;
		}
		target_inode = search_in_inode(target_dir, length, inode_table[target_inode-1], 'd');
		if(target_inode > 0){
			cur_level += 1;
		} else {
			return -ENOENT;
		}
		head += 1;
		length = 1;
                while(head < strlen(path) && (path)[head] == '/'){
                    head++;
                }
                target_dir = &(path[head]);
        }
        return target_inode;
}







/* Return the index of a free block. (return 12 if block 13 is free)*/
int find_new_block(char* type){ //modified!!!!!!!!!!!!!!!what if full???????
        int found = 0;
	unsigned char* map;
	int range, i, bit_index, byte_index;
	if(*type == 'i'){
		map = inode_bit_map;
		range = table_size;
		i = 12;
	} else if(*type == 'd'){
		map = block_bit_map;
		range = dblock_size;
		i = 9;
	} else {
		printf("Usage: i/d\n");
		exit(-1);
	}
	while(i < range){
	        byte_index = (i - 1)/8;
		bit_index = (i - 1)%8;
		if(map[byte_index] & 1<<bit_index){
			i += 1;
		} else {
			map[byte_index] |= (1<<bit_index);
                        found = 1;
			break;
		}
	}
	
	/* Return the index of the free block. */
	if(found){
                return i;
        }else{
                return -1;
        }
}

/* Find a useable inode block, and initialize it. 
 * return the number of alloctaed block.
 */
int allocate_inode(){
	char *type = "i";
	/* Note new_block is the block number, not the index. */
	int new_block = find_new_block(type);
	if(new_block == -1){
            return -1;
        }
	/* Set all the values to 0. */
	printf("The index of new block is %d\n", new_block);
	memset(&inode_table[new_block - 1], 0, sizeof(struct ext2_inode));
	gd->bg_free_inodes_count -= 1;  //modified!!!!!!!!!!!!!!!!!
	return new_block;
}

/* Find a useable data block and initialize it.
 * return the number of allocated block(not the index.)
 */
int allocate_dblock(){
	char *type = "d";
	int new_block = find_new_block(type);
	if(new_block == -1){
            return -1;
        }
	/* Set all the values to 0. */
	printf("The index of new data block is %d\n", new_block);
	memset(disk + EXT2_BLOCK_SIZE * new_block, 0, EXT2_BLOCK_SIZE);
	gd->bg_free_blocks_count -= 1;  //modified!!!!!!!!!!!!!!!!!!!!!!!
	return new_block;
}


/* Given a path, compute the number of levels of the given path.
 * note that path should be in this form a/b (retrive the pre , back trailing '/'s).
 */
int compute_level(char *path){     //algorithm modified!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	int head = 0;
	int res = 0;
        int new_level_flag = 1;
        int stop = 0;
	while(head < strlen(path)){		
		while(path[head] == '/'){
                        new_level_flag = 1;
                        head += 1; 
                        if(head >= strlen(path)){
                                stop = 1;
                        }
                }
                if(stop){
                        break;
                }
	        if(new_level_flag){
                        res += 1;
                        new_level_flag = 0;
                }
                head ++;
        }
	return res;
}

/* Initialize global variables. */
int init_disk(int argc, char** argv){
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
	
    gd = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
    sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    inode_bit_map = (unsigned char *)(disk + EXT2_BLOCK_SIZE*gd->bg_inode_bitmap);
    block_bit_map = (unsigned char *)(disk + EXT2_BLOCK_SIZE*gd->bg_block_bitmap);
    inode_table = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE*gd->bg_inode_table);
    table_start = gd->bg_inode_table;
    table_size = sb->s_inodes_count;
    dblock_size = sb->s_blocks_count;
    return 0;
	
}

/*Calculate the rec_len of a struct dir_entry (rec_len is always a multiple of 4)*/
unsigned short calculate_reclen(struct ext2_dir_entry *entry){
    unsigned short res = 0;
    res += sizeof(unsigned int);
    res += sizeof(unsigned short);
    res += sizeof(unsigned char);
    res += sizeof(unsigned char);
    res += entry->name_len;
    if(res%4 == 0){
        return res;
    }else{
        return 4*(res/4 + 1);
    }
}





