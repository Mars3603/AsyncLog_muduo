// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/CountDownLatch.h>

using namespace muduo;

CountDownLatch::CountDownLatch(int count)
  : mutex_(),  //定义了一个mutex对象。
    condition_(mutex_),  //引用关系，不负责mutex的析构
    count_(count)
{
}


/*
  生产者消费者 教科书式模型
*/
void CountDownLatch::wait()
{
  MutexLockGuard lock(mutex_);
  while (count_ > 0) {
    condition_.wait();     //
  }
}

void CountDownLatch::countDown()   //等于0 ，就通知所有的线程。  
{
  MutexLockGuard lock(mutex_);
  --count_;
  if (count_ == 0) {
    condition_.notifyAll();
  }
}

int CountDownLatch::getCount() const   
{
  MutexLockGuard lock(mutex_);
  return count_;
}

