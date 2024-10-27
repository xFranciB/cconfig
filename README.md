The library was created to work with the C locale, you should change it using `setlocale` while using it.
```c
setlocale(LC_NUMERIC, "C");
```

