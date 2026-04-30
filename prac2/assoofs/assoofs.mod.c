#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xa61fd7aa, "__check_object_size" },
	{ 0x092a35a2, "_copy_to_user" },
	{ 0xe6cb68a5, "kmem_cache_free" },
	{ 0x601411ad, "mount_bdev" },
	{ 0xacb0b2b2, "unregister_filesystem" },
	{ 0x1641838f, "kmem_cache_destroy" },
	{ 0xbd03ed67, "__ref_stack_chk_guard" },
	{ 0xb5ac1c73, "__kmem_cache_create_args" },
	{ 0xacb0b2b2, "register_filesystem" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0xd2444fe9, "kmem_cache_alloc_noprof" },
	{ 0x888b8f57, "strcmp" },
	{ 0xd74da6d4, "new_inode" },
	{ 0xcf16fe6f, "current_time" },
	{ 0x001e298b, "inode_set_ctime_to_ts" },
	{ 0x86c49e96, "nop_mnt_idmap" },
	{ 0x270e7dd2, "inode_init_owner" },
	{ 0x9428f51c, "d_add" },
	{ 0x12e6946c, "d_make_root" },
	{ 0x7a49c207, "mark_buffer_dirty" },
	{ 0x12ca377a, "sync_dirty_buffer" },
	{ 0x82fd7238, "__ubsan_handle_shift_out_of_bounds" },
	{ 0x357aaab3, "mutex_lock_interruptible" },
	{ 0xf46d5bf3, "mutex_unlock" },
	{ 0x092a35a2, "_copy_from_user" },
	{ 0x43a349ca, "strlen" },
	{ 0xa53f4e29, "memcpy" },
	{ 0xe54e0a6b, "__fortify_panic" },
	{ 0x937c98ef, "kill_block_super" },
	{ 0xd272d446, "__fentry__" },
	{ 0xe8213e80, "_printk" },
	{ 0x45c388de, "__bread_gfp" },
	{ 0x5a844b26, "__x86_indirect_thunk_rax" },
	{ 0x7a49c207, "__brelse" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0x546c19d9, "validate_usercopy_range" },
	{ 0xbebe66ff, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xa61fd7aa,
	0x092a35a2,
	0xe6cb68a5,
	0x601411ad,
	0xacb0b2b2,
	0x1641838f,
	0xbd03ed67,
	0xb5ac1c73,
	0xacb0b2b2,
	0xd272d446,
	0xd2444fe9,
	0x888b8f57,
	0xd74da6d4,
	0xcf16fe6f,
	0x001e298b,
	0x86c49e96,
	0x270e7dd2,
	0x9428f51c,
	0x12e6946c,
	0x7a49c207,
	0x12ca377a,
	0x82fd7238,
	0x357aaab3,
	0xf46d5bf3,
	0x092a35a2,
	0x43a349ca,
	0xa53f4e29,
	0xe54e0a6b,
	0x937c98ef,
	0xd272d446,
	0xe8213e80,
	0x45c388de,
	0x5a844b26,
	0x7a49c207,
	0xd272d446,
	0x546c19d9,
	0xbebe66ff,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"__check_object_size\0"
	"_copy_to_user\0"
	"kmem_cache_free\0"
	"mount_bdev\0"
	"unregister_filesystem\0"
	"kmem_cache_destroy\0"
	"__ref_stack_chk_guard\0"
	"__kmem_cache_create_args\0"
	"register_filesystem\0"
	"__stack_chk_fail\0"
	"kmem_cache_alloc_noprof\0"
	"strcmp\0"
	"new_inode\0"
	"current_time\0"
	"inode_set_ctime_to_ts\0"
	"nop_mnt_idmap\0"
	"inode_init_owner\0"
	"d_add\0"
	"d_make_root\0"
	"mark_buffer_dirty\0"
	"sync_dirty_buffer\0"
	"__ubsan_handle_shift_out_of_bounds\0"
	"mutex_lock_interruptible\0"
	"mutex_unlock\0"
	"_copy_from_user\0"
	"strlen\0"
	"memcpy\0"
	"__fortify_panic\0"
	"kill_block_super\0"
	"__fentry__\0"
	"_printk\0"
	"__bread_gfp\0"
	"__x86_indirect_thunk_rax\0"
	"__brelse\0"
	"__x86_return_thunk\0"
	"validate_usercopy_range\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "66CD005C76645FAB63B94E0");
