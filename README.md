# serializable_thread_pool
这是一种特殊的线程池，适用于特定任务场景-- 例如mysql binlog重做。重做的时候，对于属于同一个表的binlog，需要串行执行（要保证执行顺序）。但对于不同表的binlog，需要并行执行提高效率。











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
 
 
 
 
 
 
//添加任务 
//pool：线程池  routine：线程回调函数   arg：要传给routine的参数

//free ：释放或delete arg的函数

//因为要在 check_func 中用到arg ，所以在routine不应该释放arg，而由线程池惰性释放

int threadpool_add(threadpool_t *pool, void (*routine)(void *),
                   void *arg, void (*free)(void *) );
                                    
 
使用方法与普通的线程池相同，不过要在添加任务之前，设置并行检查函数。
详细代码见 test

本线程池已经生产环境检验。

