#ifndef LOGD_H
#define LOGD_H

#define logd_buf_size 4096

#warning "DO WE NEED STARTED?"
struct logd_resource
{
  int fd;
  char *socket_name;
  char buf[logd_buf_size];
  int index;
  bool started;
  struct logd_resource *next; /* For flush thread */
};

typedef struct logd_resource *logd_resource_t;

void p_thread_logd(void *ptr);
void logd_write(logd_resource_t resource, char *msg, int len);
void logd_init_resource(logd_resource_t sock_resource, const char *socket_file);

#endif
