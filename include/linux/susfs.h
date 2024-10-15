#ifndef KSU_SUSFS_H
#define KSU_SUSFS_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/mount.h>
#include <linux/hashtable.h>

/********/
/* ENUM */
/********/
/* shared with userspace ksu_susfs tool */
#define CMD_SUSFS_ADD_SUS_PATH 0x55550
#define CMD_SUSFS_ADD_SUS_MOUNT 0x55560
#define CMD_SUSFS_ADD_SUS_KSTAT 0x55570
#define CMD_SUSFS_UPDATE_SUS_KSTAT 0x55571
#define CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY 0x55572
#define CMD_SUSFS_ADD_TRY_UMOUNT 0x55580
#define CMD_SUSFS_SET_UNAME 0x55590
#define CMD_SUSFS_ENABLE_LOG 0x555a0
#define CMD_SUSFS_SUS_SU 0x60000

#define SUSFS_MAX_LEN_PATHNAME 256 // 256 should address many paths already unless you are doing some strange experimental stuff, then set your own desired length

/*
 * inode->i_state => storing flag 'INODE_STATE_'
 * inode->android_kabi_reserved1 => storing fake i_ino
 * inode->android_kabi_reserved2 => storing fake i_dev
 * inode->super_block->android_kabi_reserved1 => storing fake i_nlink
 * inode->super_block->android_kabi_reserved2 => storing fake i_size
 * inode->super_block->android_kabi_reserved3 => storing fake i_blocks
 * mount->vfsmount->android_kabi_reserved1 => storing fake mnt id
 * mount->vfsmount->android_kabi_reserved2 => storing fake mnt group id (peer id)
 * task_struct->android_kabi_reserved1 => storing flag 'TASK_STRUCT_KABI1_'
 * task_struct->android_kabi_reserved2 => record last valid fake mnt_id to zygote pid
 * user_struct->android_kabi_reserved1 => storing flag 'USER_STRUCT_KABI1_'
 */

#define INODE_STATE_SUS_PATH 16777216 // 1 << 24
#define INODE_STATE_SUS_MOUNT 33554432 // 1 << 25
#define INODE_STATE_SUS_KSTAT 67108864 // 1 << 26

#define TASK_STRUCT_KABI1_IS_ZYGOTE 1 // 1 << 0

#define USER_STRUCT_KABI1_NON_ROOT_USER_APP_PROFILE 16777216 // 1 << 24, for distinguishing root/no-root granted user app process

/*********/
/* MACRO */
/*********/
#define getname_safe(name) (name == NULL ? ERR_PTR(-EINVAL) : getname(name))
#define putname_safe(name) (IS_ERR(name) ? NULL : putname(name))

/**********/
/* STRUCT */
/**********/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
struct st_susfs_sus_path {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
};

struct st_susfs_sus_path_hlist {
	unsigned long                    target_ino;
	char                             target_pathname[SUSFS_MAX_LEN_PATHNAME];
	struct hlist_node                node;
};
#endif

/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
struct st_susfs_sus_mount {
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long           target_dev;
};

struct st_susfs_sus_mount_list {
	struct list_head                        list;
	struct st_susfs_sus_mount               info;
};
#endif

/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
struct st_susfs_sus_kstat {
	int                     is_statically;
	unsigned long           target_ino; // the ino after bind mounted or overlayed
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned long           spoofed_ino;
	unsigned long           spoofed_dev;
	unsigned int            spoofed_nlink;
	long long               spoofed_size;
	long                    spoofed_atime_tv_sec;
	long                    spoofed_mtime_tv_sec;
	long                    spoofed_ctime_tv_sec;
	long                    spoofed_atime_tv_nsec;
	long                    spoofed_mtime_tv_nsec;
	long                    spoofed_ctime_tv_nsec;
	unsigned long           spoofed_blksize;
	unsigned long long      spoofed_blocks;
};

struct st_susfs_sus_kstat_list {
	struct list_head                        list;
	struct st_susfs_sus_kstat               info;
};
#endif

/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
struct st_susfs_try_umount {
	char                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	int                     mnt_mode;
};

struct st_susfs_try_umount_list {
	struct list_head                        list;
	struct st_susfs_try_umount              info;
};
#endif

/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
struct st_susfs_uname {
	char        release[__NEW_UTS_LEN+1];
	char        version[__NEW_UTS_LEN+1];
};
#endif

/* sus_su */
#ifdef CONFIG_KSU_SUSFS_SUS_SU
struct st_sus_su {
	bool        enabled;
	char        drv_path[256];
	int         maj_dev_num;
};
#endif

/***********************/
/* FORWARD DECLARATION */
/***********************/
/* sus_path */
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
int susfs_add_sus_path(struct st_susfs_sus_path* __user user_info);
int susfs_sus_ino_for_filldir64(unsigned long ino);
#endif
/* sus_mount */
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
int susfs_add_sus_mount(struct st_susfs_sus_mount* __user user_info);
#endif
/* sus_kstat */
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
int susfs_add_sus_kstat(struct st_susfs_sus_kstat* __user user_info);
#endif
/* try_umount */
#ifdef CONFIG_KSU_SUSFS_TRY_UMOUNT
int susfs_add_try_umount(struct st_susfs_try_umount* __user user_info);
void susfs_try_umount(uid_t target_uid);
#endif
/* spoof_uname */
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
int susfs_set_uname(struct st_susfs_uname* __user user_info);
int susfs_spoof_uname(struct new_utsname* tmp);
#endif
/* set_log */
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_set_log(bool enabled);
#endif
/* sus_su */
#ifdef CONFIG_KSU_SUSFS_SUS_SU
int susfs_sus_su(struct st_sus_su* __user user_info);
#endif
/* susfs_init */
void susfs_init(void);

#endif
