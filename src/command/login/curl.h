
#ifndef MOCO_OAUTH2_HTTP_H
#define MOCO_OAUTH2_HTTP_H
#include <stddef.h>
extern int curl_post(const char *url, const char *post_fields,
                     char **response_body);
extern size_t curl_callback(void *contents, size_t size, size_t nmemb,
                            void *userp);
struct MemoryBlock {
  char *memory;
  size_t size;
};
#endif
