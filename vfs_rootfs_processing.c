// Part I. 注册rootfs文件系统.
// 0. init_rootfs function [@fs/ramfs/inode.c]
int __init init_rootfs(void)
{
  int err;

	err = bdi_init(&ramfs_backing_dev_info);
	if (err)
		return err;

	//*** this is the main operation.
	err = register_filesystem(&rootfs_fs_type);
	if (err)
		bdi_destroy(&ramfs_backing_dev_info);

	return err;
}

// 1. rootfs_fs_type function [@fs/ramfs/inode.c]
static struct file_system_type rootfs_fs_type = {
	.name		= "rootfs",
	.mount		= rootfs_mount,
	.kill_sb	= kill_litter_super,
};

// 2. register_filesystem function [@fs/filesystem.c]
int register_filesystem(struct file_system_type * fs)
{
	int res = 0;
	struct file_system_type ** p;

	BUG_ON(strchr(fs->name, '.')); 
	if (fs->next)
		return -EBUSY;
	write_lock(&file_systems_lock);
	//*** this is the main operation.
	p = find_filesystem(fs->name, strlen(fs->name));
	if (*p)
		res = -EBUSY;
	else
		*p = fs;
	write_unlock(&file_systems_lock);
	return res;
}

// 3. find_filesystem function [@fs/filesystem.c]
static struct file_system_type **find_filesystem(const char *name, unsigned len)
{
	struct file_system_type **p;
	//*** this is the main operation.
	for (p=&file_systems; *p; p=&(*p)->next)
		if (strlen((*p)->name) == len &&
		    strncmp((*p)->name, name, len) == 0)
			break;
	return p;
}
// process of registeration is over.


// Part II. 初始化"挂载树" -- rootfs
// 4. init_mount_tree function [@fs/namespace.c]
static void __init init_mount_tree(void)
{
	struct vfsmount *mnt;
	struct mnt_namespace *ns;
	struct path root;

	//*** this is the main operation.
	mnt = do_kern_mount("rootfs", 0, "rootfs", NULL);
	if (IS_ERR(mnt))
		panic("Can't create rootfs");

	//*** create & initialize namespace.
	ns = create_mnt_ns(mnt);
	if (IS_ERR(ns))
		panic("Can't allocate initial namespace");

	init_task.nsproxy->mnt_ns = ns;
	get_mnt_ns(ns);
	//*** end namespace

	//*** initialize process root directory(not "/") & pwd(present working directory)
	root.mnt = mnt;
	root.dentry = mnt->mnt_root;

	set_fs_pwd(current->fs, &root);
	set_fs_root(current->fs, &root);
	//*** end root & pwd initialization.
}

// 5. do_kern_mount function [@fs/namespace.c]
static struct vfsmount *
do_kern_mount(const char *fstype, int flags, const char *name, void *data)
{
	// 通过get_fs_type函数, 可以获得rootfs的file_system_type结构, 这些结构是
	// 在第一部分中设置的.
	struct file_system_type *type = get_fs_type(fstype);
	struct vfsmount *mnt;
	if (!type)
		return ERR_PTR(-ENODEV);
	//*** this is the main operation.
	mnt = vfs_kern_mount(type, flags, name, data);
	if (!IS_ERR(mnt) && (type->fs_flags & FS_HAS_SUBTYPE) &&
	    !mnt->mnt_sb->s_subtype)
		mnt = fs_set_subtype(mnt, fstype);
	//*** put_filesystem()会使用到type->owner变量, 但是rootfs的file_system_type结构体变量中,
	//*** owner变量为Null, 所以对于rootfs来说, put_filesystem()什么都没用做.
	put_filesystem(type);
	return mnt;
}

// 6. vfs_kern_mount function [@fs/namespace.c]
struct vfsmount *
vfs_kern_mount(struct file_system_type *type, int flags, const char *name, void *data)
{
	struct mount *mnt;
	struct dentry *root;

	if (!type)
		return ERR_PTR(-ENODEV);

	//*** 分配一个vfsmount结构体
	mnt = alloc_vfsmnt(name);
	if (!mnt)
		return ERR_PTR(-ENOMEM);

	if (flags & MS_KERNMOUNT)
		mnt->mnt.mnt_flags = MNT_INTERNAL;

	//*** this is the main operation.
	root = mount_fs(type, flags, name, data);
	if (IS_ERR(root)) {
		free_vfsmnt(mnt);
		return ERR_CAST(root);
	}

	//*** 初始化rootfs的vfsmount结构体
	mnt->mnt.mnt_root = root;
	mnt->mnt.mnt_sb = root->d_sb;
	mnt->mnt_mountpoint = mnt->mnt.mnt_root;
	mnt->mnt_parent = mnt;
	br_write_lock(&vfsmount_lock);
	list_add_tail(&mnt->mnt_instance, &root->d_sb->s_mounts);
	br_write_unlock(&vfsmount_lock);
	//*** end initializing vfsmount struct
	return &mnt->mnt;
}

