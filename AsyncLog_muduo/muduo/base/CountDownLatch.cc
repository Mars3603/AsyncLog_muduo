// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/CountDownLatch.h>

using namespace muduo;

CountDownLatch::CountDownLatch(int count)
  : mutex_(),  //������һ��mutex����
    condition_(mutex_),  //���ù�ϵ��������mutex������
    count_(count)
{
}


/*
  ������������ �̿���ʽģ��
*/
void CountDownLatch::wait()
{
  MutexLockGuard lock(mutex_);
  while (count_ > 0) {
    condition_.wait();     //
  }
}

void CountDownLatch::countDown()   //����0 ����֪ͨ���е��̡߳�  
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

