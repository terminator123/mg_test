#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct mt_share_data {
	int id;
	int id_max;
	int * dat_list;
	pthread_mutex_t calc_mutex;
};

struct mt_share_data2 {
	int base;
	int status;
	int uniq_id;
	int id_max;
	int count;
	//
	int * dat_list;
	int value;
	//
	pthread_mutex_t src_mutex;
	//
	pthread_mutex_t calc_mutex;
	pthread_cond_t calc_cond;
	pthread_mutex_t update_mutex;
	pthread_cond_t update_cond;
};

void * calc_worker(void * dat) {
	struct mt_share_data * share_data = (struct mt_share_data *)dat;
	int id = 0;
	int count = 0;
	//
	while(1) {
		pthread_mutex_lock(&(share_data->calc_mutex));
		id = share_data->id;
		share_data->id += 1;
		pthread_mutex_unlock(&(share_data->calc_mutex));
		//
		if(id >= share_data->id_max) {
			break;
		}
		//
		printf("%zu --> %d %d, count: %d\n", pthread_self(), id, share_data->dat_list[id], count);
		count += 1;
	}
	//
	return 0;
}

int test1(int dat_size, int thread_num) {
	struct mt_share_data share_data;
	int i = 0;
	pthread_t * pid_list = 0;
	//
	share_data.id = 0;
	share_data.id_max = dat_size;
	if((share_data.dat_list = (int *)malloc(sizeof(int) * dat_size)) == NULL) {
		printf("share_data.dat_list malloc error: %zu\n", sizeof(int) * dat_size);
		exit(1);
	}
	pthread_mutex_init(&(share_data.calc_mutex), NULL);
	for(i = 0; i < dat_size; i ++) {
		share_data.dat_list[i] = i;
	}
	//
	if((pid_list = (pthread_t *)malloc(sizeof(pthread_t) * thread_num)) == NULL) {
		printf("pid_list malloc error: %zu\n", sizeof(pthread_t) * thread_num);
		exit(1);
	}
	//
	for(i = 0; i < thread_num; i ++) {
		pthread_create(&(pid_list[i]), NULL, calc_worker, &share_data);
	}
	for(i = 0; i < thread_num; i ++) {
		pthread_join(pid_list[i], NULL);
	}
	//
	pthread_mutex_destroy(&(share_data.calc_mutex));
	//
	free(share_data.dat_list);
	free(pid_list);
	//
	return 0;
}

/////////////////////////////////////////////////////////////////
void * calc2_worker(void * dat) {
	struct mt_share_data2 * share_data2 = (struct mt_share_data2 *)dat;
	int id = 0;
	//
	while(share_data2->status > 0) {
		//做一些计算工作
		while(1) {
			pthread_mutex_lock(&(share_data2->src_mutex));
			id = share_data2->uniq_id;
			share_data2->uniq_id += 1;
			pthread_mutex_unlock(&(share_data2->src_mutex));
			//
			if(id >= share_data2->id_max) {
				break;
			}
			share_data2->dat_list[id] += share_data2->base;
			printf("worker: %zu, base now: %d, id: %d\n", pthread_self(), share_data2->base, id);
		}
		printf("worker: %zu, work finish\n", pthread_self());
		//计算完成，发送更新信号
		pthread_mutex_lock(&(share_data2->update_mutex));
		share_data2->count -= 1;
		pthread_cond_signal(&(share_data2->update_cond));
		pthread_mutex_lock(&(share_data2->calc_mutex));//这一行为什么一定要放在这里？而不是放在解锁之后
		pthread_mutex_unlock(&(share_data2->update_mutex));
		//等待计算信号
		//pthread_mutex_lock(&(share_data2->calc_mutex));
		printf("worker: %zu, wait to calc\n", pthread_self());
		pthread_cond_wait(&(share_data2->calc_cond), &(share_data2->calc_mutex));//为什么所有的计算线程都在等待同一个信号？这样做会不会有问题？
		pthread_mutex_unlock(&(share_data2->calc_mutex));
		printf("worker: %zu, ready to run\n", pthread_self());
	}
	//
	return 0;
}

