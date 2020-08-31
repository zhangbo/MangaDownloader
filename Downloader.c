#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
 
/* somewhat unix-specific */ 
#include <sys/time.h>
#include <unistd.h>

#include "Downloader.h"
 
#ifndef CURLPIPE_MULTIPLEX
/* This little trick will just make sure that we don't enable pipelining for
   libcurls old enough to not have this symbol. It is _not_ defined to zero in
   a recent libcurl header. */ 
#define CURLPIPE_MULTIPLEX 0
#endif
 
static void dump(const char *text, int num, unsigned char *ptr, size_t size,
          char nohex)
{
  size_t i;
  size_t c;
 
  unsigned int width = 0x10;
 
  if(nohex)
    /* without the hex output, we can fit more on screen */ 
    width = 0x40;
 
  fprintf(stderr, "%d %s, %lu bytes (0x%lx)\n",
          num, text, (unsigned long)size, (unsigned long)size);
 
  for(i = 0; i<size; i += width) {
 
    fprintf(stderr, "%4.4lx: ", (unsigned long)i);
 
    if(!nohex) {
      /* hex not disabled, show it */ 
      for(c = 0; c < width; c++)
        if(i + c < size)
          fprintf(stderr, "%02x ", ptr[i + c]);
        else
          fputs("   ", stderr);
    }
 
    for(c = 0; (c < width) && (i + c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */ 
      if(nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D &&
         ptr[i + c + 1] == 0x0A) {
        i += (c + 2 - width);
        break;
      }
      fprintf(stderr, "%c",
              (ptr[i + c] >= 0x20) && (ptr[i + c]<0x80)?ptr[i + c]:'.');
      /* check again for 0D0A, to avoid an extra \n if it's at width */ 
      if(nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D &&
         ptr[i + c + 2] == 0x0A) {
        i += (c + 3 - width);
        break;
      }
    }
    fputc('\n', stderr); /* newline */ 
  }
}
 
static int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  const char *text;
  struct transfer *t = (struct transfer *)userp;
  unsigned int num = t->num;
  (void)handle; /* prevent compiler warning */ 
 
  switch(type) {
  case CURLINFO_TEXT:
    fprintf(stderr, "== %u Info: %s", num, data);
    /* FALLTHROUGH */ 
  default: /* in case a new one is introduced to shock us */ 
    return 0;
 
  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }
 
  dump(text, num, (unsigned char *)data, size, 1);
  return 0;
}
 
void setup(struct transfer *t, const char* url, const char* fileId)
{
  char filename[128];
  CURL *hnd;
 
  hnd = t->easy = curl_easy_init();
 
  curl_msnprintf(filename, 128, "%s", fileId);
 
  t->out = fopen(filename, "wb");
  if(!t->out) {
    fprintf(stderr, "error: could not open file %s for writing: %s\n",
            filename, strerror(errno));
    exit(1);
  }
 
  /* write to this file */ 
  curl_easy_setopt(hnd, CURLOPT_WRITEDATA, t->out);
 
  /* set the same URL */ 
  curl_easy_setopt(hnd, CURLOPT_URL, url);
 
  /* please be verbose */ 
  curl_easy_setopt(hnd, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(hnd, CURLOPT_DEBUGFUNCTION, my_trace);
  curl_easy_setopt(hnd, CURLOPT_DEBUGDATA, t);
 
  /* HTTP/2 please */ 
  curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
 
  /* we use a self-signed test server, skip verification during debugging */ 
  curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 0L);
 
#if (CURLPIPE_MULTIPLEX > 0)
  /* wait for pipe connection to confirm */ 
  curl_easy_setopt(hnd, CURLOPT_PIPEWAIT, 1L);
#endif
}