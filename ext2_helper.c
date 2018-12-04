#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>

/* This is a helper function that helps with assignment 4. */
/* The disk opend. */
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
void increase_free_inodes();
void increase_free_blocks();
int sen_in_inode(char* file_name, int length, struct ext2_inode inode, char type);
struct ext2_dir_entry* sen_in_db(char* target_name, int dir_block, char type);
int free_map(char type, int block_num);
int num_free_inodes();
int num_free_dblocks();
int add_entry(int dir_inode_num, struct ext2_dir_entry *entry_to_add);

extern unsigned char *disk;
extern unsigned char *block_bit_map;
extern unsigned char *inode_bit_map;
extern struct ext2_inode *inode_table;
extern int table_size;
extern int dblock_size;
extern int table_start;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gd;

/* Count number of free data blocks from bitmap. */
int num_free_dblocks(){
    int res = 0;
	int byte_index, bit_index;
	for(int i = 0; i < dblock_size; i++){
	    byte_index = (i)/8;
		bit_index = (i)%8;
		if((block_bit_map[byte_index] & 1<<bit_index) == 0){
			res += 1;
		}
	}
	return res;
}

/* Count number of free inode blocks from bitmap.*/
int num_free_inodes(){
    int res = 0;
	int byte_index, bit_index;
	for(int i = 0; i < table_size; i++){
	    byte_index = (i)/8;
		bit_index = (i)%8;
		if((inode_bit_map[byte_index] & 1<<bit_index) == 0){
			res += 1;
		}
	}
	return res;
}


/* Given a dir_block, search for a entry having name target_name. */
int search_in_db(char* target_name, int dir_block, char type){
	int read_count = 0;
	/* While not read the whole data block. */
	while(read_count < EXT2_BLOCK_SIZE){
		struct ext2_dir_entry* cur_entry= (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE * dir_block + read_count);
		char cur_name[cur_entry->name_len + 1];
		memset(cur_name, '\0', cur_entry->name_len + 1);
		char cur_type;
		strncpy(cur_name, cur_entry->name, cur_entry->name_len);
		cur_name[cur_entry->name_len] = '\0';
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
	/* If not found in current node. */
	return -1;
}

/* Given a path, compute the number of levels of the given path.
 * note that path should be in this form a/b (retrive the pre , back trailing '/'s).
 */
int compute_level(char *path){
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


/* Given a type and block number, free the correspond bitmap. */
int free_map(char type, int block_num){
    char* map;
	int bit_index, byte_index;
	if(type == 'd'){
	    map = block_bit_map;
	} else {
	    map = inode_bit_map;
	}
	
	byte_index = (block_num - 1)/8;
    bit_index = (block_num - 1)%8;
	printf("Set %c map's block %d to 0\n", type, block_num);
    map[byte_index] &= ~(1 << (bit_index));
    return 0;
}

/* Helper function that returns a pointer to the entry with same name and type 
 * in a given dir_block.
 */
struct ext2_dir_entry* sen_in_db(char* target_name, int dir_block, char type){
	int read_count = 0;
	/* While not read the whole data block. */
	while(read_count < EXT2_BLOCK_SIZE){
		struct ext2_dir_entry* cur_entry= (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE * dir_block + read_count);
		char cur_name[cur_entry->name_len + 1];
		memset(cur_name, '0', cur_entry->name_len + 1);
		char cur_type;
		strncpy(cur_name, cur_entry->name, cur_entry->name_len);
		cur_name[cur_entry->name_len] = '\0';
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
			/* This line is provided for rm dir usage. To tell how much block is raad
			 * to ensure not reading the entry from next data block.
			 */
			cur_entry->name_len = read_count;
			return cur_entry;
		}
		read_count += cur_entry->rec_len;
	}
	/* If not found in current node. */
	return NULL;
}

/*
 * Given a file name, return the correspond 
 */
int sen_in_inode(char* file_name, int length, struct ext2_inode inode, char type){
    int i, j, k, res;
	// Given level1/level2, length = 5
	//       0000000
	char target_name[length + 2];
	memset(target_name, '\0', length + 2);
	/* length is the index of when the filename ends. */
	strncpy(target_name, file_name, length + 1);
	
	
	/* Search for direct blocks. */
	for(i = 0; i < 12; i++){
		res = search_in_db(target_name, inode.i_block[i], type);
	    if(res > 0){
		    return inode.i_block[i];
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
			    return inode.i_block[i];
		    }
		}
	}
	
	/* Search for indirect pointers. */
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
			        return inode.i_block[i];
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
			            return inode.i_block[i];
		            }
			    }
		}
	}
	
	/* Go over all but still not find. */
	return -1;
}

