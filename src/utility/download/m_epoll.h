//
// Created by root on 4/7/26.
//

#ifndef MOCO_M_EPOLL_H
#define MOCO_M_EPOLL_H
#include "interface.h"

#include <bits/pthreadtypes.h>

typedef struct {
  int pipe_read_fd;
} EpollThreadArgs;

typedef enum {
  EPOLL_SIGNAL_RUN = 0,
  EPOLL_SIGNAL_EXIT = 1,
} EpollSignal;

extern int g_task_pipe_write_fd;
extern pthread_t epoll_download_thread_id;
extern volatile EpollSignal g_epoll_signal;

extern void init_epoll_download_task();
extern void wait_epoll_download_task();
extern void wait_download_task(const char *store);
extern void stop_epoll_download_task();
extern void submit_download_task(package *p);

#endif // MOCO_M_EPOLL_H
