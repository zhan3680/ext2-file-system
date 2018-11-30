#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

void print_exercise1(struct ext2_super_block *sb, struct ext2_group_desc *gd);
void print_bit_map(unsigned char *bit_map, int num_byte, char type);
void print_inode_table(struct ext2_inode *inode_table, unsigned char *inode_bit_map, int table_size);
void print_inode(struct ext2_inode *inode, int inode_number);
void print_directory(struct ext2_inode * inode_table, unsigned char *inode_bit_map, int table_size);
void print_directory_inode(struct ext2_inode *inode, int inode_number);
void print_directory_block(int block_num);

void print_exercise1(struct ext2_super_block *sb, struct ext2_group_desc *gd){
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap); 
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd->bg_used_dirs_count);
}

void print_bit_map(unsigned char *bit_map, int num_byte, char type){
     if(type == 'b'){
         printf("Block bitmap: ");
     }else if(type == 'i'){
         printf("Inode bitmap: ");
     }
     int byte, bit;
     unsigned char mask;
     for(byte = 0; byte < num_byte; byte++){
         for(bit = 0; bit < 8; bit++){
             mask = 1 << bit;
             if(bit_map[byte] & mask){
                 printf("1");
             }else{
                 printf("0");
             }
         }
         printf(" ");
     }
     printf("\n");
}

void print_inode_table(struct ext2_inode * inode_table, unsigned char *inode_bit_map, int table_size){
    //print the second inode in the inode_table
	printf("Inodes:\n");
    print_inode(&inode_table[1], 2);

    //print the used inode, from 12-th on
    int byte_index, bit_index;
    for(int index = 12-1; index < table_size; index++){
        byte_index = index/8;
        bit_index = index%8;
        if(inode_bit_map[byte_index] & 1<<bit_index){
            print_inode(&inode_table[index], index + 1);
        }
    }
                                  
}

void print_inode(struct ext2_inode *inode, int inode_num){
    char type;
    if(S_ISREG(inode->i_mode)){
        type = 'f';
    }else if(S_ISDIR(inode->i_mode)){
        type = 'd';
    }else if(S_ISLNK(inode->i_mode)){
        type = 'l';
    }else{
        type = '0';
    }
    
    printf("[%d] type: %c size: %d links: %d blocks: %d\n", inode_num, type, inode->i_size, inode->i_links_count, inode->i_blocks);
    printf("[%d] Blocks: ", inode_num);
    int i = 0;
    while(inode->i_block[i] != 0){
        printf("% d", inode->i_block[i]);
        i++;
    }     
    printf("\n");
}

void print_directory(struct ext2_inode * inode_table, unsigned char *inode_bit_map, int table_size){
     printf("\n");
	 printf("Directory Blocks:");

     //print the inode with inode number 2, this is the root dir
     print_directory_inode(&inode_table[1],2);

     //print other directory inodes;
     int byte_index, bit_index;
     for(int index = 12-1; index < table_size; index++){
         byte_index = index/8;
         bit_index = index%8;
         if(inode_bit_map[byte_index] & 1<<bit_index){
             if(S_ISDIR(inode_table[index].i_mode)){   
                 print_directory_inode(&inode_table[index], index + 1);
             }
         }
     }
}

void print_directory_inode(struct ext2_inode *inode, int inode_number){
    printf("\n");
    printf("   DIR BLOCK NUM:");
    int i = 0;
    while(inode->i_block[i] != 0){
        printf(" %d (for inode %d)", inode->i_block[i], inode_number);
        i++;
    }  
    int j = 0;
    while(inode->i_block[j] != 0){
        print_directory_block(inode->i_block[j]);
        j++;
    }
}

void print_directory_block(int block_num){
    //check document 4.1
    //if inode == 0, then this entry is invalid;
    //always use current entry(byte position) + rec_len to get to next entry
    //if current entry + rec_len == 1024, then this is the las entry
    struct ext2_dir_entry *cur_entry = (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE*block_num);
    int current_rec = 0;
    char type;
    while(current_rec + cur_entry->rec_len < EXT2_BLOCK_SIZE){
        if(cur_entry->inode != 0){
            if(cur_entry->file_type == EXT2_FT_DIR){
                type = 'd';
            }else if(cur_entry->file_type == EXT2_FT_REG_FILE){
                type = 'f';
            }else if(cur_entry->file_type == EXT2_FT_SYMLINK){
                type = 'l';
            }else{
                type = '0';
            }
			printf("\n");
            printf("Inode: %d rec_len: %d name_len: %d type= %c ", cur_entry->inode, cur_entry->rec_len, cur_entry->name_len, type);
            char *name = (char *)(disk + EXT2_BLOCK_SIZE*block_num + current_rec + sizeof(struct ext2_dir_entry));
            printf("name=%.*s", cur_entry -> name_len, name);
        }       
        current_rec += cur_entry->rec_len;
        cur_entry = (struct ext2_dir_entry*)(disk + EXT2_BLOCK_SIZE*block_num + current_rec); 
        //note: reason we cannot use cur_entry += cur_entry->rec_len here: this will be equivalent to change cur_entry to
        //cur_entry[cur_entry->rec_len] or cur_entry = cur_entry + sizeof(ext2_dir_entry)* cur_entry->rec_len. Same thing happens when
        //you use cur_entry = (ext2_dir_entry *)(cur_entry + cur_entry->rec_len)       

        //similarly, we cannot use char *name = (char*)(cur_entry + sizeof(struct ext2_dir_entry));
    }
    //print last entry in this directory block
    if(cur_entry->inode != 0){
        if(cur_entry->file_type == EXT2_FT_DIR){
            type = 'd';
        }else if(cur_entry->file_type == EXT2_FT_REG_FILE){
            type = 'f';
        }else if(cur_entry->file_type == EXT2_FT_SYMLINK){
            type = 'l';
        }else{
            type = '0';
        }
		printf("\n");
        printf("Inode: %d rec_len: %d name_len: %d type= %c ", cur_entry->inode, cur_entry->rec_len, cur_entry->name_len, type);
        char *name_last = (char *)(disk + EXT2_BLOCK_SIZE*block_num + current_rec + sizeof(struct ext2_dir_entry));
        printf("name=%.*s", cur_entry -> name_len, name_last);
    }         
}

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    //exercise1 
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
    print_exercise1(sb, gd);

    //exercise2 part(a): print bitmaps
    unsigned char *block_bit_map = (unsigned char *)(disk + EXT2_BLOCK_SIZE*gd->bg_block_bitmap);
    unsigned char *inode_bit_map = (unsigned char *)(disk + EXT2_BLOCK_SIZE*gd->bg_inode_bitmap);
    print_bit_map(block_bit_map, sb->s_blocks_count/8, 'b');
    print_bit_map(inode_bit_map, sb->s_inodes_count/8, 'i');
    printf("\n");

    //exercise2 part(2): print (used)inodes
    struct ext2_inode *inode_table = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE*gd->bg_inode_table);
    print_inode_table(inode_table, inode_bit_map, sb->s_inodes_count);

    //exercise3
    print_directory(inode_table, inode_bit_map, sb->s_inodes_count);

    return 0;
}
