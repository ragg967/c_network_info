#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

// Dynamic thread configuration based on system capabilities
#define MAX_PING_THREADS 128
#define MAX_SUBNET_THREADS 16
#define IP_STR_LEN 16
#define CMD_LEN 64
#define SUBNET_LEN 16

// Thread pool structure for better resource management
typedef struct {
  pthread_t *threads;
  int thread_count;
  int active_threads;
  pthread_mutex_t mutex;
  pthread_cond_t condition;
} thread_pool_t;

// Enhanced task structure with better alignment
typedef struct alignas(64) {
  char ip[IP_STR_LEN];
  _Atomic int alive;
  _Atomic int processed;
} ping_task_t;

// Subnet scanning task for parallel subnet processing
typedef struct {
  const char *subnet;
  int start_host;
  int end_host;
  int *responders;
  int thread_id;
} subnet_task_t;

// Global atomic counters for thread-safe access
static _Atomic int total_hosts_scanned = 0;
static _Atomic int total_responders = 0;
static _Atomic int subnets_scanned = 0;
static _Atomic int active_ping_threads = 0;

// Thread pool for better resource management
static thread_pool_t ping_pool = {0};
static thread_pool_t subnet_pool = {0};

// Get optimal thread count based on system
static auto get_optimal_thread_count() -> int {
  int cores = get_nprocs();
  // Use more threads than cores for I/O bound operations like ping
  return cores > 0 ? cores * 4 : 64;
}

// Initialize thread pool
static auto init_thread_pool(thread_pool_t *pool, int max_threads) -> int {
  pool->threads = malloc(max_threads * sizeof(pthread_t));
  if (!pool->threads)
    return -1;

  pool->thread_count = max_threads;
  pool->active_threads = 0;

  if (pthread_mutex_init(&pool->mutex, nullptr) != 0) {
    free(pool->threads);
    return -1;
  }

  if (pthread_cond_init(&pool->condition, nullptr) != 0) {
    pthread_mutex_destroy(&pool->mutex);
    free(pool->threads);
    return -1;
  }

  return 0;
}

// Cleanup thread pool
static auto cleanup_thread_pool(thread_pool_t *pool) -> void {
  if (pool->threads) {
    free(pool->threads);
    pool->threads = nullptr;
  }
  pthread_mutex_destroy(&pool->mutex);
  pthread_cond_destroy(&pool->condition);
}

// Enhanced ping worker with better error handling
static auto ping_worker(void *arg) -> void * {
  ping_task_t *task = (ping_task_t *)arg;
  char cmd[CMD_LEN];

  atomic_fetch_add(&active_ping_threads, 1);

  // C23: More efficient sprintf with compile-time checking
  int ret = snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1",
                     task->ip);
  if (ret < 0 || ret >= (int)sizeof(cmd)) {
    atomic_store(&task->alive, 0);
    atomic_store(&task->processed, 1);
    atomic_fetch_sub(&active_ping_threads, 1);
    return nullptr;
  }

  ret = system(cmd);
  atomic_store(&task->alive, (ret == 0) ? 1 : 0);
  atomic_store(&task->processed, 1);
  atomic_fetch_sub(&active_ping_threads, 1);

  return nullptr;
}

