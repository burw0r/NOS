# sending ABCDEFG to input_dev
echo "[+] testing with string \"ABCEDFG\""
echo "ABCEDFG" >> /dev/shofer_in

# sending to 7 to control_dev
echo "[+] ioctl 7"
./test/ioctl /dev/shofer_control 7

# reading from output_dev 
read OUT < /dev/shofer_out
echo "[+] read $OUT from shofer_out"
