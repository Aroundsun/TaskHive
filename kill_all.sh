#!/bin/bash

echo "正在杀死所有 worker、scheduler、client 进程..."
pkill -9 worker
pkill -9 scheduler
pkill -9 client
echo "清理完成。当前相关进程："
ps -ef | grep -E 'worker|scheduler|client' | grep -v grep 