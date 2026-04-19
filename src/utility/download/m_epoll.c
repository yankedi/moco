//
// Created by root on 4/7/26.
//

/*
 *
 *次代码从gemini学习而来未审查
 *
 *progress_init
 *progress_render_locked
 *progress_on_submit
 *progress_on_done
 *progress_on_retry
 *progress_on_link
 *progress_shutdown均为ai生成
 *
 *
 *TODO 重制进度条
 *
 */

#include "m_epoll.h"
#include "m_exit.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <sysexits.h>
#include <unistd.h>

#define MAX_EVENTS 64
#define M_MULTI_MAX_TOTAL_CONN 16L
#define M_MULTI_MAX_HOST_CONN 6L
#define M_MULTI_MAX_CACHE_CONN 32L

int g_task_pipe_write_fd = -1;
pthread_t epoll_download_thread_id;
volatile EpollSignal g_epoll_signal = EPOLL_SIGNAL_RUN;

typedef struct {
  int epoll_fd;
  int timer_fd;
  CURLM *multi;
  int running_handles;
  int active_transfers;
} EpollInfo;

typedef struct {
  CURL *easy;
  FILE *fp;
  char *tmp_path;
  EpollInfo *ep;
  package *p;
} ConnInfo;

typedef struct {
  int enabled;
  int rows;
  int cols;
  unsigned long total;
  unsigned long done;
  unsigned long linked;
  unsigned long retry;
} ProgressUI;

static ProgressUI g_progress = {0};
static pthread_mutex_t g_progress_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_pending_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_pending_cond = PTHREAD_COND_INITIALIZER;
static unsigned long g_pending_tasks = 0;

typedef struct DoneNode {
  char *store;
  struct DoneNode *next;
} DoneNode;

static DoneNode *g_done_head = NULL;

static void record_download_done(const char *store) {
  if (!store) {
    return;
  }
  DoneNode *node = malloc(sizeof(DoneNode));
  if (!node) {
    return;
  }
  node->store = m_strdup(store);
  pthread_mutex_lock(&g_pending_lock);
  node->next = g_done_head;
  g_done_head = node;
  pthread_cond_broadcast(&g_pending_cond);
  pthread_mutex_unlock(&g_pending_lock);
}

static int has_download_done_locked(const char *store) {
  for (DoneNode *node = g_done_head; node; node = node->next) {
    if (strcmp(node->store, store) == 0) {
      return 1;
    }
  }
  return 0;
}

static void clear_done_list(void) {
  pthread_mutex_lock(&g_pending_lock);
  DoneNode *node = g_done_head;
  g_done_head = NULL;
  pthread_mutex_unlock(&g_pending_lock);
  while (node) {
    DoneNode *next = node->next;
    free(node->store);
    free(node);
    node = next;
  }
}

static void pending_task_submitted(void) {
  pthread_mutex_lock(&g_pending_lock);
  g_pending_tasks++;
  pthread_mutex_unlock(&g_pending_lock);
}

static void pending_task_finished(void) {
  pthread_mutex_lock(&g_pending_lock);
  if (g_pending_tasks > 0) {
    g_pending_tasks--;
  }
  if (g_pending_tasks == 0) {
    pthread_cond_broadcast(&g_pending_cond);
  }
  pthread_mutex_unlock(&g_pending_lock);
}

static void free_conninfo(ConnInfo *conn) {
  if (!conn) {
    return;
  }
  if (conn->fp) {
    fclose(conn->fp);
    conn->fp = NULL;
  }
  if (conn->tmp_path) {
    free(conn->tmp_path);
    conn->tmp_path = NULL;
  }
  if (conn->p) {
    free_package(conn->p);
    conn->p = NULL;
  }
  free(conn);
}

