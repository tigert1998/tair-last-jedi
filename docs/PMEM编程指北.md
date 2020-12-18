## PMEM编程指北

小白是个普通大学生，但是他有幸得到了一块AEP，他决定把之前写的学生管理系统搬到AEP上来

首先我们假设有如下的结构体

```c
struct Student {
    char name[16];
};
```

那么接下来我们想要构造一个新的学生，如果介质是普通的内存，我们自然会写出这样的代码

```c
struct Student* new_student(char* name) {
	struct Student* stu = (struct Student*)malloc(sizeof(struct Student));
    strcpy(stu->name, name);
    return stu;
}
```

但是如果在持久型存储上，上面的代码结果是不确定的。因为程序会crash，机器也可能掉电关机，这些导致程序非正常退出的因素都可能导致上述的数据写入出现不一致的状态。如果程序在调用strcpy前失败，我们还可以通过学生名为空发现这不是一个合理的学生（假定之前整块AEP为空），但是如果strcpy执行中失败，Alicebobby同学可能在重启之后成为了Alice同学。

为了解决这个问题，我们会想到添加一些字段来进行校验。这是改进后的结构体。

```c
struct Student {
    char name[16];
    uint16_t length;
};
```

这时我们会这样构造对象

```c
struct Student* new_student(char* name) {
	struct Student* stu = (struct Student*)malloc(sizeof(struct Student));
    uint16_t len = strlen(name);
    stu->length = len;
    strcpy(stu->name, name);
    return stu;
}
```

我们就可以在重启后通过对比length与实际写入的长度来确认这个对象的构造是否正确（假定语句总是顺序执行的）。但是，cpu的缓存机制也可能产生特殊的影响。

我们继续假定有这样的结构体与更新操作

```c
struct Students{
	struct Student* student[MAX_STUDENT_CNT];
	uint32_t student_cnt;
};

void push_back(struct Students* students, char* name) {
    struct Student* stu = new_student(name);
    student[student] = stu;
    student_cnt++;
}
```

首先，数据开始的时候被存储在cpu的多级cache中，在通常的策略下，cpu并不会在每次有更新时就将脏数据写回，这在进行持久化内存编程时产生了很大的影响，在不使用特殊手段的情况下，我们很难预测cpu将哪一部分cache写回了。因此，理论上如果此时系统掉电，那么将会出现cache中的数据丢失的现象，对应到这个场景，则是我们可能得到这样的数据

```c
students:
	student: {NULL, NULL,NULL...},
	student_cnt: 1,
};
```

我们可以注意到，虽然我们的student字段是最先赋值的，但是cpu将这部分所属的cache选择在最后写回，实际写入的学生数和记录中的学生数对不上了。这是我们所不想见到的。

为了回避这些问题，我们需要了解这样几个指令，分别是flush系的CLFLUSH、CLFLUSHOPT和CLWB，以及SFENCE。

CLFLUSH会命令cpu将对应cacheline逐出，强制性的写回介质，这在一定程度上可以解决我们的问题，但是这是一个同步指令，将会阻塞流水线，损失了一定的运行速度，于是Intel添加了新的指令CLFLUSHOPT和CLWB，这是两个异步的指令。尽管都能写回介质，区别在前者会清空cacheline，后者则会保留，这使得在大部分场景下CLWB可能有更高的性能。

但是事情没有那么简单，异步的代价是我们对于cache下刷的顺序依旧不可预测，对应到这个场景就是name依然可能先于length下刷，于是我们需要使用SFENCE提供保证，SFENCE强制sfence指令前的写操作必须在sfence指令后的写操作前完成。

除此之外，8byte的写入操作本身也是原子的。

cache的问题解决的差不多了。但是除了cache，编译器的优化策略与cpu的乱序执行也可能产生类似的效果。

因此Intel提供了PMDK，在PMDK中的libpmem中提供了这样一些API，用以处理所有可能产生影响的情况。

```c
void pmem_flush(const void *addr, size_t len);
void pmem_drain(void);
void pmem_persist(const void *addr, size_t len);
```

`pmem_flush`即是之前所说的flush系指令的封装，只不过libpmem会在装载时获取相关信息自动选择最优的指令，而`pmem_drain`则是对sfence的封装。至于`pmem_persist`，仅仅只是连续调用了`pmem_flush`和`pmem_drain`。考虑到`pmem_drain`可能会阻塞一些操作，更好的做法是对数据结构里互不相干的几个字段分别flush，最后一并调用`pmem_drain`，以将阻塞带来的问题降到最低。

用上面几个API来补全这个例子，就是

```c
void push_back(struct Students* students, char* name) {
    struct Student* stu = new_student(name); // you also need to make this function persist
    student[student] = stu;
    pmem_persist(&student[student], sizeof(struct Student*));
    // or
    // pmem_flush(&student[student], sizeof(struct Student*));
	// pmem_drain();
    student_cnt++;
	pmem_persist(&student_cnt, sizeof(student_cnt));
    // the same
    // pmem_flush(&student_cnt, sizeof(student_cnt));
	// pmem_drain();
}
```

再举一个例子好了，我们可能会想要以链表形式组织数据，那么

```c
struct StudentNode {
	struct Student* stu;
    struct StudentNode* next;
};
void insert(struct StudentNode** head, struct StudentNode* new_stu) {
    new_stu->next = *head;
    *head = new_stu;
}
```

看出问题了吗？同样的，head很有可能先于next写回AEP，在这种情况下发生断电我们就会丢失整条链表的大部分信息。因此，我们也需要手动处理持久化。

```c
void insert(struct StudentNode** head, struct StudentNode* new_stu) {
    new_stu->next = *head;
    pmem_persist(&new_stu->next, sizeof(struct StudentNode*));
    *head = new_stu;
    pmem_persist(*head, sizeof(struct StudentNode*));
}
```

最后则是libpmem里的几个API

```c
void *pmem_memmove_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memset_persist(void *pmemdest, int c, size_t len);
void *pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memset_nodrain(void *pmemdest, int c, size_t len);
```

语义上讲，这些函数都相当于调用glibc的同名函数再补上`pmem_flush`和`pmem_drain（可选）`，只不过no_drain提供了更细的粒度，让我们有能力对多个不相干的数据分别写入，最后一并调用`pmem_drain`。