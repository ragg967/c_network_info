
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static void find_ip() {
  char ip[16];
  int start = 1, end = 254;          // Range for last octet
  const char *subnet = "192.168.50"; // Use your network's subnet
  int responders = 0;

  for (int i = start; i <= end; i++) {
    snprintf(ip, sizeof(ip), "%s.%d", subnet, i);

    // Build ping command: -c 1 (one packet), -W 1 (1 second timeout)
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1", ip);

    int ret = system(cmd);
    if (ret == 0) {
      printf("Host alive: %s\n", ip);
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
