// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/ThreadPool.h>

#include <muduo/base/Exception.h>

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>

using namespace muduo;

ThreadPool::ThreadPool(const string& name)
  : mutex_(),
    cond_(mutex_),
    name_(name),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads);  //初始化线程池的数量。  
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i);
    threads_.push_back(new muduo::Thread(     //new Thread 创建一个线程，把线程池中的指针放到 ptr_vector中，创建的线程所绑定的函数时 runInThread,线程的名称是线程池的名称+ id 
          boost::bind(&ThreadPool::runInThread, this), name_+id));
    threads_[i].start();  // 启动线程，跳到 runInThread中执行。
  }
}

void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  running_ = false;
  cond_.notifyAll();
  }
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1));
}

void ThreadPool::run(const Task& task)
{
  if (threads_.empty())   // 如果线程池中的队列长度为空，则可以直接执行这个任务了，不需要将其添加到任务队列
  {
    task();
  }
  else
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(task);   // 任务队列不为空的前提下，添加任务
    cond_.notify();
  }
}

ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_)    //等待任务到来或者线程池结束。
  {
    cond_.wait();
  }
  Task task;                       
  if(!queue_.empty())
  {
    task = queue_.front();   //一旦有任务到来，就取出任务。
    queue_.pop_front();
  }
  return task;
}

void ThreadPool::runInThread()
{
  try
  {
    while (running_)  
    {
      Task task(take());
      if (task)    //接上面，取出任务后执行任务。
      {
        task();
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

