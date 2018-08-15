# serializable_thread_pool
普通的线程池中，所有任务都是并行执行的，不适合某些场景：
例如用线程池记录日志（多个日志文件），每次记录一条日志。
对于不同的日志文件而言，可以并行记录；但是对于同一个日志文件，需要串行记录（日志是有顺序的）。
普通的线程池不能保证属于同一文件的日志串行化记录，本线程池可以。










接口：

//任务结构体
typedef struct {
    void (*function)(void *);//任务执行函数
    void *argument;//function 参数
	void (*freer)(void *);//argument 惰性释放函数
} task_t;
 
 
//创建线程池
threadpool_t *threadpool_create(int thread_count, int queue_size, int flags);
 
 
//回调函数类型，用于检查两个任务是否可以并行执行
typedef  bool check_func(const threadpool_task_t *t1,const threadpool_task_t *t2);
//设置回调函数
int threadpool_set_concurrence_check(threadpool_t *pool,check_func *checker);
 
 
 
 
 
 
//添加任务 pool：线程池  routine：线程回调函数   arg：要传给routine的参数
//释放或delete arg的函数
//因为要在 check_func 中用到arg ，所以在routine不应该是否arg，而由线程池惰性释放
int threadpool_add(threadpool_t *pool, void (*routine)(void *),
                   void *arg, void (*free)(void *) );
                   
                   
                   
使用示例见test

