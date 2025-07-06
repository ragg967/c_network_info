#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_THREADS 64

typedef struct {
  int ip_last_octet;
  const char *subnet;
  int alive;
} ping_task_t;

void *ping_worker(void *arg) {
  ping_task_t *task = (ping_task_t *)arg;
  char ip[16];
  snprintf(ip, sizeof(ip), "%s.%d", task->subnet, task->ip_last_octet);

  char cmd[64];
  snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1", ip);

  int ret = system(cmd);
  task->alive = (ret == 0) ? 1 : 0;
  return NULL;
}

static void find_ip() {
  int start = 1, end = 254;
  const char *subnet = "192.168.50";
  int total = end - start + 1;
  ping_task_t tasks[254];
  pthread_t threads[MAX_THREADS];
  int responders = 0;

  int idx = 0;
  for (int i = start; i <= end;) {
    int batch = 0;
    for (; batch < MAX_THREADS && i <= end; ++batch, ++i, ++idx) {
      tasks[idx].ip_last_octet = i;
      tasks[idx].subnet = subnet;
      tasks[idx].alive = 0;
      pthread_create(&threads[batch], NULL, ping_worker, &tasks[idx]);
    }
    for (int j = 0; j < batch; ++j) {
      pthread_join(threads[j], NULL);
    }
  }

  for (int i = 0; i < total; ++i) {
    if (tasks[i].alive) {
      printf("Host alive: %s.%d\n", subnet, tasks[i].ip_last_octet);
      responders++;
    }
  }
  printf("\nTotal responders: %d\n", responders);
}

int main() {
  printf("\n");
  find_ip();
  return 0;
}
