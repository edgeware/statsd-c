#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "logd.h"
#include "uthash/utlist.h"

#define LOGD_FLUSH_INTERVAL 5

/* We need a list of logd resources for the flush thread */
struct logd_resource *resources = NULL;

static void logd_write_unbuffered(logd_resource_t resource, char *msg, int len);

void p_thread_logd(void *ptr)
{
  while(true) {
    sleep(LOGD_FLUSH_INTERVAL);
    printf("logd flush now\n");
    struct logd_resource *resource;
    LL_FOREACH(resources, resource) {
      printf("flushing resource %s\n", resource->socket_name);
      if(resource->index > 0) {
        logd_write_unbuffered(resource, resource->buf, resource->index);
        resource->index = 0;
      }
    }
  }
}

void logd_init_resource(logd_resource_t resource, const char *socket_file)
{
  if(!resource->started)
    {
      resource->socket_name = (char*)socket_file;
      resource->index = 0;
      resource->fd = -1;
      LL_PREPEND(resources, resource);
      resource->started = true;
    }
}

void logd_connect_resource(logd_resource_t resource)
{
  int len;
  struct sockaddr_un remote;

  if ((resource->fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
    {
      printf("failed to create socket for logd resource %s (%m)\n", resource->socket_name);
      return;
    }
  remote.sun_family = AF_UNIX;
  strcpy(remote.sun_path, resource->socket_name);
  len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(resource->fd, (struct sockaddr *)&remote, len) == -1)
    {
      close(resource->fd);
      resource->fd = -1;
      printf("failed to connect to logd resource %s (%m)\n", resource->socket_name);
    }
}

static void
logd_write_unbuffered(logd_resource_t resource, char *msg, int len)
{
  if (resource->fd == -1)
    logd_connect_resource(resource);
  if (resource->fd == -1)
    {
      printf("unable to connect to logd resource %s\n", resource->socket_name);
      return;
    }
  
  if (send(resource->fd, msg, len, 0) == -1)
    {
      /* This can happen if logd was restarted */
      logd_connect_resource(resource);
      if (resource->fd != -1)
	if (send(resource->fd, msg, len, 0) == -1) 
	  {
	    close(resource->fd);
	    resource->fd = -1;
	    printf("failed to write to logd resource %s (%m)\n", resource->socket_name);
	  }
    }
}

void
logd_write(logd_resource_t resource, char *msg, int len)
{
  if(len > logd_buf_size)
    {
      printf("too long log line (%d bytes), data dropped\n", len);
      return;
    }

  /* Time to send buffer to logd? */
  if(logd_buf_size - resource->index < len)
    {
      logd_write_unbuffered(resource, resource->buf, resource->index);
      resource->index = 0;
    }

  memcpy(resource->buf + resource->index, msg, len);
  resource->index += len;
}

