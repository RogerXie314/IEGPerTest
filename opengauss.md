# opengauss - 轻量版



## 安装

### 乌班图

#### 使用官方适配centos7.6的安装包

```
# 更新APT
apt update
apt upgrade

# ubuntu默认使用dash作为sh解释器，会导致普通用户没有source命令，此外om里面的脚本均是bash类型，因此需要先修改系统的sh为bash
unlink /bin/sh
ln -s /bin/bash /bin/sh

#安装数据库需要的依赖。 此外需要建立一个readline.so.7版本的软连接。
apt install -y libreadline-dev libaio-dev net-tool libncurses5 m4 flex bison patch glibc-source lsb—release
ln -s /usr/lib/x86_64-linux-gnu/libreadline.so.8 /usr/lib/x86_64-linux-gnu/libreadline.so.7

#创建用户组和用户，并修改 .bashrc ,注释掉case..esac
groupadd dbgrp
useradd -m -g dbgrp -s /bin/bash omm
passwd omm
```

#vi /home/omm/.bashrc 注释掉这几行：（说明：这几行含义是只允许命令行做source，从脚本里面做source会直接return，导致后面追加的环境变量没法加载）

![img](https://docs.opengauss.org/zh/docs/latest/docs/InstallationGuide/figures/adapt_ubuntu.png)

```
# 创建安装目录
mkdir -p /opt/software/openGauss/data
mkdir -p /opt/software/openGauss/install
mkdir -p /opt/software/openGauss/log
chown -R omm:dbgrp /opt/software/openGauss
# 切换用户
su - omm
# 上传安装包/home/omm/software/opengauss目录下
mkdir -r /home/omm/software/opengauss
# 解压安装包
cd home/omm/software/opengauss
tar -zxvf openGauss-Lite-6.0.1-CentOS7-x86_64.tar.gz
# 安装
echo "Wnt.1@3456" | sh ./install.sh --mode single -D /opt/software/openGauss/data -R  /opt/software/openGauss/install -l /opt/software/openGauss/log
# 安装成功后自动设置环境变量，使环境变量生效。
source /home/omm/.bashrc
# 先备份OpenGauss的库文件
#mv /opt/software/openGauss/install/lib/libstdc++.so.6 /opt/software/openGauss/install/lib/libstdc++.so.6.bak
# 创建指向系统库的符号链接
#ln -s /usr/lib/x86_64-linux-gnu/libstdc++.so.6 /opt/software/openGauss/install/lib/libstdc++.so.6
# 启动数据库
gs_ctl start
# 启动会抛出一些告警，其中 WARNING:  Failed to open file /sys/fs/cgroup/cpuset/cpuset.cpus是因为ubuntu使用的cgroup V2，而openguass只适配了cgroupV1，目前这个影响未知，猜测可能会影响openguass的cpu、内存限制，有可能会导致openguass使用超出限制。其他的告警，在centos服务器上也会有，可能是轻量版的原因，我是在centos7.4上测试的，而安装包适配的是centos7.6，也可能是这个导致的告警。
```

#### 编译源码安装-失败放弃了

还需要安装华为云的 OBS SDK 开发包，耗时间太长了，放弃了

编译时间非常长，我用4核4G的虚拟机安装，gcc编译耗时3小时，建议用高端服务器编译安装，用编译后的so文件去给低端服务器安装。

```
apt install build-essential unzip lbzip2 gcc-multilib g++-multilib flex bison -y
apt install -y libaio-dev libncurses5-dev libpam0g-dev libffi-dev python3-dev libtool libltdl-dev python3-dev libssl-dev bison 
mkdir -p /root/openGauss
cd /root/openGauss
# 安装gcc-10.3.0 /opt/gcc/gcc10.3
wget https://github.com/gcc-mirror/gcc/archive/refs/tags/releases/gcc-10.3.0.zip
unzip gcc-10.3.0.zip
cd gcc-releases-gcc-10.3.0
./contrib/download_prerequisites
cd ..
mkdir build-gcc && cd build-gcc
../gcc-releases-gcc-10.3.0/configure --prefix=/opt/gcc/gcc10.3 --enable-languages=c,c++ --disable-multilib
make -j$(nproc)  
make install
# 安装cmake3.18 /usr/local/cmake3.18
cd /root/openGauss
wget https://cmake.org/files/v3.18/cmake-3.18.0.zip
unzip cmake-3.18.0.zip
cd cmake-3.18.0
./bootstrap --prefix=/usr/local/cmake3.18 --parallel=$(nproc) --no-system-curl  --no-qt-gui 
make -j$(nproc)
make install
# 修改环境变量
export CMAKEROOT=/usr/local/cmake3.18
export GCC_PATH=/opt/gcc/gcc10.3
export CC=$GCC_PATH/bin/gcc
export CXX=$GCC_PATH/bin/g++
export LD_LIBRARY_PATH=$GCC_PATH/lib64:$LD_LIBRARY_PATH
export PATH=$GCC_PATH/bin:$PATH
export PATH="/usr/local/cmake3.18/bin:$PATH"

# 编译全量三方组件
cd /root/openGauss
wget https://obs.cn-north-4.myhuaweicloud.com/gitcode-repo-archive-bj/f704b8e2dc467907dff121f2f5753a03df42af00df6e07ccbe16a43d5da2b25a5091021.zip?AWSAccessKeyId=X364SUL65RWWOYRBZEMU&Expires=1750306095&response-content-disposition=attachment%3Bfilename%3D%22openGauss-third_party-6.0.0.zip%22&Signature=zzGrZLYM2s%2Fbznbo0Jk%2BJs3KDHU%3D
unzip openGauss-third_party-6.0.0.zip
cd openGauss-third_party-6.0.0/build
chmod u+x *
./build_all.sh
```

```
mkdir -p /root/openGauss
cd /root/openGauss
# 安装gcc
wget https://github.com/gcc-mirror/gcc/archive/refs/tags/releases/gcc-10.3.0.zip
unzip gcc-10.3.0.zip
cd gcc-releases-gcc-10.3.0

apt install -y lbzip2 bzip2
./contrib/download_prerequisites

apt install -y build-essential m4
mkdir -p /root/openGauss/gmp-build && cd /root/openGauss/gmp-build
/root/openGauss/gcc-releases-gcc-10.3.0/gmp/configure --prefix=/opt/gcc/gcc10.3/gmp
make
make install

mkdir -p /root/openGauss/mpfr-build && cd /root/openGauss/mpfr-build
/root/openGauss/gcc-releases-gcc-10.3.0/mpfr/configure --prefix=/opt/gcc/gcc10.3/mpfr --with-gmp=/opt/gcc/gcc10.3/gmp
make
make install

mkdir -p /root/openGauss/mpc-build && cd /root/openGauss/mpc-build
/root/openGauss/gcc-releases-gcc-10.3.0/mpc/configure --prefix=/opt/gcc/gcc10.3/mpc --with-gmp=/opt/gcc/gcc10.3/gmp --with-mpfr=/opt/gcc/gcc10.3/mpfr
make
make install

mkdir -p /root/openGauss/isl-build && cd /root/openGauss/isl-build
/root/openGauss/gcc-releases-gcc-10.3.0/isl/configure --prefix=/opt/gcc/gcc10.3/isl --with-gmp-prefix=/opt/gcc/gcc10.3/gmp
make
make install

apt install -y libc6-dev

export LD_LIBRARY_PATH=/opt/gcc/gcc10.3/gmp/lib:/opt/gcc/gcc10.3/mpfr/lib:/opt/gcc/gcc10.3/mpc/lib:/opt/gcc/gcc10.3/isl/lib:${LD_LIBRARY_PATH}
export C_INCLUDE_PATH=/opt/gcc/gcc10.3/gmp/include:/opt/gcc/gcc10.3/mpfr/include:/opt/gcc/gcc10.3/mpc/include:/opt/gcc/gcc10.3/isl/include:${C_INCLUDE_PATH}

apt install -y build-essential flex bison gawk m4 texinfo libncurses5-dev
mkdir -p /root/openGauss/gcc-build 
cd /root/openGauss/gcc-build
/root/openGauss/gcc-releases-gcc-10.3.0/configure --prefix=/opt/gcc/gcc10.3/gcc --with-gmp=/opt/gcc/gcc10.3/gmp --with-mpfr=/opt/gcc/gcc10.3/mpfr --with-mpc=/opt/gcc/gcc10.3/mpc --with-isl=/opt/gcc/gcc10.3/isl --enable-languages=c,c++ --disable-multilib
make
make install

# 安装cmake
apt install -y libssl-dev libmd-dev

cd /root/openGauss
unzip cmake-3.18.0.zip
mkdir -p /root/openGauss/cmake-build && cd /root/openGauss/cmake-build
#/root/openGauss/cmake-3.18.0/configure --prefix=/usr/local/cmake3.18
/root/openGauss/cmake-3.18.0/bootstap --prefix=/usr/local/cmake3.18
make 
make install

export CMAKEROOT=/usr/local/cmake3.18     ##编译cmake指定的--prefix
export GCC_PATH=/opt/gcc/gcc10.3        ##编译gcc指定的--prefix
export CC=$GCC_PATH/gcc/bin/gcc
export CXX=$GCC_PATH/gcc/bin/g++
export LD_LIBRARY_PATH=$GCC_PATH/gcc/lib64:$GCC_PATH/isl/lib:$GCC_PATH/mpc/lib/:$GCC_PATH/mpfr/lib/:$GCC_PATH/gmp/lib/:$CMAKEROOT/share:$LD_LIBRARY_PATH
export PATH=$GCC_PATH/gcc/bin:$CMAKEROOT/bin:$PATH

# 编译第三库
apt install -y autoconf automake libtool python3 python3-pip  libffi-dev libaio-dev zlib1g-dev libreadline-dev libnuma-dev
ln -s /usr/bin/python3 /usr/bin/python

cd /root/openGauss
wget wget https://obs.cn-north-4.myhuaweicloud.com/gitcode-repo-archive-bj/f704b8e2dc467907dff121f2f5753a03df42af00df6e07ccbe16a43d5da2b25a5091021.zip?AWSAccessKeyId=X364SUL65RWWOYRBZEMU&Expires=1750306095&response-content-disposition=attachment%3Bfilename%3D%22openGauss-third_party-6.0.0.zip%22&Signature=zzGrZLYM2s%2Fbznbo0Jk%2BJs3KDHU%3D
unzip openGauss-third_party-6.0.0.zip
# 注释 mv lib64 lib ,安装过程中没有lib64这个目录,在 Ubuntu 系统中，lib64目录并非默认存在，64 位库通常存放在lib目录
vim /root/openGauss/openGauss-third_party-6.0.0/dependency/cJSON/build_component.sh
# 注释掉#mv  ../../../../install_comm/lib64/libzstd* ../../../../install_comm/lib/
vim /root/openGauss/openGauss-third_party-6.0.0/dependency/zstd/buidl.sh

cd openGauss-third_party-6.0.0/build
unlink /bin/sh
ln -s /bin/bash /bin/sh
bash build_all.sh

# 编译openguass
apt install libxml2-dev
ln -s /usr/include/libxml2/libxml /usr/include/libxml
pip install esdk-obs-python --trusted -host pypi.org
apt install libunicode-map-perl libunicode-string-perl
root@wnt:~/openGauss/openGauss-server# bash -x build.sh -m release -3rd /root/openGauss/binarylibs
```



### 麒麟 TODO

```
#适配AArch架构的系统，虚拟机安装不了，需要在特定的服务器上测试
```



### 统信 TODO

```
#适配AArch架构的系统，虚拟机安装不了，需要在特定的服务器上测试
```



## 卸载

```
cd  /home/omm/software/opengauss
sh uninstall.sh --delete-data 
```

## 管理

### 登陆

```
# 以当前操作系统的用户登陆，即omm用户
gsql -d postgres -p 5432
```



### 数据库

用户必须拥有数据库创建的权限或者是数据库的系统管理员权限才能创建数据库。

```
# 查看当前数据库存储编码，如果数据库的编码为SQL_ASCII，对象名中含有多字节字符（例如中文），超过数据库对象名长度限制（63字节）的时候，数据库将会将最后一个字节（而不是字符）截断，可能造成出现半个字符的情况。
show server_encoding;
# 创建一个新的数据库db_tpcc。
CREATE DATABASE db_tpcc;
# 使用\l元命令查看数据库系统的数据库列表
\l
# 使用如下命令通过系统表pg_database查询数据库列表。
SELECT datname FROM pg_database;
# 为数据库重新命名
ALTER DATABASE db_tpcc RENAME TO human_tpcds;
# 删除数据库
DROP DATABASE human_tpcds;
```

### 表空间

表空间是一个目录，可以存在多个，里面存储的是它所包含的数据库的各种物理文件。由于表空间是一个目录，仅是起到了物理隔离的作用，其管理功能依赖于文件系统。

通过使用表空间，管理员可以控制一个数据库安装的磁盘布局。这样有以下优点：

- 如果初始化数据库所在的分区或者卷空间已满，又不能逻辑上扩展更多空间，可以在不同的分区上创建和使用表空间，直到系统重新配置空间。
- 表空间允许管理员根据数据库对象的使用模式安排数据位置，从而提高性能。
  - 一个频繁使用的索引可以放在性能稳定且运算速度较快的磁盘上，比如一种固态设备。
  - 一个存储归档的数据，很少使用的或者对性能要求不高的表可以存储在一个运算速度较慢的磁盘上。
- 管理员通过表空间可以设置占用的磁盘空间。用以在和其他数据共用分区的时候，防止表空间占用相同分区上的其他空间。
- 表空间对应于一个文件系统目录，使用表空间配额管理会使性能有30%左右的影响，MAXSIZE指定每个数据库节点的配额大小，误差范围在500MB以内。请根据实际的情况确认是否需要设置表空间的最大值。

openGauss自带了两个表空间：pg_default和pg_global。

- 默认表空间pg_default：用来存储非共享系统表、用户表、用户表index、临时表、临时表index、内部临时表的默认表空间。对应存储目录为实例数据目录下的base目录。
- 共享表空间pg_global：用来存放共享系统表的表空间。对应存储目录为实例数据目录下的global目录。

```
# 创建用户jack
CREATE USER jack IDENTIFIED BY 'Wnt.1@3456';
# 创建表空间
CREATE TABLESPACE fastspace RELATIVE LOCATION 'tablespace/tablespace_1';

omm@wnt:/opt/software/openGauss/dataa/pg_location$ tree
.
└── tablespace
    └── tablespace_1
        └── PG_9.2_201611171_wnt
            └── pgsql_tmp

4 directories, 0 files
# 数据库系统管理员执行如下命令将“fastspace”表空间的访问权限赋予数据用户jack。
GRANT CREATE ON TABLESPACE fastspace TO jack;
# 在指定表空间创建表
CREATE TABLE foo(i int) TABLESPACE fastspace;
# 查到系统和用户定义的全部表空间
SELECT spcname FROM pg_tablespace;
# 查询表空间的当前使用情况
SELECT PG_TABLESPACE_SIZE('fastspace');
# 对表空间fastspace重命名为fspace
ALTER TABLESPACE fastspace RENAME TO fspace;
# 删除表空间
DROP USER jack CASCADE;
DROP TABLE foo;
DROP TABLESPACE fspace;
```

### 对象

#### 模式

schema又称作模式。通过管理schema，允许多个用户使用同一数据库而不相互干扰，可以将数据库对象组织成易于管理的逻辑组，同时便于将第三方应用添加到相应的schema下而不引起冲突。

- openGauss包含一个或多个已命名数据库。用户和用户组在openGauss范围内是共享的，但是其数据并不共享。任何与服务器连接的用户都只能访问连接请求里声明的那个数据库。

- 一个数据库可以包含一个或多个已命名的schema，schema又包含表及其他数据库对象，包括数据类型、函数、操作符等。同一对象名可以在不同的schema中使用而不会引起冲突。例如，schema1和schema2都可以包含一个名为mytable的表。

- 和数据库不同，schema不是严格分离的。用户根据其对schema的权限，可以访问所连接数据库的schema中的对象。进行schema权限管理首先需要对数据库的权限控制进行了解。

- 不能创建以PG_为前缀的schema名，该类schema为数据库系统预留的。

- 在每次创建新用户时，系统会在当前登录的数据库中为新用户创建一个同名Schema。对于其他数据库，若需要同名Schema，则需要用户手动创建。

- 通过未修饰的表名（名称中只含有表名，没有“schema名”）引用表时，系统会通过search_path（搜索路径）来判断该表是哪个schema下的表。pg_temp和pg_catalog始终会作为搜索路径顺序中的前两位，无论二者是否出现在search_path中，或者出现在search_path中的任何位置。search_path（搜索路径）是一个schema名列表，在其中找到的第一个表就是目标表，如果没有找到则报错。（某个表即使存在，如果它的schema不在search_path中，依然会查找失败）在搜索路径中的第一个schema叫做"当前schema"。它是搜索时查询的第一个schema，同时在没有声明schema名时，新创建的数据库对象会默认存放在该schema下。

- 每个数据库都包含一个pg_catalog schema，它包含系统表和所有内置数据类型、函数、操作符。pg_catalog是搜索路径中的一部分，始终在临时表所属的模式后面，并在search_path中所有模式的前面，即具有第二搜索优先级。这样确保可以搜索到数据库内置对象。如果用户需要使用和系统内置对象重名的自定义对象时，可以在操作自定义对象时带上自己的模式。

- 默认情况下，用户只能访问属于自己的schema中的数据库对象。如果需要访问其他schema的对象，则该schema的所有者应该赋予他对该schema的usage权限。

  

  通过将模式的CREATE权限授予某用户，被授权用户就可以在此模式中创建对象。注意默认情况下，所有角色都拥有在public模式上的USAGE权限，但是普通用户没有在public模式上的CREATE权限。普通用户能够连接到一个指定数据库并在它的public模式中创建对象是不安全的，如果普通用户具有在public模式上的CREATE权限则建议通过如下语句撤销该权限。

  

  我们在按照业务划分Schema时，例如：finance（财务数据）、hr（人力资源）、report（报表），提升团队协作效率，在openguass也可以创建多个数据库，而使用Schema的优势在于可以Schema 间的表可直接通过 SQL 关联（如`SELECT * FROM finance.accounts JOIN hr.employees`），如果多个数据库需要借助一些工具，因为任何与服务器连接的用户都只能访问连接请求里声明的那个数据库。若业务间需严格物理隔离（如不同租户数据），可考虑多数据库；否则 Schema 划分是更优解。

  ```
  # 创建一个schema
  CREATE SCHEMA myschema;
  # 令在创建schema时指定owner。
  CREATE SCHEMA myschema AUTHORIZATION omm;
  # 在myschema下创建mytable表。
  CREATE TABLE myschema.mytable(id int, name varchar(20));
  # 查询myschema下mytable表的所有数据。
   SELECT * FROM myschema.mytable;
  # 查看搜索路径。
  SHOW SEARCH_PATH;
  # 将搜索路径设置为myschema、public，首先搜索myschema。
  SET SEARCH_PATH TO myschema, public;
  # 查看现有的schema
  SELECT current_schema();
  # 创建用户jack，并将myschema的usage权限赋给用户jack。
  CREATE USER jack IDENTIFIED BY 'Wnt.1@3456';
  GRANT USAGE ON schema myschema TO jack;
  # 将用户jack对于myschema的usage权限收回
  REVOKE USAGE ON schema myschema FROM jack;
  # 当schema为空时，即该schema下没有数据库对象，使用DROP SCHEMA命令进行删除。例如删除名为nullschema的空schema。
  DROP SCHEMA IF EXISTS nullschema;
  # 当schema非空时，如果要删除一个schema及其包含的所有对象，需要使用CASCADE关键字。例如删除myschema及该schema下的所有对象。
  DROP SCHEMA myschema CASCADE;
  # 删除用户jack
   DROP USER jack;
  ```

#### 表

行存储是指将表按行存储到硬盘分区上，列存储是指将表按列存储到硬盘分区上。默认情况下，创建的表为行存储。

![img](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/DeveloperGuide/figures/zh-cn_image_0289900299.png)

行、列存储有如下优缺点：

| **存储模型** | **优点**                                                     | **缺点**                                                  |
| :----------- | :----------------------------------------------------------- | :-------------------------------------------------------- |
| 行存         | 数据被保存在一起。INSERT/UPDATE容易。                        | 选择(Selection)时即使只涉及某几列，所有数据也都会被读取。 |
| 列存         | 查询时只有涉及到的列会被读取。投影(Projection)很高效。任何列都能作为索引。 | 选择完成时，被选择的列要重新组装。INSERT/UPDATE比较麻烦。 |

一般情况下，如果表的字段比较多（大宽表），查询中涉及到的列不多的情况下，适合列存储。如果表的字段个数比较少，查询大部分字段，那么选择行存储比较好。

| 存储类型 | 适用场景                                                     |
| :------- | :----------------------------------------------------------- |
| 行存     | 点查询(返回记录少，基于索引的简单查询)。增、删、改操作较多的场景。 |
| 列存     | 统计分析类查询 (关联、分组操作较多的场景)。即席查询（查询条件不确定，行存表扫描难以使用索引）。 |



行存表：频繁更新、频繁的少量插入、每次查询时，只涉及了表的少数（<50%总列数）几个列、

列存表：一次插入大批量数据、表的列数很多、压缩率高但会消耗更多的CPU资源

```
# 创建表
CREATE TABLE table1(id int, a char(6), b varchar(6),c varchar(6));
# 向表中插入数据
INSERT INTO table1 VALUES(3,'87A123','87A123','87A123');
INSERT INTO table1 VALUES(4,'87A123','87A123','87A123'),(5,'87A123','87A123','87A123');
# 创建备份表
CREATE TABLE table2(id int, a char(6), b varchar(6),c varchar(6));
INSERT INTO table2 SELECT * FROM table1;
# 删除备份表
DROP TABLE table2 CASCADE;
# 更新表中数据
update table1 set a='wnt',b='dev' where id=3;
# 查看表中的数据
select * from table1;
# 使用系统表pg_tables查询数据库所有表的信息。
SELECT * FROM pg_tables;
# 使用gsql的\d+命令查询表的属性。
\d+ table1;
                               Table "public.table1"
 Column |         Type         | Modifiers | Storage  | Stats target | Description
--------+----------------------+-----------+----------+--------------+-------------
 id     | integer              |           | plain    |              |
 a      | character(6)         |           | extended |              |
 b      | character varying(6) |           | extended |              |
 c      | character varying(6) |           | extended |              |
Has OIDs: no
Options: orientation=row, compression=no

# 删除表中内容
delete from table1 where id=3;
# 删除全部内容
TRUNCATE TABLE table1;
# 删除表
DROP TABLE table1;
# 默认创建表的类型。数据按行进行存储，即一行数据是连续存储。适用于对数据需要经常更新的场景。
openGauss=# CREATE TABLE customer_t1
(
  state_ID   CHAR(2),
  state_NAME VARCHAR2(40),
  area_ID    NUMBER
);
# 数据按列进行存储，即一列所有数据是连续存储的。单列查询IO小，比行存表占用更少的存储空间。适合数据批量插入、更新较少和以查询为主统计分析类的场景。列存表不适合点查询。
openGauss=# CREATE TABLE customer_t2
(
  state_ID   CHAR(2),
  state_NAME VARCHAR2(40),
  area_ID    NUMBER
)
WITH (ORIENTATION = COLUMN);

```



#### 分区表

openGauss数据库支持的分区表为范围分区表、间隔分区表、列表分区表、哈希分区表。

- 范围分区表：将数据基于范围映射到每一个分区，这个范围是由创建分区表时指定的分区键决定的。这种分区方式是最为常用的，并且分区键经常采用日期，例如将销售数据按照月份进行分区。
- 间隔分区表：是一种特殊的范围分区表，相比范围分区表，新增间隔值定义，当插入记录找不到匹配的分区时，可以根据间隔值自动创建分区。
- 列表分区表：将数据中包含的键值分别存储再不同的分区中，依次将数据映射到每一个分区，分区中包含的键值由创建分区表时指定。
- 哈希分区表：将数据根据内部哈希算法依次映射到每一个分区中，包含的分区个数由创建分区表时指定。

分区表和普通表相比具有以下优点：

- 改善查询性能：对分区对象的查询可以仅搜索自己关心的分区，提高检索效率。
- 增强可用性：如果分区表的某个分区出现故障，表在其他分区的数据仍然可用。
- 方便维护：如果分区表的某个分区出现故障，需要修复数据，只修复该分区即可。
- 均衡I/O：可以把不同的分区映射到不同的磁盘以平衡I/O，改善整个系统性能。

普通表若要转成分区表，需要新建分区表，然后把普通表中的数据导入到新建的分区表中。因此在初始设计表时，请根据业务提前规划是否使用分区表。

```
# 创建分区表
CREATE SCHEMA tpcds;
CREATE TABLE tpcds.customer_address
(
    ca_address_sk       integer                  NOT NULL   ,
    ca_address_id       character(16)            NOT NULL   ,
    ca_street_number    character(10)                       ,
    ca_street_name      character varying(60)               ,
    ca_street_type      character(15)                       ,
    ca_suite_number     character(10)                       ,
    ca_city             character varying(60)               ,
    ca_county           character varying(30)               ,
    ca_state            character(2)                        ,
    ca_zip              character(10)                       ,
    ca_country           character varying(20)               ,
    ca_gmt_offset       numeric(5,2)                        ,
    ca_location_type    character(20)
)
PARTITION BY RANGE (ca_address_sk)
(
        PARTITION P1 VALUES LESS THAN(5000),
        PARTITION P2 VALUES LESS THAN(10000),
        PARTITION P3 VALUES LESS THAN(15000),
        PARTITION P4 VALUES LESS THAN(20000),
        PARTITION P5 VALUES LESS THAN(25000),
        PARTITION P6 VALUES LESS THAN(30000),
        PARTITION P7 VALUES LESS THAN(40000),
        PARTITION P8 VALUES LESS THAN(MAXVALUE)
)
ENABLE ROW MOVEMENT;
# 插入数据
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (1,'a');
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (5001,'b');
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (10001,'c');
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (15001,'c');
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (20001,'c');
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (25001,'c');
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (30001,'c');
insert into tpcds.customer_address(ca_address_sk,ca_address_id) values (40001,'c');
# 查询表
SELECT * FROM tpcds.customer_address;
SELECT * FROM tpcds.customer_address PARTITION (P6);
SELECT * FROM tpcds.customer_address PARTITION FOR (35888);
# 重命名分区P8为P9。
ALTER TABLE tpcds.customer_address RENAME PARTITION P8 TO P9;
# 删除分区P9
ALTER TABLE tpcds.customer_address DROP PARTITION P8;
# 增加分区P8，范围为 40000<= P8<=50000。
 ALTER TABLE tpcds.customer_address ADD PARTITION P8 VALUES LESS THAN (50000);
# 删除分区表
DROP TABLE tpcds.customer_address;
```



#### 索引

索引可以提高数据的访问速度，但同时也增加了插入、更新和删除操作的处理时间。所以是否要为表增加索引，索引建立在哪些字段上，是创建索引前必须要考虑的问题。需要分析应用程序的业务处理、数据使用、经常被用作查询的条件或者被要求排序的字段来确定是否建立索引。

索引建立在数据库表中的某些列上。因此，在创建索引时，应该仔细考虑在哪些列上创建索引。

- 在经常需要搜索查询的列上创建索引，可以加快搜索的速度。

- 在作为主键的列上创建索引，强制该列的唯一性和组织表中数据的排列结构。

- 在经常需要根据范围进行搜索的列上创建索引，因为索引已经排序，其指定的范围是连续的。

- 在经常需要排序的列上创建索引，因为索引已经排序，这样查询可以利用索引的排序，加快排序查询时间。

- 在经常使用WHERE子句的列上创建索引，加快条件的判断速度。

- 为经常出现在关键字ORDER BY、GROUP BY、DISTINCT后面的字段建立索引。

  > ![img](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/DatabaseAdministrationGuide/public_sys-resources/icon-note.gif) **说明：**
  >
  > - 索引创建成功后，系统会自动判断何时引用索引。当系统认为使用索引比顺序扫描更快时，就会使用索引。
  > - 索引创建成功后，必须和表保持同步以保证能够准确地找到新数据，这样就增加了数据操作的负荷。因此请定期删除无用的索引。
  > - 分区表索引分为LOCAL索引与GLOBAL索引，一个LOCAL索引对应一个具体分区，而GLOBAL索引则对应整个分区表。

- 在开启逻辑复制的场景下，如需创建包含系统列的主键索引，必须将该表的REPLICA IDENTITY属性设置为FULL或是使用USING INDEX指定不包含系统列的、唯一的、非局部的、不可延迟的、仅包括标记为NOT NULL的列的索引。

  ```
  # 创建分区表
  CREATE TABLE tpcds.customer_address
  (
      ca_address_sk       integer                  NOT NULL   ,
      ca_address_id       character(16)            NOT NULL   ,
      ca_street_number    character(10)                       ,
      ca_street_name      character varying(60)               ,
      ca_street_type      character(15)                       ,
      ca_suite_number     character(10)                       ,
      ca_city             character varying(60)               ,
      ca_county           character varying(30)               ,
      ca_state            character(2)                        ,
      ca_zip              character(10)                       ,
      ca_country           character varying(20)               ,
      ca_gmt_offset       numeric(5,2)                        ,
      ca_location_type    character(20)
  )
  PARTITION BY RANGE (ca_address_sk)
  (
          PARTITION P1 VALUES LESS THAN(5000),
          PARTITION P2 VALUES LESS THAN(10000),
          PARTITION P3 VALUES LESS THAN(15000),
          PARTITION P4 VALUES LESS THAN(20000),
          PARTITION P5 VALUES LESS THAN(25000),
          PARTITION P6 VALUES LESS THAN(30000),
          PARTITION P7 VALUES LESS THAN(40000),
          PARTITION P8 VALUES LESS THAN(MAXVALUE)
  )
  ENABLE ROW MOVEMENT;
  # 创建唯一索引
  CREATE UNIQUE INDEX customer_address_unique_index ON tpcds.customer_address(ca_address_sk);
  # 创建多字段索引
  CREATE INDEX more_column_index ON tpcds.customer_address(ca_address_sk ,ca_street_number);
  # 查询索引
  \di+ tpcds.customer_address_unique_index 
  # 删除索引
  DROP INDEX tpcds.customer_address_unique_index;
  # 删除表，删除表自动删除索引
  DROP TABLE tpcds.customer_address
  ```

  

#### 序列

序列Sequence是用来产生唯一整数的数据库对象。序列的值是按照一定规则自增的整数。因为自增所以不重复，因此说Sequence具有唯一标识性。这也是Sequence常被用作主键的原因。

通过序列使某字段成为唯一标识符的方法有两种：

- 一种是声明字段的类型为[序列整型](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/数值类型.html#table12345)，由数据库在后台自动创建一个对应的Sequence。
- 另一种是使用[CREATE SEQUENCE](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-SEQUENCE.html)自定义一个新的Sequence，然后将nextval('sequence_name')函数读取的序列值，指定为某一字段的默认值，这样该字段就可以作为唯一标识符。

```
# 声明字段类型为序列整型来定义标识符字段,系统自动创建索引
CREATE TABLE T1
(
    id    serial,
    name  text
);
#  创建序列，并通过nextval('sequence_name')函数指定为某一字段的默认值
CREATE SEQUENCE seq1 cache 100;
# 指定为某一字段的默认值，使该字段具有唯一标识属性。
CREATE TABLE T2 
( 
    id   int not null default nextval('seq1'),
    name text
);
#将序列和一个表的指定字段进行关联。这样，在删除那个字段或其所在表的时候会自动删除已关联的序列。
ALTER SEQUENCE seq1 OWNED BY T2.id;
# 查看序列
\ds
                List of relations
 Schema |   Name    |   Type   | Owner | Storage
--------+-----------+----------+-------+---------
 public | seq1      | sequence | omm   |
 public | t1_id_seq | sequence | omm   |
(2 rows)
# 删除表
drop table T2;
drop table T1;
```

### 用户及权限

#### 默认权限机制

数据库对象创建后，进行对象创建的用户就是该对象的所有者。数据库安装后的默认情况下，未开启[三权分立](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/DatabaseAdministrationGuide/三权分立.html)，数据库系统管理员具有与对象所有者相同的权限。也就是说对象创建后，默认只有对象所有者或者系统管理员可以查询、修改和销毁对象，以及通过[GRANT](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/GRANT.html)将对象的权限授予其他用户。

为使其他用户能够使用对象，必须向用户或包含该用户的角色授予必要的权限。

#### 管理员

##### 初始用户

数据库安装过程中自动生成的帐户称为初始用户。初始用户拥有系统的最高权限，能够执行所有的操作。如果安装时不指定初始用户名称则该帐户与进行数据库安装的操作系统用户同名。如果在安装时不指定初始用户的密码，安装完成后密码为空，在执行其他操作前需要通过gsql客户端修改初始用户的密码。如果初始用户密码为空，则除修改密码外无法执行其他SQL操作以及升级、节点替换等操作。

初始用户会绕过所有权限检查。建议仅将此初始用户作为DBA管理用途，而非业务应用。

##### 系统管理员

系统管理员是指具有SYSADMIN属性的帐户，默认安装情况下具有与对象所有者相同的权限，但不包括dbe_perf模式的对象权限。

要创建新的系统管理员，请以初始用户或者系统管理员用户身份连接数据库，并使用带SYSADMIN选项的[CREATE USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-USER.html)语句或 [ALTER USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-USER.html)语句进行设置

```
# 创建新的系统管理员
CREATE USER sysadmin WITH SYSADMIN password "xxxxxxxxx";
# 给旧的用户设置为系统管理员
ALTER USER joe SYSADMIN;
```

##### 安全管理员

安全管理员是指具有CREATEROLE属性的账户，具有创建、修改、删除用户或角色的权限。 要创建新的安全管理员，三权分立关闭时，请以系统管理员或者安全管理员身份连接数据库，三权分立打开时，请以安全管理员身份连接数据库，并使用带CREATEROLE选项的[CREATE USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-USER.html)语句或 [ALTER USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-USER.html)语句进行设置。

```
# 创建新的安全管理员
CREATE USER createrole WITH CREATEROLE password "xxxxxxxxxxx";
# 给旧的用户设置为安全管理员
ALTER USER joe CREATEROLE;
```

##### 审计管理员

审计管理员是指具有AUDITADMIN属性的账户，具有查看和删除审计日志的权限。 要创建新的审计管理员，三权分立关闭时，请以系统管理员或者安全管理员身份连接数据库，三权分立打开时，请以安全管理员身份连接数据库，并使用带AUDITADMIN选项的[CREATE USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-USER.html)语句或 [ALTER USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-USER.html)语句进行设置。

```
CREATE USER auditadmin WITH AUDITADMIN password "xxxxxxxxxxx";
 ALTER USER joe AUDITADMIN;
```

##### 监控管理员

监控管理员是指具有MONADMIN属性的帐户，具有查看dbe_perf模式下视图和函数的权限，亦可以对dbe_perf模式的对象权限进行授予或收回。

要创建新的监控管理员，请以系统管理员身份连接数据库，并使用带MONADMIN选项的[CREATE USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-USER.html)语句或[ALTER USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-USER.html)语句进行设置。

```
CREATE USER monadmin WITH MONADMIN password "xxxxxxxxx";
ALTER USER joe MONADMIN;
```

##### 运维管理员

运维管理员是指具有OPRADMIN属性的帐户。

要创建新的运维管理员，请以初始用户身份连接数据库，并使用带OPRADMIN选项的[CREATE USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-USER.html)语句或[ALTER USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-USER.html)语句进行设置。

```
CREATE USER opradmin WITH OPRADMIN password "xxxxxxxxx";
ALTER USER joe OPRADMIN;
```

##### 安全策略管理员

安全策略管理员是指具有POLADMIN属性的帐户，具有创建资源标签，脱敏策略和统一审计策略的权限。

要创建新的安全策略管理员，请以系统管理员用户身份连接数据库，并使用带POLADMIN选项的[CREATE USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-USER.html)语句或[ALTER USER](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-USER.html)语句进行设置。

```
CREATE USER poladmin WITH POLADMIN password "xxxxxxxxx";
ALTER USER joe POLADMIN;
```

#### 三权分立

在实际业务管理中，为了避免系统管理员拥有过度集中的权利带来高风险，可以设置三权分立。将系统管理员的部分权限分立给安全管理员和审计管理员，形成系统管理员、安全管理员和审计管理员三权分立。

三权分立后，系统管理员将不再具有CREATEROLE属性（安全管理员）和AUDITADMIN属性（审计管理员）能力。即不再拥有创建角色和用户的权限，并不再拥有查看和维护数据库审计日志的权限。关于CREATEROLE属性和AUDITADMIN属性的更多信息请参考[CREATE ROLE](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-ROLE.html)。

三权分立后，系统管理员只会对自己作为所有者的对象有权限。

初始用户的权限不受三权分立设置影响。因此建议仅将此初始用户作为DBA管理用途，而非业务应用。

三权分立的设置办法为：将参数[enableSeparationOfDuty](https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/DatabaseReference/操作审计.html#zh-cn_topic_0283136929_zh-cn_topic_0237124747_zh-cn_topic_0059777487_s0a79ea55efa1431d8e3e06e4b8219cd6)设置为on。

#### 用户

```
# 创建用户joe，并设置用户拥有CREATEDB属性。https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-USER.html
CREATE USER joe WITH CREATEDB PASSWORD "xxxxxxxxx";
# 修改用户 https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-USER.html
 ALTER USER joe IDENTIFIED BY 'Abcd@123' REPLACE 'xxxxxxxxx';
# 删除用户 https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/DROP-USER.html
DROP USER joe CASCADE;
# 查看用户列表
SELECT * FROM pg_user; 
# 查看用户属性
SELECT * FROM pg_authid; 
```

#### 角色

```
# 创建角色，https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/CREATE-ROLE.html
CREATE ROLE manager IDENTIFIED BY 'xxxxxxxxx';
# 修改角色 https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/ALTER-ROLE.html
ALTER ROLE manager IDENTIFIED BY '$$$$$$$$' REPLACE 'xxxxxxxxx';
# 删除角色 https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/DROP-ROLE.html
DROP ROLE manager;
```

#### 权限

```
# 参考https://docs.opengauss.org/zh/docs/6.0.0-lite/docs/SQLReference/GRANT.html
# 将系统权限授权给用户或者角色。
GRANT ALL PRIVILEGES TO joe;
# 撤销权限
REVOKE ALL PRIVILEGES FROM joe;
```



## 运维

```
# 查看状态 需要使用omm用户，其他用户没有配置环境变量，找不到启动命令 
gs_ctl status 
# 启动
gs_ctl start
# 本地登陆
gsql -d postgres -p 5432
# 登陆后就可以创建数据库
create database test
# 创建成功后，查看数据库
\list # \l 也可以
# 切换数据库
\c test
# 创建用户
CREATE USER wnt IDENTIFIED BY 'Wnt.1@3456';
# 创建表空间
CREATE TABLESPACE fastspace RELATIVE LOCATION 'tablespace/tablespace_1';
# 查询全部的表空间
SELECT spcname FROM pg_tablespace;
# 查询表空间使用情况
SELECT PG_TABLESPACE_SIZE('fastspace');
# 数据库系统管理员执行如下命令将“fastspace”表空间的访问权限赋予数据用户wnt。
GRANT CREATE ON TABLESPACE fastspace TO wnt;
# 在指定表空间创建表
CREATE TABLE foo(i int) TABLESPACE fastspace;
# 将数据库所有权限赋给wnt用户
GRANT ALL PRIVILEGES ON DATABASE test TO wnt;
# 设置wnt可以远程访问数据test
vi /opt/software/openGauss/data/pg_hba.conf
# 增加配置
host    test    wnt    0.0.0.0/0    sha256

vi /opt/software/openGauss/dataa/postgresql.conf
listen_addresses = '*'

# 重启
 gs_ctl restart
# 验证登陆
gsql -d test -p 5432 -U wnt -h 192.168.234.168
```



### 主从复制

| **特征**    | **物理复制**                               | **逻辑复制**                                 |
| ----------- | ------------------------------------------ | -------------------------------------------- |
| `wal_level` | `replica` 或 `logical`                     | **必须为 `logical`**                         |
| 系统表      | `pg_publication` 和 `pg_subscription` 为空 | `pg_publication` 和 `pg_subscription` 有记录 |
| 日志关键词  | `streaming WAL`                            | `logical replication`                        |
| 数据目录    | `pg_replslot/` 为空或少量系统槽            | `pg_replslot/` 有用户创建的槽文件            |
| 复制粒度    | 整库级别                                   | **表级控制**                                 |

#### 物理复制

- **原理**：基于 WAL（预写式日志）机制，通过流复制协议实现。主库将数据修改操作记录在 WAL 日志中，备库实时接收这些 WAL 日志，并将其重放到自身的数据文件上，从而保持与主库数据的一致性。
- **特点**：
  - **数据一致性高**：基于物理层面数据块的复制，能确保数据的高度一致性。
  - **复制效率高**：直接复制数据文件来同步数据，速度相比逻辑复制更快。
  - **依赖存储结构**：与数据库的物理结构紧密相关，通常只能在相同或高度兼容的数据库版本和平台之间进行恢复。
  - **备机只读**：一般情况下，备机在物理复制过程中处于只读状态，以保证数据的一致性和同步的准确性。
- **使用场景**：适用于对数据一致性要求高、需要快速同步大量数据的场景，如数据库的主从架构部署、异地灾备等，以实现数据的实时备份和高可用性。

```
# 主从复制
# 先安装主节点 注意这个localPort和remotePort千万不要和openguass本身的服务端口（默认为5432）一样
echo "Wnt.1@3456" | sh ./install.sh --mode primary -D /opt/software/openGauss/data -R  /opt/software/openGauss/install -l /opt/software/openGauss/log  -C "replconninfo1='localhost=192.168.234.170 localport=15432 remotehost=192.168.234.171 remoteport=15432'" --start
# 再安装从节点
echo "Wnt.1@3456" | sh -x ./install.sh --mode standby  -D /opt/software/openGauss/data -R  /opt/software/openGauss/install -l /opt/software/openGauss/log  -C "replconninfo1='localhost=192.168.234.171 localport=15432 remotehost=192.168.234.170 remoteport=15432'" --start

# 查看状态
gs_ctl query
[2025-06-13 09:29:19.202][11407][][gs_ctl]: gs_ctl query ,datadir is /opt/software/openGauss/data
 HA state:
        local_role                     : Primary
        static_connections             : 1
        db_state                       : Normal
        detail_information             : Normal

 Senders info:
        sender_pid                     : 11406
        local_role                     : Primary
        peer_role                      : Standby
        peer_state                     : Normal
        state                          : Streaming
        sender_sent_location           : 0/A000408
        sender_write_location          : 0/A000408
        sender_flush_location          : 0/A000408
        sender_replay_location         : 0/A000408
        receiver_received_location     : 0/A000408
        receiver_write_location        : 0/A000408
        receiver_flush_location        : 0/A000408
        receiver_replay_location       : 0/A000408
        sync_percent                   : 100%
        sync_state                     : Sync
        sync_priority                  : 1
        sync_most_available            : On
        channel                        : 192.168.234.171:15432-->192.168.234.170:44499

 Receiver info:
No information

# 从节点停止后，启动时需要执行这个命令，先修复数据再启动
gs_ctl build -b [mode]

mode的取值：
full：通过全量镜像的方式重新同步DN主机的数据目录。
incremental：通过解析Xlog日志获取主备DN差异的数据进行增量修复备DN。
警告：
增量重建适用于主备双主等因日志造成的不一致场景。
备机数据文件损坏、数据目录丢失等故障通过增量重建的方式无法修复，此时可通过全量重建的方式重新修复备机。
auto(不指定)：先增量，根据失败后是否可以再增量选择继续增量或者全量，三次增量失败后进行全量。
standby_full：使用备机重建备机或容灾场景下的首备重建灾备数据库实例内故障的备机。
copy_secure_files：在流式容灾场景下，获取指定节点数据目录下gs_secure_files目录对应内容。
copy_upgrade_file：在dorado容灾场景下，获取指定节点下升级所需指定文件。
cross_cluster_full：在dorado容灾场景下，通过全量镜像的方式重新同步主机的数据目录。
cross_cluster_incremental：基于共享存储的同城双中心场景下的跨集群增量build。
check：检测是否需要build，如果需要，返回build的类型。
默认值：auto

# 主节点启动
gs_ctl start -M primary

# 当从节点下线时，主节点数据库操作时会卡住，等待从节点上线，如果想让主节点不等待从节点，需要修改配置
vim /opt/software/openGauss/data/postgresql.conf
most_available_sync = off

# 计划内主备切换，即主备节点都正常的情况下
# 在备节点执行（切换至主节点）,自测时发现如果原主节点下线，备节点执行这个命令不会成功
gs_ctl switchover -m fast
# 在主节点执行（切换至备节点）
gs_ctl switchover -m fast -S standby

# 在主节点下线时，备节点快速切换为主节点
gs_ctl failover
```

#### 逻辑复制 

- **原理**：OpenGauss 提供逻辑解码功能，将物理日志反解析为逻辑日志。通过数据复制工具（如 DRS）将逻辑日志转化为 SQL 语句，然后在对端数据库回放，从而实现异构数据库之间的数据同步。
- **特点**：
  - **异构兼容性好**：支持 OpenGauss 与多种异构数据库（如 MySQL、Oracle 等）之间的单向、双向逻辑复制，降低了对目标数据库的形态限制。
  - **基于事务和逻辑操作**：逻辑日志以事务为单位，在事务提交后输出，更关注数据的逻辑变化，可在不同的数据库架构和存储方式之间进行数据复制。
  - **支持部分操作**：从 OpenGauss 6.0.0 - RC1 版本开始支持部分 DDL 操作，但仍有一些限制，例如不支持列存复制。
  - **数据可读写**：目标库在进行数据同步期间可以进行数据的读写操作，数据同步时延低。
- **使用场景**：常用于数据库数据实时迁移、双库双活以及支持滚动升级等场景，能满足不同类型数据库之间数据同步的需求，同时在数据同步过程中不影响目标数据库的正常使用。

```
# 应该用不到，暂时不考虑
```

### 备份与恢复

备份与恢复类型可分为逻辑备份与恢复、物理备份与恢复、闪回恢复。

- 逻辑备份与恢复：通过逻辑导出对数据进行备份，逻辑备份只能基于备份时刻进行数据转储，所以恢复时也只能恢复到备份时保存的数据。对于故障点和备份点之间的数据，逻辑备份无能为力，逻辑备份适合备份那些很少变化的数据，当这些数据因误操作被损坏时，可以通过逻辑备份进行快速恢复。如果通过逻辑备份进行全库恢复，通常需要重建数据库，导入备份数据来完成，对于可用性要求很高的数据库，这种恢复时间太长，通常不被采用。由于逻辑备份具有平台无关性，所以更为常见的是，逻辑备份被作为一个数据迁移及移动的主要手段。

- 闪回恢复：利用回收站的闪回恢复删除的表。数据库的回收站功能类似于windows系统的回收站，将删除的表信息保存到回收站中。利用MVCC机制闪回恢复到指定时间点或者CSN点。

  两种备份恢复类型对比

  | **备份类型**   | **应用场景**                                                 | **支持的介质** | **工具名称** | **恢复时间**                                                 | **优缺点**                                                   |
  | :------------- | :----------------------------------------------------------- | :------------- | :----------- | :----------------------------------------------------------- | :----------------------------------------------------------- |
  | 逻辑备份与恢复 | 适合于数据量小的场景。可以备份单表和多表，单database和所有database。备份后的数据需要使用gsql或者gs_restore工具恢复。数据量大时，恢复需要较长时间。 | 磁盘SSD        | gs_dump      | 纯文本格式数据恢复时间长。归档格式数据恢复时间中等。         | 导出数据库相关信息的工具，用户可以自定义导出一个数据库或其中的对象（模式、表、视图等）。支持导出的数据库可以是默认数据库postgres，也可以是自定义数据库。导出的格式可选择纯文本格式或者归档格式。纯文本格式的数据只能通过gsql进行恢复，恢复时间较长。归档格式的数据只能通过gs_restore进行恢复，恢复时间较纯文本格式短。 |
  |                |                                                              |                | gs_dumpall   | 数据恢复时间长。                                             | 导出所有数据库相关信息工具，它可以导出openGauss数据库的所有数据，包括默认数据库postgres的数据、自定义数据库的数据、以及openGauss所有数据库公共的全局对象。只能导出纯文本格式的数据，导出的数据只能通过gsql进行恢复，恢复时间较长。 |
  | 闪回恢复       | 适用于：1）误删除表的场景；2）需要将表中的数据恢复到指定时间点或者CSN。 |                | 无           | 可以将表的状态恢复到指定时间点或者是表结构删除前的状态，恢复时间快。 | 闪回技术能够有选择性的高效撤销一个已提交事务的影响，从人为错误中恢复。在采用闪回技术之前，只能通过备份恢复、PITR等手段找回已提交的数据库修改，恢复时长需要数分钟甚至数小时。采用闪回技术后，恢复已提交的数据库修改前的数据，只需要秒级，而且恢复时间和数据库大小无关。闪回支持两种恢复模式：基于MVCC多版本的数据恢复：适用于误删除、误更新、误插入数据的查询和恢复，用户通过配置旧版本保留时间，并执行相应的查询或恢复命令，查询或恢复到指定的时间点或CSN点。基于类似windows系统回收站的恢复：适用于误DROP、误TRUNCATE的表的恢复。用户通过配置回收站开关，并执行相应的恢复命令，可以将误DROP、误TRUNCATE的表找回。 |

#### 逻辑备份与恢复

##### 导出

###### gs_dump

```
# 以操作系统用户omm登录数据库主节点，使用gs_dump导出postgres数据库。
gs_dump -U jack -f /home/omm/backup/userdatabase_backup.tar -p 8000 postgres -F t 
Password:
#使用gs_dump同时导出hr和public模式。
gs_dump -U jack -f /home/omm/backup/MPPDB_schema_backup -p 8000 human_resource -n hr -n public -F d 
Password:
#使用gs_dump导出human_resource数据库数据。用户jack不具备导出数据库human_resource的权限，而角色role1具备该权限，要实现导出数据库human_resource，需要将role1赋权给jack，然后可以在导出命令中设置--role角色为role1，使用role1的权限，完成导出目的。导出文件格式为tar归档格式。
gs_dump -U jack -f /home/omm/backup/MPPDB_backup.tar -p 8000 human_resource --role role1 --rolepassword  abc@1234 -F t
Password:
```

**表 1** 常用参数说明

| 参数           | 参数说明                                                     | 举例                                      |
| :------------- | :----------------------------------------------------------- | :---------------------------------------- |
| -U             | 连接数据库的用户名。                                         | -U jack                                   |
| -W             | 指定用户连接的密码。如果主机的认证策略是trust，则不会对数据库管理员进行密码验证，即无需输入-W选项。如果没有-W选项，并且不是数据库管理员，会提示用户输入密码。 | -W abcd@123                               |
| -f             | 将导出文件发送至指定目录文件夹。如果这里省略，则使用标准输出。 | -f /home/omm/backup/MPPDB*_*schema_backup |
| -p             | 指定服务器所侦听的TCP端口或本地Unix域套接字后缀，以确保连接。 | -p 8000                                   |
| dbname         | 需要导出的数据库名称                                         | human_resource                            |
| -n             | 只导出与模式名称匹配的模式，此选项包括模式本身和所有它包含的对象。单个模式：-n *schemaname*多个模式：多次输入-n *schemaname* | 单个模式：-n hr多个模式：-n hr -n public  |
| -F             | 选择导出文件格式。-F参数值如下：p：纯文本格式c：自定义归档d：目录归档格式t：tar归档格式 | -F d                                      |
| --role         | 指定导出使用的角色名。选择该选项，会使导出工具连接数据库后，发起一个SET ROLE角色名命令。当所授权用户（由-U指定）没有导出工具要求的权限时，该选项会起到作用，即切换到具备相应权限的角色。 | -r role1                                  |
| --rolepassword | 指定具体角色用户的角色密码。                                 | --rolepassword abc@1234                   |

###### gs_dumall

```
#以操作系统用户omm登录数据库主节点，使用gs_dumpall一次导出所有数据库信息。
gs_dumpall -U omm -f /home/omm/backup/MPPDB_backup.sql -p 8000 
#使用gs_dumpall导出表空间对象信息。
gs_dumpall -U omm -f /home/omm/backup/MPPDB_tablespace.sql -p 8000 -t
Password:
```

**表 1** 常用参数说明

| 参数 | 参数说明                                                     | 举例                                       |
| :--- | :----------------------------------------------------------- | :----------------------------------------- |
| -U   | 连接数据库的用户名，需要是openGauss管理员用户。              | -U omm                                     |
| -W   | 指定用户连接的密码。如果主机的认证策略是trust，则不会对数据库管理员进行密码验证，即无需输入-W选项；如果没有-W选项，并且不是数据库管理员，会提示用户输入密码。 | -W abcd@123                                |
| -f   | 将导出文件发送至指定目录文件夹。如果这里省略，则使用标准输出。 | -f /home/omm/backup/*MPPDB_tablespace*.sql |
| -p   | 指定服务器所侦听的TCP端口或本地Unix域套接字后缀，以确保连接。 | -p 8000                                    |
| -t   | 或者--tablespaces-only，只转储表空间，不转储数据库或角色。   | -t                                         |

##### 导入

###### gsql

```
gsql -d 数据库名 -h 主机名 -p 端口号 -U 用户名 -f SQL文件路径
```

###### gs_restore

```
#以操作系统用户omm登录数据库主节点,使用gs_restore命令，从postgres整个数据库内容的导出文件中，将数据库的所有对象的定义导入到backupdb。
gs_restore -U jack /home/omm/backup/MPPDB_backup.tar -p 8000 -d backupdb -s -e -c
Password:

#gs_restore 只能导入归档文件
gs_restore -d test -p 5432 -j 4 backup.dump
```

**表 1** 常用参数说明

| 参数 | 参数说明                                                     | 举例        |
| :--- | :----------------------------------------------------------- | :---------- |
| -U   | 连接数据库的用户名。                                         | -U jack     |
| -W   | 指定用户连接的密码。如果主机的认证策略是trust，则不会对数据库管理员进行密码验证，即无需输入-W选项；如果没有-W选项，并且不是数据库管理员，会提示用户输入密码。 | -W abcd@123 |
| -d   | 连接数据库dbname，并直接将数据导入到该数据库中。             | -d backupdb |
| -p   | 指定服务器所侦听的TCP端口或本地Unix域套接字后缀，以确保连接。 | -p 8000     |
| -e   | 当发送SQL语句到数据库时如果出现错误，则退出。默认状态下会忽略错误任务并继续执行导入，且在导入后会显示一系列错误信息。 | -           |
| -c   | 在重新创建数据库对象前，清理（删除）已存在于将要导入的数据库中的数据库对象。 | -           |
| -s   | 只导入模式定义，不导入数据。当前的序列值也不会被导入。       | -           |
| -j   | 运行gs_restore最耗时的部分（如加载数据、创建index、或创建约束）使用并发任务。该选项能大幅缩短导入时间，即将一个大型数据库导入到某一多处理器的服务器上。 | -j 4        |

#### 闪回恢复

```
# 应该用不到，暂时不考虑
```

## 开发

### jdbc

```
## 在pom.xml中添加依赖
<dependency>
    <groupId>org.opengauss</groupId>
    <artifactId>opengauss-jdbc</artifactId>
    <version>6.0.1</version>
</dependency>
## 如果maven的中央仓库中没有这个版本，需要从官网上下载对应的openGauss Connectors jar,手动安装到本地的maven仓库中，注意解压后会有两个jar，安装的是postgresql.ja 驱动配置为org.postgresql.Driver。
mvn install:install-file -Dfile=path/to/your/jar/postgresql.jar  -DgroupId=org.opengauss -DartifactId=opengauss-jdbc -Dversion=6.0.1 -Dpackaging=jar
## 修改jdbc配置文件
spring.datasource.url=jdbc:postgresql://192.168.40.178:5432/soms?currentSchema=public
spring.datasource.username=wnt
spring.datasource.password=Wnt.1@3456
spring.datasource.driver-class-name=org.postgresql.Driver

使用opengauss-jdbc-6.0.1.jar
mvn install:install-file -Dfile=opengauss-jdbc-6.0.1.jar  -DgroupId=org.opengauss -DartifactId=opengauss-jdbc -Dversion=6.0.1 -Dpackaging=jar
## 修改jdbc配置文件， # 这个必须指定db-type为postgresql，同时修改DruidDataSourceProperties、DataSourceConfig对dbType进行复制，否则初始化失败
spring.datasource.url=jdbc:opengauss://192.168.40.115:5432/soms?currentSchema=public
spring.datasource.username=wnt
spring.datasource.password=Wnt.1@3456
spring.datasource.driver-class-name=org.opengauss.Driver
spring.datasource.db-type=postgresql 

```

## 工具

### 数据库管理

#### DBeaver

- 添加驱动，选择数据库-驱动管理器

![img](https://gitee.com/DaKeLuo/image-bed/raw/master/images/20260715115547969.jpeg)

- 点击新建按钮

![img](https://gitee.com/DaKeLuo/image-bed/raw/master/images/20260715115554462.jpeg)

- 设置驱动参数

![img](https://gitee.com/DaKeLuo/image-bed/raw/master/images/20260715115601056.jpeg)

- 添加驱动文件，这个文件可以从官网下connector相关jar

![img](https://gitee.com/DaKeLuo/image-bed/raw/master/images/20260715115605707.jpeg)

- 新建数据库连接

![img](https://gitee.com/DaKeLuo/image-bed/raw/master/images/20260715115609838.jpeg)

- 选择之前添加的驱动

![img](https://gitee.com/DaKeLuo/image-bed/raw/master/images/20260715115614717.jpeg)

- 填写参数

![img](https://gitee.com/DaKeLuo/image-bed/raw/master/images/20260715115621771.jpeg)

## 兼容性

### MySQL

#### 评估工具

##### assessment

[项目目录预览 - Plugin - GitCode](https://gitcode.com/opengauss/Plugin/tree/master/contrib/assessment)

需要编译安装openguass、assessment，编译一直失败，放弃了