// Parallel subnet scanner worker
static auto subnet_worker(void *arg) -> void * {
  subnet_task_t *subnet_task = (subnet_task_t *)arg;

  printf("[Thread %d] Scanning %s.%d-%d...\n", subnet_task->thread_id,
         subnet_task->subnet, subnet_task->start_host, subnet_task->end_host);

  auto total = subnet_task->end_host - subnet_task->start_host + 1;

  // Use dynamic thread count based on system capabilities
  int ping_threads = get_optimal_thread_count();
  if (ping_threads > MAX_PING_THREADS)
    ping_threads = MAX_PING_THREADS;

  ping_task_t *tasks =
      aligned_alloc(alignof(ping_task_t), total * sizeof(ping_task_t));
  if (!tasks) {
    fprintf(stderr, "[Thread %d] Memory allocation failed\n",
            subnet_task->thread_id);
    subnet_task->responders = 0;
    return nullptr;
  }

  pthread_t *threads = malloc(ping_threads * sizeof(pthread_t));
  if (!threads) {
    fprintf(stderr, "[Thread %d] Thread allocation failed\n",
            subnet_task->thread_id);
    free(tasks);
    subnet_task->responders = 0;
    return nullptr;
  }

  int responders = 0;
  typeof(subnet_task->start_host) idx = 0;

  // Parallel ping execution with dynamic batching
  for (typeof(subnet_task->start_host) i = subnet_task->start_host;
       i <= subnet_task->end_host;) {
    typeof(ping_threads) batch = 0;

    // Fill batch with optimal thread count
    for (; batch < ping_threads && i <= subnet_task->end_host;
         ++batch, ++i, ++idx) {
      int ret = snprintf(tasks[idx].ip, sizeof(tasks[idx].ip), "%s.%d",
                         subnet_task->subnet, i);
      if (ret < 0 || ret >= (int)sizeof(tasks[idx].ip)) {
        continue;
      }

      atomic_store(&tasks[idx].alive, 0);
      atomic_store(&tasks[idx].processed, 0);

      if (pthread_create(&threads[batch], nullptr, ping_worker, &tasks[idx]) !=
          0) {
        fprintf(stderr, "[Thread %d] Ping thread creation failed for %s\n",
                subnet_task->thread_id, tasks[idx].ip);
        atomic_store(&tasks[idx].processed, 1);
        continue;
      }
    }

    // Wait for batch completion with timeout
    for (typeof(batch) j = 0; j < batch; ++j) {
      pthread_join(threads[j], nullptr);
    }

    // Progress indicator for long-running subnet scans
    if (idx % 50 == 0) {
      printf("[Thread %d] Progress: %d/%d hosts\n", subnet_task->thread_id, idx,
             total);
    }
  }

  // Collect results
  for (typeof(total) i = 0; i < total; ++i) {
    if (atomic_load(&tasks[i].alive)) {
      printf("[Thread %d] ✓ Host alive: %s\n", subnet_task->thread_id,
             tasks[i].ip);
      responders++;
    }
  }

  if (responders > 0) {
    printf("[Thread %d] → %d responders found in %s\n", subnet_task->thread_id,
           responders, subnet_task->subnet);
  } else {
    printf("[Thread %d] (no responses in %s)\n", subnet_task->thread_id,
           subnet_task->subnet);
  }

  // Update global counters atomically
  atomic_fetch_add(&total_hosts_scanned, total);
  atomic_fetch_add(&total_responders, responders);
  atomic_fetch_add(&subnets_scanned, 1);

  subnet_task->responders = responders;

  free(tasks);
  free(threads);
  return nullptr;
}

// Enhanced parallel subnet scanning
static auto scan_subnets_parallel(const char *const *subnets, int count,
                                  const char *description) -> void {
  printf("=== %s (Parallel Mode) ===\n", description);
  printf("Scanning %d subnets with up to %d parallel threads...\n\n", count,
         MAX_SUBNET_THREADS);

  // Determine optimal number of subnet threads
  int subnet_threads = count < MAX_SUBNET_THREADS ? count : MAX_SUBNET_THREADS;

  pthread_t *threads = malloc(subnet_threads * sizeof(pthread_t));
  subnet_task_t *tasks = malloc(count * sizeof(subnet_task_t));

  if (!threads || !tasks) {
    fprintf(stderr, "Failed to allocate memory for parallel scanning\n");
    free(threads);
    free(tasks);
    return;
  }

  // Process subnets in parallel batches
  for (int i = 0; i < count;) {
    int batch_size =
        (count - i) < subnet_threads ? (count - i) : subnet_threads;

    // Create batch of subnet scanning tasks
    for (int j = 0; j < batch_size; ++j, ++i) {
      tasks[i].subnet = subnets[i];
      tasks[i].start_host = 1;
      tasks[i].end_host = 254;
      tasks[i].responders = 0;
      tasks[i].thread_id = j + 1;

      if (pthread_create(&threads[j], nullptr, subnet_worker, &tasks[i]) != 0) {
        fprintf(stderr, "Failed to create thread for subnet %s\n", subnets[i]);
        continue;
      }
    }

    // Wait for current batch to complete
    for (int j = 0; j < batch_size; ++j) {
      pthread_join(threads[j], nullptr);
    }

    printf("Batch complete: %d/%d subnets processed\n\n", i, count);
  }

  free(threads);
  free(tasks);
}

