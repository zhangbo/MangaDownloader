#include "crawler.h"
#include "Downloader.h"

#include <string.h>
#include <signal.h>
#include <pthread.h>

int max_con = 200;
int max_total = 20000;
int max_requests = 500;
int follow_relative_links = 1;
const char *start_page = "https://www.ikanwxz.top/book/419";
int pending_interrupt = 0;

int max_threads = 16;
int global = 0;

pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

static char* findUrlFileId(char* s)
{
  char* ret = strrchr(s, '/');
  if (ret)
  {
    return ret + 1;
  }
  return NULL;
}

static char* replaceStr(char* s, const char* findStr, unsigned int pos, const char* replaceStr)
{
  if (s == NULL || pos >= strlen(s) - strlen(findStr) || findStr == NULL)
  {
    return NULL;
  }
  int i = 0, findout = 1;
  while (i < strlen(findStr))
  {
    if (findStr[i] != s[pos + i])
    {
      findout = 0;
    }
    i++;
  }

  if (!findout)
  {
    return NULL;
  }
  char* newStr = (char*)malloc((strlen(s) + strlen(replaceStr) - strlen(findStr)) * sizeof(char));
  int afterReplacePos = pos + strlen(findStr), start = 0;
  for (int j = 0; j < (strlen(s) + strlen(replaceStr) - strlen(findStr)); ++j)
  {
    if (j < pos) {
      newStr[j] = s[j];
    } else if (j >= pos && j < pos + strlen(replaceStr)) {
      newStr[j] = replaceStr[j - pos];
    } else {
      if (j == pos + strlen(replaceStr))
      {
        start = j;
      }
      newStr[j] = s[afterReplacePos + j - start];
    }
  }
  return newStr;
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *fp)
{
  return fwrite(ptr, size, nmemb, (FILE *)fp);
}

static void downloadFile2(links* l)
{
  CURL *curl = curl_easy_init();
  if(curl) {
    for(int i = 0; i < l->rows; ++i) {
      char* url = l->arr[i];
      FILE *fp = fopen(findUrlFileId(url), "wb");
      if(fp) {
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)fp);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
     
        curl_easy_setopt(curl, CURLOPT_URL, url);
     
        /* do not send TFTP options requests */
        curl_easy_setopt(curl, CURLOPT_TFTP_NO_OPTIONS, 1L);
     
        /* Perform the request */
        curl_easy_perform(curl);
     
        fclose(fp);
      }
    }
  }
  curl_easy_cleanup(curl);
}

static void downloadFile(links* l)
{
  if (l == NULL)
  {
    return;
  }
  struct transfer trans[l->rows];
  CURLM *multi_handle;
  int i;
  int still_running = 0;

  multi_handle = curl_multi_init();
  // l->rows = l->rows > 3 ? 3 : l->rows;
 
  for(i = 0; i < l->rows; i++) {
    char* url = l->arr[i];
    setup(&trans[i], url, findUrlFileId(url));
    free(url);
    /* add the individual transfer */ 
    curl_multi_add_handle(multi_handle, trans[i].easy);
  }

  curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

  curl_multi_perform(multi_handle, &still_running);

  while(still_running) {
    struct timeval timeout;
    int rc; /* select() return code */ 
    CURLMcode mc; /* curl_multi_fdset() return code */ 
 
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd = -1;
 
    long curl_timeo = -1;
 
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);
 
    /* set a suitable timeout to play around with */ 
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
 
    curl_multi_timeout(multi_handle, &curl_timeo);
    if(curl_timeo >= 0) {
      timeout.tv_sec = curl_timeo / 1000;
      if(timeout.tv_sec > 1)
        timeout.tv_sec = 1;
      else
        timeout.tv_usec = (curl_timeo % 1000) * 1000;
    }
 
    /* get file descriptors from the transfers */ 
    mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
 
    if(mc != CURLM_OK) {
      fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
      break;
    }
 
    /* On success the value of maxfd is guaranteed to be >= -1. We call
       select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
       no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
       to sleep 100ms, which is the minimum suggested value in the
       curl_multi_fdset() doc. */ 
 
    if(maxfd == -1) {
#ifdef _WIN32
      Sleep(100);
      rc = 0;
#else
      /* Portable sleep for platforms other than Windows. */ 
      struct timeval wait = { 0, 100 * 1000 }; /* 100ms */ 
      rc = select(0, NULL, NULL, NULL, &wait);
#endif
    }
    else {
      /* Note that on some platforms 'timeout' may be modified by select().
         If you need access to the original value save a copy beforehand. */ 
      rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    }
 
    switch(rc) {
    case -1:
      /* select error */ 
      break;
    case 0:
    default:
      /* timeout or readable/writable sockets */ 
      curl_multi_perform(multi_handle, &still_running);
      break;
    }
  }
 
  for(i = 0; i < l->rows; i++) {
    curl_multi_remove_handle(multi_handle, trans[i].easy);
    curl_easy_cleanup(trans[i].easy);
  }
 
  curl_multi_cleanup(multi_handle);

}

static void sighandler(int dummy)
{
  pending_interrupt = 1;
}

static void* pthread_work(void* l)
{
  if (l == NULL)
  {
    return NULL;
  }
  links* arg = (links*)l;
  while (global < arg->rows)
  {
    pthread_mutex_lock(&count_mutex);
    char* link = arg->arr[global++];
    pthread_mutex_unlock(&count_mutex);
    CURLM *handle = curl_easy_init();
    links* jpgLinks = parseHTMLWithUrl(handle, link, (char*)"//div//img/@data-original", (char*)"^http.*jpg$", 0);
    downloadFile2(jpgLinks);
    free(jpgLinks->arr);
    free(jpgLinks);
    curl_easy_cleanup(handle);
  }
  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char const *argv[])
{
	signal(SIGINT, sighandler);
  LIBXML_TEST_VERSION;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURLM *handle = curl_easy_init();

  links* l = parseHTMLWithUrl(handle, start_page, (char*)"//li//a/@href", (char*)"^/chapter/.*$", 1);

  pthread_t threads[max_threads];
  int thread_count;
  for (thread_count = 0; thread_count < max_threads; ++thread_count)
  {
    if (pthread_create(&threads[thread_count], NULL, &pthread_work, l) != 0)
    {
      printf("Can not create thread #%d\n", thread_count);
      break;
    }
  }
  for (int i = 0; i < thread_count; ++i)
  {
    if (pthread_join(threads[i], NULL) != 0)
      {
        printf("Can not join thread #%d\n", i);
      }  
  }
  pthread_mutex_destroy(&count_mutex);

  //

  free(l->arr);
  free(l);
 
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
  curl_easy_cleanup(handle);
  curl_global_cleanup();
  return 0;
}