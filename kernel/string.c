#include "types.h"
#include "string.h"

// ------------------------------------------------------------
// 标准内核内存与字符串函数（不依赖 glibc）
// ------------------------------------------------------------

void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  for(uint i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

int
memcmp(const void *v1, const void *v2, uint n)
{
  const uchar *s1 = v1;
  const uchar *s2 = v2;

  while(n-- > 0){
    if(*s1 != *s2)
      return *s1 - *s2;
    s1++;
    s2++;
  }

  return 0;
}

void*
memmove(void *dst, const void *src, uint n)
{
  const char *s = src;
  char *d = dst;

  if(n == 0 || dst == src)
    return dst;

  // 如果目标在源后面，反向复制防止覆盖
  if(s < d && s + n > d){
    s += n;
    d += n;
    while(n-- > 0)
      *--d = *--s;
  } else {
    while(n-- > 0)
      *d++ = *s++;
  }
  return dst;
}

// memcpy exists to placate GCC. Use memmove.
void*
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}

int
strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q){
    n--;
    p++;
    q++;
  }
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

char*
strncpy(char *s, const char *t, int n)
{
  char *os = s;
  while(n-- > 0 && (*s++ = *t++) != 0)
    ;
  while(n-- > 0)
    *s++ = 0;
  return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char*
safestrcpy(char *s, const char *t, int n)
{
  char *os = s;
  if(n <= 0)
    return os;
  while(--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

int
strlen(const char *s)
{
  int n = 0;
  while(s[n])
    n++;
  return n;
}