static void progress_render_locked(const int running_handles) {
  if (!g_progress.enabled) {
    return;
  }

  const int total = (int)g_progress.total;
  const int done = (int)g_progress.done;
  const int linked = (int)g_progress.linked;
  const int retry = (int)g_progress.retry;
  const int cols = g_progress.cols > 0 ? g_progress.cols : 100;
  int bar_width = cols - 45;
  if (bar_width < 10)
    bar_width = 10;
  if (bar_width > 50)
    bar_width = 50;

  int filled = 0;
  double percent = 0.0;
  if (total > 0) {
    percent = ((double)done * 100.0) / (double)total;
    filled = (int)((double)done * (double)bar_width / (double)total);
    if (filled > bar_width)
      filled = bar_width;
  }

  char bar[64];
  for (int i = 0; i < bar_width; ++i) {
    bar[i] = i < filled ? '#' : '-';
  }
  bar[bar_width] = '\0';

  printf("\0337");
  printf("\033[%d;1H\033[2K", g_progress.rows);
  printf("\033[1;36m任务进度\033[0m [%s] %d/%d %5.1f%% run:%d link:%d retry:%d",
         bar, done, total, percent, running_handles, linked, retry);
  printf("\0338");
  fflush(stdout);
}

static void progress_init(void) {
  if (!isatty(STDOUT_FILENO)) {
    return;
  }

  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_row < 2) {
    return;
  }

  pthread_mutex_lock(&g_progress_lock);
  g_progress.enabled = 1;
  g_progress.rows = ws.ws_row;
  g_progress.cols = ws.ws_col;
  g_progress.total = 0;
  g_progress.done = 0;
  g_progress.linked = 0;
  g_progress.retry = 0;

  // Keep the last terminal line as a fixed status line.
  printf("\033[?25l");
  printf("\033[1;%dr", g_progress.rows - 1);
  printf("\033[%d;1H\033[2K", g_progress.rows);
  printf("\033[1;1H");
  progress_render_locked(0);
  pthread_mutex_unlock(&g_progress_lock);
  fflush(stdout);
}

static void progress_on_submit(void) {
  if (!g_progress.enabled) {
    progress_init();
  }
  pthread_mutex_lock(&g_progress_lock);
  g_progress.total++;
  progress_render_locked(0);
  pthread_mutex_unlock(&g_progress_lock);
}

static void progress_on_link(const int running_handles) {
  pthread_mutex_lock(&g_progress_lock);
  g_progress.linked++;
  progress_render_locked(running_handles);
  pthread_mutex_unlock(&g_progress_lock);
}

static void progress_on_retry(const int running_handles) {
  pthread_mutex_lock(&g_progress_lock);
  g_progress.retry++;
  progress_render_locked(running_handles);
  pthread_mutex_unlock(&g_progress_lock);
}

static void progress_on_done(const int running_handles) {
  pthread_mutex_lock(&g_progress_lock);
  g_progress.done++;
  progress_render_locked(running_handles);
  pthread_mutex_unlock(&g_progress_lock);
}

static void progress_shutdown(const int running_handles) {
  pthread_mutex_lock(&g_progress_lock);
  if (g_progress.enabled) {
    progress_render_locked(running_handles);
    printf("\033[r");
    printf("\033[%d;1H\033[2K", g_progress.rows);
    printf("\033[?25h");
    fflush(stdout);
  }
  g_progress.enabled = 0;
  pthread_mutex_unlock(&g_progress_lock);
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *data) {
  ConnInfo *conn = data;
  if (!conn->fp) {
    conn->fp = fopen(conn->tmp_path, "wb+");
    if (!conn->fp) {
      fprintf(stderr,
              "[网络调试] 打开临时文件失败: url=%s tmp=%s errno=%d(%s)\n",
              conn->p->url, conn->tmp_path, errno, strerror(errno));
      return 0;
    }
  }
  return fwrite(ptr, size, nmemb, conn->fp);
}

static int timer_cb(CURLM *multi, long timeout_ms, void *userp) {
  EpollInfo *ep = userp;
  struct itimerspec its;

  if (timeout_ms > 0) {
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = timeout_ms / 1000;
    its.it_value.tv_nsec = (timeout_ms % 1000) * 1000 * 1000;
  } else if (timeout_ms == 0) {
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1;
  } else {
    memset(&its, 0, sizeof(struct itimerspec));
  }

  timerfd_settime(ep->timer_fd, 0, &its, NULL);
  return 0;
}

