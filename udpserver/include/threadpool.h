#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <iostream>
#include <future>

const int TASK_MAX_THREADHOLD = 256;//任务队列上限阈值

enum class PoolMode//枚举
{
	MODE_FIXED,//固定数量的线程
	MODE_CACHED,//线程数量可动态增长
};

//线程类型
class Thread
{
public:
	//线程函数对象类型
	using ThreadFunc = std::function<void()>;//using别名指定一个类型，ThreadFunc表示返回值是void并且没有参数的函数类型
	//线程构造
	Thread(ThreadFunc func)//func用来绑定Threadpool中的threadFunc作为线程函数
		:func_(func) {}
		
		// 线程析构
	~Thread() = default;
	//启动线程
	void start()
	{
		//创建一个线程去执行一个线程函数
		std::thread t(func_);//C++11来说 用基本线程类thread来创建线程对象t，传入线程函数func_
		t.detach();//设置分离线程 pthread_detach pthread_t设置成分离线程
		//t是函数内部局部对象，出了这个大括号t生命周期结束被释放，但是func_和t分离，func_继续运行。
	}
private:
	ThreadFunc func_;//用类的成员变量 func_来接收Thread(ThreadFunc func)中传进来的func
};

//线程池类型
class ThreadPool
{
public:
	//线程池构造
	//线程池构造
	ThreadPool()
		: initThreadSize_(0)//初始线程数量，此处是创建线程池的初始化，后面在start中重新赋值
		, taskSize_(0)//实时任务的数量
		, taskQueMaxThreadHold_(TASK_MAX_THREADHOLD)//任务队列上限阈值
		, poolMode_(PoolMode::MODE_FIXED)//默认工作模式
		, isPoolRunning_(false)
	{}
	//线
	//线程池析构
	~ThreadPool() = default;

	void setMode(PoolMode mode)//设置线程池工作模式
	{
		poolMode_ = mode;
	}
	////设置初始的线程数量

	//void setInitThreadSize_(int size);
	//设置task任务队列上限阈值
	void setTaskQueMaxThreadHold_(int threadhold)
	{
		taskQueMaxThreadHold_ = threadhold;
	}
	//给线程池提交任务
	template<typename Func,typename... Args >
	auto submitTask(Func&& func , Args&&...args) -> std::future<decltype(func(args...))>
	{
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType>result = task->get_future();

		//获取锁,unique_lock类似于unique_ptr，可以自动解锁不用手动unlock
		std::unique_lock<std::mutex> lock(taskQueMtx_);
	
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreadHold_; }))
		{
			//表示notFull_等待1s，条件依然没有满足
			std::cerr << "task queue is full ,submmit task fail." << std::endl;
			
			auto task = std::make_shared<std::packaged_task<RType()>>
				(
					[]() -> RType
								{
									return RType(); 
								}
				);
			(*task)();
			return task->get_future();
		}
		taskQue_.emplace([task]() {(*task)();});
		taskSize_++;
		//因为放了新任务，任务队列肯定不空了，在notEmpty_上进行通知,分配线程执行任务
		notEmpty_.notify_all();
		//在这定义result

		return result;//TODO: (未实现如果在自己定义的拷贝构造函数中使用move会怎么
	}

	void start(int initThreadSize = 4)//开启线程池，在声明时直接给定初始线程数量
	{

		isPoolRunning_ = true;//设置线程池运行状态
		//记录初试线程个数
		initThreadSize_ = initThreadSize;//已经有初值
		//创建线程对象
		for (int i = 0; i < initThreadSize_; i++)
		{
			//创建thread线程对象的时候，把线程函数给到thread线程对象
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
			//对应的Thread中要提供相应的构造函数，使用function接收bind返回的函数对象
			threads_.emplace_back(std::move(ptr));//放入容器中要拷贝构造一份ptr，但是unique不允许拷贝，使用move资源转移
		}
		//启动所有线程  std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start();//需要去执行一个线程函数
			//这个start区分 Threadpool的start ，此处是Thread类中的成员函数start，在这里真正启动线程
		}
	}
		
		ThreadPool(const ThreadPool&) = delete;//不允许拷贝构造，使用线程池只允许重新定义 
	ThreadPool& operator = (const ThreadPool&) = delete;
private:
	//定义线程函数，线程函数没有在Thread类中，而是在线程池中直接指定
	void threadFunc()
	{
		//std::cout << "begin threadFunc tid" << std::this_thread::get_id() << std::endl;
		//
		//std::cout << "end threadFunc tid" << std::this_thread::get_id() << std::endl;
		for (;; )//每一个线程都一直取任务 
		{
			Task task;
			{
				//先获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);
				//	std::cout << " tid :" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;

					//等待notEmpty条件
				notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
				//std::cout << " tid :" << std::this_thread::get_id() << "获取任务成功" << std::endl;

				//从任务队列的头部取一个任务出来
				task = taskQue_.front();
				taskQue_.pop();//取出来把任务删除掉
				taskSize_--;

				//如果依然有剩余任务，继续通知其他线程执行任务，其他线程由等待状态变成阻塞状态准备抢锁
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}
				//取出一个任务，进行通知,通知可以继续提交生产任务
				notFull_.notify_all();
			}	//unique_lock出这个作用域就把锁释放掉
			//从当前负责执行这个任务
			if (task != nullptr)//任务不为空执行任务
			{
				task();
			}
			//std::cout << "end threadFunc tid" << std::this_thread::get_id() << std::endl;

		}
	}
	//bool checkRunningState() const;

private://linux 下要做相应修改 指出 c++使用的版本
	//需要使用智能指针，因为裸指针没有析构函数，要手动析构，使用智能指针自动析构
	std::vector<std::unique_ptr< Thread>> threads_;//线程列表

	size_t initThreadSize_;//初试的线程数量

	using Task = std::function<void()>;
	std::queue< Task > taskQue_;//任务队列
	std::atomic_int taskSize_;//任务的数量

	int taskQueMaxThreadHold_;//任务队列数量上限阈值
	std::mutex taskQueMtx_;//保证任务队列的线程安全
	std::condition_variable notFull_;//表示任务队列不满
	std::condition_variable notEmpty_;//表示任务队列不空
	PoolMode poolMode_;//当前线程池的工作模式
	std::atomic_bool isPoolRunning_; //当前线程池的启动状态
};

#endif // !THREADPOOL_H
