#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

MODULE_LICENSE("GPL");

/*
 *  Prototipos de funciones
 */
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);
int assoofs_fill_super(struct super_block *sb, void *data, int silent);
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no);
static struct inode *assoofs_get_inode(struct super_block *sb, int ino);
ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos);
ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos);
static int assoofs_iterate(struct file *filp, struct dir_context *ctx);
static int assoofs_create(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
struct dentry *assoofs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode);
static int assoofs_remove(struct inode *dir, struct dentry *dentry);
int assoofs_sb_get_a_freeinode(struct super_block *sb, unsigned long *inode);
void assoofs_save_sb_info(struct super_block *vsb);
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block);
void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode);
int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info);
struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search);
int assoofs_sb_set_a_freeinode(struct super_block *sb, unsigned long inode_no);
int assoofs_sb_set_a_freeblock(struct super_block *sb, uint64_t block_no);

/*
 *  Estructuras de datos necesarias
 */

static struct file_system_type assoofs_type = {
    .owner   = THIS_MODULE,
    .name    = "assoofs",
    .mount   = assoofs_mount,
    .kill_sb = kill_block_super,
};

const struct file_operations assoofs_file_operations = {
    .read  = assoofs_read,
    .write = assoofs_write,
};

const struct file_operations assoofs_dir_operations = {
    .owner          = THIS_MODULE,
    .iterate_shared = assoofs_iterate,
};

static struct inode_operations assoofs_inode_ops = {
    .create = assoofs_create,
    .lookup = assoofs_lookup,
    .mkdir  = assoofs_mkdir,
    .unlink = assoofs_remove,
    .rmdir  = assoofs_remove,
};

static const struct super_operations assoofs_sops = {
    .drop_inode = generic_delete_inode,
};


/*
 *  Funciones que realizan operaciones sobre ficheros
 */

ssize_t assoofs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos) {
    printk(KERN_INFO "assoofs_read request\n");

    struct assoofs_inode_info *inode_info = filp->f_inode->i_private;
    struct super_block *sb = filp->f_inode->i_sb;
    struct buffer_head *bh;
    char *buffer;
    ssize_t nbytes;

    if (*ppos >= inode_info->file_size)
        return 0;

    bh = sb_bread(sb, inode_info->data_block_number);
    if (!bh)
        return -EIO;

    buffer = (char *)bh->b_data;
    nbytes = min((size_t)(inode_info->file_size - *ppos), len);

    if (copy_to_user(buf, buffer + *ppos, nbytes)) {
        brelse(bh);
        return -EFAULT;
    }

    *ppos += nbytes;
    brelse(bh);
    return nbytes;
}

ssize_t assoofs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos) {
    printk(KERN_INFO "assoofs_write request\n");

    struct assoofs_inode_info *inode_info = filp->f_inode->i_private;
    struct super_block *sb = filp->f_inode->i_sb;
    struct buffer_head *bh;
    char *buffer;

    bh = sb_bread(sb, inode_info->data_block_number);
    if (!bh)
        return -EIO;

    buffer = (char *)bh->b_data;

    if (copy_from_user(buffer + *ppos, buf, len)) {
        brelse(bh);
        return -EFAULT;
    }

    *ppos += len;
    inode_info->file_size = *ppos;

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    assoofs_save_inode_info(sb, inode_info);
    return len;
}


/*
 *  Funciones que realizan operaciones sobre directorios
 */

static int assoofs_iterate(struct file *filp, struct dir_context *ctx) {
    printk(KERN_INFO "assoofs_iterate request\n");

    struct inode *inode = filp->f_inode;
    struct assoofs_inode_info *inode_info = inode->i_private;
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    if (ctx->pos >= inode_info->dir_children_count)
        return 0;

    bh = sb_bread(sb, inode_info->data_block_number);
    record = (struct assoofs_dir_record_entry *)bh->b_data;

    for (i = 0; i < inode_info->dir_children_count; i++) {
        if (record->entry_removed == ASSOOFS_FALSE) {
            dir_emit(ctx, record->filename, strlen(record->filename), record->inode_no, DT_UNKNOWN);
            ctx->pos++;
        }
        record++;
    }

    brelse(bh);
    return 0;
}


