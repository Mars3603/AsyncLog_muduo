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
  threads_.reserve(numThreads);  //��ʼ���̳߳ص�������  
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i);
    threads_.push_back(new muduo::Thread(     //new Thread ����һ���̣߳����̳߳��е�ָ��ŵ� ptr_vector�У��������߳����󶨵ĺ���ʱ runInThread,�̵߳��������̳߳ص�����+ id 
          boost::bind(&ThreadPool::runInThread, this), name_+id));
    threads_[i].start();  // �����̣߳����� runInThread��ִ�С�
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
  if (threads_.empty())   // ����̳߳��еĶ��г���Ϊ�գ������ֱ��ִ����������ˣ�����Ҫ������ӵ��������
  {
    task();
  }
  else
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(task);   // ������в�Ϊ�յ�ǰ���£��������
    cond_.notify();
  }
}

ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_)    //�ȴ������������̳߳ؽ�����
  {
    cond_.wait();
  }
  Task task;                       
  if(!queue_.empty())
  {
    task = queue_.front();   //һ��������������ȡ������
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
      if (task)    //�����棬ȡ�������ִ������
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

