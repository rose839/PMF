# PMF
pmf(process management frame)是一种多进程的网络请求处理模型，它由一个master进程和多个worker进程组成。

1、master进程启动时会创建一个socket，但是不会接收、处理请求，而是由fork出的worker子进程完成请求的接收及处理。
master进程的主要工作是管理worker进程，负责fork或杀掉worker进程，比如当请求比较多worker进程处理不过来时，
master进程会尝试fork新的worker进程进行处理，而当空闲worker进程比较多时则会杀掉部分子进程，避免占用、浪费系统资源。
worker进程的主要工作是处理请求，每个worker进程会竞争地Accept请求，接收成功后处理请求，处理完成后关闭请求，继续等待新的连接，一个worker进程只能处理一个请求，只有将一个请求处理完成后才会处理下一个请求。
式

2、master进程与worker进程之间不会直接进行通信，master通过共享内存获取worker进程的信息，比如worker进程当前状态、已处理请求数等，master进程通过发送信号的方式杀掉worker进程。
pmf可以同时监听多个端口，每个端口对应一个worker pool，每个pool下对应多个worker进程，这些归属不同pool的worker进程仍由一个master管理。

3、在pmf.conf中通过[pool name]声明一个worker pool，每个pool各自配置监听的地址、进程管理方式、worker进程数等。

4、Fpm三种不同的进程管理方式，具体要使用哪种模式可以在conf配置中通过pm指定，例如：pm＝dynamic。
- 静态模式（static）：这种方式比较简单，在启动时master根据pm．max_children配置fork出相应数量的worker进程，也就是worker进程数是固定不变的。
- 动态模式（dynamic）：这种模式比较常用，在Fpm启动时会根据pm．start_servers配置初始化一定数量的worker。运行期间如果master发现空闲worker数低于pm．min_spare_servers配置数（表示请求比较多，worker处理不过来了）则会fork worker进程，但总的worker数不能超过pm．max_children；如果master发现空闲worker数超过了pm．max_spare_servers（表示闲着的worker太多了）则会杀掉一些worker，避免占用过多资源，master通过这4个值来动态控制worker的数量。
- 按需模式（ondemand）：这种模式很像传统的cgi，在启动时不分配worker进程，等到有请求了后再通知master进程fork worker进程，也就是来了请求以后再fork子进程进行处理。总的worker数不超过pm．max_children，处理完成后worker进程不会立即退出，当空闲时间超过pm．process_idle_timeout后再退出。
