#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

/* resizable buffer */ 
typedef struct {
  char *buf;
  size_t size;
} memory;

size_t follow_links(CURLM *multi_handle, memory *mem, char *url, const char* regex, int follow_relative_links);
int is_html(char *ctype);
CURL *make_handle(char *url);


#ifdef __cplusplus
}
#endif