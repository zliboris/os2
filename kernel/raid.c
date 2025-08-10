#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"

static struct structura{
  int exists;
  int ver;
  enum RAID_TYPE raidt;
  int working[DISKS];
}Raid = {.exists = 0,.ver = 0};

static int attempted_load = 0;

void raid_make(enum RAID_TYPE raidt){
  Raid.exists = 1;
  Raid.ver++;
  Raid.raidt = raidt;
  for(int i=1 ; i <= DISKS ; i++) Raid.working[i-1] = 1;
  for(int i=1 ; i <= DISKS ; i++) write_block(i, 0, (uchar*) &Raid);
}

void raid_clear(){
  Raid.exists = 0;
  Raid.ver = 0;
  Raid.raidt = 0;
  for(int i=1 ; i <= DISKS ; i++)if(Raid.working[i - 1]) write_block(i, 0, (uchar*) &Raid);
  for(int i = 1; i <= DISKS; i++)Raid.working[i - 1] = 0;
}

void raid_set_working(int diskn, int working){
  Raid.working[diskn - 1] = working;
  Raid.ver++;
  for(int i=1 ; i <= DISKS ; i++)if(Raid.working[i - 1] == 1) write_block(i, 0, (uchar*) &Raid);
}

void raid_load(){
  struct structura* raiddata[DISKS];
  int newest_ver = 0, newest_ver_disk = 0;
  for(int i = 0; i < DISKS; i++){
    raiddata[i] = kalloc();
    read_block(i+1, 0,(uchar*) raiddata[i]);
  }
  for(int i = 0; i < DISKS; i++){
    if(!raiddata[i]->exists){
      for(int j = 0; j < DISKS; j++)kfree(raiddata[j]);
      return;
    }
    if(raiddata[i]->ver > newest_ver){
      newest_ver = raiddata[i]->ver;
      newest_ver_disk = i;
    }
  }

  Raid.exists = raiddata[newest_ver_disk]->exists;
  Raid.ver = raiddata[newest_ver_disk]->ver;
  Raid.raidt = raiddata[newest_ver_disk]->raidt;
  for(int i = 0; i < DISKS; i++) 
    Raid.working[i] = raiddata[newest_ver_disk]->working[i];

  for(int j = 0; j < DISKS; j++) kfree(raiddata[j]);
}

int init_raid(enum RAID_TYPE raid){

  if(attempted_load == 0){
    raid_load();
    attempted_load = 1;
  }

  if(Raid.exists == 1) return -1;

  switch(raid){

    case RAID0: 

      if(DISKS < 2) return -1;

      break;

    case RAID1: 

      if(DISKS < 2) return -1;

      break;

    case RAID0_1: 

      if(DISKS < 4) return -1;

      break;

    case RAID4: 

      if(DISKS < 3) return -1;

      break;

    case RAID5: 

      if(DISKS < 3) return -1;

      break;
  }

  raid_make(raid);

  return 0;
}

int read_with_check(int diskn, int blkn, uchar* data){
  if(!Raid.working[diskn - 1]){
    //printf("ne radi disk %d\n",diskn);
    return -1;}
  if(blkn + 1 >= DISK_SIZE / BSIZE) return -2;
  read_block(diskn, blkn + 1, data);
    //printf("radi disk %d\n",diskn);
  return 1;
}

uchar* calculate_lost_data(int blkn, int dead_disk, uchar* restored_data){
  uint64* data[DISKS];
  for(int i = 1; i <= DISKS ; i++){
    data[i-1] = kalloc();
    if(i == dead_disk) continue;
    read_with_check(i,blkn,(uchar*)data[i-1]);
  }
  for(int i = 0; i * 8 < BSIZE; i++)
  {
    data[dead_disk-1][i] = 0;
    for(int j = 1; j <= DISKS; j++)
    {
      if(j == dead_disk)continue;
      data[dead_disk-1][i] ^= data[j-1][i];
    }
  }
  
  uchar* rtn = (uchar*) data[dead_disk-1];
  for(int i = 1; i <= DISKS ; i++){
    if(i == dead_disk) continue;
    kfree(data[i-1]);
  }
  memmove(restored_data,rtn,BSIZE);
  kfree(rtn);

  return restored_data;
}

