/*
 * A sample, extra-simple block driver. Updated for kernel 2.6.31.
 *
 * (C) 2003 Eklektix, Inc.
 * (C) 2010 Pat Patterson <pat at superpat dot com>
 * Redistributable under the terms of the GNU GPL.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

MODULE_LICENSE("Dual BSD/GPL");
static char *Version = "1.4";

static int major_num = 0;
module_param(major_num, int, 0);
static int logical_block_size = 512;
module_param(logical_block_size, int, 0);
static int nsectors = 1024; /* How big the drive is */
module_param(nsectors, int, 0);

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512

/*
 * Our request queue.
 */
static struct request_queue *Queue;

/*
 * The internal representation of our device.
 */
static struct sbd_device {
	unsigned long size;
	spinlock_t lock;
	u8 *data;
	struct gendisk *gd;
} Device;

/* ***Note that the crypto key must be 16 characters long */
struct crypto_cipher *tfm;
static char *key = "1234567890123456";
module_param(key, charp, 0644);
static int keylen = 16;
module_param(keylen, int, 0644);

/*
 * Handle an I/O request.
 */
static void sbd_transfer(struct sbd_device *dev, sector_t sector,
		unsigned long nsect, char *buffer, int write) {
	unsigned long offset = sector * logical_block_size;
	unsigned long nbytes = nsect * logical_block_size;
  u8 *dest;
  u8 *source;

  // If write, else read 
  if(write) {
    printk("sbd.c :: sbd_transfer() | WRITE - Transferring Data \n");
  }
  else {
    printk("sbd.c :: sbd_transfer() | READ - Transferring Data \n");
  }
  if((offset + nbytes) > dev->size) {
    printk("sbd.c :: sbd_transfer() | OFFSET - Too far %ld NBYTES: %ld \n", offset, nbytes);
    return;
  }

  if(crypto_cipher_setkey(tfm, key, keylen) == 0) {
    printk("sbd.c :: sbd_transfer() | Crypto key is encrypted! \n");
  }
  else {
    printk("sbd.c :: sbd_transfer() | Crypto key was unable to be set. \n");
  }

  /*
   * Basically, transfer all the data (encryption/decryption) block by block until
   * a specified length is reached (n bytes)
   * If Writing: Transfer data from BLOCK to DEVICE
   * If Reading: Transfer data from DEVICE to BLOCK
   */
   if(write) {
     printk("sbd.c :: sbd_transfer() | Writing %lu bytes to device data \n", nbytes);

     dest = dev->data + offset;
     source = buffer;

     int i;
     for(i=0; i<nbytes; i+=crypto_cipher_blocksize(tfm)) {
       // Use the crypto_cipher handler and tfm to encrypt data block by block
       crypto_cipher_encrypt_one(
         tfm,                               // Cipher handler
         dev->data + offset + i,            // Destination
         buffer + i                         // Souce
       );
     }

     printk("sbd.c :: sbd_transfer() | UNENCRYPTED DATA: \n");
     for(i=0; i<100; i++) {
       printk("%u", (unsigned) *dest++);
     }
     printk("\n");

     printk("sbd.c :: sbd_transfer() | ENCRYPTED DATA: \n");
     for(i=0;i<100;i++) {
       printk("%u", (unsigned) *source++);
     }
     printk("\n");
   }
   else {
     printk("sbd.c :: sbd_transfer() | Reading %lu bytes to device data \n", nbytes);

     dest = dev->data + offset;
     source = buffer;

     int i;
     for(i=0; i<nbytes; i+=crypto_cipher_blocksize(tfm)) {
       // Use the crypto_cipher handler and tfm to decrypt data block by block
       crypto_cipher_decrypt_one(
         tfm,                               // Cipher handler
         dev->data + offset + i,            // Destination
         buffer + i                         // Souce
       );
     }

     printk("sbd.c :: sbd_transfer() | UNENCRYPTED DATA: \n");
     for(i=0; i<100; i++) {
       printk("%u", (unsigned) *dest++);
     }
     printk("\n");

     printk("sbd.c :: sbd_transfer() | ENCRYPTED DATA: \n");
     for(i=0;i<100;i++) {
       printk("%u", (unsigned) *source++);
     }
     printk("\n");
   }
   printk("sbd.c :: sbd_transfer() | Transfer and Encryption Complete! \n");
}

