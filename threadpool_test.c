#include <stdlib.h>
#include <unistd.h>
#include "threadpool.h"
#include <iostream>

static int counter1 = 0;
static int counter2 = 0;
#define atomicIncr(var,count) __sync_add_and_fetch(&var,(count))


using namespace std;
void thread_1_callback(void * arg){
	int count = *(int *)arg;
	//pthread_t tid = pthread_self();
	//cout<< "in thread "<<tid<<",func 1"<< endl;		
	for(int i=0;i<count;i++) atomicIncr(counter1,1);
}

void freer(void *arg){
	delete (char *)arg;
}
void thread_2_callback(void * arg){
	int count = *(int *)arg;
	//pthread_t tid = pthread_self();
	
	//cout<< "in thread "<<tid<<",func 2"<< endl;	
	for(int i=0;i<count;i++){
		int t = counter2;
		t++;
		counter2 = t;
	}
}

bool concurrency_check(const threadpool_task_t *t1,const threadpool_task_t *t2){
	if(t1->function == &thread_2_callback && t2->function == &thread_2_callback) return false;
	return true;
}

int main(int argc,char **argv){

 int nthd = 8;
 int ntask = 1024;
 int flag = 0;
 threadpool_t *pool = threadpool_create(nthd,ntask,flag);
 threadpool_set_concurrence_check(pool,concurrency_check);

 for(int i =0;i<10000;i++){
	int ret = -1;
	do{
		ret = threadpool_add(pool, thread_1_callback,new int(i), freer);
	}while(ret < 0);

	do{
		ret = threadpool_add(pool, thread_2_callback,new int(i), freer);
	}while(ret < 0);
	

 }

 //

  for(int i =0;i<10000;i++){
	int ret = -1;
	do{
		ret = threadpool_add(pool, thread_1_callback,new int(i), freer);
	}while(ret < 0);

	do{
		ret = threadpool_add(pool, thread_2_callback,new int(i), freer);
	}while(ret < 0);
	

 }
 sleep(5);

 while(counter1 != 99990000 || counter2 != 99990000);
 
 
 cout << "============================"<<endl;
 cout<<"counter1 = "<<counter1 <<endl;
 cout<<"counter2 = "<<counter2 <<endl;
 
}
