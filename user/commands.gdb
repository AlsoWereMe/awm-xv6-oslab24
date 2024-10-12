# 设置断点，捕获 initcode
b exec

# 运行到断点
c

# 打印当前进程的名称，即initcode
p cpus[$tp]->proc->name

# 设置断点，运行至main，捕获init程序
b main

# 运行到断点
c

# 打印当前进程的名称，即init
p cpus[$tp]->proc->name