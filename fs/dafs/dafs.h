#ifndef _DAFS_H_
#define _DAFS_H_

#define DA_OP_OPEN 0
#define DA_OP_CREAT 1
#define DA_OP_READ 2
#define DA_OP_WRITE 3
#define DA_OP_CLOSE 4
#define DA_OP_LINK 5
#define DA_OP_LSEEK 6
#define DA_OP_UNLINK 7
#define DA_OP_ISATTY 8
#define DA_OP_FCNTL 9
#define DA_OP_STAT 10
#define DA_OP_FSTAT 11
#define DA_OP_GETCWD 12
#define DA_OP_CHDIR 13
#define DA_OP_MKDIR 14
#define DA_OP_RMDIR 15
#define DA_OP_FINDFIRST 16
#define DA_OP_FINDNEXT 17
#define DA_OP_FINDCLOSE 18
#define DA_OP_CHMOD 19
#define DA_OP_PREAD 20
#define DA_OP_PWRITE 21

#define OS_TYPE_FILE 1
#define OS_TYPE_DIR 2
#define OS_TYPE_SYMLINK 3
#define OS_TYPE_CHARDEV 4
#define OS_TYPE_BLOCKDEV 5
#define OS_TYPE_FIFO 6
#define OS_TYPE_SOCK 7

#define DA_O_RDONLY 0
#define DA_O_WRONLY 1
#define DA_O_RDWR 2
#define DA_O_APPEND 8
#define DA_O_CREAT 0x0200
#define DA_O_TRUNC 0x0400
#define DA_O_EXCL 0x0800

#define DA_O_AFFINITY_THREAD_0 0x10000
#define DA_O_AFFINITY_THREAD_1 0x20000
#define DA_O_AFFINITY_THREAD_2 0x40000
#define DA_O_AFFINITY_THREAD_3 0x80000
#define DA_O_AFFINITY_SHIFT 16

#define DA_S_IWUSR	0200	/* 0x80 */
#define DA_S_IRUSR	0400	/* 0x100 */

struct da_stat {
	short st_dev;
	unsigned short st_ino;
	unsigned st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	short st_rdev;
	int st_size;
	int st_atime;
	int st_spare1;
	int st_mtime;
	int st_spare2;
	int st_ctime;
	int st_spare3;
	int st_blksize;
	int st_blocks;
	int st_spare4[2];
};

#define _A_SUBDIR 0x10

struct da_finddata {
	unsigned long size;
	unsigned long attrib;
	char name[260];
};

#endif
