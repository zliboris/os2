#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"

struct sleeplock raid_lock;
static int lock_init = 0;


uint64 sys_init_raid(void){
  int raidtype;
  argint(0,&raidtype);
  if(raidtype >= RAID0 && raidtype <= RAID5){
    if( lock_init ==  0 ){lock_init = 1;initsleeplock(&raid_lock,"Kljuc");}
    acquiresleep(&raid_lock);
    uint64 ret = (uint64) init_raid( (enum RAID_TYPE) raidtype);
    releasesleep(&raid_lock);
    return  ret;
  }
  return (uint64) -1;
}

uint64 sys_read_raid(void){
  if( lock_init ==  0 ){lock_init = 1;initsleeplock(&raid_lock,"Kljuc");}
  acquiresleep(&raid_lock);
  int blkn;
  uint64 data;
  argint(0,&blkn);
  argaddr(1,&data);

  uint blocks,disks,size;
  if(info_raid(&blocks,&size,&disks) == -1 || blkn >= blocks){releasesleep(&raid_lock); return -1;}


  uchar* datap = kalloc();
  int rtn = read_raid(blkn, datap);
  struct proc *p = myproc();
  if(copyout(p->pagetable, data, (char*)datap, BSIZE) < 0){
    kfree(datap);
    releasesleep(&raid_lock);
    return -1;
  }
  kfree(datap);
  releasesleep(&raid_lock);
  return rtn;
}

uint64 sys_write_raid(void){
  if( lock_init ==  0 ){lock_init = 1;initsleeplock(&raid_lock,"Kljuc");}
  acquiresleep(&raid_lock);
  int blkn;
  uint64 data;
  argint(0,&blkn);
  argaddr(1,&data);

  uint blocks,disks,size;
  if(info_raid(&blocks,&size,&disks) == -1 || blkn >= blocks){
    releasesleep(&raid_lock);
    return -1;}

  uchar* datap = kalloc();
  struct proc *p = myproc();
  if(copyin(p->pagetable, (char*) datap, data, BSIZE) < 0){
    kfree(datap);
    releasesleep(&raid_lock);
    return -1;
  }
  int rtn = write_raid(blkn, datap);
  kfree(datap);
  releasesleep(&raid_lock);
  return rtn;
}

uint64 sys_disk_fail_raid(void){
  if( lock_init ==  0 ){lock_init = 1;initsleeplock(&raid_lock,"Kljuc");}
  acquiresleep(&raid_lock);
  int diskn;
  argint(0,&diskn);
  uint64 ret = (uint64) disk_fail_raid(diskn);
  releasesleep(&raid_lock);
  return ret;
}

uint64 sys_disk_repaired_raid(void){
  if( lock_init ==  0 ){lock_init = 1;initsleeplock(&raid_lock,"Kljuc");}
  acquiresleep(&raid_lock);
  int diskn;
  argint(0,&diskn);
  uint64 ret = (uint64) disk_repaired_raid(diskn);
  releasesleep(&raid_lock);
  return ret;
}

uint64 sys_info_raid(void){
  if( lock_init ==  0 ){lock_init = 1;initsleeplock(&raid_lock,"Kljuc");}
  acquiresleep(&raid_lock);
  uint blkn, blks, diskn;
  int rtn = info_raid(&blkn, &blks, &diskn);
  uint64 blknp, blksp, disknp;
  argaddr(0, &blknp);
  argaddr(1, &blksp);
  argaddr(2, &disknp);
  struct proc *p = myproc();
  if(copyout(p->pagetable, blknp, (char*)&blkn, sizeof(blkn)) < 0 ||
     copyout(p->pagetable, blksp, (char*)&blks, sizeof(blks)) < 0 ||
     copyout(p->pagetable, disknp, (char*)&diskn, sizeof(diskn)) < 0){
    releasesleep(&raid_lock);
    return -1;}

  releasesleep(&raid_lock);
  return rtn;
}

uint64 sys_destroy_raid(void){
  if( lock_init ==  0 ){lock_init = 1;initsleeplock(&raid_lock,"Kljuc");}
  acquiresleep(&raid_lock);
  uint64 ret = (uint64) destroy_raid();
  releasesleep(&raid_lock);
  return ret;
}