/*
 * Given a file name, return the correspond 
 */
int search_in_inode(char* file_name, int length, struct ext2_inode inode, char type){
	int i, j, k, res;
	// Given level1/level2, length = 5
	//       0000000
	char target_name[length + 1];
	memset(target_name, '\0', length + 2);
	/* length is the index of when the filename ends. */
	strncpy(target_name, file_name, length + 1);
	
	
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
	
	/* Search for indirect pointers. */
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
	struct ext2_inode* root = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * i_node_start);
	int end = 0;
	char* target_dir = path;
	int cur_level = 0;
	int i, byte_index, bit_index, target_inode;
	
	/* Given an absolute path, want to search level by level. */
	/* level1/level2 -> end should be 6*/
	// end < 12 && end + 1 ne /
	while( end < strlen(target_dir) - 1 && (path)[end + 1] != '/'){
		end += 1;
	}
	if(level == 1){
		return search_in_inode(target_dir, end, inode_table[1], type);
	}
	target_inode = search_in_inode(target_dir, end, inode_table[1], 'd');
	target_dir = &target_dir[end + 2];
	if(target_inode > 0){
		cur_level += 1;
		i = target_inode;
	} else {
		i = 11;
	}
	while( end < strlen(target_dir) - 1 && (target_dir)[end + 1] != '/'){
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

/* Return the index of a free block. (return 12 if block 13 is free)*/
int find_new_block(char* type){
	char* map;
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
			break;
		}
	}
	
	/* Return the index of the free block. */
	return i;
}

/* Find a useable inode block, and initialize it. 
 * return the number of alloctaed block.
 */
int allocate_inode(){
	char *type = "i";
	/* Note new_block is the block number, not the index. */
	int new_block = find_new_block(type);
	
	/* Set all the values to 0. */
	printf("The index of new block is %d\n", new_block);
	memset(&inode_table[new_block - 1], 0, sizeof(struct ext2_inode));
	sb->s_free_inodes_count -= 1;
	gd->bg_free_inodes_count -= 1;
	return new_block;
}

/* Find a useable data block and initialize it.
 * return the number of allocated block(not the index.)
 */
int allocate_dblock(){
	char *type = "d";
	int new_block = find_new_block(type);
	
	/* Set all the values to 0. */
	printf("The index of new data block is %d\n", new_block);
	memset(disk + EXT2_BLOCK_SIZE * new_block, 0, EXT2_BLOCK_SIZE);
	sb->s_free_blocks_count -= 1;
	gd->bg_free_blocks_count -= 1;
	return new_block;
}

/* Increase number of free blocks. */
void increase_free_blocks(){
    sb->s_free_blocks_count += 1;
	gd->bg_free_blocks_count += 1;	
}

void increase_free_inodes(){
    sb->s_free_inodes_count += 1;
	gd->bg_free_inodes_count += 1;
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
		// Same as name_len + 4 - name_len %4?
        return 4*(res/4 + 1);
    }
}




/*
 *add a directory entry to a directory inode, return 0 on success, return -ENOSPC on failure
 */
