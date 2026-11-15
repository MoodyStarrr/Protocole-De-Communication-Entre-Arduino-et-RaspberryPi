#ifndef QUEUE_H
#define QUEUE_H

#include "protocol.h"
#define SIZE_MAX_QUEUE 10

/* Here for now 
typedef struct {
  bool frame_ready;
  pthread_cond_t ready;
  pthread_mutex_t mutex;
  frame_t copied_message;
}framer_thread_state_t;
*/

typedef struct {
  uint8_t head,tail,count;
  frame_t array[SIZE_MAX_QUEUE];
}queue_buffer_t;

typedef enum {
  QUEUE_FULL,
  QUEUE_EMPTY,
  QUEUE_AVAILABLE,
}queue_status_t;

void queue_init(queue_buffer_t * queue);
queue_status_t queue_put(queue_buffer_t * queue, frame_t * received);
queue_status_t queue_get(queue_buffer_t * queue, frame_t * to_send);
#endif