/*
 *  Funciones que realizan operaciones sobre inodos
 */

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
    struct assoofs_inode_info *dir_padre;
    struct super_block *sb;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    printk(KERN_INFO "assoofs_lookup request\n");

    dir_padre = parent_inode->i_private;
    sb = parent_inode->i_sb;
    bh = sb_bread(sb, dir_padre->data_block_number);

    record = (struct assoofs_dir_record_entry *)bh->b_data;
    for (i = 0; i < dir_padre->dir_children_count; i++) {
        if (!strcmp(record->filename, child_dentry->d_name.name) && record->entry_removed == ASSOOFS_FALSE) {
            struct inode *inode = assoofs_get_inode(sb, record->inode_no);
            inode_init_owner(&nop_mnt_idmap, inode, parent_inode, ((struct assoofs_inode_info *)inode->i_private)->mode);
            d_add(child_dentry, inode);
            brelse(bh);
            return NULL;
        }
        record++;
    }

    brelse(bh);
    return NULL;
}

static int assoofs_create(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    struct super_block *sb;
    struct inode *in;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *dir_contents;
    struct timespec64 ts;

    printk(KERN_INFO "assoofs_create New file request\n");

    sb = dir->i_sb;
    in = new_inode(sb);
    assoofs_sb_get_a_freeinode(sb, &in->i_ino);
    in->i_sb = sb;
    in->i_op = &assoofs_inode_ops;
    in->i_fop = &assoofs_file_operations;
    ts = current_time(in);
    inode_set_ctime(in, ts.tv_sec, ts.tv_nsec);
    inode_set_mtime(in, ts.tv_sec, ts.tv_nsec);
    inode_set_atime(in, ts.tv_sec, ts.tv_nsec);

    inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
    inode_info->inode_no = in->i_ino;
    inode_info->mode = mode;
    inode_info->file_size = 0;
    in->i_private = inode_info;

    assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
    assoofs_add_inode_info(sb, inode_info);

    inode_init_owner(&nop_mnt_idmap, in, dir, mode);
    d_add(dentry, in);

    parent_inode_info = dir->i_private;
    bh = sb_bread(sb, parent_inode_info->data_block_number);
    dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
    dir_contents += parent_inode_info->dir_children_count;
    dir_contents->inode_no = inode_info->inode_no;
    strcpy(dir_contents->filename, dentry->d_name.name);
    dir_contents->entry_removed = ASSOOFS_FALSE;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    parent_inode_info->dir_children_count++;
    assoofs_save_inode_info(sb, parent_inode_info);

    return 0;
}

struct dentry *assoofs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode) {
    struct super_block *sb;
    struct inode *in;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *dir_contents;
    struct timespec64 ts;

    printk(KERN_INFO "assoofs_mkdir New directory request\n");

    sb = dir->i_sb;
    in = new_inode(sb);
    assoofs_sb_get_a_freeinode(sb, &in->i_ino);
    in->i_sb = sb;
    in->i_op = &assoofs_inode_ops;
    in->i_fop = &assoofs_dir_operations;
    ts = current_time(in);
    inode_set_ctime(in, ts.tv_sec, ts.tv_nsec);
    inode_set_mtime(in, ts.tv_sec, ts.tv_nsec);
    inode_set_atime(in, ts.tv_sec, ts.tv_nsec);

    inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
    inode_info->inode_no = in->i_ino;
    inode_info->mode = S_IFDIR | mode;
    inode_info->dir_children_count = 0;
    in->i_private = inode_info;

    assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
    assoofs_add_inode_info(sb, inode_info);

    inode_init_owner(&nop_mnt_idmap, in, dir, S_IFDIR | mode);
    d_add(dentry, in);

    parent_inode_info = dir->i_private;
    bh = sb_bread(sb, parent_inode_info->data_block_number);
    dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
    dir_contents += parent_inode_info->dir_children_count;
    dir_contents->inode_no = inode_info->inode_no;
    strcpy(dir_contents->filename, dentry->d_name.name);
    dir_contents->entry_removed = ASSOOFS_FALSE;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    parent_inode_info->dir_children_count++;
    assoofs_save_inode_info(sb, parent_inode_info);

    return NULL;
}

