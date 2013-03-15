/*
 *
 *	Linux open operation: from userspace to driver.
 *
 */
 
 // Part One
 // 0. system call entrance [@fs/open.c]
 SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, int, mode)
{
	long ret;

	if (force_o_largefile())
		flags |= O_LARGEFILE;
	//** this is the main operation
	ret = do_sys_open(AT_FDCWD, filename, flags, mode);
	/* avoid REGPARM breakage on x86: */
	asmlinkage_protect(3, ret, filename, flags, mode);
	return ret;
}

// 1. do_sys_open [@fs/open.c]
long do_sys_open(int dfd, const char __user *filename, int flags, int mode)
{
	char *tmp = getname(filename);
	int fd = PTR_ERR(tmp);

	if (!IS_ERR(tmp)) {
		fd = get_unused_fd_flags(flags);
		if (fd >= 0) {
			//** this is the main operation
			struct file *f = do_filp_open(dfd, tmp, flags, mode, 0);
			if (IS_ERR(f)) {
				put_unused_fd(fd);
				fd = PTR_ERR(f);
			} else {
				fsnotify_open(f->f_path.dentry);
				fd_install(fd, f);
			}
		}
		putname(tmp);
	}
	return fd;
}

// 2. do_filp_open
struct file *do_filp_open(int dfd, const char *pathname,
		int open_flag, int mode, int acc_mode)
{
	struct file *filp;
	struct nameidata nd;
	// ......
	filp = get_empty_filp();
	if (filp == NULL)
		goto exit_parent;
	nd.intent.open.file = filp;
	// ......
	filp = nameidata_to_filp(nd);
	// ......
}

// 3. nameidata_to_filp
struct file *nameidata_to_filp(struct nameidata *nd)
{
	const struct cred *cred = current_cred();
	struct file *filp;

	/* Pick up the filp from the open intent */
	filp = nd->intent.open.file;
	/* Has the filesystem initialised the file for us? */
	if (filp->f_path.dentry == NULL)
		// this is the main operation
		filp = __dentry_open(nd->path.dentry, nd->path.mnt, filp, NULL, cred);
	else
		path_put(&nd->path);
	return filp;
}

// 4. __dentry_open
static struct file *__dentry_open(struct dentry *dentry, struct vfsmount *mnt,
					struct file *f,
					int (*open)(struct inode *, struct file *),
					const struct cred *cred)
{
	struct inode *inode;
	int error;

	f->f_mode = OPEN_FMODE(f->f_flags) | FMODE_LSEEK |	FMODE_PREAD | FMODE_PWRITE;
	inode = dentry->d_inode;
	// ......
	f->f_mapping = inode->i_mapping;
	f->f_path.dentry = dentry;
	f->f_path.mnt = mnt;
	f->f_pos = 0;
	//** this is the main operation
	f->f_op = fops_get(inode->i_fop);
	file_move(f, &inode->i_sb->s_files);
	// ......
	//** this is the main operation
	if (!open && f->f_op)
		open = f->f_op->open;
	if (open) {
		error = open(inode, f);
	}
}

//********** 现在我们知道： struct file中的 f->f_op就是struct inode中的inode->i_fop，阶段性胜利！ **************//
// Part Two
// 下面来看inode->i_fop又是如何关联到driver中的内容的

// 0. init_special_inode , i_rdev 表示设备号 [@fs/inode.c]
// 手动执行命令mknode时会调用到mknode函数(比如ext3_mknode())，然后调用到init_special_inode()函数
void init_special_inode(struct inode *inode, umode_t mode, dev_t rdev)
{
	inode->i_mode = mode;
	if (S_ISCHR(mode)) {
		inode->i_fop = &def_chr_fops;
		inode->i_rdev = rdev;
	} else if (S_ISBLK(mode)) {
		inode->i_fop = &def_blk_fops;
		inode->i_rdev = rdev;
	} else if (S_ISFIFO(mode))
		inode->i_fop = &def_fifo_fops;
	else if (S_ISSOCK(mode))
		inode->i_fop = &bad_sock_fops;
	else
		printk(KERN_DEBUG "init_special_inode: bogus i_mode (%o) for"
				  " inode %s:%lu\n", mode, inode->i_sb->s_id,
				  inode->i_ino);
}

// def_chr_fops [@fs/char_dev.c]
const struct file_operations def_chr_fops = {
	.open = chrdev_open,
};

// chrdev_open
/*
 * Called every time a character special file is opened
 */
static int chrdev_open(struct inode *inode, struct file *filp)
{
	struct cdev *p;
	struct cdev *new = NULL;
	int ret = 0;

	spin_lock(&cdev_lock);
	p = inode->i_cdev;
	if (!p) {
		struct kobject *kobj;
		int idx;
		spin_unlock(&cdev_lock);
		kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);
		if (!kobj)
			return -ENXIO;
		// new即是我们的driver结构对应的struct cdev结构
		new = container_of(kobj, struct cdev, kobj);
		spin_lock(&cdev_lock);
		/* Check i_cdev again in case somebody beat us to it while
		   we dropped the lock. */
		p = inode->i_cdev;
		// 第一次打开时，p==NULL
		if (!p) {
			inode->i_cdev = p = new;
			list_add(&inode->i_devices, &p->list);
			new = NULL;
		} else if (!cdev_get(p))
			ret = -ENXIO;
	} else if (!cdev_get(p))
		ret = -ENXIO;
	spin_unlock(&cdev_lock);
	cdev_put(new);
	if (ret)
		return ret;

	ret = -ENXIO;
	// 现在将我们的driver结构对应的struct cdev结构中的fops赋值给filp->f_op
	filp->f_op = fops_get(p->ops);
	if (!filp->f_op)
		goto out_cdev_put;

	// 如果我们实现open()的话，就调用它，注意open的参数哦，尤其是那个filp.
	if (filp->f_op->open) {
		ret = filp->f_op->open(inode,filp);
		if (ret)
			goto out_cdev_put;
	}

	return 0;

 out_cdev_put:
	cdev_put(p);
	return ret;
}

// 通过两部分上下结合，我们就知道：驱动中的file operations是如何赋值到struct file的f_op中去了。
