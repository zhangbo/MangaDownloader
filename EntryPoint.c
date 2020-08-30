#include "crawler.h"
#include <signal.h>

struct MemoryStruct {
    char *memory;
    size_t size;
    size_t reserved;
    CURL *c;
};

int max_con = 200;
int max_total = 20000;
int max_requests = 500;
int follow_relative_links = 1;
char *start_page = "http://www.ikanwxz.top/book/20";
int pending_interrupt = 0;
static void sighandler(int dummy)
{
  pending_interrupt = 1;
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

int main(int argc, char const *argv[])
{
	signal(SIGINT, sighandler);
  LIBXML_TEST_VERSION;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURLM *handle = curl_easy_init();

  struct MemoryStruct chunk;

  chunk.memory = malloc(1);
  chunk.memory[0] = '\0';
  chunk.size = 0;
  chunk.reserved = 0;
  chunk.c = handle;

  char* sourceCode;

  curl_easy_setopt(handle, CURLOPT_URL, start_page);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&chunk);

  int res = curl_easy_perform(handle);

  if (res == CURLE_OK) {
    sourceCode = strdup(chunk.memory);
    free(chunk.memory);
  }
 
  /* enables http/2 if available */ 
// #ifdef CURLPIPE_MULTIPLEX
//   curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
// #endif
 
//   /* sets html start page */ 
//   curl_multi_add_handle(multi_handle, make_handle(start_page));
 
//   int msgs_left;
//   int pending = 0;
//   int complete = 0;
//   int still_running = 1;

//   curl_multi_perform(multi_handle, &still_running);
//   /* See how the transfers went */ 
//   CURLMsg *m = NULL;
//   m = curl_multi_info_read(multi_handle, &msgs_left);
//   if(m->msg == CURLMSG_DONE) {
//     CURL *handle = m->easy_handle;
//     char *url;
//     memory *mem;
//     curl_easy_getinfo(handle, CURLINFO_PRIVATE, &mem);
//     curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &url);
//     if(m->data.result == CURLE_OK) {
//       long res_status;
//       curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &res_status);
//       if(res_status == 200) {
//         char *ctype;
//         curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &ctype);
//         printf("[%d] HTTP 200 (%s): %s\n", complete, ctype, url);
//         if(is_html(ctype) && mem->size > 100) {
//           if(pending < max_requests && (complete + pending) < max_total) {
//             pending += follow_links(multi_handle, mem, url, (char*)"^/chapter/.*$", follow_relative_links);
//             still_running = 1;
//           }
//         }
//       }
//       else {
//         printf("[%d] HTTP %d: %s\n", complete, (int) res_status, url);
//       }
//     }
//     else {
//       printf("[%d] Connection failure: %s\n", complete, url);
//     }
//     curl_multi_remove_handle(multi_handle, handle);
//     curl_easy_cleanup(handle);
//     free(mem->buf);
//     free(mem);
//     complete++;
//     pending--;
//   }
//   curl_multi_cleanup(multi_handle);
  curl_global_cleanup();
  return 0;
}