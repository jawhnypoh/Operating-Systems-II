/*
 * Elevator sstf
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct sstf_data {
  struct list_head queue;

  // Add representations for the request direction and the head
  int direction;
  sector_t head;
};

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
  struct request *next) {
    list_del_init(&next->queuelist);
}

static int sstf_dispatch(struct request_queue *q, int force) {
  struct sstf_data *sd = q->elevator->elevator_data;
  printk("LOOK Algorithm: sstf_dispatch() is starting up the dispatch \n");

  if(!list_empty(&sd->queue)) {
    struct request *rq, *next_rq, *previous_rq;

    // Next and previous requests get the request that's greater/less than the current node
    next_rq = list_entry(sd->queue.next, struct request, queuelist);
    previous_rq = list_entry(sd->queue.prev, struct request, queuelist);

    // Evaluate the nodes in the list
    if(next_rq != previous_rq) {
      printk("sstf_dispatch(): There are multiple requests! \n");

      // Check the direction
      if(sd->direction == 0) {
        printk("sstf_dispatch: Moving backwards...\n ");

        // See where the next request is in relation to our current request
        if(sd->head > previous_rq->__sector) {
          // Request is further back
          rq = previous_rq;
        }
        else {
          // Otherwise, request is further forwards
          sd->direction = 1;
          rq = next_rq;
        }
      }
      else {
        printk("sstf_dispatch(): Moving forwards...\n");

        // See where the next request is in relation to our current request
        if(sd->head < next_rq->__sector) {
          // Request is further forwards
          rq = next_rq;
        }
        else {
          // Request is further back
          sd->direction = 0;
          rq = previous_rq;
        }
      }
    }
    else {
      // There's only one node in the list if next == previous
      printk("sstf_dispatch(): There's only one node! \n");
      rq = next_rq;
    }
    printk("sstf_dispatch() is running...\n");

    // Delete from the queue
    list_del_init(&rq->queuelist);

    // Get read head for new position
    sd->head = blk_rq_pos(rq) + blk_rq_sectors(rq);

    // Send the elevator request
    elv_dispatch_add_tail(q, rq);

    printk("sstf_dispatch() has finished running. \n");
    printk("sstf_dispatch(): SSTF reading: %llu\n", (unsigned long long) rq->__sector);
    return 1;
  }
  return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq) {
  struct sstf_data *sd = q->elevator->elevator_data;
  struct request *next_rq, *previous_rq;

  printk("LOOK Algorithm: sstf_add_request() is starting to add! \n");

  if(list_empty(&sd->queue)) {
    printk("sstf_add_request(): List is empty...\n");

    // Just add request because empty list
    list_add(&rq->queuelist, &sd->queue);
  }
  else {
    printk("sstf_add_request(): Searching for a place for the request...\n");

    // Find where the request could be placed into the request list
    next_rq = list_entry(sd->queue.next, struct request, queuelist);
    previous_rq = list_entry(sd->queue.prev, struct request, queuelist);

    // Iterate through the list and find exact spot to place
    while(blk_rq_pos(rq) > blk_rq_pos(next_rq)) {
      next_rq = list_entry(sd->queue.next, struct request, queuelist);
      previous_rq = list_entry(sd->queue.prev, struct request, queuelist);
    }

    // Add the request to the proper location in the list
    list_add(&rq->queuelist, &previous_rq->queuelist);
    printk("sstf_add_request(): Found the location! \n");
  }

  printk("LOOK Algorithm: sstf_add_request() - SSTF adding: %llu\n", (unsigned long long) rq->__sector);

}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq) {
  struct sstf_data *sd = q->elevator->elevator_data;

  if (rq->queuelist.prev == &sd->queue) {
    return NULL;
  }
  return list_prev_entry(rq, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq) {
  struct sstf_data *sd = q->elevator->elevator_data;

  if(rq->queuelist.next == &sd->queue) {
    return NULL;
  }
  return list_next_entry(rq, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e) {
  struct sstf_data *sd;
  struct elevator_queue *eq;

  eq = elevator_alloc(q, e);
  if(!eq) {
    return -ENOMEM;
  }

  sd = kmalloc_node(sizeof(*sd), GFP_KERNEL, q->node);
  if(!sd) {
    kobject_put(&eq->kobj);
    return -ENOMEM;
  }
  eq->elevator_data = sd;

  INIT_LIST_HEAD(&sd->queue);

  spin_lock_irq(q->queue_lock);

  return 0;
}

static void sstf_exit_queue(struct elevator_queue *e) {
  struct sstf_data *sd = e->elevator_data;

  BUG_ON(!list_empty(&sd->queue));
  kfree(sd);
}

static struct elevator_type elevator_sstf = {
  .ops.sq = {
    .elevator_merge_req_fn    = sstf_merged_requests,
    .elevator_dispatch_fn     = sstf_dispatch,
    .elevator_add_req_fn      = sstf_add_request,
    .elevator_former_req_fn   = sstf_former_request,
    .elevator_latter_req_fn   = sstf_latter_request,
    .elevator_init_fn         = sstf_init_queue,
    .elevator_exit_fn         = sstf_exit_queue,
  },
  .elevator_name = "look",
  .elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void) {
  return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void) {
  elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Johnny Po and Yeongae Lee");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO Scheduler");
