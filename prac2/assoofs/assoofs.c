#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include "assoofs.h"

MODULE_LICENSE("GPL");

/* Caché de inodos (Anexo B) */
static struct kmem_cache *assoofs_inode_cache;

/* Semáforos (Anexo C) */
static DEFINE_MUTEX(assoofs_sb_lock);
static DEFINE_MUTEX(assoofs_inodestore_lock);

/*
 *  Prototipos de funciones
 */
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);
int assoofs_fill_super(struct super_block *sb, void *data, int silent);
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no);
static struct inode *assoofs_get_inode(struct super_block *sb, int ino);
ssize_t assoofs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos);
ssize_t assoofs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos);
static int assoofs_iterate(struct file *filp, struct dir_context *ctx);
static int assoofs_create(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
struct dentry *assoofs_mkdir(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode);
static int assoofs_remove(struct inode *dir, struct dentry *dentry);
static int assoofs_move(struct mnt_idmap *idmap, struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry, unsigned int flags);
int assoofs_sb_get_a_freeinode(struct super_block *sb, unsigned long *inode);
void assoofs_save_sb_info(struct super_block *vsb);
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block);
void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode);
int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info);
struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search);
int assoofs_sb_set_a_freeinode(struct super_block *sb, unsigned long inode_no);
int assoofs_sb_set_a_freeblock(struct super_block *sb, uint64_t block_no);

/* Caché de inodos: función para destruir inodos (Anexo B) */
static void assoofs_destroy_inode(struct inode *inode) {
    struct assoofs_inode_info *inode_info = inode->i_private;
    printk(KERN_INFO "Freeing private data of inode %p (%lu)\n", inode_info, inode->i_ino);
    kmem_cache_free(assoofs_inode_cache, inode_info);
}

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
    .rename = assoofs_move,
};

/* Usando nuestra propia función de destrucción de inodos para la caché (Anexo B) */
static const struct super_operations assoofs_sops = {
    .destroy_inode = assoofs_destroy_inode,
};


/*
 *  Funciones que realizan operaciones sobre ficheros
 */

ssize_t assoofs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos) {
    printk(KERN_INFO "assoofs_read request\n");

    struct assoofs_inode_info *inode_info = filp->f_path.dentry->d_inode->i_private;
    struct buffer_head *bh;
    char *buffer;
    int nbytes;

    if (*ppos >= inode_info->file_size)
        return 0;

    bh = sb_bread(filp->f_path.dentry->d_inode->i_sb, inode_info->data_block_number);
    if (!bh)
        return -EIO;

    buffer = (char *)bh->b_data;
    buffer += *ppos;
    nbytes = min((size_t)(inode_info->file_size - *ppos), len);

    if (copy_to_user(buf, buffer, nbytes)) {
        brelse(bh);
        return -EFAULT;
    }

    *ppos += nbytes;
    brelse(bh);
    return nbytes;
}

ssize_t assoofs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos) {
    printk(KERN_INFO "assoofs_write request\n");

    struct assoofs_inode_info *inode_info = filp->f_path.dentry->d_inode->i_private;
    struct super_block *sb = filp->f_path.dentry->d_inode->i_sb;
    struct buffer_head *bh;
    char *buffer;

    if (*ppos + len >= ASSOOFS_DEFAULT_BLOCK_SIZE) {
        printk(KERN_ERR "No hay suficiente espacio en el disco para escribir.\n");
        return -ENOSPC;
    }

    bh = sb_bread(sb, inode_info->data_block_number);
    if (!bh)
        return -EIO;

    buffer = (char *)bh->b_data;
    buffer += *ppos;

    if (copy_from_user(buffer, buf, len)) {
        brelse(bh);
        return -EFAULT;
    }

    *ppos += len;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    inode_info->file_size = *ppos;
    assoofs_save_inode_info(sb, inode_info);
    return len;
}


/*
 *  Funciones que realizan operaciones sobre directorios
 */