int test2(int dat_size, int thread_num) {
	struct mt_share_data2 share_data2;
	int i = 0;
	pthread_t * pid_list = 0;
	//
	share_data2.base = 1;
	share_data2.status = 1; //1为进行中；0为任务完成
	share_data2.uniq_id = 0;
	share_data2.id_max = dat_size;
	share_data2.count = thread_num;
	if((share_data2.dat_list = (int *)malloc(sizeof(int) * dat_size)) == NULL) {
		printf("share_data2.dat_list malloc error: %zu", sizeof(int) * dat_size);
		exit(1);
	}
	share_data2.value = 0;
	pthread_mutex_init(&(share_data2.src_mutex), NULL);
	pthread_mutex_init(&(share_data2.calc_mutex), NULL);
	pthread_cond_init(&(share_data2.calc_cond), NULL);
	pthread_mutex_init(&(share_data2.update_mutex), NULL);
	pthread_cond_init(&(share_data2.update_cond), NULL);
	//
	for(i = 0; i < dat_size; i ++) {
		share_data2.dat_list[i] = rand() % 7;
	}
	//
	if((pid_list = (pthread_t *)malloc(sizeof(pthread_t) * thread_num)) == NULL) {
		printf("pid_list malloc error: %zu", sizeof(pthread_t) * thread_num);
		exit(1);
	}
	//
	printf("main thread ready\n");
	//主线程就绪，启动工作线程
	for(i = 0; i < thread_num; i ++) {
		pthread_create(&(pid_list[i]), NULL, calc2_worker, &share_data2);
	}
	//主线程进入工作状态
	while(share_data2.status > 0) {
		pthread_mutex_lock(&(share_data2.update_mutex));
		while(share_data2.count > 0) {
			pthread_cond_wait(&(share_data2.update_cond), &(share_data2.update_mutex));
			//share_data2.count -= 1;//为什么不能在这里修改count，而是在工作线程里面修改？
			printf("main worker, count: %d\n", share_data2.count);//////////////可以关注一下这一行的输出结果
		}
		printf("main wake\n");
		pthread_mutex_unlock(&(share_data2.update_mutex));
		//更新数据
		for(i = 0; i < dat_size; i ++) {
			share_data2.value += share_data2.dat_list[i];
		}
		printf("main worker, value: %d, base: %d\n", share_data2.value, share_data2.base);
		if(share_data2.value > 1000000) {
			share_data2.status = 0;
		} else {
			//更新数据
			share_data2.base *= 2;
			//任务重置
			share_data2.count = thread_num;
			share_data2.uniq_id = 0;
		}
		//
		pthread_mutex_lock(&(share_data2.calc_mutex));//为什么这里一定要拿到计算锁，去掉会怎么样？主线程和工作线程都在抢计算锁，会不会有问题？
		printf("main worker get calc_mutex\n");
		//唤醒所有计算线程
		pthread_cond_broadcast(&(share_data2.calc_cond));
		pthread_mutex_unlock(&(share_data2.calc_mutex));
	}
	printf("main worker, done\n");
	//等待工作线程退出
	for(i = 0; i < thread_num; i ++) {
		pthread_join(pid_list[i], NULL);
	}
	//释放资源
	pthread_mutex_destroy(&(share_data2.src_mutex));
	pthread_mutex_destroy(&(share_data2.calc_mutex));
	pthread_cond_destroy(&(share_data2.calc_cond));
	pthread_mutex_destroy(&(share_data2.update_mutex));
	pthread_cond_destroy(&(share_data2.update_cond));
	//
	free(pid_list);
	free(share_data2.dat_list);
	//
	return 0;
}

///////////////////////////////////////////////////////////////////////////
int main(int argc, char * argv[]) {
	int dat_size = 0;
	int thread_num = 0;
	//
	if(argc < 3) {
		printf("demo: %s <data_size> <thread_num>\n", argv[0]);
		return 0;
	}
	//
	dat_size = atoi(argv[1]);
	thread_num = atoi(argv[2]);
	//
	test1(dat_size, thread_num);
	test2(dat_size, thread_num);
	//
	return 0;
}

//编译方法：
//gcc -O2 -g -o mt multi_thread.c -lpthread -Wall
//运行：
//./mt 10 3

