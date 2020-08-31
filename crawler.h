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

typedef struct $
{
	char** arr;
	size_t rows;
} links;

size_t follow_links(CURLM *multi_handle, memory *mem, char *url, const char* regex, int follow_relative_links);
int is_html(char *ctype);
CURL *make_handle(char *url);
links* parseHTMLWithUrl(CURLM* easy_handle, const char* startPage, const char* nodeXml, const char* regularExpression, int follow_relative_links);


#ifdef __cplusplus
}
#endif