#include<stdio.h>
#include "ext2_fs.h"
#include<unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<stdlib.h>
#include<string.h>


#define BLOCK_OFFSET(block) (1024 + (block-1)*block_size)

unsigned int group_count; 
struct ext2_super_block super;  //super block
struct ext2_group_desc group;	//Group descriptors
unsigned int block_size=0;      //block size


static int super_block(int ,struct ext2_super_block*);
static void group_disc(int ,struct ext2_group_desc * );
static void block_bitmap(int );
static void inode_bitmap(int );
static void read_inode (int ,int ,struct ext2_inode *);
static int file_check(unsigned short );
static void dir_entry(int ,struct ext2_inode *,unsigned int);

int main(int argc,char *argv[])
{
	int fd,i;
	unsigned char *bitmap;	
	struct ext2_inode inode;


	if(argc!=2){
		printf("Error Usage:- < %s file_name >\n",argv[1]);
		return -1;
	}

	if((fd = open(argv[1],O_RDONLY)) < 0)
	{
		perror("open");
		return -1;
	}

	lseek(fd,1024,SEEK_SET); // skip first boot block(1024 bytes);


	printf("\n------------------super block---------------------\n");
	super_block(fd,&super); // super block

	block_size=1024 << super.s_log_block_size;
	printf("block_size%x\n",block_size);


	printf("\n------------------Group Descriptors----------------\n");
	group_disc(fd,&group);

	printf("\n-------------------inode bitmap-------------------\n");
	block_bitmap(fd );

	printf("\n-------------------inode bitmap-------------------\n");
	inode_bitmap(fd );


	printf("\n-------------------inode table-------------------\n");
	read_inode(fd,2,&inode);

	return 0;
}

static int super_block(int fd,struct ext2_super_block *s)
{

	read(fd,s,sizeof(struct ext2_super_block));

	if(super.s_magic!=EXT2_SUPER_MAGIC)
	{
		printf("SUPER_MAGIC\n");
		exit(1);
	}

	printf("Inodes count =%x\n",super.s_inodes_count);
	printf("blocks count =%d\n",super.s_blocks_count);
	printf("reserved block count =%x\n",super.s_r_blocks_count);
	printf("free block count =%x\n",super.s_free_blocks_count);
	printf("free Inodes count =%x\n",super.s_free_inodes_count);
	printf("First Data Block=%x\n",super.s_first_data_block);
	printf("Fragment size=%x\n",super.s_log_frag_size);
	printf("s_frags_per_group=%x\n",super.s_frags_per_group);
	printf("Blocks per group =%x\n",super.s_blocks_per_group);
	printf("Fragments per group =%x\n",super.s_frags_per_group);
	printf("First non-reserved inode=%x\n",super.s_first_ino);
	printf("size of inode structure=%x\n",super.s_inode_size);
	printf("Inodes per group =%x\n",super.s_inodes_per_group);

	return 0;	
}

void group_disc(int fd,struct ext2_group_desc * g)
{

	/* calculate number of block groups on the disk */
	unsigned int group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

	/* calculate size of the group descriptor list in bytes */
	unsigned int descr_list_size = group_count * sizeof(struct ext2_group_desc);

	printf("group_count=%d\n",group_count);

	printf("group descriptor list=%d\n\n",descr_list_size);


	lseek(fd, 1024 + block_size, SEEK_SET);

	read(fd, g, sizeof(group));

	printf("Reading first group-descriptor from device  ext2.img :\n"
			"Blocks bitmap block: %u\n"
			"Inodes bitmap block: %u\n"
			"Inodes table block : %u\n"
			"Free blocks count  : %u\n"
			"Free inodes count  : %u\n"
			"Directories count  : %u\n"
			,
			group.bg_block_bitmap,
			group.bg_inode_bitmap,
			group.bg_inode_table,
			group.bg_free_blocks_count,
			group.bg_free_inodes_count,
			group.bg_used_dirs_count);    

}

void block_bitmap(int fd)
{

	unsigned char * bitmap;
	unsigned int offset;

	bitmap = (char*)malloc(block_size);
	offset =BLOCK_OFFSET(group.bg_block_bitmap);

	printf("offset=%x\n",offset);

	lseek(fd, offset, SEEK_SET);
	read(fd, bitmap,1024);   /* read bitmap from disk */

	//	for(i=0;i<1024;i++)
	//		printf("%x ",bitmap[i]);

	printf("\n");
	free(bitmap);

}

