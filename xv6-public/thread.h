struct stat;
struct rtcdate;

int thread_create(thread_t *thread,void(*func) (void*),void* arg);
int thread_join(thread_t thread,void **retval);
int thread_exit(void *retval);
