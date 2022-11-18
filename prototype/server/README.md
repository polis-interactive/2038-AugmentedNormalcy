# Notes

## Compiling cuda

To compile for broose's ghetto laptop, the barely supported maxwell architecture, needed the following gencode added to the nvidia compiler:

```
$ nvcc -gencode=arch=compute_50,code=sm_50 hello.cu -o hello
```
