#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"


uint64 sys_init_raid(void){
  int raidtype;
  argint(0,&raidtype);
  if(raidtype >= RAID0 && raidtype <= RAID5){
    return (uint64) init_raid( (enum RAID_TYPE) raidtype);
  }
  return (uint64) -1;
}

uint64 sys_read_raid(void){
  int blkn;
  uint64 data;
  argint(0,&blkn);
  argaddr(1,&data);

  uint blocks,disks,size;
  if(info_raid(&blocks,&size,&disks) == -1 || blkn >= blocks)return -1;

  uchar* datap = kalloc();
  int rtn = read_raid(blkn, datap);
  struct proc *p = myproc();
  if(copyout(p->pagetable, data, (char*)datap, BSIZE) < 0){
    kfree(datap);
    return -1;
  }
  kfree(datap);
  return rtn;
}

uint64 sys_write_raid(void){
  int blkn;
  uint64 data;
  argint(0,&blkn);
  argaddr(1,&data);

  uint blocks,disks,size;
  if(info_raid(&blocks,&size,&disks) == -1 || blkn >= blocks)return -1;

  uchar* datap = kalloc();
  struct proc *p = myproc();
  if(copyin(p->pagetable, (char*) datap, data, BSIZE) < 0){
    kfree(datap);
    return -1;
  }
  int rtn = write_raid(blkn, datap);
  kfree(datap);
  return rtn;
}

uint64 sys_disk_fail_raid(void){
  int diskn;
  argint(0,&diskn);
  return (uint64) disk_fail_raid(diskn);
}

uint64 sys_disk_repaired_raid(void){
  int diskn;
  argint(0,&diskn);
  return (uint64) disk_repaired_raid(diskn);
}

uint64 sys_info_raid(void){
  uint blkn, blks, diskn;
  int rtn = info_raid(&blkn, &blks, &diskn);
  uint64 blknp, blksp, disknp;
  argaddr(0, &blknp);
  argaddr(1, &blksp);
  argaddr(2, &disknp);
  struct proc *p = myproc();
  if(copyout(p->pagetable, blknp, (char*)&blkn, sizeof(blkn)) < 0 ||
     copyout(p->pagetable, blksp, (char*)&blks, sizeof(blks)) < 0 ||
     copyout(p->pagetable, disknp, (char*)&diskn, sizeof(diskn)) < 0)
    return -1;

  return rtn;
}

uint64 sys_destroy_raid(void){
  destroy_raid();
  return 0;
}

