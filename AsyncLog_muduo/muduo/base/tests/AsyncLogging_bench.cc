#include <muduo/base/AsyncLogging.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>
#include <sys/resource.h>
#include <muduo/base/ThreadPool.h>

int kRollSize = 500*1000*1000;

muduo::AsyncLogging* g_asyncLog = NULL;

void asyncOutput(const char* msg, int len) {
  g_asyncLog->append(msg, len);
}

void bench() {
  int cnt = 0;
  const int kBatch = 300*10000;
  for (int i = 0; i < kBatch; ++i) {
    LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz "
      << " "
      << cnt;
    ++cnt;
  }
  struct timespec ts = { 0, 500*1000*1000 };
  nanosleep(&ts, NULL);
}


int main(int argc, char* argv[]) { 
	
	{
    // set max virtual memory to 2GB.
    size_t kOneGB = 1000*1024*1024;
    rlimit rl = { 2*kOneGB, 2*kOneGB };
    setrlimit(RLIMIT_AS, &rl);
  }
	
  printf("pid = %d\n", getpid());

  char name[256] = { '\0' };
  strncpy(name, argv[0], sizeof name - 1);
  muduo::AsyncLogging log(::basename(name), kRollSize);
  log.start();
  g_asyncLog = &log;
  muduo::Logger::setOutput(asyncOutput);    // 业务线程通过此函数将产生的日志消息添加到日志缓冲区中
  
  
  muduo::Timestamp start(muduo::Timestamp::now());
  muduo::ThreadPool pool("pool");
  pool.start(20);
  for(int i=0;i<20;i++){
  	pool.run(bench);	
  }
  muduo::Timestamp end(muduo::Timestamp::now());
  
  	
  printf("%f\n", timeDifference(end, start));
  
  return 0;
}
