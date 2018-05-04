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

  // TODO: Add representations for the request direction and the head
}

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
  struct request *next) {
    list_del_init(&next->queuelist);
}

static int sstf_dispatch(struct request_queue *q, int force) {
  struct sstf_data *sd = q->elevator->elevator_data;
  printk("LOOK Algorithm: sstf_dispatch() is starting up the dispatch \n");

  // TODO: Sort and handle the requests
  if(!list_empty(&sd->queue)){

    struct request *rq;

    rq = list_entry(sd->enqueue.next, struct request, queuelist);
    list_del_init(&rq->queuelist);
    elv_dispatch_sort(q, rq);

    return 1;
  }
  return 0; 
}

static void sstf_add_request(struct request_queue *q, struct request *rq) {
  struct sstf_data *sd = q->elevator->elevator_data;
  struct request *next_rq; 
  struct request *previous_rq;

  // TODO: Finish up add function 

  if(list_empty(&sd->queue)){
    //list is empty, simply add new queue
    list_add(rq->rueuelist, &sd->queue);
  }
  else{
    struct list_head* head;
    list_for_each(head, &sd->queue){




    }
  }
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
  .elevator_name = "sstf"
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


MODULE_AUTHOR("Johnny Po");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO Scheduler");
