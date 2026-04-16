#include "m_exit.h"

#include "env.h"
#include "utility/download/m_epoll.h"

#include <curl/curl.h>
#include <stdlib.h>

void m_exit(int status) {
  wait_epoll_download_task();
  stop_epoll_download_task();
  curl_global_cleanup();
  free_env();
  exit(status);
}
