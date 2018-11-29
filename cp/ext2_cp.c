#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "errno.h"
#include "ext2_helper_modified.h"

/*disk components*/
unsigned char *disk;
unsigned char *block_bit_map;
unsigned char *inode_bit_map;
struct ext2_inode *inode_table;
int table_size;
int dblock_size;
int table_start;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;



int main(int argc, char **argv){
    int case2 = 0;   //indicates there is AT LEAST POSSIBILITY for case2
    
    //check the arguments
    if(argc != 4){
        printf("Usage: ext2_cp <image file name> <path to source file> <path to dest>\n");
        exit(100);
    }
    
    //copy the contents of the arguments
    int head = 0;
    while(head < strlen(argv[3]) && argv[3][head] == '/'){
        head++;
    }    
    char *argv3 = &(argv[3][head]);
    char source[strlen(argv[2])+1];
    strncpy(source, argv[2], strlen(argv[2]));
    source[strlen(argv[2])] = '\0';
    char dest[strlen(argv3)+1];
    strncpy(dest, argv3, strlen(argv3));
    dest[strlen(argv3)] = '\0';

    //open file for read
    FILE *fp = fopen(source, "r");
    if(fp == NULL){
        perror("fopen: ");
        exit(-1);
    }
    
    //get rid of trailing and heading slashes
    int tail = strlen(dest)-1;
    while(tail >= 0 && dest[tail] == '/'){
        tail--;
    }
    dest[tail+1] = '\0';

    //extract filename
    tail = strlen(source)-1;
    int len_filename = 0;
    while(tail >= 0 && source[tail] != '/'){
        tail--;
        len_filename++;
    }
    char filename[len_filename+1];
    strncpy(filename, &(source[tail+1]), len_filename);
    filename[len_filename] = '\0';

    //initialize disk
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


    //find the inode number of the directory in which we should do the copy
    //case1: we are given a directory as "dest"
    //case2: we are given a valid path of a file as "dest"

    //preparation for case2
    int length_of_last = 0;
    char dest_without_last[EXT2_NAME_LEN+1];
    char alternative_filename[EXT2_NAME_LEN+1];
    if(compute_level(dest) > 1){
	    tail = strlen(dest)-1;
	    while(tail >= 0 && dest[tail] != '/'){
		tail--;
		length_of_last++;
	    }
	    strncpy(dest_without_last, dest, tail);
	    dest_without_last[tail] = '\0';
	    strncpy(alternative_filename, &(dest[tail+1]), length_of_last);
	    alternative_filename[length_of_last] = '\0';
            //get rid of trailing slashes of dest_without_last
            tail = strlen(dest_without_last)-1;
            while(tail >= 0 && dest_without_last[tail] == '/'){
                tail--;
            }
            dest_without_last[tail+1] = '\0';
            case2 = 1;
    }
       
    //new dir entry, new inode
    int dir_inode_num;
    if(cd_revised(dest,'d') > 0){  //case1
        dir_inode_num = cd_revised(dest,'d');
        case2 = 0;   
    }else if(case2 && cd_revised(dest_without_last,'d') > 0){ //case2
        dir_inode_num = cd_revised(dest_without_last,'d');
    }else{ //invalid path
        return ENOENT;
    }
    
    int name_length_cp;
    if(case2){
        name_length_cp = strlen(alternative_filename);
    }else{
        name_length_cp = strlen(filename);
    }

    if(case2){  //newly updated!!!!!!!!!!!!!!!!!!!!!!!
        if(search_in_inode(alternative_filename, strlen(alternative_filename), inode_table[dir_inode_num-1], 'f') > 0){
            printf("result of search_in_inode: %d\n",search_in_inode(alternative_filename, strlen(alternative_filename), inode_table[dir_inode_num-1], 'f'));
            return EEXIST;
        }
    }else{
        if(search_in_inode(filename, strlen(filename), inode_table[dir_inode_num-1], 'f') > 0){
            printf("result of search_in_inode: %d\n",search_in_inode(filename, strlen(filename), inode_table[dir_inode_num-1], 'f'));
            return EEXIST;
        }
    }
 
    //get inode_num of the new file
    int inode_num = allocate_inode(); //inode_num-1 is the index in inode_table
    if(inode_num == -1){
        return ENOSPC;
    }

    struct ext2_dir_entry entry_cp;
    entry_cp.inode = inode_num;
    entry_cp.name_len = name_length_cp;
    entry_cp.file_type |= EXT2_FT_REG_FILE;
    entry_cp.rec_len = calculate_reclen(&entry_cp);  

    struct ext2_inode *cp_inode = (struct ext2_inode *)(&(inode_table[inode_num-1]));
    cp_inode->i_mode |= EXT2_S_IFREG;
    cp_inode->i_links_count = 1;
    
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
        return ENOSPC;
    }   
    struct ext2_dir_entry *cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index);
    int cur_rec = 0;
    int entry_inserted = 0;
    while(!entry_inserted){
	    while(cur_rec + cur_entry->rec_len < EXT2_BLOCK_SIZE){
		cur_rec += cur_entry->rec_len;
                cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);   
	    }   
            if((EXT2_BLOCK_SIZE - cur_rec) - calculate_reclen(cur_entry) < entry_cp.rec_len){
                if(i_block_index >= 11){
                    return ENOSPC;
                }else{
                    block_index = allocate_dblock(); 
                    if(block_index == -1){
                        return ENOSPC;  
                    }
                    i_block_index += 1;
                    //block_index -= 1;
                    dir_inode->i_block[i_block_index] = block_index;
                    cur_rec = 0;
                    cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);
                    cur_entry->inode = entry_cp.inode;
                    cur_entry->name_len = entry_cp.name_len;
                    cur_entry->file_type |= EXT2_FT_REG_FILE;
                    cur_entry->rec_len = EXT2_BLOCK_SIZE - cur_rec;
                    if(case2){
                        strncpy(cur_entry->name, alternative_filename, cur_entry->name_len+1);
                    }else{
                        strncpy(cur_entry->name, filename, cur_entry->name_len+1);
                    }
                    entry_inserted = 1;
                }                
            }else{
                    cur_entry->rec_len = calculate_reclen(cur_entry);
                    cur_rec += cur_entry->rec_len;
                    cur_entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE*block_index + cur_rec);
                    cur_entry->inode = entry_cp.inode;
                    cur_entry->name_len = entry_cp.name_len;
                    cur_entry->file_type |= EXT2_FT_REG_FILE;
                    cur_entry->rec_len = EXT2_BLOCK_SIZE - cur_rec;
                    if(case2){
                        strncpy(cur_entry->name, alternative_filename, cur_entry->name_len+1);
                    }else{
                        strncpy(cur_entry->name, filename, cur_entry->name_len+1);
                    }
                    entry_inserted = 1;
            }
    }

    //start copying data
    int finish_copying = 0;
    char buf[EXT2_BLOCK_SIZE+1];
    int have_read;
    int block_index_in_inode = 0;
    int indirect = 0;
    int indirect_block_index;
    unsigned int *indirect_block;
    while(!finish_copying){
        if(indirect == 0 && block_index_in_inode == 12){
            indirect = 1;
            indirect_block_index = allocate_dblock();
            if(indirect_block_index == -1){
                return ENOSPC;
            }
            //indirect_block_index--;
            cp_inode->i_block[12] = indirect_block_index;
            indirect_block = (unsigned int *)(disk + EXT2_BLOCK_SIZE*indirect_block_index);
            block_index_in_inode = 0;
        }
        have_read = fread(buf, 1, EXT2_BLOCK_SIZE, fp);
        if(have_read > 0){
            //printf("hey!!!!!!!!!!!!!!!!!!!!\n");
            buf[have_read] = '\0';
            int data_block_index = allocate_dblock();
            if(data_block_index == -1){
                return ENOSPC;
            }
            //data_block_index--;
            if(indirect){
                indirect_block[block_index_in_inode] = data_block_index;
            }else{
                cp_inode->i_block[block_index_in_inode] = data_block_index;
            }
            char *copy_dest = (char *)(disk + EXT2_BLOCK_SIZE*data_block_index);
            strncpy(copy_dest, buf, strlen(buf));  //are there other ways to copy data? check!!!!!!!!!!!!!!!!!!!!!!!!
            cp_inode->i_size += have_read;
            cp_inode->i_blocks += 2;

            block_index_in_inode += 1;

        }else{
            finish_copying = 1;
        }
    }

    printf("cp finished!\n");
}


