void add_download(EpollInfo *ep, package *p) {
  ConnInfo *conn = calloc(1, sizeof(ConnInfo));
  if (!conn) {
    fprintf(stderr, "[下载任务] 分配 ConnInfo 失败\n");
    free_package(p);
    return;
  }
  conn->ep = ep;
  conn->p = p;
  conn->easy = curl_easy_init();
  if (!conn->easy) {
    fprintf(stderr, "[下载任务] curl_easy_init 失败: %s\n", conn->p->url);
    free_conninfo(conn);
    return;
  }

  curl_easy_setopt(conn->easy, CURLOPT_URL, conn->p->url);
  curl_easy_setopt(conn->easy, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(conn->easy, CURLOPT_WRITEDATA, conn);
  curl_easy_setopt(conn->easy, CURLOPT_PRIVATE, conn);
  curl_easy_setopt(conn->easy, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(conn->easy, CURLOPT_CAINFO,
                   "/etc/ssl/certs/ca-certificates.crt");
  curl_easy_setopt(conn->easy, CURLOPT_CAPATH, "/etc/ssl/certs");
  conn->tmp_path = get_tmp(conn->p->store);
  if (!conn->tmp_path) {
    fprintf(stderr, "[下载任务] 获取临时路径失败: %s\n", conn->p->url);
    free_conninfo(conn);
    return;
  }
  curl_multi_add_handle(ep->multi, conn->easy);
  ep->active_transfers++;
}

static int socket_cb(CURL *easy, curl_socket_t s, int what, void *userp,
                     void *socketp) {
  EpollInfo *ep = userp;
  struct epoll_event event;
  int action = EPOLL_CTL_ADD;
  memset(&event, 0, sizeof(event));
  event.data.fd = s;
  if (what == CURL_POLL_REMOVE) {
    action = EPOLL_CTL_DEL;
  } else {
    if (what == CURL_POLL_IN)
      event.events = EPOLLIN;
    else if (what == CURL_POLL_OUT)
      event.events = EPOLLOUT;
    else if (what == CURL_POLL_INOUT)
      event.events = EPOLLIN | EPOLLOUT;
    if (socketp) {
      action = EPOLL_CTL_MOD;
    } else {
      action = EPOLL_CTL_ADD;
      curl_multi_assign(ep->multi, s, (void *)1);
    }
  }
  if (epoll_ctl(ep->epoll_fd, action, s, &event) != 0) {
    fprintf(stderr, "epoll_ctl error\n");
  }
  return 0;
}

static void check_multi_info(EpollInfo *ep) {
  CURLMsg *msg;
  int msg_left;
  CURL *easy;
  ConnInfo *conn;

  while ((msg = curl_multi_info_read(ep->multi, &msg_left))) {
    if (msg->msg == CURLMSG_DONE) {
      easy = msg->easy_handle;
      CURLcode res = msg->data.result;
      long http_code = 0;
      curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &http_code);
      curl_easy_getinfo(easy, CURLINFO_PRIVATE, (void **)&conn);
      char *sha1 = NULL;
      if (conn->fp) {
        sha1 = m_sha1(conn->fp);
        fclose(conn->fp);
        conn->fp = NULL;
      }
      if (sha1 == NULL) {
        sha1 = m_strdup("");
      }
      int success = 0;
      const int http_ok = (http_code >= 200 && http_code < 300);
      if (res == CURLE_OK && http_ok && strcmp(conn->p->sha1, sha1) == 0) {
        rename(conn->tmp_path, conn->p->store);
        if (conn->p->path && strcmp(conn->p->store, conn->p->path) != 0) {
          link_mkdir(conn->p->store, conn->p->path);
          if (access(conn->p->path, F_OK) == 0) {
            progress_on_link(ep->running_handles);
            success = 1;
          }
        } else {
          success = 1;
        }
      } else if (res == CURLE_OK && http_ok &&
                 strcmp(conn->p->sha1, "-1") == 0) {
        if (rename(conn->tmp_path, conn->p->store) == 0) {
          success = 1;
        } else {
          perror("rename");
        }
      }
      if (success) {
        progress_on_done(ep->running_handles);
        pending_task_finished();
        record_download_done(conn->p->store);
      } else {
        char *effective_url = NULL;
        curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective_url);
        fprintf(stderr,
                "[网络调试] 下载失败: url=%s effective=%s store=%s tmp=%s "
                "curl=%s http=%ld\n",
                conn->p->url, effective_url ? effective_url : "(null)",
                conn->p->store ? conn->p->store : "(null)",
                conn->tmp_path ? conn->tmp_path : "(null)",
                curl_easy_strerror(res), http_code);
        printf("retrying\n");
        package *p_copy = malloc(sizeof(package));
        if (p_copy) {
          p_copy->path = m_strdup(conn->p->path);
          p_copy->url = m_strdup(conn->p->url);
          p_copy->sha1 = m_strdup(conn->p->sha1);
          p_copy->store = m_strdup(conn->p->store);
          add_download(ep, p_copy);
          progress_on_retry(ep->running_handles);
        } else {
          fprintf(stderr, "[失败] 重试包分配失败: %s\n", conn->p->url);
          pending_task_finished();
        }
      }
      curl_multi_remove_handle(ep->multi, easy);
      curl_easy_cleanup(easy);
      free(sha1);
      conn->easy = NULL;
      free_conninfo(conn);
      if (ep->active_transfers > 0) {
        ep->active_transfers--;
      }
    }
  }
}

