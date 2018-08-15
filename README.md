# serializable_thread_pool
这是一种特殊的线程池，适用于特定场景-- 在所有的任务当中，有需要串行化执行的一个或多个子集。

在普通的线程池中，所有任务都是并行执行的，对于其中一些有关联的，需要串行执行的任务，则不适合使用这种线程池；

例如一些任务涉及到写文件。对于写同一文件的不同任务，显然需要串行执行。

然而普通线程池并不能满足这种场景--在普通线程池中所有任务都是并发执行的。

可串行化线程池正是为类似的场景准备的。










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

