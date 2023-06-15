#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.i
  int flushing; //* wait until flushing ends - it has to be done without intervention.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
int sync(void);
int flush(void);
//static void commit();

void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
//* Changed feature in Project #3 - Buffered I/O
//* begin_op will do one more job; call sync() if buffer fulled.
//* Standard of full: LOGSIZE - 2 (Some blocks need to be reserved for log operation. I set this reserve amount as 2 blocks.)
//* Based on log number and # outstanding blocks, check if the log full.
//* If the log full, call sync(), flush the buffer. 
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){ 
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE - 2){ //* Reserve for 2 block for log operation
      // this op might exhaust log space; wait for commit.
      //sleep(&log, &log.lock);
      sync();
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
//* Changed feature in Project #3 - buffered I/O
//* Commit will NOT flush buffer.
//* Flush will be totally managed by systemcall sync();
//* end_op will now just resolve outstanding block; reduce the number if end_op called.
//* It will work like an 'marker'. (Mark the range of operation need to be logged.)
void
end_op(void)
{
  acquire(&log.lock);
  //* Resolve Outstanding Block
  log.outstanding -= 1;

  if (log.outstanding == 0){
    //* It won't call sync
    //* As all outstanding blocks are resloved,
    //* Wake the log wating the outstanding blocks to be resolved.
    wakeup(&log); //* This wakeup is for waiting log in sync();
  }

  //* for the case log.outstanding > 0
  //* begin_op() will now not sleep if the buffer is full.
  //* No need to wait such condition; begin_op will immediately flush the buffer.

  release(&log.lock);
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    brelse(from);
    brelse(to);
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n)
    log.lh.n++;
  b->flags |= B_DIRTY; // prevent eviction
  release(&log.lock);
}


//* sync()
//* sync will check the buffer and flush it.
// Step 1) If the lock isn't held (e.g. called from user program.) it will internally acquire log.lock.
// Step 2) If there is still outstanding block, wait until it resolved (wait until end_op calls)
// Step 3) Flush the buffer, write the changes on the disk
// Step 4) if the log.lock acquired internally, release it.
int
sync(void){
  int logLocked = log.lock.locked; //* check log lock state
  int blockConsumed = 0; //* Value need to be returned; # block flushed.

  if(logLocked == 0){ //* If the lock isn't held
    acquire(&log.lock); //* Call Internally
  }

  while(log.outstanding > 0){ //* If there is an outstanding block
    sleep(&log, &log.lock); //* Wait for it
  }

  blockConsumed = flush(); //* Flush buffers

  if(logLocked == 0){ //* If the lock called internally
    release(&log.lock);   //* release the lock.
  }

  return blockConsumed; //* Return # block flushed
}

//* flush()
//* flush will do the original commit job.
//* remove the all elements in buffer, and write those contents into disk.
int
flush(void){
  int flushed = 0;

  //* Prevent flush if currenlty commiting
  log.committing = 1;
  release(&log.lock); //* release the log lock
  log.flushing = 1; //* Flush start; Another sync() call will be ignored.

  if(log.lh.n > 0) { //* Original Commit Call
    //* Flush buffer.
    write_log();
    write_head();
    install_trans();
    flushed = log.lh.n;
    log.lh.n = 0;
    write_head();
  }

  acquire(&log.lock); //* acquire the log lock
  //* release & acquire are implemented in here based on original end_op call.
  //* without these calls, it will panic ("sched locks"), which is a deadlock in xv6.
  log.committing = 0;
  log.flushing = 0; //* Flush end; Now another sync() call will be allowed.

  return flushed;
}

int
sys_sync(){
  if(log.flushing == 1){
    //* Currently flushing buffers!
    cprintf("sync: busy!\n");
    return -1;
  }
  
  return sync();
}

