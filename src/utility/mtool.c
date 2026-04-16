
#include "mtool.h"
#include "interface.h"
#include "m_exit.h"

#include <openssl/evp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

char *m_strdup(const char *s) {
  if (s == NULL) return NULL;
  char *dup = strdup(s);
  if (dup == NULL) {
    fprintf(stderr, "OOM！\n");
    m_exit(EX_OSERR);
  }
  return dup;
}

int m_asprintf(char **strp, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = vasprintf(strp, fmt, ap);
  va_end(ap);

  if (len == -1) {
    fprintf(stderr, "OOM！\n");
    m_exit(EX_OSERR);
  }
  return len;
} // TODO 审查ai生成的函数

void *m_malloc(const size_t size) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "OOM!\n");
    m_exit(EX_OSERR);
  }
  return ptr;
}

char *m_sha1(FILE *fp) {
  if (!fp)
    return "";
  rewind(fp);
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  const EVP_MD *md = EVP_sha1();
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  if (mdctx == NULL)
    return NULL;
  if (1 != EVP_DigestInit_ex(mdctx, md, NULL)) {
    EVP_MD_CTX_free(mdctx);
    return "";
  }
  char buffer[BUFSIZ];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, BUFSIZ, fp)) != 0) {
    EVP_DigestUpdate(mdctx, buffer, bytes_read);
  }
  EVP_DigestFinal_ex(mdctx, hash, &hash_len);
  EVP_MD_CTX_free(mdctx);
  char *sha1 = m_malloc(41);
  for (int i = 0; i < hash_len; ++i)
    sprintf(sha1 + (i * 2), "%02x", hash[i]);
  sha1[40] = '\0';
  return sha1;
} // TODO 未理解原理

char *m_replace(const char *target, const m_string str) {
  if (!target || !str.key || !str.value) return NULL;
  char *pos = strstr(target, str.key);
  if (pos == NULL) {
    free(pos);
    return NULL;
  }
  size_t head_len = pos - target;
  size_t key_len = strlen(str.key);
  size_t val_len = strlen(str.value);
  size_t tail_len = strlen(pos + key_len);
  size_t total_len = head_len + val_len + tail_len + 1;
  char *result = m_malloc(total_len);
  memcpy(result, target, head_len);
  memcpy(result + head_len, str.value, val_len);
  memcpy(result + head_len + val_len, pos + key_len, tail_len + 1);

  return result;
}