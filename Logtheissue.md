# 问题记录与解决

## 问题一
调度器主线程没有阻塞，很快就结束了，而在停止函数中先将running_ 设置为false 导致部分的线程不能正常的工作，比如
接收客户端的任务器请求的线程还没有开始接收客户端的tcp 连接就结束了
### 解决办法
让主线程阻塞，先别着急修改running_状态- 缓兵之计 后边可以检查检查调度器析构函数逻辑

## 问题二
注册zk 节点老是失败
### 解决办法
查看多错误码 -101 表示节点目录不存在
错误原因:父目录不存在，zk 不能多级创建，
解决方法：创建节点前检查根节点，不存在则创建

## 问题三
调度器将任务给rabbitMQ 工作器没有获取
问题原因：服务器配置有问题，主机名不一致
解决方法：先使用 hostname -s 查看当前主机名，如何看etc/hosts 是否有，没有则加上