static int assoofs_remove(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb = dir->i_sb;
    struct inode *inode = d_inode(dentry);
    struct assoofs_inode_info *inode_info = inode->i_private;
    struct assoofs_inode_info *parent_inode_info = dir->i_private;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    printk(KERN_INFO "assoofs_remove request\n");

    bh = sb_bread(sb, parent_inode_info->data_block_number);
    record = (struct assoofs_dir_record_entry *)bh->b_data;

    for (i = 0; i < parent_inode_info->dir_children_count; i++) {
        if (record->inode_no == inode_info->inode_no) {
            record->entry_removed = ASSOOFS_TRUE;
            break;
        }
        record++;
    }

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    assoofs_sb_set_a_freeinode(sb, inode->i_ino);
    assoofs_sb_set_a_freeblock(sb, inode_info->data_block_number);

    return 0;
}

/*
 *  Auxiliares para liberar inodos y bloques
 */

int assoofs_sb_set_a_freeinode(struct super_block *sb, unsigned long inode_no) {
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    assoofs_sb->free_inodes &= ~(1 << inode_no);
    assoofs_save_sb_info(sb);
    return 0;
}

int assoofs_sb_set_a_freeblock(struct super_block *sb, uint64_t block_no) {
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    assoofs_sb->free_blocks &= ~(1 << block_no);
    assoofs_save_sb_info(sb);
    return 0;
}

/*
 *  Función auxiliar: obtener un inodo a partir de su número
 */
static struct inode *assoofs_get_inode(struct super_block *sb, int ino) {
    struct assoofs_inode_info *inode_info;
    struct inode *inode;
    struct timespec64 ts;

    printk(KERN_INFO "assoofs_get_inode\n");

    inode_info = assoofs_get_inode_info(sb, ino);

    inode = new_inode(sb);
    inode->i_ino = ino;
    inode->i_sb = sb;
    inode->i_op = &assoofs_inode_ops;

    if (S_ISDIR(inode_info->mode))
        inode->i_fop = &assoofs_dir_operations;
    else if (S_ISREG(inode_info->mode))
        inode->i_fop = &assoofs_file_operations;
    else
        printk(KERN_ERR "Unknown inode type\n");

    ts = current_time(inode);
    inode_set_ctime(inode, ts.tv_sec, ts.tv_nsec);
    inode_set_mtime(inode, ts.tv_sec, ts.tv_nsec);
    inode_set_atime(inode, ts.tv_sec, ts.tv_nsec);

    inode->i_private = inode_info;
    return inode;
}

/*
 *  Función auxiliar: obtener información persistente de un inodo
 */
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no) {
    struct assoofs_inode_info *inode_info = NULL;
    struct buffer_head *bh;
    struct assoofs_super_block_info *afs_sb;
    struct assoofs_inode_info *buffer = NULL;
    int i;

    printk(KERN_INFO "assoofs_get_inode_info\n");

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;

    afs_sb = sb->s_fs_info;
    for (i = 0; i < afs_sb->inodes_count; i++) {
        if (inode_info->inode_no == inode_no) {
            buffer = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
            memcpy(buffer, inode_info, sizeof(*buffer));
            break;
        }
        inode_info++;
    }

    brelse(bh);
    return buffer;
}

int assoofs_sb_get_a_freeinode(struct super_block *sb, unsigned long *inode) {
    struct assoofs_super_block_info *assoofs_sb;
    int i;

    printk(KERN_INFO "assoofs_sb_get_a_freeinode request\n");

    assoofs_sb = sb->s_fs_info;

    for (i = 1; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++) {
        if (~(assoofs_sb->free_inodes) & (1 << i))
            break;
    }

    if (i == ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED) {
        printk(KERN_ERR "No free inodes available\n");
        return -ENOSPC;
    }

    *inode = i;
    assoofs_sb->free_inodes |= (1 << i);
    assoofs_save_sb_info(sb);
    return 0;
}

void assoofs_save_sb_info(struct super_block *vsb) {
    struct buffer_head *bh;
    struct assoofs_super_block_info *sb;

    printk(KERN_INFO "assoofs_save_sb_info request\n");

    sb = vsb->s_fs_info;
    bh = sb_bread(vsb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
    memcpy(bh->b_data, sb, sizeof(struct assoofs_super_block_info));
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
}

int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block) {
    struct assoofs_super_block_info *assoofs_sb;
    int i;

    printk(KERN_INFO "assoofs_sb_get_a_freeblock request\n");

    assoofs_sb = sb->s_fs_info;

    for (i = 2; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++) {
        if (~(assoofs_sb->free_blocks) & (1 << i))
            break;
    }

    if (i == ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED) {
        printk(KERN_ERR "No free blocks available\n");
        return -ENOSPC;
    }

    *block = i;
    assoofs_sb->free_blocks |= (1 << i);
    assoofs_save_sb_info(sb);
    return 0;
}