// C23: Generic macro for parallel subnet scanning
#define SCAN_SUBNET_ARRAY_PARALLEL(subnets, name)                              \
  do {                                                                         \
    constexpr auto count = sizeof(subnets) / sizeof(subnets[0]);               \
    scan_subnets_parallel(subnets, count, name);                               \
  } while (0)

// Optimized subnet arrays with better organization
static const char *const common_class_c_subnets[] = {
    "192.168.1",  "192.168.0",   "192.168.2",   "192.168.3",   "192.168.4",
    "192.168.5",  "192.168.10",  "192.168.11",  "192.168.20",  "192.168.25",
    "192.168.50", "192.168.100", "192.168.101", "192.168.200", "192.168.254"};

static const char *const common_class_b_subnets[] = {
    "172.16.0", "172.16.1", "172.16.2", "172.16.10", "172.17.0", "172.17.1",
    "172.18.0", "172.19.0", "172.20.0", "172.21.0",  "172.22.0", "172.23.0",
    "172.24.0", "172.25.0", "172.30.0", "172.31.0"};

static const char *const common_class_a_subnets[] = {
    "10.0.0",  "10.0.1",   "10.0.2",   "10.0.10", "10.1.0",  "10.1.1",
    "10.1.2",  "10.1.10",  "10.2.0",   "10.2.1",  "10.10.0", "10.10.1",
    "10.20.0", "10.100.0", "10.200.0", "10.254.0"};

static auto scan_all_common_private_networks_parallel() -> void {
  auto start_time = time(nullptr);

  printf(
      "Starting PARALLEL comprehensive scan of common private networks...\n");
  printf(
      "System detected: %d CPU cores, using up to %d threads per operation\n",
      get_nprocs(), get_optimal_thread_count());
  printf("This will scan the most commonly used private IP ranges in "
         "parallel.\n\n");

  // Reset atomic counters
  atomic_store(&total_hosts_scanned, 0);
  atomic_store(&total_responders, 0);
  atomic_store(&subnets_scanned, 0);

  // Parallel scan of different network classes
  SCAN_SUBNET_ARRAY_PARALLEL(common_class_c_subnets,
                             "Common Class C Private Networks");
  SCAN_SUBNET_ARRAY_PARALLEL(common_class_b_subnets,
                             "Common Class B Private Networks");
  SCAN_SUBNET_ARRAY_PARALLEL(common_class_a_subnets,
                             "Common Class A Private Networks");

  // Localhost scan
  const char *const localhost[] = {"127.0.0"};
  scan_subnets_parallel(localhost, 1, "Localhost Network");

  auto end_time = time(nullptr);
  auto elapsed = difftime(end_time, start_time);

  // Load final atomic values
  auto final_subnets = atomic_load(&subnets_scanned);
  auto final_hosts = atomic_load(&total_hosts_scanned);
  auto final_responders = atomic_load(&total_responders);

  printf("========================================\n");
  printf("        PARALLEL SCAN COMPLETE\n");
  printf("========================================\n");
  printf("Total subnets scanned: %d\n", final_subnets);
  printf("Total hosts scanned: %d\n", final_hosts);
  printf("Total responders found: %d\n", final_responders);
  printf("Scan duration: %.0f seconds\n", elapsed);
  printf("System cores utilized: %d\n", get_nprocs());
  if (elapsed > 0) {
    printf("Average rate: %.1f hosts/second\n", final_hosts / elapsed);
    printf("Parallel efficiency: %.1fx speedup\n",
           final_hosts / elapsed / get_nprocs());
  }
  printf("========================================\n");
}