void submit_download_task(package *p) {
  if (g_task_pipe_write_fd == -1)
    return;
  if (access(p->store, F_OK) == 0) { // 缓存命中
    if (p->path && strcmp(p->store, p->path) != 0) {
      link_mkdir(p->store, p->path);
    }
    record_download_done(p->store);
    return;
  }

  package *p_copy = calloc(1, sizeof(package));
  p_copy->url = m_strdup(p->url);
  p_copy->sha1 = m_strdup(p->sha1);
  p_copy->path = m_strdup(p->path);
  p_copy->store = m_strdup(p->store);

  if (write(g_task_pipe_write_fd, &p_copy, sizeof(package *)) !=
      sizeof(package *)) {
    fprintf(stderr, "写入管道失败\n");
    free_package(p_copy);
  } else {
    pending_task_submitted();
    progress_on_submit();
  }
}

void *epoll_download(void *arg) {
  EpollThreadArgs *t_args = arg;
  EpollInfo ep;
  memset(&ep, 0, sizeof(EpollInfo));
  ep.epoll_fd = epoll_create1(0);
  ep.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

  int pipe_read_fd = t_args->pipe_read_fd;
  free(t_args);
  int flags = fcntl(pipe_read_fd, F_GETFL, 0);
  fcntl(pipe_read_fd, F_SETFL, flags | O_NONBLOCK);

  ep.multi = curl_multi_init();
  curl_multi_setopt(ep.multi, CURLMOPT_MAX_TOTAL_CONNECTIONS,
                    M_MULTI_MAX_TOTAL_CONN);
  curl_multi_setopt(ep.multi, CURLMOPT_MAX_HOST_CONNECTIONS,
                    M_MULTI_MAX_HOST_CONN);
  curl_multi_setopt(ep.multi, CURLMOPT_MAXCONNECTS, M_MULTI_MAX_CACHE_CONN);
  struct epoll_event event;

  event.events = EPOLLIN;
  event.data.fd = ep.timer_fd;
  epoll_ctl(ep.epoll_fd, EPOLL_CTL_ADD, ep.timer_fd, &event); // 监听计时器

  event.events = EPOLLIN;
  event.data.fd = pipe_read_fd;
  epoll_ctl(ep.epoll_fd, EPOLL_CTL_ADD, pipe_read_fd, &event); // 监听pipe可读

  curl_multi_setopt(ep.multi, CURLMOPT_SOCKETFUNCTION, socket_cb);
  curl_multi_setopt(ep.multi, CURLMOPT_SOCKETDATA, &ep);
  curl_multi_setopt(ep.multi, CURLMOPT_TIMERFUNCTION, timer_cb);
  curl_multi_setopt(ep.multi, CURLMOPT_TIMERDATA, &ep);

  struct epoll_event events[MAX_EVENTS];
  ep.running_handles = 0;
  ep.active_transfers = 0;

  while (1) {
    if (g_epoll_signal == EPOLL_SIGNAL_EXIT && ep.active_transfers == 0 &&
        ep.running_handles == 0) {
      break;
    }
    int num_events = epoll_pwait(ep.epoll_fd, events, MAX_EVENTS, -1, NULL);
    // 取出就绪的文件描述符
    if (num_events < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    for (int i = 0; i < num_events;
         i++) { // 由于取出的文件描述符已经被重排，直接遍历即可
      if (events[i].data.fd == ep.timer_fd) { // 定时器到期
        uint64_t count;
        read(ep.timer_fd, &count, sizeof(uint64_t));
        curl_multi_socket_action(ep.multi, CURL_SOCKET_TIMEOUT, 0,
                                 &ep.running_handles);
        // printf("[epoll定时] count=%llu running=%d\n", (unsigned long
        // long)count,
        //        ep.running_handles);
      } else if (events[i].data.fd == pipe_read_fd) {
        package *new_task;
        while (1) {
          ssize_t n = read(pipe_read_fd, &new_task, sizeof(package *));
          if (n == 0) {
            break;
          }
          if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            break;
          }
          if (n != sizeof(package *)) {
            break;
          }
          add_download(&ep, new_task);
          curl_multi_socket_action(ep.multi, CURL_SOCKET_TIMEOUT, 0,
                                   &ep.running_handles);
          // printf("[epoll任务挂载后] running=%d\n", ep.running_handles);
        }
      } else {
        int action = 0;
        if (events[i].events & EPOLLIN)
          action |= CURL_CSELECT_IN;
        if (events[i].events & EPOLLOUT)
          action |= CURL_CSELECT_OUT;
        curl_multi_socket_action(ep.multi, events[i].data.fd, action,
                                 &ep.running_handles);
        // printf("[epoll socket事件] fd=%d action=%d running=%d\n",
        //        events[i].data.fd, action, ep.running_handles);
      }
    }
    if (g_epoll_signal == EPOLL_SIGNAL_EXIT && ep.active_transfers == 0 &&
        ep.running_handles == 0) {
      break;
    }
    check_multi_info(&ep);
  }
  curl_multi_cleanup(ep.multi);
  progress_shutdown(ep.running_handles);
  close(ep.epoll_fd);
  close(ep.timer_fd);
  close(pipe_read_fd);
  return 0;
}