static void sbd_request(struct request_queue *q) {
	struct request *req;

	req = blk_fetch_request(q);
	while (req != NULL) {
		// blk_fs_request() was removed in 2.6.36 - many thanks to
		// Christian Paro for the heads up and fix...
		//if (!blk_fs_request(req)) {
		if (req == NULL || (req->cmd_type != REQ_TYPE_FS)) {
			printk (KERN_NOTICE "Skip non-CMD request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}
		sbd_transfer(
      &Device,
      blk_rq_pos(req),
      blk_rq_cur_sectors(req),
			req->buffer,
      rq_data_dir(req));

    printk("sbd.c :: sbd_request() | Request Data Transferred \n");

		if ( ! __blk_end_request_cur(req, 0) ) {
			req = blk_fetch_request(q);
		}

    printk("sbd.c :: sbd_request() | Requests Complete \n");
	}
}

/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), which
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int sbd_getgeo(struct block_device * block_device, struct hd_geometry * geo) {
	long size;

  printk("sbd.c :: sbd_getgeo() | Begin Partiioning \n");
	/* We have no real geometry, of course, so make something up. */
	size = Device.size * (logical_block_size / KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;

  printk("sbd.c :: sbd_getgeo() | Finished Partitioning \n");

	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations sbd_ops = {
		.owner  = THIS_MODULE,
		.getgeo = sbd_getgeo
};

static int __init sbd_init(void) {
  // File I/O
	mm_segment_t oldfs;
	struct file *filp = NULL;
	unsigned long long offset = 0;
	ssize_t size;

  printk("sbd.c :: sbd_init() | Begin Initialize \n");

  // Register block device
  major_num = register_blkdev(major_num, "sbd");

  // If device is busy...
  if(major_num < 0) {
    printk("sbd.c :: sbd_init() | Cannot register Block Device \n");
    return -EBUSY;
  }

  // Set up device and set to all 0
  memset(&Device, 0, sizeof(struct sbd_device));
	Device.size = nsectors * logical_block_size;
	Device.data = vmalloc(Device.size);

	memset(Device.data, 0, Device.size);

  // Check if device data is allocated
  if (Device.data == NULL) {
		printk("sbd.c :: sbd_init() | Cannot allocate Device.data\n");
		unregister_blkdev(major_num, "sbd");
		return -ENOMEM;
	}

  printk("sbd.c :: sbd_init() | Device size is %ld \n", Device.size);

  // Copy device data from a file, or create if does not exist
  oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open("/Data", O_RDONLY | O_CREAT, S_IRWXUGO);

  printk("sbd.c :: sbd_init() | Attempting to open /Data... \n");

  // Check for errors or NULL
  if(IS_ERR(filp)) {
    printk("sbd.c :: sbd_init() | Cannot open /Data \n");
    set_fs(oldfs);
  }
  else {
    // Read the file
    size = vfs_read(filp, Device.data, Device.size, &offset);
    printk("sbd.c :: sbd_init() | File output -- Size: %d, Offset: %llu \n", size, offset);

    // Close file
    set_fs(oldfs);
    filp_close(filp, 0);
    printk("sbd.c :: sbd_init() | File Close \n");
  }

  // Initialize spin lock
  spin_lock_init(&Device.lock);

  // Initialize queue and call sbd_request() when requets come in
  theQueue = blk_init_queue(sbd_request, &Device.lock);
  if (theQueue == NULL) {
    printk("sdb.c :: sdb_init() | Cannot initialize queue \n");
    unregister_blkdev(major_num, "sbd");
    vfree(Device.data);
    return -ENOMEM;
  }

  // Set logical_block_size for the queue
  blk_queue_logical_block_size(theQueue, logical_block_size);

  // Initialize gendisk
  Device.gd = alloc_disk(16);
	if (!Device.gd) {
		printk("sdb.c :: sdb_init() | Cannot allocate gendisk \n");
		unregister_blkdev(major_num, "sbd");
		vfree(Device.data);
		return -ENOMEM;
	}

  /*
   * Initialize crypto and set keys
   * crypto_alloc_cipher: crypto driver name, type, and mask
   */
   tfm = crypto_alloc_cipher("aes", 0, 0);

   if(IS_ERR(tfm)) {
     printk("sdb.c :: sdb_init() | Cannot allocate Cipher \n");
   }
   else {
     printk("sdb.c :: sdb_init() | Cipher allocated \n");
   }

   // Print statements for debugging purposes
   printk("sdb.c :: sdb_init() | Block Cipher Size - %u \n", crypto_cipher_blocksize(tfm));
   printk("sdb.c :: sdb_init() | Crypto Key - %s \n, key");
   printk("sdb.c :: sdb_init() | Length of Key - %d \n, keylen");

   // Add gendisk structure
   Device.gd->major = major_num;
	 Device.gd->first_minor = 0;
	 Device.gd->fops = &sbd_ops;
	 Device.gd->private_data = &Device;
	 strcpy(Device.gd->disk_name, "sbd0");
	 set_capacity(Device.gd, nsectors);
	 Device.gd->queue = theQueue;

   // Register partition in Device.gd with kernel
   add_disk(Device.gd);

   printk("sdb.c :: sdb_init() | Initialization successful! \n");

   return 0;
}

static void __exit sbd_exit(void) {
  struct file *filp = NULL;
  mm_segment_t oldfs;
	ssize_t size;
	unsigned long long offset = 0;

  printk("sdb.c :: sdb_exit() | Exiting... \n");

  // Write data to file first
  oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open("/Data", O_WRONLY | O_TRUNC | O_CREAT, S_IRWXUGO);

  if(IS_ERR(filp)) {
    printk("sdb.c :: sdb_exit() | Cannot open file \n");
    set_fs(oldfs);
  }
  else {
    printk("sdb.c :: sdb_exit() | File opened \n");

    // Write to the file
    size = vfs_write(filp, Device.data, Device.size, &offset);
    printk("sdb.c :: sdb_exit() | File Written :: %d Offset - %llu \n", size, offset);

    // Close file
    set_fs(oldfs);
		filp_close(filp, 0);
    printk("sdb.c :: sdb_exit() | File closed \n");
  }

	del_gendisk(Device.gd);
	put_disk(Device.gd);
	unregister_blkdev(major_num, "sbd");
	blk_cleanup_queue(Queue);
	vfree(Device.data);

  crypto_free_cipher(tfm);

  printk("sdb.c :: sdb_exit() | Exited Module \n");
}

module_init(sbd_init);
module_exit(sbd_exit);

MODULE_AUTHOR("Johnny Po");
MODULE_DESCRIPTION("Block Device Driver");