int read_raid(int blkn, uchar* data){

  if(attempted_load == 0){
    raid_load();
    attempted_load = 1;
  }

  if(!Raid.exists)return -1;

  int i = 1, br_disk, br_bloka;

  switch(Raid.raidt){

    case RAID0: 

      if(read_with_check(blkn % DISKS + 1, blkn / DISKS, data) == -1) return -1;

      break;

    case RAID1: 

      while(i <= DISKS && read_with_check(i++, blkn, data) == -1);
      if(i > DISKS) return -1;

      break;

    case RAID0_1: 

      if(read_with_check(blkn % (DISKS / 2) + 1, blkn / (DISKS / 2), data) == -1 && read_with_check(blkn % (DISKS / 2) + (DISKS / 2) + 1, blkn / (DISKS / 2), data) == -1) return -1;

      break;

    case RAID4: 

      br_disk = blkn % (DISKS - 1) + 1;
      br_bloka = blkn / (DISKS - 1); 

      if(-1 == read_with_check(br_disk, br_bloka, data)){
        for(int i=1; i <= DISKS; i++)
        {
          if(i == br_disk) continue;
          if(!Raid.working[i-1]) return -1;
        }
        calculate_lost_data(br_bloka, br_disk, data);
      }

      break;

    case RAID5: 

      br_disk = blkn % (DISKS - 1) + 1;
      br_bloka = blkn / (DISKS - 1); 

      if(br_disk >= DISKS - br_bloka % DISKS) br_disk++;

      if(-1 == read_with_check(br_disk, br_bloka, data)){
        for(int i=1; i < DISKS; i++)
        {
          if(i == br_disk) continue;
          if(!Raid.working[i-1]) return -1;
        }
        calculate_lost_data(br_bloka, br_disk, data);
      }

      break;
  }

  return 0;
}

int write_with_check(int diskn, int blkn, uchar* data){
  if(!Raid.working[diskn - 1]) return -1;
  if(blkn + 1 >= DISK_SIZE / BSIZE) return -2;
  write_block(diskn, blkn + 1, data);
  return 1;
}