static int assoofs_iterate(struct file *filp, struct dir_context *ctx) {
    printk(KERN_INFO "assoofs_iterate request\n");

    struct inode *inode;
    struct super_block *sb;
    struct assoofs_inode_info *inode_info;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    inode = filp->f_path.dentry->d_inode;
    sb = inode->i_sb;
    inode_info = inode->i_private;

    if (ctx->pos)
        return 0;

    if (!S_ISDIR(inode_info->mode))
        return -1;

    bh = sb_bread(sb, inode_info->data_block_number);
    record = (struct assoofs_dir_record_entry *)bh->b_data;

    for (i = 0; i < inode_info->dir_children_count; i++) {
        if (record->entry_removed == ASSOOFS_FALSE) {
            dir_emit(ctx, record->filename, ASSOOFS_FILENAME_MAXLEN, record->inode_no, DT_UNKNOWN);
            ctx->pos += sizeof(struct assoofs_dir_record_entry);
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
    printk(KERN_INFO "assoofs_lookup request\n");

    struct assoofs_inode_info *parent_info = parent_inode->i_private;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    bh = sb_bread(sb, parent_info->data_block_number);
    record = (struct assoofs_dir_record_entry *)bh->b_data;

    for (i = 0; i < parent_info->dir_children_count; i++) {
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
    printk(KERN_INFO "assoofs_create New file request\n");

    struct super_block *sb;
    struct inode *inode;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *dir_contents;
    struct timespec64 ts;

    sb = dir->i_sb;
    inode = new_inode(sb);
    inode->i_sb = sb;
    ts = current_time(inode);
    inode_set_ctime(inode, ts.tv_sec, ts.tv_nsec);
    inode_set_mtime(inode, ts.tv_sec, ts.tv_nsec);
    inode_set_atime(inode, ts.tv_sec, ts.tv_nsec);
    inode->i_op = &assoofs_inode_ops;
    assoofs_sb_get_a_freeinode(sb, &inode->i_ino);

    /* Caché de inodos: usar kmem_cache_alloc en lugar de kmalloc (Anexo B) */
    inode_info = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
    inode_info->inode_no = inode->i_ino;
    inode_info->mode = mode;
    inode_info->file_size = 0;
    inode->i_private = inode_info;
    inode->i_fop = &assoofs_file_operations;

    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    d_add(dentry, inode);

    assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
    assoofs_add_inode_info(sb, inode_info);

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
    printk(KERN_INFO "assoofs_mkdir New directory request\n");

    struct super_block *sb;
    struct inode *inode;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *dir_contents;
    struct timespec64 ts;

    sb = dir->i_sb;
    inode = new_inode(sb);
    inode->i_sb = sb;
    ts = current_time(inode);
    inode_set_ctime(inode, ts.tv_sec, ts.tv_nsec);
    inode_set_mtime(inode, ts.tv_sec, ts.tv_nsec);
    inode_set_atime(inode, ts.tv_sec, ts.tv_nsec);
    inode->i_op = &assoofs_inode_ops;
    assoofs_sb_get_a_freeinode(sb, &inode->i_ino);

    /* Caché de inodos: usar kmem_cache_alloc en lugar de kmalloc (Anexo B) */
    inode_info = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
    inode_info->inode_no = inode->i_ino;
    inode_info->mode = S_IFDIR | mode;
    inode_info->dir_children_count = 0;
    inode->i_private = inode_info;
    inode->i_fop = &assoofs_dir_operations;

    inode_init_owner(&nop_mnt_idmap, inode, dir, inode_info->mode);
    d_add(dentry, inode);

    assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);
    assoofs_add_inode_info(sb, inode_info);

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
    printk(KERN_INFO "assoofs_remove\n");

    struct super_block *sb = dir->i_sb;
    struct inode *inode_remove = dentry->d_inode;
    struct assoofs_inode_info *inode_info_remove = inode_remove->i_private;
    struct assoofs_inode_info *parent_inode_info = dir->i_private;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *dir_contents;
    int i;

    bh = sb_bread(sb, parent_inode_info->data_block_number);
    dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;

    for (i = 0; i < parent_inode_info->dir_children_count; i++) {
        if (!strcmp(dir_contents->filename, dentry->d_name.name) && dir_contents->inode_no == inode_remove->i_ino) {
            printk(KERN_INFO "Found dir_record_entry to remove: %s\n", dir_contents->filename);
            dir_contents->entry_removed = ASSOOFS_TRUE;
            break;
        }
        dir_contents++;
    }

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    assoofs_sb_set_a_freeinode(sb, inode_info_remove->inode_no);
    assoofs_sb_set_a_freeblock(sb, inode_info_remove->data_block_number);

    return 0;
}

/* Mover archivos (Anexo D) */
static int assoofs_move(struct mnt_idmap *idmap, struct inode *old_dir, struct dentry *old_dentry,
                        struct inode *new_dir, struct dentry *new_dentry, unsigned int flags) {
    printk(KERN_INFO "assoofs_move request\n");

    struct super_block *sb = old_dir->i_sb;
    struct inode *inode = d_inode(old_dentry);
    struct assoofs_inode_info *inode_info = inode->i_private;
    struct assoofs_inode_info *old_parent_info = old_dir->i_private;
    struct assoofs_inode_info *new_parent_info = new_dir->i_private;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    /* Actualizar el nombre en el directorio origen */
    bh = sb_bread(sb, old_parent_info->data_block_number);
    record = (struct assoofs_dir_record_entry *)bh->b_data;

    for (i = 0; i < old_parent_info->dir_children_count; i++) {
        if (record->inode_no == inode_info->inode_no) {
            record->entry_removed = ASSOOFS_TRUE;
            break;
        }
        record++;
    }

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    old_parent_info->dir_children_count--;
    assoofs_save_inode_info(sb, old_parent_info);

    /* Añadir entrada en el directorio destino */
    bh = sb_bread(sb, new_parent_info->data_block_number);
    record = (struct assoofs_dir_record_entry *)bh->b_data;
    record += new_parent_info->dir_children_count;
    record->inode_no = inode_info->inode_no;
    strcpy(record->filename, new_dentry->d_name.name);
    record->entry_removed = ASSOOFS_FALSE;

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    new_parent_info->dir_children_count++;
    assoofs_save_inode_info(sb, new_parent_info);

    return 0;
}

/*
 *  Auxiliares para liberar inodos y bloques
 */

int assoofs_sb_set_a_freeinode(struct super_block *sb, unsigned long inode_no) {
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    printk(KERN_INFO "assoofs_sb_set_a_freeinode request\n");
    printk(KERN_INFO "El inodo que eliminamos es: %lu\n", inode_no);
    assoofs_sb->free_inodes &= ~(1 << inode_no);
    assoofs_save_sb_info(sb);
    return 0;
}

int assoofs_sb_set_a_freeblock(struct super_block *sb, uint64_t block_no) {
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    printk(KERN_INFO "El bloque eliminamos es: %llu\n", block_no);
    assoofs_sb->free_blocks &= ~(1 << block_no);
    assoofs_save_sb_info(sb);
    return 0;
}

/*
 *  Función auxiliar: obtener un inodo a partir de su número
 */
static struct inode *assoofs_get_inode(struct super_block *sb, int ino) {
    printk(KERN_INFO "assoofs_get_inode\n");

    struct assoofs_inode_info *inode_info;
    struct inode *inode;
    struct timespec64 ts;

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
        printk(KERN_ERR "Unknown inode type. Neither a directory nor a file.\n");

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
    printk(KERN_INFO "assoofs_get_inode_info\n");

    struct assoofs_inode_info *inode_info = NULL;
    struct buffer_head *bh;
    struct assoofs_super_block_info *afs_sb;
    struct assoofs_inode_info *buffer = NULL;
    int i;

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;

    afs_sb = sb->s_fs_info;
    for (i = 0; i < afs_sb->inodes_count; i++) {
        if (inode_info->inode_no == inode_no) {
            /* Caché de inodos: usar kmem_cache_alloc en lugar de kmalloc (Anexo B) */
            buffer = kmem_cache_alloc(assoofs_inode_cache, GFP_KERNEL);
            memcpy(buffer, inode_info, sizeof(*buffer));
            break;
        }
        inode_info++;
    }

    brelse(bh);
    return buffer;
}

int assoofs_sb_get_a_freeinode(struct super_block *sb, unsigned long *inode) {
    printk(KERN_INFO "assoofs_sb_get_a_freeinode request\n");

    struct assoofs_super_block_info *assoofs_sb;
    int i;

    /* Semáforo: proteger acceso al superbloque (Anexo C) */
    mutex_lock_interruptible(&assoofs_sb_lock);

    assoofs_sb = sb->s_fs_info;

    for (i = 1; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++) {
        if (~(assoofs_sb->free_inodes) & (1 << i))
            break;
    }

    if (i == ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED) {
        mutex_unlock(&assoofs_sb_lock);
        printk(KERN_ERR "No free inodes available\n");
        return -ENOSPC;
    }

    printk(KERN_INFO "Free inode number %d is free\n", i);
    *inode = i;
    assoofs_sb->free_inodes |= (1 << i);
    assoofs_save_sb_info(sb);

    mutex_unlock(&assoofs_sb_lock);
    return 0;
}

void assoofs_save_sb_info(struct super_block *vsb) {
    printk(KERN_INFO "assoofs_save_sb_info request\n");

    struct buffer_head *bh;
    struct assoofs_super_block_info *sb = vsb->s_fs_info;

    bh = sb_bread(vsb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
    bh->b_data = (char *)sb;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
}

int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block) {
    printk(KERN_INFO "assoofs_sb_get_a_freeblock request\n");

    struct assoofs_super_block_info *assoofs_sb;
    int i;

    /* Semáforo: proteger acceso al superbloque (Anexo C) */
    mutex_lock_interruptible(&assoofs_sb_lock);

    assoofs_sb = sb->s_fs_info;

    for (i = 2; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++) {
        if (~(assoofs_sb->free_blocks) & (1 << i))
            break;
    }

    if (i == ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED) {
        mutex_unlock(&assoofs_sb_lock);
        printk(KERN_ERR "No free blocks available\n");
        return -ENOSPC;
    }

    printk(KERN_INFO "Free block number %d is free\n", i);
    *block = i;
    assoofs_sb->free_blocks |= (1 << i);
    assoofs_save_sb_info(sb);

    mutex_unlock(&assoofs_sb_lock);
    return 0;
}

void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode) {
    printk(KERN_INFO "assoofs_add_inode_info request\n");

    struct assoofs_super_block_info *assoofs_sb;
    struct buffer_head *bh;
    struct assoofs_inode_info *inode_info;

    /* Semáforo: proteger acceso al almacén de inodos (Anexo C) */
    mutex_lock_interruptible(&assoofs_inodestore_lock);

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

    mutex_unlock(&assoofs_inodestore_lock);
}

struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search) {
    printk(KERN_INFO "assoofs_search_inode_info request\n");

    uint64_t count = 0;

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
    printk(KERN_INFO "assoofs_save_inode_info request\n");

    struct buffer_head *bh;
    struct assoofs_inode_info *inode_pos;

    /* Semáforo: proteger acceso al almacén de inodos (Anexo C) */
    mutex_lock_interruptible(&assoofs_inodestore_lock);

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_pos = assoofs_search_inode_info(sb, (struct assoofs_inode_info *)bh->b_data, inode_info);

    if (!inode_pos) {
        brelse(bh);
        mutex_unlock(&assoofs_inodestore_lock);
        printk(KERN_ERR "The inode was not found in the inode store\n");
        return -EIO;
    }

    memcpy(inode_pos, inode_info, sizeof(*inode_pos));
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    mutex_unlock(&assoofs_inodestore_lock);
    return 0;
}

/*
 *  Inicialización del superbloque
 */
int assoofs_fill_super(struct super_block *sb, void *data, int silent) {
    printk(KERN_INFO "assoofs_fill_super request\n");

    struct buffer_head *contenido_bloque_0;
    struct assoofs_super_block_info *assoofs_sb;
    struct inode *root_inode;
    struct timespec64 ts;

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

    /* Caché de inodos: inicializar la caché (Anexo B) */
    assoofs_inode_cache = kmem_cache_create("assoofs_inode_cache",
        sizeof(struct assoofs_inode_info), 0,
        (SLAB_RECLAIM_ACCOUNT), NULL);
    if (!assoofs_inode_cache)
        return -ENOMEM;

    ret = register_filesystem(&assoofs_type);
    return ret;
}

static void __exit assoofs_exit(void) {
    int ret;
    printk(KERN_INFO "assoofs_exit request\n");

    ret = unregister_filesystem(&assoofs_type);

    /* Caché de inodos: destruir la caché (Anexo B) */
    kmem_cache_destroy(assoofs_inode_cache);
}

module_init(assoofs_init);
module_exit(assoofs_exit);