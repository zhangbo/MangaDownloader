#include <string.h>
#include <math.h>
#include <regex.h>

#include "crawler.h"

struct MemoryStruct {
    char *memory;
    size_t size;
    size_t reserved;
    CURL *c;
};
 
static size_t grow_buffer(void *contents, size_t sz, size_t nmemb, void *ctx)
{
  size_t realsize = sz * nmemb;
  memory *mem = (memory*) ctx;
  char *ptr = realloc(mem->buf, mem->size + realsize);
  if(!ptr) {
    /* out of memory */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
  mem->buf = ptr;
  memcpy(&(mem->buf[mem->size]), contents, realsize);
  mem->size += realsize;
  return realsize;
}
 
CURL *make_handle(char *url)
{
  CURL *handle = curl_easy_init();
 
  /* Important: use HTTP2 over HTTPS */ 
  curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
  curl_easy_setopt(handle, CURLOPT_URL, url);
 
  /* buffer body */ 
  memory *mem = malloc(sizeof(memory));
  mem->size = 0;
  mem->buf = malloc(1);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, grow_buffer);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, mem);
  curl_easy_setopt(handle, CURLOPT_PRIVATE, mem);
 
  /* For completeness */ 
  curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(handle, CURLOPT_TIMEOUT, 5L);
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 2L);
  curl_easy_setopt(handle, CURLOPT_COOKIEFILE, "");
  curl_easy_setopt(handle, CURLOPT_FILETIME, 1L);
  curl_easy_setopt(handle, CURLOPT_USERAGENT, "mini crawler");
  curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
  curl_easy_setopt(handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
  curl_easy_setopt(handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
  curl_easy_setopt(handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);
  return handle;
}
 
/* HREF finder implemented in libxml2 but could be any HTML parser */ 
size_t follow_links(CURLM *multi_handle, memory *mem, char *url, const char* regularExpression, int follow_relative_links)
{
  regex_t regex;
  int reti;
  reti = regcomp(&regex, regularExpression, 0);
  if (reti) {
    printf("Could not compile regex\n");
    return 0;
  }
  int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
  htmlDocPtr doc = htmlReadMemory(mem->buf, mem->size, url, NULL, opts);
  if(!doc)
    return 0;
  xmlChar *xpath = (xmlChar*) "//a/@href";
  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathObjectPtr result = xmlXPathEvalExpression(xpath, context);
  xmlXPathFreeContext(context);
  if(!result)
    return 0;
  xmlNodeSetPtr nodeset = result->nodesetval;
  if(xmlXPathNodeSetIsEmpty(nodeset)) {
    xmlXPathFreeObject(result);
    return 0;
  }
  unsigned int count = 0;
  int i;
  for(i = 0; i < nodeset->nodeNr; i++) {
    double r = rand();
    int x = r * nodeset->nodeNr / RAND_MAX;
    const xmlNode *node = nodeset->nodeTab[x]->xmlChildrenNode;
    xmlChar *href = xmlNodeListGetString(doc, node, 1);
    reti = regexec(&regex, (char*)href, 0, NULL, 0);
    if (reti)
    {
      continue;
    }
    if(follow_relative_links) {
      xmlChar *orig = href;
      href = xmlBuildURI(href, (xmlChar *) url);
      xmlFree(orig);
    }
    char *link = (char *) href;
    // printf("%s\n", link);
    // if(!link || strlen(link) < 20)
    //   continue;
    if(!strncmp(link, "http://", 7) || !strncmp(link, "https://", 8)) {
      curl_multi_add_handle(multi_handle, make_handle(link));
    }
    xmlFree(link);
  }
  regfree(&regex);
  xmlXPathFreeObject(result);
  return count;
}
 
int is_html(char *ctype)
{
  return ctype != NULL && strlen(ctype) > 10 && strstr(ctype, "text/html");
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    if (mem->reserved == 0)
    {
        CURLcode res;
        double filesize = 0.0;

        res = curl_easy_getinfo(mem->c, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
        if((CURLE_OK == res) && (filesize>0.0))
        {
            mem->memory = realloc(mem->memory, (int)filesize + 2);
            if (mem->memory == NULL) {
                printf("not enough memory (realloc returned NULL)\n");
                return 0;
            }
            mem->reserved = (int)filesize + 1;
        }
    }

    if ((mem->size + realsize + 1) > mem->reserved)
    {
        mem->memory = realloc(mem->memory, mem->size + realsize + 1);
        mem->reserved = mem->size + realsize + 1;
        if (mem->memory == NULL) {
            printf("not enough memory (realloc returned NULL)\n");
            return 0;
        }
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

links* parseHTMLWithUrl(CURLM* easy_handle, const char* startPage, const char* nodeXml, const char* regularExpression, int follow_relative_links)
{
  struct MemoryStruct chunk;

  chunk.memory = malloc(1);
  chunk.memory[0] = '\0';
  chunk.size = 0;
  chunk.reserved = 0;
  chunk.c = easy_handle;

  char* sourceCode;

  curl_easy_setopt(easy_handle, CURLOPT_URL, startPage);
  curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, (void *)&chunk);

  int res = curl_easy_perform(easy_handle);

  if (res == CURLE_OK) {
    sourceCode = strdup(chunk.memory);
    free(chunk.memory);
  }
  regex_t regex;
  int reti;
  reti = regcomp(&regex, regularExpression, 0);
  if (reti) {
    printf("Could not compile regex\n");
    return NULL;
  }
  int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
  htmlDocPtr doc = htmlReadMemory(sourceCode, chunk.size, startPage, NULL, opts);
  if(!doc)
    return NULL;
  xmlChar *xpath = (xmlChar*) nodeXml;
  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  xmlXPathObjectPtr result = xmlXPathEvalExpression(xpath, context);
  xmlXPathFreeContext(context);
  if(!result)
    return NULL;
  xmlNodeSetPtr nodeset = result->nodesetval;
  if(xmlXPathNodeSetIsEmpty(nodeset)) {
    xmlXPathFreeObject(result);
    return NULL;
  }

  links* l = (links*)malloc(sizeof(links));
  char** array = (char**)malloc((nodeset->nodeNr) * sizeof(char*));
  int idx = 0;
  for(int i = 0; i < nodeset->nodeNr; ++i) {
    const xmlNode *node = nodeset->nodeTab[i]->xmlChildrenNode;
    xmlChar *href = xmlNodeListGetString(doc, node, 1);
    reti = regexec(&regex, (char*)href, 0, NULL, 0);
    if (reti)
    {
      continue;
    }
    if(follow_relative_links) {
      xmlChar *orig = href;
      href = xmlBuildURI(href, (xmlChar *)startPage);
      xmlFree(orig);
    }
    char *link = (char *) href;
    array[idx] = strdup(link);
    xmlFree(link);
    idx++;
  }
  l->arr = array;
  l->rows = idx;
  regfree(&regex);
  return l;
}