void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode) {
    struct assoofs_super_block_info *assoofs_sb;
    struct buffer_head *bh;
    struct assoofs_inode_info *inode_info;

    printk(KERN_INFO "assoofs_add_inode_info request\n");

    assoofs_sb = sb->s_fs_info;
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;
    inode_info += assoofs_sb->inodes_count;
    memcpy(inode_info, inode, sizeof(struct assoofs_inode_info));
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    if (assoofs_sb->inodes_count <= inode->inode_no) {
        assoofs_sb->inodes_count++;
        assoofs_save_sb_info(sb);
    }
}

struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search) {
    uint64_t count = 0;

    printk(KERN_INFO "assoofs_search_inode_info request\n");

    while (start->inode_no != search->inode_no && count < ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count) {
        count++;
        start++;
    }

    if (start->inode_no == search->inode_no)
        return start;
    else
        return NULL;
}

int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info) {
    struct buffer_head *bh;
    struct assoofs_inode_info *inode_pos;

    printk(KERN_INFO "assoofs_save_inode_info request\n");

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_pos = assoofs_search_inode_info(sb, (struct assoofs_inode_info *)bh->b_data, inode_info);

    if (!inode_pos) {
        brelse(bh);
        printk(KERN_ERR "The inode was not found in the inode store\n");
        return -EIO;
    }

    memcpy(inode_pos, inode_info, sizeof(*inode_pos));
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}

/*
 *  Inicialización del superbloque
 */
int assoofs_fill_super(struct super_block *sb, void *data, int silent) {
    struct buffer_head *contenido_bloque_0;
    struct assoofs_super_block_info *assoofs_sb;
    struct inode *root_inode;
    struct timespec64 ts;

    printk(KERN_INFO "assoofs_fill_super request\n");

    contenido_bloque_0 = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
    assoofs_sb = (struct assoofs_super_block_info *)contenido_bloque_0->b_data;

    if (assoofs_sb->magic != ASSOOFS_MAGIC || assoofs_sb->block_size != ASSOOFS_DEFAULT_BLOCK_SIZE) {
        brelse(contenido_bloque_0);
        return -EINVAL;
    }

    sb->s_magic = ASSOOFS_MAGIC;
    sb->s_maxbytes = ASSOOFS_DEFAULT_BLOCK_SIZE;
    sb->s_op = &assoofs_sops;
    sb->s_fs_info = assoofs_sb;
    brelse(contenido_bloque_0);

    root_inode = new_inode(sb);
    inode_init_owner(&nop_mnt_idmap, root_inode, NULL, S_IFDIR);
    root_inode->i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER;
    root_inode->i_sb = sb;
    root_inode->i_op = &assoofs_inode_ops;
    root_inode->i_fop = &assoofs_dir_operations;
    ts = current_time(root_inode);
    inode_set_ctime(root_inode, ts.tv_sec, ts.tv_nsec);
    inode_set_mtime(root_inode, ts.tv_sec, ts.tv_nsec);
    inode_set_atime(root_inode, ts.tv_sec, ts.tv_nsec);
    root_inode->i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER);
    sb->s_root = d_make_root(root_inode);

    return 0;
}

/*
 *  Montaje de dispositivos assoofs
 */
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
    struct dentry *ret;
    printk(KERN_INFO "assoofs_mount request\n");
    ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);
    if (IS_ERR(ret))
        printk(KERN_ERR "Error mounting assoofs\n");
    return ret;
}

static int __init assoofs_init(void) {
    int ret;
    printk(KERN_INFO "assoofs_init request\n");
    ret = register_filesystem(&assoofs_type);
    return ret;
}

static void __exit assoofs_exit(void) {
    int ret;
    printk(KERN_INFO "assoofs_exit request\n");
    ret = unregister_filesystem(&assoofs_type);
}

module_init(assoofs_init);
module_exit(assoofs_exit);