int add_entry(int dir_inode_num, struct ext2_dir_entry *entry_to_add){
    struct ext2_inode *dir_inode = (struct ext2_inode *)(&(inode_table[dir_inode_num-1]));    
    int block_index = -1;
    int i_block_index;
    for(i_block_index = 0; i_block_index < 12; i_block_index++){
        if(dir_inode->i_block[i_block_index+1] == 0){
            block_index = dir_inode->i_block[i_block_index];
            break;
        }
    }
    if(block_index == -1){
        return -ENOSPC;
    }   
    struct ext2_dir_entry *temp;
    int cur_rec_when_we_are_at_temp;  //modified!!!!!!!!!!!!!!!!!!!!
    struct ext2_dir_entry *cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index);
    int cur_rec = 0;
    int entry_inserted = 0;
    while(!entry_inserted){
	    while(cur_rec + cur_entry->rec_len < EXT2_BLOCK_SIZE){   //each dir at least has 2 entries: "." and ".."
		    cur_rec += cur_entry->rec_len;
		    cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);   
	    }
	    //int cur_entry_rec_len = calculate_reclen(cur_entry);
	    /* Line 549 -- 568 modifies the last entry? */
		//if(cur_rec + cur_entry_rec_len < EXT2_BLOCK_SIZE){
	    //    struct ext2_dir_entry *next_entry_possibly_a_gap = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec + cur_entry_rec_len);
	    //    temp = cur_entry;
        //    temp->rec_len = calculate_reclen(temp); //modified!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //    cur_rec_when_we_are_at_temp = cur_rec;  //modified!!!!!!!!!!!!!!!!!!!!!!!!!!!
	    //    while(next_entry_possibly_a_gap->inode != 0 ){ //we have a gap at the end of current block!!!
		//        cur_rec += cur_entry_rec_len;
		//        cur_entry = next_entry_possibly_a_gap;
		//		temp->rec_len += cur_entry_rec_len; //modified!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//        if(cur_rec + cur_entry->rec_len >= EXT2_BLOCK_SIZE){
		//            break;
		//        }
		//        cur_entry_rec_len = calculate_reclen(cur_entry);
        //        
		//        if(cur_rec + cur_entry_rec_len >= EXT2_BLOCK_SIZE || cur_entry_rec_len == cur_entry->rec_len){ //modified!!!!!!!!!!!!!!!!!!!!!!!!!!
		//            break;
		//       }
		//        next_entry_possibly_a_gap = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec + cur_entry_rec_len);    
		//    }
	    //}
	    //temp->rec_len = cur_rec + cur_entry_rec_len - EXT2_BLOCK_SIZE + temp->rec_len;
        if((EXT2_BLOCK_SIZE - cur_rec) - calculate_reclen(cur_entry) < entry_to_add->rec_len){
            if(i_block_index >= 11){
                return -ENOSPC;
            }else{
                block_index = allocate_dblock(); 
                if(block_index == -1){
                    return -ENOSPC;  
                }
                temp->rec_len = EXT2_BLOCK_SIZE - cur_rec_when_we_are_at_temp; //modified!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                i_block_index += 1;
                //block_index -= 1;
                dir_inode->i_block[i_block_index] = block_index;
                cur_rec = 0;
                cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);
                cur_entry->inode = entry_to_add->inode;
                cur_entry->name_len = entry_to_add->name_len;
                cur_entry->file_type = entry_to_add->file_type;
                cur_entry->rec_len = EXT2_BLOCK_SIZE - cur_rec;
                strncpy(cur_entry->name, entry_to_add->name, cur_entry->name_len);
                entry_inserted = 1;
            }                
        }else{
                    //no need to update temp->rec_len here;
                    cur_entry->rec_len = calculate_reclen(cur_entry);
                    cur_rec += cur_entry->rec_len;
                    cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);
                    cur_entry->inode = entry_to_add->inode;
                    cur_entry->name_len = entry_to_add->name_len;
                    cur_entry->file_type = entry_to_add->file_type;
                    cur_entry->rec_len = EXT2_BLOCK_SIZE - cur_rec;
                    strncpy(cur_entry->name, entry_to_add->name, cur_entry->name_len);
                    entry_inserted = 1;
            }
    }
    return 0;
}

