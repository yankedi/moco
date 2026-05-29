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
#include <notcurses/notcurses.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sysexits.h>
#include <unistd.h>

#define MAX_EVENTS 64
#define M_MULTI_MAX_TOTAL_CONN 16L
#define M_MULTI_MAX_HOST_CONN 6L
#define M_MULTI_MAX_CACHE_CONN 32L

static int g_task_pipe_write_fd = -1;
static pthread_t epoll_download_thread_id;
static volatile EpollSignal g_epoll_signal = EPOLL_SIGNAL_RUN;

static _Atomic int g_dl_total = 0;
static _Atomic int g_dl_done = 0;
static _Atomic int g_dl_linked = 0;
static _Atomic int g_dl_retry = 0;
static char g_dl_done_log[64][256];
static _Atomic int g_dl_done_log_pos = 0;

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

static pthread_mutex_t g_pending_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_pending_cond = PTHREAD_COND_INITIALIZER;
static _Atomic unsigned long g_pending_tasks = 0;

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

// TUI
static struct notcurses *tui_nc;
static struct ncprogbar *tui_pbar;
static struct ncplane *tui_list, *tui_info;
static int tui_list_h, tui_r, tui_g, tui_b;

static void tui_init(void) {
  notcurses_options o = {.flags = NCOPTION_SUPPRESS_BANNERS};
  tui_nc = notcurses_core_init(&o, NULL);
  if (!tui_nc) return;
  unsigned dimy, dimx;
  struct ncplane *std = notcurses_stddim_yx(tui_nc, &dimy, &dimx);
  int pg_h = 1, info_h = 1;
  tui_list_h = dimy - pg_h - info_h - 1;
  tui_list = ncplane_create(std,
    &(struct ncplane_options){.y=1,.x=1,.rows=tui_list_h,.cols=dimx-2});
  tui_info = ncplane_create(std,
    &(struct ncplane_options){.y=dimy-pg_h-info_h,.x=1,.rows=info_h,.cols=dimx-2});
  struct ncplane *pgp = ncplane_create(std,
    &(struct ncplane_options){.y=dimy-pg_h,.x=1,.rows=pg_h,.cols=dimx-2});
  ncplane_set_base(pgp, "", 0, 0);
  tui_r = 99; tui_g = 176; tui_b = 248;
  ncprogbar_options po = {};
  ncchannel_set_rgb8(&po.ulchannel,tui_r,tui_g,tui_b);
  ncchannel_set_rgb8(&po.urchannel,tui_r,tui_g,tui_b);
  ncchannel_set_rgb8(&po.blchannel,(int)(tui_r*.35),(int)(tui_g*.35),(int)(tui_b*.35));
  ncchannel_set_rgb8(&po.brchannel,(int)(tui_r*.35),(int)(tui_g*.35),(int)(tui_b*.35));
  tui_pbar = ncprogbar_create(pgp, &po);
}

static void tui_render_frame(void) {
  ncplane_erase(tui_list);
  int tp = g_dl_done_log_pos, rows = tp > tui_list_h ? tui_list_h : tp;
  for (int i = 0; i < rows; i++) {
    int idx = (tp - 1 - i) % 64, c = 200 - (i * 200 / rows);
    if (c < 50) c = 50;
    ncplane_set_fg_rgb8(tui_list, c, c, c);
    ncplane_printf_yx(tui_list, tui_list_h - 1 - i, 1, " %s", g_dl_done_log[idx]);
  }
  double pct = g_dl_total > 0 ? (double)g_dl_done/(double)g_dl_total : 0;
  ncprogbar_set_progress(tui_pbar, pct);
  ncplane_set_fg_rgb8(tui_info, tui_r, tui_g, tui_b);
  ncplane_printf_yx(tui_info, 0, 1, "Download: %d/%d  %.1f%%", g_dl_done, g_dl_total, pct*100);
  ncplane_set_fg_rgb8(tui_info, 100, 200, 100);
  ncplane_printf_yx(tui_info, 0, 45, "Linked: %d", g_dl_linked);
  if (g_dl_retry) {
    ncplane_set_fg_rgb8(tui_info, 255, 100, 100);
    ncplane_printf_yx(tui_info, 0, 62, "Retry: %d", g_dl_retry);
  }
}

static void tui_render(void) {
  if (!tui_nc) return;
  tui_render_frame();
  notcurses_render(tui_nc);
}

static void tui_shutdown(void) {
  if (!tui_nc) return;
  ncprogbar_destroy(tui_pbar);
  notcurses_stop(tui_nc); tui_nc = NULL;
  printf("Downloaded: %d  Linked: %d  Total: %d\n", g_dl_done, g_dl_linked, g_dl_done + g_dl_linked);
}

static inline void dl_on_submit(const char *path) {
  (void)path;
  g_dl_total++;
  if (!tui_nc) tui_init();
}

static inline void dl_on_done(const char *path) {
  g_dl_done++;
  int pos = g_dl_done_log_pos++ % 64;
  snprintf(g_dl_done_log[pos], sizeof(g_dl_done_log[0]), "%.240s", path);
}

static inline void dl_on_retry(void) { g_dl_retry++; }
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
            g_dl_linked++;
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
        dl_on_done(conn->p->path);
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
          dl_on_retry();
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
  if (access(p->store, F_OK) == 0) { // cache hit
    if (p->path && strcmp(p->store, p->path) != 0) {
      link_mkdir(p->store, p->path);
    }
    g_dl_linked++;
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
    dl_on_submit(p->path);
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
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 16666667; // 60fps
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    pthread_cond_timedwait(&g_pending_cond, &g_pending_lock, &ts);
    pthread_mutex_unlock(&g_pending_lock);
    if (tui_nc) tui_render();
    pthread_mutex_lock(&g_pending_lock);
  }
  pthread_mutex_unlock(&g_pending_lock);
  tui_shutdown();
}

void wait_download_task(const char *store) {
  if (!store) {
    return;
  }
  pthread_mutex_lock(&g_pending_lock);
  while (!has_download_done_locked(store)) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 16666667;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    pthread_cond_timedwait(&g_pending_cond, &g_pending_lock, &ts);
    if (has_download_done_locked(store)) break;
    pthread_mutex_unlock(&g_pending_lock);
    if (tui_nc) tui_render();
    pthread_mutex_lock(&g_pending_lock);
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
  tui_shutdown();
  clear_done_list();
}