void init_epoll_download_task() {
  int task_pipe[2];
  if (pipe(task_pipe) != 0) {
    perror("pipe create error");
    m_exit(EX_OSERR);
  }
  g_task_pipe_write_fd = task_pipe[1];
  EpollThreadArgs *args = malloc(sizeof(EpollThreadArgs));
  args->pipe_read_fd = task_pipe[0];
  pthread_create(&epoll_download_thread_id, NULL, epoll_download, args);
}

void wait_epoll_download_task() {
  pthread_mutex_lock(&g_pending_lock);
  while (g_pending_tasks > 0) {
    pthread_cond_wait(&g_pending_cond, &g_pending_lock);
  }
  pthread_mutex_unlock(&g_pending_lock);
}

void wait_download_task(const char *store) {
  if (!store) {
    return;
  }
  pthread_mutex_lock(&g_pending_lock);
  while (!has_download_done_locked(store)) {
    pthread_cond_wait(&g_pending_cond, &g_pending_lock);
  }
  pthread_mutex_unlock(&g_pending_lock);
}

void stop_epoll_download_task() {
  g_epoll_signal = EPOLL_SIGNAL_EXIT;
  if (g_task_pipe_write_fd != -1) {
    close(g_task_pipe_write_fd);
    g_task_pipe_write_fd = -1;
  }
  pthread_join(epoll_download_thread_id, NULL);
  clear_done_list();
}