/*
 *add a directory entry to a directory inode, return 0 on success, return -ENOSPC on failure
 */
int add_entry_o(int dir_inode_num, struct ext2_dir_entry *entry_to_add){
    struct ext2_inode *dir_inode = (struct ext2_inode *)(&(inode_table[dir_inode_num-1]));    
    int block_index = -1;
    int i_block_index;
    for(i_block_index = 0; i_block_index < 12; i_block_index++){
        if(dir_inode->i_block[i_block_index+1] == 0){
            block_index = dir_inode->i_block[i_block_index];
            break;
        }
    }
    if(block_index == -1){
        return -ENOSPC;
    }   
	struct ext2_dir_entry *temp;
    struct ext2_dir_entry *cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index);
    int cur_rec = 0;
    int entry_inserted = 0;
    while(!entry_inserted){
	    while(cur_rec + cur_entry->rec_len < EXT2_BLOCK_SIZE){   //each dir at least has 2 entries: "." and ".."
		    cur_rec += cur_entry->rec_len;
            cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);   
	    }
        int cur_entry_rec_len = calculate_reclen(cur_entry);
        if(cur_rec + cur_entry_rec_len < EXT2_BLOCK_SIZE){
            struct ext2_dir_entry *next_entry_possibly_a_gap = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec + cur_entry_rec_len);
            temp = cur_entry;
			while(next_entry_possibly_a_gap->inode != 0 /*&& cur_rec + cur_entry->rec_len < EXT2_BLOCK_SIZE*/){ //we have a gap at the end of current block!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                cur_rec += cur_entry_rec_len;
                cur_entry = next_entry_possibly_a_gap;
                if(cur_rec + cur_entry->rec_len >= EXT2_BLOCK_SIZE){
                    break;
                }
                cur_entry_rec_len = calculate_reclen(cur_entry);
                if(cur_rec + cur_entry_rec_len >= EXT2_BLOCK_SIZE){
                    break;
                }
                next_entry_possibly_a_gap = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec + cur_entry_rec_len);    
                }
            }
			temp->rec_len = cur_rec + cur_entry_rec_len - EXT2_BLOCK_SIZE + temp->rec_len;
            if((EXT2_BLOCK_SIZE - cur_rec) - calculate_reclen(cur_entry) < entry_to_add->rec_len){
                if(i_block_index >= 11){
                    return -ENOSPC;
                }else{
                    block_index = allocate_dblock(); 
                    if(block_index == -1){
                        return -ENOSPC;  
                    }
                    i_block_index += 1;
                    //block_index -= 1;
                    dir_inode->i_block[i_block_index] = block_index;
                    cur_rec = 0;
                    cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);
                    cur_entry->inode = entry_to_add->inode;
                    cur_entry->name_len = entry_to_add->name_len;
                    cur_entry->file_type = entry_to_add->file_type;
                    cur_entry->rec_len = EXT2_BLOCK_SIZE - cur_rec;
                    strncpy(cur_entry->name, entry_to_add->name, cur_entry->name_len);
                    entry_inserted = 1;
                }                
            }else{
                    cur_entry->rec_len = calculate_reclen(cur_entry);
                    cur_rec += cur_entry->rec_len;
                    cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);
                    cur_entry->inode = entry_to_add->inode;
                    cur_entry->name_len = entry_to_add->name_len;
                    cur_entry->file_type = entry_to_add->file_type;
                    cur_entry->rec_len = EXT2_BLOCK_SIZE - cur_rec;
                    strncpy(cur_entry->name, entry_to_add->name, cur_entry->name_len);
                    entry_inserted = 1;
            }
    }
    return 0;
}