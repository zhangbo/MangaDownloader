#ifdef __cplusplus
extern "C" {
#endif

/* curl stuff */ 
#include <curl/curl.h>
#include <curl/mprintf.h>

struct transfer {
  CURL *easy;
  unsigned int num;
  FILE *out;
};

void setup(struct transfer *t, const char* url, const char* fileId);

#ifdef __cplusplus
}
#endif