int write_raid(int blkn, uchar* data){

  if(attempted_load == 0){
    raid_load();
    attempted_load = 1;
  }

  if(!Raid.exists)return -1;

  int i = 1, br_disk, br_bloka;
  int err;
  uint64* old_data, *new_data, *parity;

  switch(Raid.raidt){

    case RAID0: 

      if(-1 == write_with_check(blkn % DISKS + 1, blkn / DISKS, data)) return -1;

      break;

    case RAID1: 

      err = 1;
      for( i = 1; i <= DISKS; i++)if(1 == write_with_check(i, blkn, data))err = 0;

      if(err) return -1;

      break;

    case RAID0_1: 

      err = 1;
      if(1 == write_with_check(blkn % (DISKS / 2) + 1, blkn / (DISKS / 2), data)) err = 0;
      if(1 == write_with_check(blkn % (DISKS / 2) + (DISKS / 2) + 1, blkn / (DISKS / 2), data)) err = 0;

      if(err) return -1;

      break;

    case RAID4: 
      
      err = 1;

      br_disk = blkn % (DISKS - 1) + 1;
      br_bloka = blkn / (DISKS - 1); 

      old_data = kalloc(); 
      new_data = (uint64*) data;
      read_with_check(br_disk,br_bloka,(uchar*) old_data);

      if(1 == write_with_check(br_disk, br_bloka, data)) err = 0;

      if(err){
        kfree(old_data);
        return -1;
      }

      parity = kalloc();
      if(-1 == read_with_check(DISKS ,br_bloka,(uchar*)parity))err = 1;

      if(err){
        kfree(old_data);
        kfree(parity);
        return 0;
      }

      for( int j = 0; j * 8 < BSIZE; j++){
        parity[j] ^= old_data[j];
        parity[j] ^= new_data[j];
      }

      write_with_check(DISKS,br_bloka,(uchar*)parity);


      kfree(old_data);
      kfree(parity);

      break;

    case RAID5: 

      err = 1;

      br_disk = blkn % (DISKS - 1) + 1;
      br_bloka = blkn / (DISKS - 1); 

      if(br_disk >= DISKS - br_bloka % DISKS) br_disk++;

      old_data = kalloc();
      new_data = (uint64*) data;
      read_with_check(br_disk,br_bloka,(uchar*) old_data);

      if(1 == write_with_check(br_disk, br_bloka, data)) err = 0;

      if(err){
        kfree(old_data);
        return -1;
      }

      parity = kalloc();
      if(-1 == read_with_check(DISKS - br_bloka % DISKS ,br_bloka,(uchar*)parity))err = 1;

      if(err){
        kfree(old_data);
        kfree(parity);
        return 0;
      }

      for( int j = 0; j * 8 < BSIZE; j++){
        parity[j] ^= old_data[j];
        parity[j] ^= new_data[j];
      }

      write_with_check(DISKS - br_bloka % DISKS ,br_bloka,(uchar*)parity);

      kfree(old_data);
      kfree(parity);

      break;
  }

  return 0;
}
int disk_fail_raid(int diskn){

  if(attempted_load == 0){
    raid_load();
    attempted_load = 1;
  }

  raid_set_working(diskn,0);


  return 0;
}
int disk_repaired_raid(int diskn){

  if(attempted_load == 0){
    raid_load();
    attempted_load = 1;
  }

  if(!Raid.exists)return -1;

  int i = 1;

  if(Raid.working[diskn - 1])return 0;
  raid_set_working(diskn,1);

  switch(Raid.raidt){

    case RAID0: 

      break;

    case RAID1: 

      while( i == diskn || (i <= DISKS && !Raid.working[i-1]))i++;
      if(i > DISKS) return -1;
      uchar* temp = kalloc();
        
      for( int j = 0; j  < DISK_SIZE / BSIZE; j++){
        read_block(i,j,temp);
        write_block(diskn,j,temp);
      }

      kfree(temp);

      break;

    case RAID0_1: 

      if(diskn > DISKS / 2) i = diskn - DISKS / 2;
      else i = diskn + DISKS / 2;

      if(!Raid.working[i-1]) return -1;

      temp = kalloc();
        
      for( int j = 0; j  < DISK_SIZE / BSIZE; j++){
        read_block(i,j,temp);
        write_block(diskn,j,temp);
      }

      kfree(temp);

      break;

    case RAID4: 

      for(i = 1; i <= DISKS; i++){
        if(i == diskn) continue;
        if(!Raid.working[i-1]) return -1;
      }

      temp = kalloc();

      for( int j = 0; j  < (DISK_SIZE / BSIZE) - 1; j++){
        
        calculate_lost_data(j,diskn,temp);
        write_with_check(diskn,j,temp);
      }

      kfree(temp);

      break;

    case RAID5: 

      for(i = 1; i <= DISKS; i++){
        if(i == diskn) continue;
        if(!Raid.working[i-1]) return -1;
      }

      temp = kalloc();

      for( int j = 0; j  < (DISK_SIZE / BSIZE) - 1; j++){
        
        calculate_lost_data(j,diskn,temp);
        write_with_check(diskn,j,temp);
      }

      kfree(temp);

      break;
  }


  return 0;
}
int info_raid(uint *blkn, uint *blks, uint *diskn){

  *blkn = 0;
  *blks = 0;
  *diskn = 0;
  if(attempted_load == 0){
    raid_load();
    attempted_load = 1;
  }

  if(!Raid.exists)return -1;

  switch(Raid.raidt){
    case RAID0: 

      *blkn = ((DISK_SIZE / BSIZE) - 1) * DISKS;

      break;

    case RAID1: 

      *blkn = (DISK_SIZE / BSIZE) - 1;

      break;

    case RAID0_1: 

      *blkn = ((DISK_SIZE / BSIZE) - 1) * (DISKS / 2);

      break;

    case RAID4: 

      *blkn = ((DISK_SIZE / BSIZE) - 1) * (DISKS - 1);

      break;

    case RAID5: 

      *blkn = ((DISK_SIZE / BSIZE) - 1) * (DISKS - 1);

      break;
  }

  *blks = BSIZE;
  

  if(Raid.raidt == RAID0_1)*diskn = DISKS - (DISKS % 2);
  else *diskn = DISKS;

  return 0;
}
int destroy_raid(){

  if(attempted_load == 0){
    raid_load();
    attempted_load = 1;
  }

  if(!Raid.exists)return -1;

  raid_clear();

  uint64* nule = kalloc();

  for(int i = 0; i * 8 < BSIZE; i++)nule[i] = 0;

  for( int i = 1; i <= DISKS; i++) write_block(i,0,(uchar*)nule);

  kfree(nule);

  return 0;
}
