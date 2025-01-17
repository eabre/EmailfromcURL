#include <string.h>
#include <curl/curl.h>

/*
 * This is the list of basic details you need to tweak to get things right.
 */
#define USERNAME "example@gmail.com"
#define PASSWORD "password"
#define SMTPSERVER "smtp.gmail.com"
#define SMTPPORT ":465" /* it is a colon+port string, but you can set it
                           to "" to use the default port */
#define RECIPIENT "<example@gmail.com>"
#define MAILFROM "<example@gmail.com>"

#define MULTI_PERFORM_HANG_TIMEOUT 60 * 1000

/* Note that you should include the actual meta data headers here as well if
   you want the mail to have a Subject, another From:, show a To: or whatever
   you think your mail should feature! */
static const char *text[]={
  "one\n",
  "two\n",
  "three\n",
  " Hello, this is CURL email SMTP\n",
  NULL
};

struct WriteThis {
  int counter;
};

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
  struct WriteThis *pooh = (struct WriteThis *)userp;
  const char *data;

  if(size*nmemb < 1)
    return 0;

  data = text[pooh->counter];

  if(data) {
    size_t len = strlen(data);
    memcpy(ptr, data, len);
    pooh->counter++; /* advance pointer */
    return len;
  }
  return 0;                         /* no more data left to deliver */
}

static struct timeval tvnow(void)
{
  /*
  ** time() returns the value of time in seconds since the Epoch.
  */
  struct timeval now;
  now.tv_sec = (long)time(NULL);
  now.tv_usec = 0;
  return now;
}

static long tvdiff(struct timeval newer, struct timeval older)
{
  return (newer.tv_sec-older.tv_sec)*1000+
    (newer.tv_usec-older.tv_usec)/1000;
}

int main(void)
{
	int n=1;
   CURL *curl;
   CURLM *mcurl;
   int still_running = 1;
   struct timeval mp_start;
   struct WriteThis pooh;
   struct curl_slist* rcpt_list = NULL;

   pooh.counter = 0;
   printf("here %d \n", n++);
   curl_global_init(CURL_GLOBAL_DEFAULT);
   printf("here %d \n", n++);
   curl = curl_easy_init();
   if(!curl)
     return 1;
   printf("here %d \n", n++);
   mcurl = curl_multi_init();
   if(!mcurl)
     return 2;
   printf("here %d \n", n++);
   rcpt_list = curl_slist_append(rcpt_list, RECIPIENT);
   /* more addresses can be added here
      rcpt_list = curl_slist_append(rcpt_list, "<others@example.com>");
   */

   char temp[256] = { 0 };
   sprintf(temp, "smtps://%s%s", SMTPSERVER, SMTPPORT);
   curl_easy_setopt(curl, CURLOPT_URL, temp);
   curl_easy_setopt(curl, CURLOPT_USERNAME, USERNAME);
   curl_easy_setopt(curl, CURLOPT_PASSWORD, PASSWORD);
   curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
   curl_easy_setopt(curl, CURLOPT_MAIL_FROM, MAILFROM);
   curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpt_list);
   curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
   curl_easy_setopt(curl, CURLOPT_READDATA, &pooh);
   curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
   curl_easy_setopt(curl, CURLOPT_SSLVERSION, 0L);
   curl_easy_setopt(curl, CURLOPT_SSL_SESSIONID_CACHE, 0L);
   printf("here %d \n", n++);
   curl_multi_add_handle(mcurl, curl);
   printf("here %d \n", n++);
   mp_start = tvnow();

  /* we start some action by calling perform right away */
  curl_multi_perform(mcurl, &still_running);
  printf("here %d \n", n++);
  while(still_running) {
    struct timeval timeout;
    int rc; /* select() return code */

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
    printf("here %d \n", n++);
    curl_multi_timeout(mcurl, &curl_timeo);
    printf("here %d, %ld \n", n++, curl_timeo);
    if(curl_timeo >= 0)
    {
      timeout.tv_sec = curl_timeo / 1000;
      if(timeout.tv_sec > 1)
        timeout.tv_sec = 1;
      else
        timeout.tv_usec = (curl_timeo % 1000) * 1000;
    }

    /* get file descriptors from the transfers */
    curl_multi_fdset(mcurl, &fdread, &fdwrite, &fdexcep, &maxfd);

    /* In a real-world program you OF COURSE check the return code of the
       function calls.  On success, the value of maxfd is guaranteed to be
       greater or equal than -1.  We call select(maxfd + 1, ...), specially in
       case of (maxfd == -1), we call select(0, ...), which is basically equal
       to sleep. */

    rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

    if (tvdiff(tvnow(), mp_start) > MULTI_PERFORM_HANG_TIMEOUT) {
      fprintf(stderr, "ABORTING TEST, since it seems "
              "that it would have run forever.\n");
      break;
    }

    switch(rc) {
    case -1:
      /* select error */
      break;
    case 0: /* timeout */
    default: /* action */
      curl_multi_perform(mcurl, &still_running);
      break;
    }
  }

  curl_slist_free_all(rcpt_list);
  curl_multi_remove_handle(mcurl, curl);
  curl_multi_cleanup(mcurl);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return 0;
}