void inode_bitmap(int fd)
{
	unsigned char * bitmap;
	unsigned int offset;

	bitmap = (char*)malloc(block_size);
	offset =BLOCK_OFFSET(group.bg_inode_bitmap);

	printf("offset=%x\n",offset);

	lseek(fd, offset, SEEK_SET);
	read(fd, bitmap,1024);   /* read bitmap from disk */

	//	for(i=0;i<1024;i++)
	//		printf("%x ",bitmap[i]);

	printf("\n");
	free(bitmap);
}



static void read_inode (int fd ,int inode_num , struct ext2_inode *inode)
{

	int i,ret;

	unsigned int inode_per_block,total_inode_blocks,position;

	inode_per_block = block_size /sizeof(struct ext2_inode); //number of inode per blocks
	total_inode_blocks = super.s_inodes_per_group/inode_per_block; // total blocks of inode table

	printf("sizeof inode=%x\n",sizeof(struct ext2_inode));
	printf("block size=%x\n",block_size);	
	printf("inode per block=%x\n",inode_per_block);
	printf("total inode table block=%x\n",total_inode_blocks);

	lseek(fd,BLOCK_OFFSET(group.bg_inode_table)+(inode_num-1)* sizeof(struct ext2_inode),SEEK_SET);

	position = lseek(fd,0,SEEK_CUR);
	printf("\nPosition=%x\n",position);

	read(fd,inode,sizeof(struct ext2_inode));
	printf("block count=%x\n",inode->i_blocks);
	ret=file_check(inode->i_mode);	

	printf("ret=%d\n",ret);

	for(i=0; i<EXT2_N_BLOCKS; i++){
		if (i < EXT2_NDIR_BLOCKS)
		{                                                                /* direct blocks */
			printf("Block %2u : %u\n", i, inode->i_block[i]);

			if(ret==2 && (inode->i_block[i]!=0)){
				dir_entry(fd,inode,inode->i_block[i]);
			}

		}
		else if (i == EXT2_IND_BLOCK)                             /* single indirect block */
			printf("Single   : %u\n", inode->i_block[i]);
		else if (i == EXT2_DIND_BLOCK)                            /* double indirect block */
			printf("Double   : %u\n", inode->i_block[i]);
		else if (i == EXT2_TIND_BLOCK)                            /* triple indirect block */
			printf("Triple   : %u\n", inode->i_block[i]);
	}

}

void dir_entry(int fd,struct ext2_inode * inode,unsigned int db)
{

	sleep(1);

	struct ext2_dir_entry_2 *entry;	
	unsigned char block[1024];
	unsigned int size,position,offset;

	lseek(fd,BLOCK_OFFSET(db),SEEK_SET);

	position=lseek(fd,0,SEEK_CUR);
	printf("position=%x\n",position);

	read(fd,block,1024);	
	entry=(struct ext2_dir_entry_2 *)block;

	size=0;

	printf("i_size=%x\n",inode->i_size);
	while(size < inode->i_size)
	{

		unsigned char name[256];

		memcpy(name,entry->name,entry->name_len);
		name[entry->name_len]=0;
		printf("%8x %s\n",entry->inode,name);


		if((entry->file_type == 2) && (entry->inode !=2))
		{

			offset = lseek(fd,0,SEEK_CUR);
			read_inode(fd,entry->inode,inode);		
			lseek(fd,offset,SEEK_SET);
		}

		sleep(1);

		entry=(void*)entry + entry->rec_len;
		size = size + entry-> rec_len;

		if(entry->rec_len == 0)
			break;

	}
	printf("\n");	

}


int file_check(unsigned short n)
{


	if(S_ISREG(n)){
		printf("---Regular file---\n");
		return 1;
	}

	else if(S_ISDIR(n)){
		printf("---directory file---\n");
		return 2;
	}

	else if(S_ISCHR(n)){
		printf("---character file---\n");
		return 3;
	}

	else if(S_ISBLK(n)){
		printf("---character file---\n");
		return 4;
	}

	else if(S_ISFIFO(n)){
		printf("---FIFO file---\n");
		return 5;
	}

	else if(S_ISSOCK(n)){
		printf("---SOCKET file---\n");
		return 6;
	}

	if(S_ISLNK(n)){
		printf("---Symbolic Link---\n");
		return 7;
	}
	else{
		printf("---Unknown---\n");
		return 0;
	}	
}