// 7. mount_fs function [@fs/super.c]
struct dentry *
mount_fs(struct file_system_type *type, int flags, const char *name, void *data)
{
	struct dentry *root;
	struct super_block *sb;
	// ......
	//*** 这里的type就是rootfs的file_system_type结构体
	//*** type->mount()函数也就是rootfs_mount()函数
	root = type->mount(type, flags, name, data);
	sb = root->d_sb;
	sb->s_flags |= MS_BORN;
	// ......
	return root;
}

// 8. rootfs_mount function [@fs/ramfs/inode.c]
static struct dentry *rootfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags|MS_NOUSER, data, ramfs_fill_super);
}

// 9. mount_nodev [@fs/super.c]
struct dentry *mount_nodev(struct file_system_type *fs_type,
	int flags, void *data,
	int (*fill_super)(struct super_block *, void *, int))
{
	int error;
	//*** 分配super_block结构体, 并做部分初始化工作.
	struct super_block *s = sget(fs_type, NULL, set_anon_super, flags, NULL);
	//*** 通过rootfs_mount()函数的调用知道, fill_super()即是 ramfs_fill_super()函数.
	error = fill_super(s, data, flags & MS_SILENT ? 1 : 0);
	// ......
	s->s_flags |= MS_ACTIVE;
	return dget(s->s_root);
}

// 10. ramfs_fill_super function [@fs/ramfs/inode.c]
int ramfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct ramfs_fs_info *fsi;
	struct inode *inode;
	int err;

	save_mount_options(sb, data);

	//*** 分配ramfs_fs_info结构体, 并做部分初始化
	fsi = kzalloc(sizeof(struct ramfs_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi)
		return -ENOMEM;

	err = ramfs_parse_options(data, &fsi->mount_opts);
	if (err)
		return err;

	//*** 初始化super_block结构体
	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_CACHE_SIZE;
	sb->s_blocksize_bits	= PAGE_CACHE_SHIFT;
	sb->s_magic		= RAMFS_MAGIC;
	sb->s_op		= &ramfs_ops;	// 这个s_op是很重要的
	sb->s_time_gran		= 1;

	//*** 为根目录("/")分配inode结构体, 并初始化.
	inode = ramfs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	//*** 为根目录("/")创建dentry结构体, 并初始化.
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;
}

// 11.1 ramfs_get_inode function [@fs/ramfs/inode.c]
// 主要是初始化inode结构体, 其中有些有趣的函数, 如get_next_ino()函数
struct inode *ramfs_get_inode(struct super_block *sb,
				const struct inode *dir, umode_t mode, dev_t dev)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode_init_owner(inode, dir, mode);
		inode->i_mapping->a_ops = &ramfs_aops;
		inode->i_mapping->backing_dev_info = &ramfs_backing_dev_info;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		switch (mode & S_IFMT) {
		default:
			//*** 注意这个地方哦, block device, char device等设备的
			//*** inode->i_fop变量都是在这里设置的, 直接跟驱动对接, 很关键的一个函数.
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &ramfs_file_inode_operations;
			inode->i_fop = &ramfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &ramfs_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
			inc_nlink(inode);
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			break;
		}
	}
	return inode;
}

// 11.2 d_make_root function [@fs/dcache.c]
// 创建根目录("/")对应的dentry结构体
struct dentry *d_make_root(struct inode *root_inode)
{
	struct dentry *res = NULL;

	if (root_inode) {
		//*** yep! 根目录("/")终于出现啦！
		static const struct qstr name = QSTR_INIT("/", 1);

		//*** alloc dentry struct
		res = __d_alloc(root_inode->i_sb, &name);
		if (res)
			//*** this is the main operation.
			d_instantiate(res, root_inode);
		else
			iput(root_inode);
	}
	return res;
}

// 12. d_instantiate() function => __d_instantiate() function [@fs/dcache.c]
static void __d_instantiate(struct dentry *dentry, struct inode *inode)
{
	spin_lock(&dentry->d_lock);
	if (inode) {
		if (unlikely(IS_AUTOMOUNT(inode)))
			dentry->d_flags |= DCACHE_NEED_AUTOMOUNT;
		hlist_add_head(&dentry->d_alias, &inode->i_dentry);
	}
	//*** this is the main operation.
	dentry->d_inode = inode;
	dentry_rcuwalk_barrier(dentry);
	spin_unlock(&dentry->d_lock);
	fsnotify_d_instantiate(dentry, inode);
}

