//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

//* Move to upper plave
static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if((type == T_FILE && ip->type == T_FILE) 
		    || type == T_SBLK) { 
      //* Return newly created inode - file and symbolic link
      return ip;
    }

    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

// Create the path new as a link to the same inode as old.
//* Link will now support symbolic link
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old, *flag;
  int length;
  struct inode *dp, *ip;

  if(argstr(0, &flag) < 0 || argstr(1, &old) < 0 || argstr(2, &new) < 0){
    cprintf("link: read argument failed\n");
    return -1;
  }

  begin_op();
  if(flag[0] == '-' && flag[1] == 'h'){
    //* Standard scheme: Hard link - share I-node
    if((ip = namei(old)) == 0){ //* namei: get inode for current old path.
      end_op();
      return -1;
    }

    ilock(ip);
    if(ip->type == T_DIR){ //* If current inode is directory
      iunlockput(ip);
      end_op();
      return -1;
    }

    ip->nlink++; //* Increase inode's linked number.
    iupdate(ip); //* iupdate() copy a modified in-memory inode to disk 
  	         //- called after every changes of ip->xxx
    iunlock(ip);

    if((dp = nameiparent(new, name)) == 0) //* nameiparent: get inode of the parent, and copy.
      goto bad;
    ilock(dp);
    if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
      iunlockput(dp);
      goto bad;
    }
    iunlockput(dp);
    iput(ip);
  }else if(flag[0] == '-' && flag[1] == 's'){
    //* Create Symbolic Link
    //* Implemeted based on sys_mkdir, sys_link.
    //* Strategy: 
    //*		- make a new inode with symbolic type.
    //*		- save path (old) information in inode - using writei to write data
    //*		- will refer saved path while operating in this inode.

    //* Step 1) create new inode in symbolic link type
    if((ip = create(new, T_SBLK, 0, 0)) == 0){ //* T_SBLK: symbolic type - need to be differentiated.
      end_op();
      cprintf("Error while creating symlink\n");
      return -1;
    } 
    //* old: will be path for current symbolic link.
    
    //* Step 2) Save path information in current inode.
    //* Required info: path length, path string
    /*
      --------
     |  path  |
     | length |
      --------
     |  path  |
     |(string)|
      --------
     */
    //* later used to refer current path and namei() it.
    length = strlen(old);
    //* Lock
    //ilock(ip);

    //* Write path length and path info
    writei(ip, (char*)&length, 0, sizeof(int));
    writei(ip, old, sizeof(int), length + 1); //* Save length information. +1 for null character '\0'

    //* Unlock
    iupdate(ip);
    iunlockput(ip);
  }
  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

/*
static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}*/

//* Modify Open - symbolic link
//*

struct inode*
followip(struct inode* ip, char* path){
  int length;
  // * Check if current inode is symbolic link
  // * if symbolic link, go to directing link and return it.
  
  //ilock(ip);

  if(ip->type == T_SBLK) {
    // * Current inode is symbolic link
    // * read path length, path information in inode.
    // * based on path information, reach inode until inode is non-symbolic link
    do {
      // * read first info: path length
      readi(ip, (char*)&length, 0, sizeof(int));

      // * read path info based on length info
      // * update path info
      readi(ip, path, sizeof(int), length + 1);
      iunlockput(ip);

      // * get inode for current path, then update ip.
      if((ip = namei(path)) == 0){
        // * Error: cannot find current inode
        // * Original inode could be deleted.
        cprintf("Error: Inode cannot found. Original file could be deleted or possible inode corruption occured.\n");
        //end_op();
        return 0;
      }
      ilock(ip); // * Lock again; readi.
      // * If newly updated ip is symbolic link,
      // * loop again until non-symbolic found. 
    } while(ip->type == T_SBLK);
  }

  return ip;
}


int
sys_open(void)
{
  char *path;
  int fd, omode, length;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && (omode != O_RDONLY && omode != O_NFSBLK)){ //* not readonly nor not follow
      iunlockput(ip);
      end_op();
      return -1;
    }
    if(ip->type == T_SBLK && (omode != O_NFSBLK)) { //* will not follow if this omode is O_NFSBLK (not follow symbolic link).
      // * Current inode is symbolic link
      // * read path length, path information in inode.
      // * based on path information, reach inode until inode is non-symbolic link.
      
      do {
        // * read first info: path length
        readi(ip, (char*)&length, 0, sizeof(int));

        // * read path info based on length info
        // * update path info
        readi(ip, path, sizeof(int), length + 1);
        iunlockput(ip);

        // * get inode for current path, then update ip.
        if((ip = namei(path)) == 0){
          // * Error: cannot find current inode
          // * Original inode could be deleted.
          cprintf("Error: Inode cannot found. Original file could be deleted or possible inode corruption occured.\n");
          end_op();
          return -1;
        }
	ilock(ip); // * Lock again; readi.
        // * If newly updated ip is symbolic link,
        // * loop again until non-symbolic found.
      } while(ip->type == T_SBLK);
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}