// Ultra-parallel full Class C range scanner
static auto scan_full_class_c_range_parallel() -> void {
  printf("=== Ultra-Parallel Full 192.168.x.x Range Scan ===\n\n");
  printf("This will scan ALL 192.168.x.x networks (256 subnets) in parallel\n");
  printf("Using maximum parallelization...\n\n");

  // Generate all 256 subnets
  char **all_subnets = malloc(256 * sizeof(char *));
  if (!all_subnets) {
    fprintf(stderr, "Memory allocation failed\n");
    return;
  }

  for (int i = 0; i < 256; i++) {
    all_subnets[i] = malloc(16);
    if (!all_subnets[i]) {
      // Cleanup on failure
      for (int j = 0; j < i; j++) {
        free(all_subnets[j]);
      }
      free(all_subnets);
      return;
    }
    snprintf(all_subnets[i], 16, "192.168.%d", i);
  }

  // Cast to const for compatibility
  const char *const *const_subnets = (const char *const *)all_subnets;
  scan_subnets_parallel(const_subnets, 256, "Full 192.168.x.x Range");

  // Cleanup
  for (int i = 0; i < 256; i++) {
    free(all_subnets[i]);
  }
  free(all_subnets);
}

// Single subnet scan (original functionality, now with better threading)
static auto scan_single_subnet_parallel(const char *base, int start_host,
                                        int end_host) -> void {
  subnet_task_t task = {.subnet = base,
                        .start_host = start_host,
                        .end_host = end_host,
                        .responders = 0,
                        .thread_id = 1};

  subnet_worker(&task);
}

auto main() -> int {
  printf("Multi-Threaded Private Network Scanner (C23 Optimized)\n");
  printf("=====================================================\n");
  printf("System: %d CPU cores detected\n", get_nprocs());
  printf("Max ping threads: %d\n", get_optimal_thread_count());
  printf("Max subnet threads: %d\n\n", MAX_SUBNET_THREADS);

  printf("Select scanning mode:\n");
  printf("1. Parallel scan of all common private networks (RECOMMENDED)\n");
  printf("2. Ultra-parallel full 192.168.x.x range (256 subnets)\n");
  printf("3. Single subnet scan (optimized threading)\n");
  printf("4. Quick parallel scan (likely networks)\n");
  printf("Enter choice (1-4): ");

  int choice;
  if (scanf("%d", &choice) != 1) {
    printf("Invalid input\n");
    return EXIT_FAILURE;
  }

  printf("\n");

  switch (choice) {
  case 1:
    scan_all_common_private_networks_parallel();
    break;

  case 2:
    scan_full_class_c_range_parallel();
    break;

  case 3: {
    char base[SUBNET_LEN];
    int start, end;

    printf("Enter subnet base (e.g., 192.168.1): ");
    if (scanf("%15s", base) != 1) {
      printf("Invalid input\n");
      return EXIT_FAILURE;
    }

    printf("Enter start host (1-254): ");
    if (scanf("%d", &start) != 1 || start < 1 || start > 254) {
      printf("Invalid start host\n");
      return EXIT_FAILURE;
    }

    printf("Enter end host (1-254): ");
    if (scanf("%d", &end) != 1 || end < 1 || end > 254 || end < start) {
      printf("Invalid end host\n");
      return EXIT_FAILURE;
    }

    printf("\n");
    scan_single_subnet_parallel(base, start, end);
    break;
  }

  case 4: {
    printf("=== Quick Parallel Scan of Likely Networks ===\n\n");
    const char *const quick_subnets[] = {"192.168.1", "192.168.0", "10.0.0",
                                         "172.16.0"};
    scan_subnets_parallel(quick_subnets, 4, "Quick Scan Networks");
    break;
  }

  default:
    printf("Invalid choice\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
