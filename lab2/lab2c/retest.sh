

sudo ./unload_shofer
make clean

make
sudo ./load_shofer MAX_MSG_NUM=10 MAX_MSG_SIZE=10 MAX_THREAD_NUM=10
./tester
