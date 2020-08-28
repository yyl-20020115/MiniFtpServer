#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "configure.h"
#include "parse_conf.h"
#include "ftp_assist.h"

void check_permission()
{
    //root 的uid为0
    if (getuid())
    {
        fprintf(stderr, "FtpServer must be started by root\n");
        exit(EXIT_FAILURE);
    }
}

void setup_signal_chld()
{
    //signal第二个参数是handler，则signal阻塞，执行handler函数
    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR)
        ERR_EXIT("signal");
}
static void handle_sigchld(int sig)
{
    pid_t pid;
    //waitpid挂起当前进程的执行，直到孩子状态发生改变
    //孩子状态发生变化，伴随着孩子进程相关资源的释放，这可以解决僵尸进程
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        --num_of_clients;
        //pid -> ip
        uint32_t* p_ip = hash_lookup_value_by_key(pid_to_ip, &pid, sizeof(pid));
        assert(p_ip != NULL); //ip必须能找到
        uint32_t ip = *p_ip;
        //ip -> clients
        unsigned int* p_value = (unsigned int*)hash_lookup_value_by_key(ip_to_clients, &ip, sizeof(ip));
        assert(p_value != NULL && *p_value > 0);
        --* p_value;

        //释放hash表的结点
        hash_free_entry(pid_to_ip, &pid, sizeof(pid));
    }
}

int main(int argc, const char *argv[])
{
    check_permission();
    setup_signal_chld();

    //解析配置文件
    parseconf_load_file("ftpserver.conf");
    print_conf();

    init_hash();  

    //创建一个监听fd
    int listenfd = tcp_server(tunable_listen_address, tunable_listen_port);

    pid_t pid = 0;
    Session_t sess;
    session_init(&sess);
    p_sess = &sess; //配置全局变量

    while(1)
    {
        //每当用户连接上，就fork一个子进程
       
        struct sockaddr_in addr;
        int peerfd = accept_timeout(listenfd, &addr, tunable_accept_timeout);
        if(peerfd == -1 && errno == ETIMEDOUT)
            continue;
        else if(peerfd == -1)
            ERR_EXIT("accept_timeout");

        //获取ip地址，并在hash中添加一条记录
        uint32_t ip = addr.sin_addr.s_addr;
        sess.ip = ip;
        add_clients_to_hash(&sess, ip);

        if ((pid = fork()) == -1)
        {
            ERR_EXIT("fork");
        }
        else if(pid == 0)
        {
            close(listenfd);

            sess.peer_fd = peerfd;
            limit_num_clients(&sess);
            session_begin(&sess);
            //这里保证每次成功执行后退出循环
            exit(EXIT_SUCCESS);
        }
        else //back to parent
        {
            //pid_to_ip
            add_pid_ip_to_hash(pid, ip);
            close(peerfd);
        }
    }

    return 0;
}

