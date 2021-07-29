#include <muduo/base/AsyncLogging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>

using namespace muduo;

AsyncLogging::AsyncLogging(const string& basename,
                           size_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(boost::bind(&AsyncLogging::threadFunc, this), "Logging"),
    latch_(1),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_()
{
  currentBuffer_->bzero();  
  nextBuffer_->bzero();
  buffers_.reserve(16); 
}

/**
 * @description: 业务线程通过此函数将产生的日志消息添加到日志缓冲区中，每当写满一个缓冲区时即通知日志线程进行写入操作
 * @param {const} char
 * @param {int} len
 */
void AsyncLogging::append(const char* logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len)
  {
    // 当前缓冲区未满，将数据追加到末尾
    currentBuffer_->append(logline, len);
  }
  else
  { 
    // 当前缓冲区已满，将当前缓冲区添加到待写入文件的已填满的缓冲区列表
    buffers_.push_back(std::move(currentBuffer_));    

    // 将当前缓冲区设置为预备缓冲区
    if (nextBuffer_)  // nextBuffer_是一个指针，同样也是指向的一个缓冲区。
    {
      currentBuffer_ = std::move(nextBuffer_); // 移动语义
    }
    else
    {
      // 这种情况，极少发生，前端写入速度太快，一下子把两块缓冲区都写完，
	  // 那么，只好分配一块新的缓冲区。
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);
    cond_.notify(); 
  }
}


void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  latch_.countDown();
  LogFile output(basename_, rollSize_, false);  //LogFile中嵌套了一个File类，初始化的时候会打开一个文件。
  // 准备两块空闲缓冲区
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();

  BufferVector buffersToWrite; //写缓冲队列。
  buffersToWrite.reserve(16);
  while (running_)
  {
    assert(newBuffer1 && newBuffer1->length() == 0); // 断言，排除异常情况。 为空，或者长度为0。
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      muduo::MutexLockGuard lock(mutex_);  
      if (buffers_.empty())  // unusual usage!（注意，这里是一个非常规用法）
      {
        cond_.waitForSeconds(flushInterval_); // 等待前端写满了一个或者多个buffer,或者一个超时时间到来
      }
      buffers_.push_back(std::move(currentBuffer_));  
      currentBuffer_ = std::move(newBuffer1); // 将空闲的newBuffer1置为当前缓冲区
      buffersToWrite.swap(buffers_); // 只是交换指针，不影响效率；buffers_与buffersToWrite交换，这样后面的代码可以在临界区之外安全地访问buffersToWrite
      if (!nextBuffer_)
      {
        nextBuffer_ = std::move(newBuffer2); // 确保前端（业务线程）始终有一个预备buffer可供调配，
                                                              // 减少前端临界区分配内存的概率，缩短前端临界区长度。
        // 因为当前端发现预备缓冲区为空的时候，会分配一块新的缓冲区。 
      }
    }  // 临界区作用域


    // 不需要再临界区中 对buffersToWrite进行操作，就一个线程读取，不会发生线程安全问题。
    assert(!buffersToWrite.empty());

    // 消息堆积
    // 前端陷入死循环，拼命发送日志消息，超过后端的处理能力，这就是典型的生产速度
    // 超过消费速度问题，会造成数据在内存中堆积，严重时引发性能问题（可用内存不足）
    // 或程序崩溃（分配内存失败）
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);
      output.append(buf, static_cast<int>(strlen(buf)));
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end()); // 丢掉多余日志，以腾出内存，仅保留 前面两块 缓冲区
    }

  	for(const auto& buffer : buffersToWrite) {
        output.append(buffer->data(), buffer->length());
    }
    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2); // 仅保存两个buffer，用于newBuffer1与newBuffer2
    }

    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer1->reset();
    }

    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    buffersToWrite.clear();
    output.flush();
  }
  output.flush();
}

